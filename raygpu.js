/* Browser platform for raygpu. Plain JavaScript; no packages or runtime SDK.
 * C generates vertices and ordered texture batches; this submits them to WebGPU.
 */
"use strict";
(async () => {
    const canvas = document.getElementById("canvas");
    const status = document.getElementById("status");
    const textures = new Map();
    const shaders = new Map();
    const events = new AbortController();
    let device, context, buffer, wasm, pipelines, sampler, textureLayout, uniformLayout, defaultPipelineLayout, pipelineLayout, audioContext, masterGain;
    const sounds = new Map();
    let stopped = false, targetFPS = 60, lastFrame;
    function close() {
        if (stopped) return;
        stopped = true;
        events.abort();
        for (const entry of textures.values()) entry.texture.destroy();
        textures.clear();
        if (buffer) buffer.destroy();
        if (context) context.unconfigure();
        if (device) device.destroy();
        if (audioContext) audioContext.close();
    }
    function fail(error) {
        console.error(error);
        close();
        status.textContent = "WebGPU error: " + (error.message || error);
    }
    try {
        if (!navigator.gpu) throw new Error("WebGPU is unavailable. Use a WebGPU-capable browser on HTTPS or localhost.");
        const adapter = await navigator.gpu.requestAdapter({powerPreference: "high-performance"});
        if (!adapter) throw new Error("No WebGPU adapter was found.");
        device = await adapter.requestDevice();
        device.addEventListener("uncapturederror", event => fail(event.error));
        device.lost.then(info => { if (!stopped) fail(new Error("Device lost: " + info.message)); });
        context = canvas.getContext("webgpu");
        if (!context) throw new Error("Cannot create a WebGPU canvas context.");
        const format = navigator.gpu.getPreferredCanvasFormat();
        const shader = device.createShaderModule({code: `
            struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };
            @group(0) @binding(0) var smp: sampler;
            @group(0) @binding(1) var tex: texture_2d<f32>;
            @vertex fn vs(@location(0) p: vec2f, @location(1) uv: vec2f, @location(2) c: vec4f) -> V {
                var o: V; o.position=vec4f(p,0,1); o.uv=uv; o.color=c; return o;
            }
            @fragment fn fs(v: V) -> @location(0) vec4f { return textureSample(tex,smp,v.uv)*v.color; }
        `});
        const compilation = await shader.getCompilationInfo();
        const errors = compilation.messages.filter(message => message.type === "error");
        if (errors.length) throw new Error(errors.map(message => message.message).join("\n"));
        const blendStates = [
            {color:{srcFactor:"src-alpha",dstFactor:"one-minus-src-alpha",operation:"add"},alpha:{srcFactor:"one",dstFactor:"one-minus-src-alpha",operation:"add"}},
            {color:{srcFactor:"src-alpha",dstFactor:"one",operation:"add"},alpha:{srcFactor:"one",dstFactor:"one",operation:"add"}},
            {color:{srcFactor:"dst",dstFactor:"one-minus-src-alpha",operation:"add"},alpha:{srcFactor:"one",dstFactor:"one-minus-src-alpha",operation:"add"}},
            {color:{srcFactor:"one",dstFactor:"one",operation:"add"},alpha:{srcFactor:"one",dstFactor:"one",operation:"add"}},
            {color:{srcFactor:"one",dstFactor:"one",operation:"reverse-subtract"},alpha:{srcFactor:"one",dstFactor:"one",operation:"add"}},
            {color:{srcFactor:"one",dstFactor:"one-minus-src-alpha",operation:"add"},alpha:{srcFactor:"one",dstFactor:"one-minus-src-alpha",operation:"add"}}
        ];
        textureLayout=device.createBindGroupLayout({entries:[
            {binding:0,visibility:GPUShaderStage.FRAGMENT,sampler:{type:"filtering"}},
            {binding:1,visibility:GPUShaderStage.FRAGMENT,texture:{sampleType:"float",viewDimension:"2d"}}
        ]});
        uniformLayout=device.createBindGroupLayout({entries:[{binding:0,visibility:GPUShaderStage.VERTEX|GPUShaderStage.FRAGMENT,
            buffer:{type:"uniform",minBindingSize:2048}}]});
        defaultPipelineLayout=device.createPipelineLayout({bindGroupLayouts:[textureLayout]});
        pipelineLayout=device.createPipelineLayout({bindGroupLayouts:[textureLayout,uniformLayout]});
        const pipelineDescriptor = blend => ({layout:defaultPipelineLayout,
            vertex:{module:shader,entryPoint:"vs",buffers:[{arrayStride:20,attributes:[
                {shaderLocation:0,offset:0,format:"float32x2"},{shaderLocation:1,offset:8,format:"float32x2"},
                {shaderLocation:2,offset:16,format:"unorm8x4"}]}]},
            fragment:{module:shader,entryPoint:"fs",targets:[{format,blend}]},
            primitive:{topology:"triangle-list",cullMode:"none"}});
        pipelines = await Promise.all(blendStates.map(blend => device.createRenderPipelineAsync(pipelineDescriptor(blend))));
        buffer = device.createBuffer({size: 262144 * 20, usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST});
        sampler = device.createSampler({magFilter: "nearest", minFilter: "nearest",
            addressModeU: "repeat", addressModeV: "repeat"});
        const textDecoder = new TextDecoder();
        const fileCache = new Map();
        function readText(pointer) {
            const bytes = new Uint8Array(wasm.memory.buffer);
            let end = pointer;
            while (end < bytes.length && bytes[end]) end++;
            return textDecoder.decode(bytes.subarray(pointer, end));
        }
        function loadFile(pointer) {
            const name = readText(pointer);
            if (fileCache.has(name)) return fileCache.get(name);
            const request = new XMLHttpRequest();
            request.open("GET", name, false);
            /* Browsers forbid setting responseType=arraybuffer on synchronous
             * document XHR. x-user-defined preserves every response byte in
             * the low 8 bits of responseText, so C can still load files with
             * a synchronous raylib-style API. */
            request.overrideMimeType("text/plain; charset=x-user-defined");
            try { request.send(); } catch (_) { return null; }
            if (request.status !== 0 && (request.status < 200 || request.status >= 300)) return null;
            const response = request.responseText;
            const data = new Uint8Array(response.length);
            for (let i = 0; i < response.length; i++) data[i] = response.charCodeAt(i) & 255;
            if (data) fileCache.set(name, data);
            return data;
        }
        const imports = {raygpu: {
            log: pointer => { const text = readText(pointer); console.log(text); status.textContent = text; },
            now: () => performance.now(),
            sin: Math.sin, cos: Math.cos,
            fps: fps => { targetFPS = fps; },
            audio_init: () => {
                if (!audioContext) {
                    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
                    if (!AudioContextClass) return 0;
                    audioContext = new AudioContextClass();
                    masterGain = audioContext.createGain();
                    masterGain.connect(audioContext.destination);
                }
                return 1;
            },
            audio_close: () => {
                for (const sound of sounds.values()) if (sound.source) sound.source.stop();
                sounds.clear();
                if (audioContext) audioContext.close();
                audioContext = masterGain = null;
            },
            audio_load: (id, pointer, frames, rate, bits, channels) => {
                if (!audioContext || !frames || !channels) return;
                const view = new DataView(wasm.memory.buffer, pointer, frames*channels*(bits/8));
                const decoded = audioContext.createBuffer(channels, frames, rate);
                for (let channel = 0; channel < channels; channel++) {
                    const output = decoded.getChannelData(channel);
                    for (let frame = 0; frame < frames; frame++) {
                        const sample = frame*channels + channel;
                        output[frame] = bits === 8 ? (view.getUint8(sample)-128)/128 :
                            bits === 16 ? view.getInt16(sample*2, true)/32768 : view.getFloat32(sample*4, true);
                    }
                }
                const previous = sounds.get(id);
                if (previous && previous.source) previous.source.stop();
                const gain = audioContext.createGain(), pan = audioContext.createStereoPanner();
                gain.connect(pan); pan.connect(masterGain);
                sounds.set(id, {buffer:decoded, source:null, gain, pan, volume:1, pitch:1,
                    offset:0, startedAt:0, paused:false, stopping:false});
            },
            audio_unload: id => {
                const sound = sounds.get(id);
                if (sound && sound.source) { sound.stopping=true; sound.source.stop(); }
                sounds.delete(id);
            },
            audio_command: (id, command, value) => {
                if (command === 7) { if (masterGain) masterGain.gain.value=value; return; }
                const sound = sounds.get(id); if (!sound || !audioContext) return;
                const start = () => {
                    const source=audioContext.createBufferSource(); source.buffer=sound.buffer;
                    source.playbackRate.value=sound.pitch; source.connect(sound.gain);
                    sound.source=source; sound.startedAt=audioContext.currentTime;
                    sound.stopping=false; sound.paused=false;
                    source.onended=()=>{if(sound.source===source){sound.source=null;if(!sound.paused)sound.offset=0;}};
                    source.start(0,Math.min(sound.offset,sound.buffer.duration));
                };
                if (command === 0) { if(sound.source){sound.stopping=true;sound.source.stop();} sound.offset=0; audioContext.resume(); start(); }
                else if (command === 1) { if(sound.source){sound.stopping=true;sound.source.stop();} sound.source=null;sound.offset=0;sound.paused=false; }
                else if (command === 2 && sound.source) { sound.offset+=(audioContext.currentTime-sound.startedAt)*sound.pitch;sound.paused=true;sound.stopping=true;sound.source.stop();sound.source=null; }
                else if (command === 3 && sound.paused) { audioContext.resume();start(); }
                else if (command === 4) { sound.volume=value;sound.gain.gain.value=value; }
                else if (command === 5 && value>0) { sound.pitch=value;if(sound.source)sound.source.playbackRate.value=value; }
                else if (command === 6) sound.pan.pan.value=value*2-1;
            },
            audio_playing: id => { const sound=sounds.get(id); return sound&&sound.source&&!sound.paused ? 1 : 0; },
            file_size: name => { const data = loadFile(name); return data ? data.length : 0; },
            file_read: (name, destination, size) => {
                const data = loadFile(name);
                if (!data || data.length !== size) return 0;
                new Uint8Array(wasm.memory.buffer, destination, size).set(data);
                return size;
            },
            init: (width, height, title) => {
                canvas.width = width; canvas.height = height;
                document.title = readText(title);
                context.configure({device, format, alphaMode: "opaque"});
            },
            window_command: (command, a, b, text) => {
                if (command === 0) document.title = readText(text);
                else if (command === 1 && a > 0 && b > 0) { canvas.width = a; canvas.height = b; }
            },
            texture: (id, pointer, width, height) => {
                const texture = device.createTexture({size: [width, height], format: "rgba8unorm",
                    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST});
                device.queue.writeTexture({texture}, new Uint8Array(wasm.memory.buffer, pointer, width*height*4),
                    {bytesPerRow: width*4, rowsPerImage: height}, [width, height]);
                const view = texture.createView();
                const group = device.createBindGroup({layout: textureLayout, entries: [
                    {binding: 0, resource: sampler}, {binding: 1, resource: view}
                ]});
                textures.set(id, {texture, view, group, sampler, width, height});
            },
            render_texture: (id, width, height) => {
                const texture=device.createTexture({size:[width,height],format,
                    usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC});
                const view=texture.createView();
                const group=device.createBindGroup({layout:textureLayout,entries:[
                    {binding:0,resource:sampler},{binding:1,resource:view}
                ]});
                textures.set(id,{texture,view,group,sampler,width,height});
            },
            texture_update: (id, x, y, width, height, pointer) => {
                const entry = textures.get(id);
                if (!entry || width <= 0 || height <= 0) return;
                device.queue.writeTexture({texture: entry.texture, origin: [x, y, 0]},
                    new Uint8Array(wasm.memory.buffer, pointer, width*height*4),
                    {bytesPerRow: width*4, rowsPerImage: height}, [width, height, 1]);
            },
            texture_params: (id, filter, wrap) => {
                const entry = textures.get(id);
                if (!entry) return;
                const linear = filter !== 0;
                const addressMode = wrap === 0 ? "repeat" : wrap === 2 ? "mirror-repeat" : "clamp-to-edge";
                entry.sampler = device.createSampler({
                    magFilter: linear ? "linear" : "nearest",
                    minFilter: linear ? "linear" : "nearest",
                    mipmapFilter: linear ? "linear" : "nearest",
                    addressModeU: addressMode, addressModeV: addressMode,
                    maxAnisotropy: filter >= 3 ? Math.min(16, 4 << (filter - 3)) : 1
                });
                entry.group = device.createBindGroup({layout: textureLayout, entries: [
                    {binding: 0, resource: entry.sampler}, {binding: 1, resource: entry.view}
                ]});
            },
            unload: id => { const entry = textures.get(id); if (entry) entry.texture.destroy(); textures.delete(id); },
            shader_load: (id, vsPointer, fsPointer) => {
                try {
                    const vs=device.createShaderModule({code:readText(vsPointer)}),fs=device.createShaderModule({code:readText(fsPointer)});
                    const list=blendStates.map(blend=>device.createRenderPipeline({layout:pipelineLayout,
                        vertex:{module:vs,entryPoint:"vs",buffers:[{arrayStride:20,attributes:[
                            {shaderLocation:0,offset:0,format:"float32x2"},{shaderLocation:1,offset:8,format:"float32x2"},
                            {shaderLocation:2,offset:16,format:"unorm8x4"}]}]},
                        fragment:{module:fs,entryPoint:"fs",targets:[{format,blend}]},primitive:{topology:"triangle-list",cullMode:"none"}}));
                    const uniformBuffer=device.createBuffer({size:2048,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
                    const uniformGroup=device.createBindGroup({layout:uniformLayout,entries:[{binding:0,resource:{buffer:uniformBuffer,size:2048}}]});
                    shaders.set(id,{pipelines:list,uniformBuffer,uniformGroup});return 1;
                } catch(error) { console.error(error);return 0; }
            },
            shader_unload: id => { const shader=shaders.get(id);if(shader)shader.uniformBuffer.destroy();shaders.delete(id); },
            shader_uniform: (id, location, pointer, size) => {
                const shader=shaders.get(id);if(!shader||location<0||location>=32)return;
                device.queue.writeBuffer(shader.uniformBuffer,location*64,new Uint8Array(wasm.memory.buffer,pointer,size));
            },
            present: (vertices, count, batches, batchCount, color, targetId) => {
                if (stopped) return;
                const target=targetId ? textures.get(targetId) : null;
                if (targetId && !target) return;
                const encoder = device.createCommandEncoder();
                const pass = encoder.beginRenderPass({colorAttachments: [{
                    view: target ? target.view : context.getCurrentTexture().createView(), loadOp: "clear", storeOp: "store",
                    clearValue: [(color & 255)/255, ((color>>>8)&255)/255, ((color>>>16)&255)/255, (color>>>24)/255]
                }]});
                if (count) {
                    device.queue.writeBuffer(buffer, 0, new Uint8Array(wasm.memory.buffer, vertices, count*20));
                    pass.setVertexBuffer(0, buffer, 0, count*20);
                    const commands = new Uint32Array(wasm.memory.buffer, batches, batchCount*9);
                    for (let i=0; i<commands.length; i+=9) {
                        const entry = textures.get(commands[i+2]);
                        if (!entry || entry===target) continue;
                        const targetWidth=target?target.width:canvas.width,targetHeight=target?target.height:canvas.height;
                        const x=Math.min(commands[i+5],targetWidth),y=Math.min(commands[i+6],targetHeight);
                        const width=Math.min(commands[i+7],targetWidth-x),height=Math.min(commands[i+8],targetHeight-y);
                        if (!width || !height) continue;
                        const shader=shaders.get(commands[i+4]);
                        const selected=shader?shader.pipelines:pipelines;
                        pass.setPipeline(selected[commands[i+3]] || selected[0]);
                        if(shader)pass.setBindGroup(1,shader.uniformGroup);
                        pass.setScissorRect(x,y,width,height);
                        pass.setBindGroup(0, entry.group); pass.draw(commands[i+1], 1, commands[i], 0);
                    }
                }
                pass.end(); device.queue.submit([encoder.finish()]);
            },
            close: () => { close(); status.textContent = "Demo closed. Reload to restart."; }
        }};
        const response = await fetch("main.wasm");
        if (!response.ok) throw new Error("Cannot load main.wasm (HTTP " + response.status + ").");
        const result = await WebAssembly.instantiate(await response.arrayBuffer(), imports);
        wasm = result.instance.exports;
        if (wasm.main() !== 0) throw new Error("C initialization failed.");
        const keyCodes = {
            Space:32, Quote:39, Comma:44, Minus:45, Period:46, Slash:47,
            Semicolon:59, Equal:61, BracketLeft:91, Backslash:92,
            BracketRight:93, Backquote:96,
            Escape:256, Enter:257, Tab:258, Backspace:259, Insert:260,
            Delete:261, ArrowRight:262, ArrowLeft:263, ArrowDown:264,
            ArrowUp:265, PageUp:266, PageDown:267, Home:268, End:269,
            CapsLock:280, ScrollLock:281, NumLock:282, PrintScreen:283,
            Pause:284, F1:290, F2:291, F3:292, F4:293, F5:294, F6:295,
            F7:296, F8:297, F9:298, F10:299, F11:300, F12:301,
            NumpadDecimal:330, NumpadDivide:331, NumpadMultiply:332,
            NumpadSubtract:333, NumpadAdd:334, NumpadEnter:335,
            NumpadEqual:336, ShiftLeft:340, ControlLeft:341, AltLeft:342,
            MetaLeft:343, ShiftRight:344, ControlRight:345, AltRight:346,
            MetaRight:347, ContextMenu:348, AudioVolumeUp:24, AudioVolumeDown:25
        };
        function raygpuKey(code) {
            if (/^Key[A-Z]$/.test(code)) return code.charCodeAt(3);
            if (/^Digit[0-9]$/.test(code)) return code.charCodeAt(5);
            if (/^Numpad[0-9]$/.test(code)) return 320 + Number(code.charAt(6));
            return keyCodes[code] || 0;
        }
        function keyboard(event) {
            const key = raygpuKey(event.code);
            if (key) {
                if (event.type === "keydown" && audioContext && audioContext.state === "suspended") audioContext.resume();
                wasm.raygpu_key(key, event.type === "keydown" ? 1 : 0);
                if (event.type === "keydown" && !event.ctrlKey && !event.altKey && !event.metaKey) {
                    const characters=Array.from(event.key);
                    if (characters.length === 1) wasm.raygpu_char(characters[0].codePointAt(0));
                }
                event.preventDefault();
            }
        }
        window.addEventListener("keydown", keyboard, {signal: events.signal});
        window.addEventListener("keyup", keyboard, {signal: events.signal});
        window.addEventListener("focus", () => wasm.raygpu_focus(1), {signal: events.signal});
        window.addEventListener("blur", () => { wasm.raygpu_focus(0); wasm.raygpu_blur(); }, {signal: events.signal});
        document.addEventListener("visibilitychange", () => {
            if (document.hidden) wasm.raygpu_blur();
            lastFrame = undefined;
        }, {signal: events.signal});
        const button = value => value === 2 ? 1 : value === 1 ? 2 : value === 0 ? 0 : -1;
        function mouse(event) {
            const bounds = canvas.getBoundingClientRect();
            if (!bounds.width || !bounds.height) return;
            wasm.raygpu_mouse((event.clientX-bounds.left)*canvas.width/bounds.width,
                (event.clientY-bounds.top)*canvas.height/bounds.height,
                event.type === "pointermove" ? -1 : button(event.button), event.type === "pointerdown" ? 1 : 0);
            if (event.type === "pointerdown") {
                if (audioContext && audioContext.state === "suspended") audioContext.resume();
                canvas.focus(); canvas.setPointerCapture(event.pointerId);
            }
        }
        for (const type of ["pointermove", "pointerdown", "pointerup"]) canvas.addEventListener(type, mouse, {signal: events.signal});
        canvas.addEventListener("wheel", event => {
            wasm.raygpu_wheel(-Math.sign(event.deltaX), -Math.sign(event.deltaY));
            event.preventDefault();
        }, {signal: events.signal, passive: false});
        canvas.addEventListener("pointercancel", () => wasm.raygpu_blur(), {signal: events.signal});
        canvas.addEventListener("contextmenu", event => event.preventDefault(), {signal: events.signal});
        function frame(timestamp) {
            if (stopped) return;
            const interval = targetFPS > 0 ? 1000/targetFPS : 0;
            if (lastFrame === undefined || timestamp-lastFrame >= interval-0.5) {
                lastFrame = timestamp;
                try { if (!wasm.raygpu_frame()) return; } catch (error) { fail(error); return; }
            }
            requestAnimationFrame(frame);
        }
        requestAnimationFrame(frame);
    } catch (error) { fail(error); }
})();
