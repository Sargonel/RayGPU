/* Browser platform for raygpu. Plain JavaScript; no packages or runtime SDK.
 * C generates vertices and ordered texture batches; this submits them to WebGPU.
 */
"use strict";
(async () => {
    const canvas = document.getElementById("canvas");
    const status = document.getElementById("status");
    const textures = new Map();
    const events = new AbortController();
    let device, context, buffer, wasm, pipeline, sampler;
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
        pipeline = await device.createRenderPipelineAsync({
            layout: "auto",
            vertex: {module: shader, entryPoint: "vs", buffers: [{arrayStride: 20, attributes: [
                {shaderLocation: 0, offset: 0, format: "float32x2"},
                {shaderLocation: 1, offset: 8, format: "float32x2"},
                {shaderLocation: 2, offset: 16, format: "unorm8x4"}
            ]}]},
            fragment: {module: shader, entryPoint: "fs", targets: [{format, blend: {
                color: {srcFactor: "src-alpha", dstFactor: "one-minus-src-alpha", operation: "add"},
                alpha: {srcFactor: "one", dstFactor: "one-minus-src-alpha", operation: "add"}
            }}]},
            primitive: {topology: "triangle-list", cullMode: "none"}
        });
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
            texture: (id, pointer, width, height) => {
                const texture = device.createTexture({size: [width, height], format: "rgba8unorm",
                    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST});
                device.queue.writeTexture({texture}, new Uint8Array(wasm.memory.buffer, pointer, width*height*4),
                    {bytesPerRow: width*4, rowsPerImage: height}, [width, height]);
                const view = texture.createView();
                const group = device.createBindGroup({layout: pipeline.getBindGroupLayout(0), entries: [
                    {binding: 0, resource: sampler}, {binding: 1, resource: view}
                ]});
                textures.set(id, {texture, view, group, sampler});
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
                entry.group = device.createBindGroup({layout: pipeline.getBindGroupLayout(0), entries: [
                    {binding: 0, resource: entry.sampler}, {binding: 1, resource: entry.view}
                ]});
            },
            unload: id => { const entry = textures.get(id); if (entry) entry.texture.destroy(); textures.delete(id); },
            present: (vertices, count, batches, batchCount, color) => {
                if (stopped) return;
                const encoder = device.createCommandEncoder();
                const pass = encoder.beginRenderPass({colorAttachments: [{
                    view: context.getCurrentTexture().createView(), loadOp: "clear", storeOp: "store",
                    clearValue: [(color & 255)/255, ((color>>>8)&255)/255, ((color>>>16)&255)/255, (color>>>24)/255]
                }]});
                if (count) {
                    device.queue.writeBuffer(buffer, 0, new Uint8Array(wasm.memory.buffer, vertices, count*20));
                    pass.setPipeline(pipeline); pass.setVertexBuffer(0, buffer, 0, count*20);
                    const commands = new Uint32Array(wasm.memory.buffer, batches, batchCount*3);
                    for (let i=0; i<commands.length; i+=3) {
                        const entry = textures.get(commands[i+2]);
                        if (!entry) continue;
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
            if (key) { wasm.raygpu_key(key, event.type === "keydown" ? 1 : 0); event.preventDefault(); }
        }
        window.addEventListener("keydown", keyboard, {signal: events.signal});
        window.addEventListener("keyup", keyboard, {signal: events.signal});
        window.addEventListener("blur", () => wasm.raygpu_blur(), {signal: events.signal});
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
            if (event.type === "pointerdown") { canvas.focus(); canvas.setPointerCapture(event.pointerId); }
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
