/* Browser platform for sargpu. Plain JavaScript; no packages or runtime SDK.
 * C generates vertices and ordered texture batches; this submits them to WebGPU.
 */
"use strict";
(async () => {
    const canvas = document.getElementById("canvas");
    const status = document.getElementById("status");
    const textures = new Map();
    const shaders = new Map();
    const meshes = new Map();
    const events = new AbortController();
    let device, context, buffer, instanceBuffer3d, sceneBuffer3d, boneBuffer3d, sceneGroup3d, defaultUniformBuffer3d, defaultUniformGroup3d, wasm, pipelines, pipeline3d, skyboxPipeline, sampler, textureLayout, uniformLayout, shaderTextureLayout, sceneLayout3d, defaultPipelineLayout, pipelineLayout, materialPipelineLayout, audioContext, masterGain;
    let depthTexture, depthWidth=0, depthHeight=0;
    const sounds = new Map();
    let stopped = false, targetFPS = 60, lastFrame, fullscreenPending = 0, fullscreenMode = 0;
    let cursorOnCanvas = false, cursorHidden = false, cursorDisabled = false, cursorShape = 0, pointerLockPending = false, mouseX = 0, mouseY = 0;
    let displayWidth = 0, displayHeight = 0, displayDpr = 0, displayViewportWidth = 0, displayViewportHeight = 0;
    function updateCanvasDisplay(force = false) {
        const dpr = Math.max(window.devicePixelRatio || 1, 0.01);
        const viewportWidth = window.innerWidth;
        const viewportHeight = window.innerHeight;
        if (!force && displayWidth === canvas.width && displayHeight === canvas.height && displayDpr === dpr &&
            displayViewportWidth === viewportWidth && displayViewportHeight === viewportHeight) return;
        const physicalWidth = canvas.width/dpr;
        const physicalHeight = canvas.height/dpr;
        const scale = Math.min(1, viewportWidth/physicalWidth, viewportHeight/physicalHeight);
        canvas.style.width = `${physicalWidth*scale}px`;
        canvas.style.height = `${physicalHeight*scale}px`;
        displayWidth = canvas.width; displayHeight = canvas.height; displayDpr = dpr;
        displayViewportWidth = viewportWidth; displayViewportHeight = viewportHeight;
    }
    function retryFullscreen() {
        if (!fullscreenPending || document.fullscreenElement || !canvas.requestFullscreen) return;
        const mode = fullscreenPending; fullscreenPending = 0; fullscreenMode = mode;
        canvas.requestFullscreen().catch(() => { fullscreenPending = mode; fullscreenMode = 0; });
    }
    const cursorNames = ["default","default","text","crosshair","pointer","ew-resize","ns-resize","nwse-resize","nesw-resize","move","not-allowed"];
    function applyCursor() { canvas.style.cursor = cursorHidden || cursorDisabled || document.pointerLockElement === canvas ? "none" : (cursorNames[cursorShape] || "default"); }
    function retryPointerLock() {
        if (!pointerLockPending || document.pointerLockElement === canvas || !canvas.requestPointerLock) return;
        pointerLockPending = false;
        try { const result = canvas.requestPointerLock();if (result && result.catch) result.catch(() => { pointerLockPending = true; }); }
        catch (_) { pointerLockPending = true; }
    }
    function close() {
        if (stopped) return;
        stopped = true;
        events.abort();
        for (const entry of textures.values()) entry.texture.destroy();
        textures.clear();
        for (const mesh of meshes.values()) { mesh.vertexBuffer.destroy(); if (mesh.indexBuffer) mesh.indexBuffer.destroy(); }
        meshes.clear();
        if (buffer) buffer.destroy();
        if (instanceBuffer3d) instanceBuffer3d.destroy();
        if (sceneBuffer3d) sceneBuffer3d.destroy();
        if (boneBuffer3d) boneBuffer3d.destroy();
        if (defaultUniformBuffer3d) defaultUniformBuffer3d.destroy();
        if (depthTexture) depthTexture.destroy();
        if (context) context.unconfigure();
        if (device) device.destroy();
        if (audioContext) audioContext.close();
    }
    function fail(error) {
        console.error(error && error.message ? error.message : String(error));
        close();
        status.textContent = "WebGPU error: " + (error.message || error);
    }
    function rebuildShaderTextures(shader) {
        const fallback = textures.values().next().value;
        if (!shader || !fallback || !shaderTextureLayout) return false;
        const entries = [];
        for (let slot=0; slot<8; slot++) {
            const texture = textures.get(shader.textureIds[slot]) || fallback;
            entries.push({binding:slot*2,resource:texture.sampler || sampler},{binding:slot*2+1,resource:texture.view});
        }
        shader.textureGroup = device.createBindGroup({layout:shaderTextureLayout,entries});
        return true;
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
            @vertex fn vs(@location(0) p: vec2f, @location(1) uv: vec2f, @location(2) c: vec4f, @location(3) z: f32) -> V {
                var o: V; o.position=vec4f(p,z,1); o.uv=uv; o.color=c; return o;
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
        shaderTextureLayout=device.createBindGroupLayout({entries:Array.from({length:16},(_,binding)=>binding%2===0?
            {binding,visibility:GPUShaderStage.FRAGMENT,sampler:{type:"filtering"}}:
            {binding,visibility:GPUShaderStage.FRAGMENT,texture:{sampleType:"float",viewDimension:"2d"}})});
        sceneLayout3d=device.createBindGroupLayout({entries:[
            {binding:0,visibility:GPUShaderStage.VERTEX|GPUShaderStage.FRAGMENT,buffer:{type:"uniform",minBindingSize:704}},
            {binding:1,visibility:GPUShaderStage.VERTEX,buffer:{type:"read-only-storage",minBindingSize:64}}
        ]});
        defaultPipelineLayout=device.createPipelineLayout({bindGroupLayouts:[textureLayout]});
        pipelineLayout=device.createPipelineLayout({bindGroupLayouts:[textureLayout,uniformLayout,shaderTextureLayout]});
        materialPipelineLayout=device.createPipelineLayout({bindGroupLayouts:[textureLayout,uniformLayout,shaderTextureLayout,sceneLayout3d]});
        const pipelineDescriptor = blend => ({layout:defaultPipelineLayout,
            vertex:{module:shader,entryPoint:"vs",buffers:[{arrayStride:24,attributes:[
                {shaderLocation:0,offset:0,format:"float32x2"},{shaderLocation:1,offset:8,format:"float32x2"},
                {shaderLocation:2,offset:20,format:"unorm8x4"},{shaderLocation:3,offset:16,format:"float32"}]}]},
            fragment:{module:shader,entryPoint:"fs",targets:[{format,blend}]},
            primitive:{topology:"triangle-list",cullMode:"none"},
            depthStencil:{format:"depth24plus",depthWriteEnabled:true,depthCompare:"less-equal"}});
        pipelines = await Promise.all(blendStates.map(blend => device.createRenderPipelineAsync(pipelineDescriptor(blend))));
        buffer = device.createBuffer({size: 262144 * 24, usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST});
        const shader3d = device.createShaderModule({code: `
            struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f,
                @location(2) normal: vec3f, @location(3) world: vec3f, @location(4) camera: vec3f };
            @group(0) @binding(0) var smp: sampler; @group(0) @binding(1) var tex: texture_2d<f32>;
            @vertex fn vs(@location(0) p: vec3f, @location(1) n: vec3f, @location(2) uv: vec2f, @location(3) c: vec4f,
                @location(4) m0: vec4f, @location(5) m1: vec4f, @location(6) m2: vec4f, @location(7) m3: vec4f,
                @location(8) v0: vec4f, @location(9) v1: vec4f, @location(10) v2: vec4f, @location(11) v3: vec4f,
                @location(12) tint: vec4f, @location(13) camera: vec3f) -> V {
                let model=mat4x4f(m0,m1,m2,m3); let vp=mat4x4f(v0,v1,v2,v3); let world=vec4f(p,1)*model;
                var o: V; o.position=world*vp; o.uv=uv; o.color=c*tint;
                let c0=cross(m1.xyz,m2.xyz); let c1=cross(m2.xyz,m0.xyz); let c2=cross(m0.xyz,m1.xyz);
                let handed=select(-1.0,1.0,dot(m0.xyz,c0)>=0);
                o.normal=normalize(vec3f(dot(n,c0),dot(n,c1),dot(n,c2))*handed);
                o.world=world.xyz; o.camera=camera; return o;
            }
            @fragment fn fs(v: V, @builtin(front_facing) front: bool) -> @location(0) vec4f {
                var normal=normalize(v.normal); if (!front) { normal=-normal; }
                let light=normalize(vec3f(0.45,0.85,0.35)); let diffuse=max(dot(normal,light),0);
                let view=normalize(v.camera-v.world); let halfVector=normalize(light+view);
                let specular=pow(max(dot(normal,halfVector),0),32)*0.22;
                let albedo=textureSample(tex,smp,v.uv)*v.color; let lighting=0.22+0.78*diffuse;
                return vec4f(albedo.rgb*lighting+specular*albedo.a,albedo.a);
            }
        `});
        pipeline3d = await device.createRenderPipelineAsync({layout:defaultPipelineLayout,
            vertex:{module:shader3d,entryPoint:"vs",buffers:[
                {arrayStride:36,stepMode:"vertex",attributes:[
                    {shaderLocation:0,offset:0,format:"float32x3"},{shaderLocation:1,offset:12,format:"float32x3"},
                    {shaderLocation:2,offset:24,format:"float32x2"},{shaderLocation:3,offset:32,format:"unorm8x4"}]},
                {arrayStride:160,stepMode:"instance",attributes:[
                    {shaderLocation:4,offset:0,format:"float32x4"},{shaderLocation:5,offset:16,format:"float32x4"},
                    {shaderLocation:6,offset:32,format:"float32x4"},{shaderLocation:7,offset:48,format:"float32x4"},
                    {shaderLocation:8,offset:64,format:"float32x4"},{shaderLocation:9,offset:80,format:"float32x4"},
                    {shaderLocation:10,offset:96,format:"float32x4"},{shaderLocation:11,offset:112,format:"float32x4"},
                    {shaderLocation:12,offset:128,format:"unorm8x4"},{shaderLocation:13,offset:144,format:"float32x3"}]}]},
            fragment:{module:shader3d,entryPoint:"fs",targets:[{format,blend:blendStates[0]}]},
            primitive:{topology:"triangle-list",cullMode:"none"},
            depthStencil:{format:"depth24plus",depthWriteEnabled:true,depthCompare:"less"}});
        instanceBuffer3d=device.createBuffer({size:8192*128,usage:GPUBufferUsage.VERTEX|GPUBufferUsage.COPY_DST});
        sceneBuffer3d=device.createBuffer({size:704,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
        boneBuffer3d=device.createBuffer({size:16384*64,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST});
        sceneGroup3d=device.createBindGroup({layout:sceneLayout3d,entries:[{binding:0,resource:{buffer:sceneBuffer3d,size:704}},{binding:1,resource:{buffer:boneBuffer3d,size:16384*64}}]});
        defaultUniformBuffer3d=device.createBuffer({size:2048,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});defaultUniformGroup3d=device.createBindGroup({layout:uniformLayout,entries:[{binding:0,resource:{buffer:defaultUniformBuffer3d,size:2048}}]});
        const advanced3d=device.createShaderModule({code:`
struct Light{positionType:vec4f,directionRange:vec4f,colorIntensity:vec4f,spotEnabled:vec4f};
struct Scene{vp:mat4x4f,camera:vec4f,ambient:vec4f,fogColor:vec4f,fog:vec4f,skyRight:vec4f,skyUp:vec4f,skyForward:vec4f,settings:vec4f,lights:array<Light,8>};
struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)uv2:vec2f,@location(2)color:vec4f,@location(3)normal:vec3f,@location(4)tangent:vec4f,@location(5)world:vec3f,@location(6)material:vec4f,@location(7)emission:vec4f,@location(8)@interpolate(flat)flags:u32};
@group(2)@binding(0)var s0:sampler;@group(2)@binding(1)var t0:texture_2d<f32>;@group(2)@binding(2)var s1:sampler;@group(2)@binding(3)var t1:texture_2d<f32>;@group(2)@binding(4)var s2:sampler;@group(2)@binding(5)var t2:texture_2d<f32>;@group(2)@binding(6)var s3:sampler;@group(2)@binding(7)var t3:texture_2d<f32>;@group(2)@binding(8)var s4:sampler;@group(2)@binding(9)var t4:texture_2d<f32>;@group(2)@binding(10)var s5:sampler;@group(2)@binding(11)var t5:texture_2d<f32>;@group(2)@binding(12)var s6:sampler;@group(2)@binding(13)var t6:texture_2d<f32>;@group(2)@binding(14)var s7:sampler;@group(2)@binding(15)var t7:texture_2d<f32>;
@group(3)@binding(0)var<uniform>scene:Scene;@group(3)@binding(1)var<storage,read>bones:array<mat4x4f>;
@vertex fn vs(@location(0)p0:vec3f,@location(1)n0:vec3f,@location(2)uv:vec2f,@location(3)c:vec4f,@location(4)ids:vec4u,@location(5)weights:vec4f,@location(6)tan:vec4f,@location(7)uv2:vec2f,@location(8)m0:vec4f,@location(9)m1:vec4f,@location(10)m2:vec4f,@location(11)m3:vec4f,@location(12)tint:vec4f,@location(13)material:vec4f,@location(14)emission:vec4f,@location(15)skin:vec4u)->V{var p=vec4f(p0,1);var n=n0;var tangent=tan.xyz;if(skin.y>0u&&dot(weights,vec4f(1))>0){let sm=bones[skin.x+ids.x]*weights.x+bones[skin.x+ids.y]*weights.y+bones[skin.x+ids.z]*weights.z+bones[skin.x+ids.w]*weights.w;p=p*sm;n=(vec4f(n,0)*sm).xyz;tangent=(vec4f(tangent,0)*sm).xyz;}let model=mat4x4f(m0,m1,m2,m3);let world=p*model;var o:V;o.position=world*scene.vp;o.uv=uv;o.uv2=uv2;o.color=c*tint;o.normal=normalize((vec4f(n,0)*model).xyz);o.tangent=vec4f(normalize((vec4f(tangent,0)*model).xyz),tan.w);o.world=world.xyz;o.material=material;o.emission=emission;o.flags=skin.z;return o;}
fn F(c:f32,f0:vec3f)->vec3f{return f0+(vec3f(1)-f0)*pow(1-c,5);}fn D(nh:f32,r:f32)->f32{let a=r*r;let a2=a*a;let d=nh*nh*(a2-1)+1;return a2/(3.14159265*d*d+0.0001);}fn G(nv:f32,nl:f32,r:f32)->f32{let k=(r+1)*(r+1)/8;return nv/(nv*(1-k)+k)*nl/(nl*(1-k)+k);}
@fragment fn fs(v:V,@builtin(front_facing)front:bool)->@location(0)vec4f{let a=textureSample(t0,s0,v.uv)*v.color;let ms=textureSample(t1,s1,v.uv);let ns=textureSample(t2,s2,v.uv);let mrs=textureSample(t3,s3,v.uv);let aos=textureSample(t4,s4,v.uv2);let es=textureSample(t5,s5,v.uv);var n=normalize(v.normal)*select(-1.0,1.0,front);if((v.flags&4u)!=0u){let tn=ns.xyz*2-1;let T=normalize(v.tangent.xyz);let B=normalize(cross(n,T)*v.tangent.w);n=normalize(mat3x3f(T,B,n)*vec3f(tn.xy*v.material.z,tn.z));}var metal=clamp(v.material.x,0,1);var rough=clamp(v.material.y,0.04,1);if((v.flags&2u)!=0u){metal*=ms.r;}if((v.flags&8u)!=0u){metal*=mrs.b;rough*=mrs.g;}let ao=select(1.0,aos.r,(v.flags&16u)!=0u);let view=normalize(scene.camera.xyz-v.world);let nv=max(dot(n,view),0.001);let f0=mix(vec3f(0.04),a.rgb,metal);var color=scene.ambient.rgb*scene.ambient.a*a.rgb*ao;for(var i=0u;i<8u;i++){let light=scene.lights[i];if(light.spotEnabled.z<0.5){continue;}var L=-light.directionRange.xyz;var atten=1.0;if(light.positionType.w>0.5){let delta=light.positionType.xyz-v.world;let dist=length(delta);L=delta/max(dist,0.0001);if(light.directionRange.w>0){atten*=pow(clamp(1-dist/light.directionRange.w,0,1),2);}if(light.positionType.w>1.5){atten*=smoothstep(light.spotEnabled.y,light.spotEnabled.x,dot(-L,normalize(light.directionRange.xyz)));}}let nl=max(dot(n,L),0);if(nl<=0){continue;}let radiance=light.colorIntensity.rgb*light.colorIntensity.a*atten;if(scene.settings.x>0.5){let H=normalize(view+L);let fr=F(max(dot(view,H),0),f0);let spec=D(max(dot(n,H),0),rough)*G(nv,nl,rough)*fr/max(4*nv*nl,0.001);color+=((vec3f(1)-fr)*(1-metal)*a.rgb/3.14159265+spec)*radiance*nl;}else{color+=a.rgb*radiance*nl;}}color+=es.rgb*v.emission.rgb*v.material.w;let dist=length(scene.camera.xyz-v.world);var fog=0.0;if(scene.fog.x==1){fog=clamp((dist-scene.fog.y)/max(scene.fog.z-scene.fog.y,0.001),0,1);}else if(scene.fog.x==2){fog=1-exp(-scene.fog.w*dist);}else if(scene.fog.x==3){let d=scene.fog.w*dist;fog=1-exp(-d*d);}return vec4f(mix(color,scene.fogColor.rgb,clamp(fog,0,1)),a.a);}`});
        const meshLayout=[{arrayStride:80,stepMode:"vertex",attributes:[{shaderLocation:0,offset:0,format:"float32x3"},{shaderLocation:1,offset:12,format:"float32x3"},{shaderLocation:2,offset:24,format:"float32x2"},{shaderLocation:3,offset:32,format:"unorm8x4"},{shaderLocation:4,offset:36,format:"uint8x4"},{shaderLocation:5,offset:40,format:"float32x4"},{shaderLocation:6,offset:56,format:"float32x4"},{shaderLocation:7,offset:72,format:"float32x2"}]},{arrayStride:128,stepMode:"instance",attributes:[{shaderLocation:8,offset:0,format:"float32x4"},{shaderLocation:9,offset:16,format:"float32x4"},{shaderLocation:10,offset:32,format:"float32x4"},{shaderLocation:11,offset:48,format:"float32x4"},{shaderLocation:12,offset:64,format:"unorm8x4"},{shaderLocation:13,offset:80,format:"float32x4"},{shaderLocation:14,offset:96,format:"unorm8x4"},{shaderLocation:15,offset:112,format:"uint32x4"}]}];
        const materialPipelineDescriptor=(vs,fs)=>({layout:materialPipelineLayout,vertex:{module:vs,entryPoint:"vs",buffers:meshLayout},fragment:{module:fs,entryPoint:"fs",targets:[{format,blend:blendStates[0]}]},primitive:{topology:"triangle-list",cullMode:"none"},depthStencil:{format:"depth24plus",depthWriteEnabled:true,depthCompare:"less-equal"}});
        pipeline3d=await device.createRenderPipelineAsync(materialPipelineDescriptor(advanced3d,advanced3d));
        const skyShader=device.createShaderModule({code:`struct Scene{vp:mat4x4f,camera:vec4f,ambient:vec4f,fogColor:vec4f,fog:vec4f,skyRight:vec4f,skyUp:vec4f,skyForward:vec4f,settings:vec4f};struct O{@builtin(position)p:vec4f,@location(0)ray:vec3f};@group(0)@binding(0)var smp:sampler;@group(0)@binding(1)var tex:texture_2d<f32>;@group(1)@binding(0)var<uniform>scene:Scene;@vertex fn vs(@builtin(vertex_index)i:u32)->O{let x=f32((i<<1u)&2u);let y=f32(i&2u);let q=vec2f(x*2-1,1-y*2);var o:O;o.p=vec4f(q,1,1);o.ray=normalize(scene.skyForward.xyz+q.x*scene.skyRight.xyz*scene.skyRight.w*scene.skyUp.w+q.y*scene.skyUp.xyz*scene.skyUp.w);return o;}@fragment fn fs(o:O)->@location(0)vec4f{let d=normalize(o.ray);let uv=vec2f(atan2(d.z,d.x)/6.2831853+0.5,acos(clamp(d.y,-1,1))/3.14159265);return textureSample(tex,smp,uv)*vec4f(scene.settings.yzw,1);}`});
        skyboxPipeline=await device.createRenderPipelineAsync({layout:device.createPipelineLayout({bindGroupLayouts:[textureLayout,sceneLayout3d]}),vertex:{module:skyShader,entryPoint:"vs"},fragment:{module:skyShader,entryPoint:"fs",targets:[{format}]},primitive:{topology:"triangle-list"},depthStencil:{format:"depth24plus",depthWriteEnabled:false,depthCompare:"less-equal"}});
        sampler = device.createSampler({magFilter: "nearest", minFilter: "nearest",
            addressModeU: "repeat", addressModeV: "repeat"});
        const textDecoder = new TextDecoder();
        const textEncoder = new TextEncoder();
        const fileCache = new Map();
        let clipboardText = "";
        let droppedNames = [];
        function readText(pointer) {
            const bytes = new Uint8Array(wasm.memory.buffer);
            let end = pointer;
            while (end < bytes.length && bytes[end]) end++;
            return textDecoder.decode(bytes.subarray(pointer, end));
        }
        function writeText(text, pointer, size) {
            if (!pointer || size <= 0) return 0;
            const encoded = textEncoder.encode(text), count = Math.min(encoded.length, size-1);
            const destination = new Uint8Array(wasm.memory.buffer, pointer, size);
            destination.set(encoded.subarray(0, count)); destination[count] = 0;
            return count+1;
        }
        function refreshClipboard() {
            if (navigator.clipboard && navigator.clipboard.readText)
                navigator.clipboard.readText().then(text => { clipboardText = text; }).catch(() => {});
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
        const imports = {sargpu: {
            log: pointer => { const text = readText(pointer); console.log(text); status.textContent = text; },
            now: () => performance.now(),
            sin: Math.sin, cos: Math.cos, math_pow: Math.pow, math_log: Math.log,
            math_exp: Math.exp, math_floor: Math.floor, math_ldexp: (value, exponent) => value*Math.pow(2, exponent),
            math_atan2: Math.atan2,
            fps: fps => { targetFPS = fps; },
            window_query: (command, index) => {
                if (index !== 0 && command >= 3 && command <= 9) return 0;
                const dpr = window.devicePixelRatio || 1;
                if (command === 0) return document.fullscreenElement === canvas && fullscreenMode === 1 ? 1 : 0;
                if (command === 1) return 1;
                if (command === 2) return 0;
                if (command === 3) return Math.round((screen.availLeft || 0)*dpr);
                if (command === 4) return Math.round((screen.availTop || 0)*dpr);
                if (command === 5) return Math.round(screen.width*dpr);
                if (command === 6) return Math.round(screen.height*dpr);
                if (command === 7 || command === 8) return 0;
                if (command === 9) return Math.round(Number(screen.refreshRate) || 60);
                if (command === 10) return Math.round(dpr*1000);
                if (command === 11) return cursorOnCanvas ? 1 : 0;
                return 0;
            },
            window_text: (command, index, pointer, size) => command === 0 && index === 0 ? writeText("Browser display", pointer, size) : 0,
            window_icon: (pointer, width, height) => {
                if (!pointer || width <= 0 || height <= 0) return;
                const pixels = new Uint8ClampedArray(wasm.memory.buffer, pointer, width*height*4).slice();
                const iconCanvas = document.createElement("canvas"); iconCanvas.width = width; iconCanvas.height = height;
                const iconContext = iconCanvas.getContext("2d"); if (!iconContext) return;
                iconContext.putImageData(new ImageData(pixels, width, height), 0, 0);
                let link = document.querySelector('link[rel~="icon"]');
                if (!link) { link = document.createElement("link"); link.rel = "icon"; document.head.appendChild(link); }
                link.href = iconCanvas.toDataURL("image/png");
            },
            gamepad_available: gamepad => {
                const pads=navigator.getGamepads?navigator.getGamepads():[];return gamepad>=0&&gamepad<pads.length&&pads[gamepad]&&pads[gamepad].connected?1:0;
            },
            gamepad_name: (gamepad, pointer, size) => {
                const pads=navigator.getGamepads?navigator.getGamepads():[],pad=gamepad>=0&&gamepad<pads.length?pads[gamepad]:null;
                return pad&&pad.connected?writeText(pad.id||`Gamepad ${gamepad+1}`,pointer,size):0;
            },
            gamepad_button: (gamepad, button) => {
                const pads=navigator.getGamepads?navigator.getGamepads():[],pad=gamepad>=0&&gamepad<pads.length?pads[gamepad]:null;
                const map=[-1,12,15,13,14,3,1,0,2,4,6,5,7,8,16,9,10,11],index=button>=0&&button<map.length?map[button]:-1;
                return pad&&pad.connected&&index>=0&&index<pad.buttons.length&&pad.buttons[index].pressed?1:0;
            },
            gamepad_axis: (gamepad, axis) => {
                const pads=navigator.getGamepads?navigator.getGamepads():[],pad=gamepad>=0&&gamepad<pads.length?pads[gamepad]:null;
                if(!pad||!pad.connected)return 0;if(axis>=0&&axis<4)return pad.axes[axis]||0;
                if(axis===4||axis===5){const button=pad.buttons[axis===4?6:7];return button?button.value*2-1:-1;}return 0;
            },
            gamepad_vibrate: (gamepad, left, right, milliseconds) => {
                const pads=navigator.getGamepads?navigator.getGamepads():[],pad=gamepad>=0&&gamepad<pads.length?pads[gamepad]:null;if(!pad)return;
                const actuator=pad.vibrationActuator||(pad.hapticActuators&&pad.hapticActuators[0]);if(!actuator)return;
                if(milliseconds<=0&&actuator.reset){actuator.reset().catch(()=>{});return;}
                if(actuator.playEffect)actuator.playEffect("dual-rumble",{duration:Math.max(0,milliseconds),strongMagnitude:Math.max(0,Math.min(1,left)),weakMagnitude:Math.max(0,Math.min(1,right))}).catch(()=>{});
                else if(actuator.pulse)actuator.pulse(Math.max(left,right),Math.max(0,milliseconds)).catch(()=>{});
            },
            clipboard_set: pointer => {
                clipboardText = readText(pointer);
                if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(clipboardText).catch(() => {});
            },
            clipboard_size: () => { refreshClipboard(); return textEncoder.encode(clipboardText).length+1; },
            clipboard_get: (pointer, size) => writeText(clipboardText, pointer, size),
            drop_count: () => droppedNames.length,
            drop_name_size: index => index >= 0 && index < droppedNames.length ? textEncoder.encode(droppedNames[index]).length+1 : 0,
            drop_name: (index, pointer, size) => index >= 0 && index < droppedNames.length ? writeText(droppedNames[index], pointer, size) : 0,
            drop_clear: () => { droppedNames = []; },
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
                    offset:0, startedAt:0, paused:false, stopping:false,streaming:false,queued:new Set(),nextTime:0});
            },
            audio_stream_update: (id, pointer, frames, rate, bits, channels) => {
                const sound=sounds.get(id);if(!sound||!audioContext||!frames||!channels)return;
                const view=new DataView(wasm.memory.buffer,pointer,frames*channels*(bits/8)),decoded=audioContext.createBuffer(channels,frames,rate);
                for(let channel=0;channel<channels;channel++){const output=decoded.getChannelData(channel);for(let frame=0;frame<frames;frame++){const sample=frame*channels+channel;output[frame]=bits===8?(view.getUint8(sample)-128)/128:bits===16?view.getInt16(sample*2,true)/32768:view.getFloat32(sample*4,true);}}
                const source=audioContext.createBufferSource();source.buffer=decoded;source.playbackRate.value=sound.pitch;source.connect(sound.gain);const when=Math.max(audioContext.currentTime,sound.nextTime||0);sound.nextTime=when+decoded.duration/sound.pitch;sound.queued.add(source);source.onended=()=>sound.queued.delete(source);source.start(when);
            },
            audio_unload: id => {
                const sound = sounds.get(id);
                if (sound && sound.source) { sound.stopping=true; sound.source.stop(); }
                if(sound)for(const source of sound.queued)source.stop();
                sounds.delete(id);
            },
            audio_command: (id, command, value) => {
                if (command === 7) { if (masterGain) masterGain.gain.value=value; return; }
                const sound = sounds.get(id); if (!sound || !audioContext) return;
                const start = () => {
                    const source=audioContext.createBufferSource(); source.buffer=sound.buffer;
                    source.playbackRate.value=sound.pitch; source.connect(sound.gain);
                    sound.source=source; sound.startedAt=audioContext.currentTime;
                    sound.nextTime=audioContext.currentTime+(sound.buffer.duration-Math.min(sound.offset,sound.buffer.duration))/sound.pitch;
                    sound.stopping=false; sound.paused=false;
                    source.onended=()=>{if(sound.source===source){sound.source=null;if(!sound.paused)sound.offset=0;}};
                    source.start(0,Math.min(sound.offset,sound.buffer.duration));
                };
                if (command === 0) { if(sound.source){sound.stopping=true;sound.source.stop();} sound.offset=sound.requestedOffset === undefined ? 0 : sound.requestedOffset;delete sound.requestedOffset;audioContext.resume(); start(); }
                else if (command === 1) { if(sound.source){sound.stopping=true;sound.source.stop();}for(const source of sound.queued)source.stop();sound.queued.clear();sound.source=null;sound.offset=0;sound.paused=false;sound.nextTime=0; }
                else if (command === 2 && (sound.source||sound.queued.size)) { if(sound.source)sound.offset+=(audioContext.currentTime-sound.startedAt)*sound.pitch;sound.paused=true;sound.stopping=true;if(sound.source)sound.source.stop();for(const source of sound.queued)source.stop();sound.queued.clear();sound.source=null;sound.nextTime=0; }
                else if (command === 3 && sound.paused) { audioContext.resume();start(); }
                else if (command === 4) { sound.volume=value;sound.gain.gain.value=value; }
                else if (command === 5 && value>0) { sound.pitch=value;if(sound.source)sound.source.playbackRate.value=value; }
                else if (command === 6) sound.pan.pan.value=value*2-1;
                else if (command === 8) sound.requestedOffset=Math.max(0,Math.min(value,sound.buffer.duration));
                else if (command === 9) sound.streaming=value!==0;
            },
            audio_playing: id => { const sound=sounds.get(id);if(!sound||sound.paused)return 0;return sound.streaming?(sound.source?1:0)+sound.queued.size:(sound.source?1:0); },
            file_size: name => { const data = loadFile(name); return data ? data.length : 0; },
            file_read: (name, destination, size) => {
                const data = loadFile(name);
                if (!data || data.length !== size) return 0;
                new Uint8Array(wasm.memory.buffer, destination, size).set(data);
                return size;
            },
            file_write: (namePointer, dataPointer, size) => {
                if (size < 0) return 0;
                const name = readText(namePointer) || "download.bin";
                const data = new Uint8Array(wasm.memory.buffer, dataPointer, size).slice();
                const url = URL.createObjectURL(new Blob([data], {type: "application/octet-stream"}));
                const link = document.createElement("a");
                link.href = url;
                link.download = name.split(/[\\/]/).pop() || "download.bin";
                document.body.appendChild(link);
                link.click();
                link.remove();
                setTimeout(() => URL.revokeObjectURL(url), 0);
                return 1;
            },
            screenshot: namePointer => {
                const name=(readText(namePointer)||"screenshot.png").split(/[\\/]/).pop();
                canvas.toBlob(blob=>{if(!blob)return;const url=URL.createObjectURL(blob),link=document.createElement("a");link.href=url;link.download=name;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);},"image/png");
            },
            init: (width, height, title) => {
                canvas.width = width; canvas.height = height;
                canvas.style.touchAction = "none";
                updateCanvasDisplay(true);
                document.title = readText(title);
                context.configure({device, format, alphaMode: "opaque"});
            },
            window_command: (command, a, b, text) => {
                if (command === 0) document.title = readText(text);
                else if (command === 1 && a > 0 && b > 0) { canvas.width = a; canvas.height = b; updateCanvasDisplay(true); }
                else if (command === 2 || command === 3) {
                    const mode = command === 2 ? 1 : 2;
                    fullscreenPending = 0;
                    if (document.fullscreenElement) {
                        if (fullscreenMode === mode) document.exitFullscreen().catch(() => {});
                        else fullscreenMode = mode;
                    } else if (canvas.requestFullscreen) {
                        fullscreenMode = mode;
                        canvas.requestFullscreen().catch(() => { fullscreenPending = mode; fullscreenMode = 0; });
                    }
                }
                else if (command === 5) canvas.style.opacity = String(Math.max(0, Math.min(255, a))/255);
                else if (command === 6) canvas.focus();
                else if (command === 7) {
                    if (a & 0x80) canvas.style.display = "none";
                    const mode = (a & 0x02) ? 1 : ((a & 0x8000) ? 2 : 0);
                    if (mode && canvas.requestFullscreen) {
                        fullscreenMode = mode;
                        canvas.requestFullscreen().catch(() => { fullscreenPending = mode; fullscreenMode = 0; });
                    }
                }
                else if (command === 8) { cursorShape = Math.max(0, Math.min(10, a)); applyCursor(); }
                else if (command === 9) { cursorHidden = a !== 0; applyCursor(); }
                else if (command === 10) {
                    cursorDisabled = a !== 0;
                    if (a) { pointerLockPending = true; retryPointerLock(); }
                    else { pointerLockPending = false; if (document.pointerLockElement === canvas && document.exitPointerLock) document.exitPointerLock(); }
                    applyCursor();
                }
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
            texture_mipmaps: (id, pointer, width, height, mipmaps) => {
                const previous=textures.get(id);if(previous)previous.texture.destroy();
                const texture=device.createTexture({size:[width,height],mipLevelCount:mipmaps,format:"rgba8unorm",usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST|GPUTextureUsage.COPY_SRC});
                let offset=0,w=width,h=height;for(let level=0;level<mipmaps;level++){
                    const bytes=w*h*4;device.queue.writeTexture({texture,mipLevel:level},new Uint8Array(wasm.memory.buffer,pointer+offset,bytes),{bytesPerRow:w*4,rowsPerImage:h},[w,h,1]);offset+=bytes;w=Math.max(1,w>>1);h=Math.max(1,h>>1);
                }
                const view=texture.createView(),selectedSampler=previous?previous.sampler:sampler;
                const group=device.createBindGroup({layout:textureLayout,entries:[{binding:0,resource:selectedSampler},{binding:1,resource:view}]});textures.set(id,{texture,view,group,sampler:selectedSampler,width,height});
            },
            texture_readback: (id, requestId) => {
                const entry=textures.get(id);if(!entry||!entry.texture)return 0;
                const rowBytes=entry.width*4,paddedRow=(rowBytes+255)&~255,size=paddedRow*entry.height;
                const buffer=device.createBuffer({size,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
                const encoder=device.createCommandEncoder();encoder.copyTextureToBuffer({texture:entry.texture},{buffer,bytesPerRow:paddedRow,rowsPerImage:entry.height},[entry.width,entry.height,1]);device.queue.submit([encoder.finish()]);
                buffer.mapAsync(GPUMapMode.READ).then(()=>{const destination=wasm.sargpu_readback_allocate(requestId,rowBytes*entry.height);if(destination){const source=new Uint8Array(buffer.getMappedRange()),output=new Uint8Array(wasm.memory.buffer,destination,rowBytes*entry.height);for(let y=0;y<entry.height;y++)output.set(source.subarray(y*paddedRow,y*paddedRow+rowBytes),y*rowBytes);if(format.startsWith("bgra"))for(let i=0;i<entry.width*entry.height;i++){const at=i*4,value=output[at];output[at]=output[at+2];output[at+2]=value;}buffer.unmap();buffer.destroy();wasm.sargpu_readback_complete(requestId,destination,entry.width,entry.height);}else{buffer.unmap();buffer.destroy();wasm.sargpu_readback_complete(requestId,0,0,0);}}).catch(()=>{buffer.destroy();wasm.sargpu_readback_complete(requestId,0,0,0);});return 1;
            },
            screen_readback: requestId => {
                createImageBitmap(canvas).then(bitmap=>{const copy=typeof OffscreenCanvas!=="undefined"?new OffscreenCanvas(canvas.width,canvas.height):document.createElement("canvas");copy.width=canvas.width;copy.height=canvas.height;const ctx=copy.getContext("2d");ctx.drawImage(bitmap,0,0);bitmap.close();const pixels=ctx.getImageData(0,0,canvas.width,canvas.height).data,destination=wasm.sargpu_readback_allocate(requestId,pixels.length);if(destination)new Uint8Array(wasm.memory.buffer,destination,pixels.length).set(pixels);wasm.sargpu_readback_complete(requestId,destination||0,destination?canvas.width:0,destination?canvas.height:0);}).catch(()=>wasm.sargpu_readback_complete(requestId,0,0,0));return 1;
            },
            render_texture: (id, width, height) => {
                const texture=device.createTexture({size:[width,height],format,
                    usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.RENDER_ATTACHMENT|GPUTextureUsage.COPY_SRC});
                const view=texture.createView();
                const group=device.createBindGroup({layout:textureLayout,entries:[
                    {binding:0,resource:sampler},{binding:1,resource:view}
                ]});
                const depthTexture=device.createTexture({size:[width,height],format:"depth24plus",usage:GPUTextureUsage.RENDER_ATTACHMENT});
                textures.set(id,{texture,view,group,sampler,width,height,depthTexture,depthView:depthTexture.createView()});
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
            unload: id => { const entry = textures.get(id); if (entry) { entry.texture.destroy();if(entry.depthTexture)entry.depthTexture.destroy(); } textures.delete(id); },
            shader_load: (id, vsPointer, fsPointer) => {
                try {
                    const vs=device.createShaderModule({code:readText(vsPointer)}),fs=device.createShaderModule({code:readText(fsPointer)});
                    const list=blendStates.map(blend=>device.createRenderPipeline({layout:pipelineLayout,
                        vertex:{module:vs,entryPoint:"vs",buffers:[{arrayStride:24,attributes:[
                            {shaderLocation:0,offset:0,format:"float32x2"},{shaderLocation:1,offset:8,format:"float32x2"},
                            {shaderLocation:2,offset:20,format:"unorm8x4"},{shaderLocation:3,offset:16,format:"float32"}]}]},
                        fragment:{module:fs,entryPoint:"fs",targets:[{format,blend}]},primitive:{topology:"triangle-list",cullMode:"none"},
                        depthStencil:{format:"depth24plus",depthWriteEnabled:true,depthCompare:"less-equal"}}));
                    const uniformBuffer=device.createBuffer({size:2048,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});
                    const uniformGroup=device.createBindGroup({layout:uniformLayout,entries:[{binding:0,resource:{buffer:uniformBuffer,size:2048}}]});
                    const shader={pipelines:list,uniformBuffer,uniformGroup,textureIds:new Array(8).fill(0),textureGroup:null};
                    if(!rebuildShaderTextures(shader)){uniformBuffer.destroy();return 0;}shaders.set(id,shader);return 1;
                } catch(error) { console.error(error);return 0; }
            },
            material_shader_load: (id, vsPointer, fsPointer) => {
                try {const vs=device.createShaderModule({code:readText(vsPointer)}),fs=device.createShaderModule({code:readText(fsPointer)});const materialPipeline=device.createRenderPipeline(materialPipelineDescriptor(vs,fs));const uniformBuffer=device.createBuffer({size:2048,usage:GPUBufferUsage.UNIFORM|GPUBufferUsage.COPY_DST});const uniformGroup=device.createBindGroup({layout:uniformLayout,entries:[{binding:0,resource:{buffer:uniformBuffer,size:2048}}]});const shader={pipelines:null,materialPipeline,uniformBuffer,uniformGroup,textureIds:new Array(8).fill(0),textureGroup:null};if(!rebuildShaderTextures(shader)){uniformBuffer.destroy();return 0;}shaders.set(id,shader);return 1;}catch(error){console.error(error);return 0;}
            },
            shader_unload: id => { const shader=shaders.get(id);if(shader)shader.uniformBuffer.destroy();shaders.delete(id); },
            shader_uniform: (id, location, pointer, size) => {
                const shader=shaders.get(id);if(!shader||location<0||location>=32)return;
                device.queue.writeBuffer(shader.uniformBuffer,location*64,new Uint8Array(wasm.memory.buffer,pointer,size));
            },
            shader_texture: (id, location, textureId) => {
                const shader=shaders.get(id);if(!shader||location<0||location>=8)return;
                shader.textureIds[location]=textureId;rebuildShaderTextures(shader);
            },
            mesh_upload: (id, vertices, vertexCount, indices, indexCount) => {
                const previous=meshes.get(id);if(previous){previous.vertexBuffer.destroy();if(previous.indexBuffer)previous.indexBuffer.destroy();}
                if(!id||!vertices||vertexCount<=0)return;
                const vertexBuffer=device.createBuffer({size:Math.max(4,vertexCount*80),usage:GPUBufferUsage.VERTEX|GPUBufferUsage.COPY_DST});
                device.queue.writeBuffer(vertexBuffer,0,new Uint8Array(wasm.memory.buffer,vertices,vertexCount*80));
                let indexBuffer=null;if(indices&&indexCount>0){const bytes=indexCount*2,padded=(bytes+3)&~3;indexBuffer=device.createBuffer({size:padded,usage:GPUBufferUsage.INDEX|GPUBufferUsage.COPY_DST});const data=new Uint8Array(padded);data.set(new Uint8Array(wasm.memory.buffer,indices,bytes));device.queue.writeBuffer(indexBuffer,0,data);}
                meshes.set(id,{vertexBuffer,indexBuffer,vertexCount,indexCount});
            },
            mesh_update: (id, vertices, vertexCount) => {
                const mesh=meshes.get(id);if(!mesh||!vertices||vertexCount!==mesh.vertexCount)return;
                device.queue.writeBuffer(mesh.vertexBuffer,0,new Uint8Array(wasm.memory.buffer,vertices,vertexCount*80));
            },
            mesh_unload: id => { const mesh=meshes.get(id);if(mesh){mesh.vertexBuffer.destroy();if(mesh.indexBuffer)mesh.indexBuffer.destroy();}meshes.delete(id); },
            present: (vertices, count, batches, batchCount, draws3d, drawCount3d, instances3d, instanceCount3d, color, targetId, scene3d, bones, boneCount, skyboxId, skyboxTint) => {
                if (stopped) return;
                const target=targetId ? textures.get(targetId) : null;
                if (targetId && !target) return;
                if(!target&&(depthWidth!==canvas.width||depthHeight!==canvas.height)){
                    if(depthTexture)depthTexture.destroy();depthWidth=canvas.width;depthHeight=canvas.height;
                    depthTexture=device.createTexture({size:[depthWidth,depthHeight],format:"depth24plus",usage:GPUTextureUsage.RENDER_ATTACHMENT});
                }
                const encoder = device.createCommandEncoder();
                const pass = encoder.beginRenderPass({colorAttachments: [{
                    view: target ? target.view : context.getCurrentTexture().createView(), loadOp: "clear", storeOp: "store",
                    clearValue: [(color & 255)/255, ((color>>>8)&255)/255, ((color>>>16)&255)/255, (color>>>24)/255]
                 }],depthStencilAttachment:{view:target?target.depthView:depthTexture.createView(),depthLoadOp:"clear",depthStoreOp:"store",depthClearValue:1}});
                if(scene3d)device.queue.writeBuffer(sceneBuffer3d,0,new Uint8Array(wasm.memory.buffer,scene3d,704));if(bones&&boneCount>0)device.queue.writeBuffer(boneBuffer3d,0,new Uint8Array(wasm.memory.buffer,bones,boneCount*64));
                const sky=textures.get(skyboxId);if(sky&&sky!==target){pass.setPipeline(skyboxPipeline);pass.setBindGroup(0,sky.group);pass.setBindGroup(1,sceneGroup3d);pass.setScissorRect(0,0,target?target.width:canvas.width,target?target.height:canvas.height);pass.draw(3);}
                if(instanceCount3d>0&&drawCount3d>0){
                    device.queue.writeBuffer(instanceBuffer3d,0,new Uint8Array(wasm.memory.buffer,instances3d,instanceCount3d*128));
                    const commands=new Uint32Array(wasm.memory.buffer,draws3d,drawCount3d*12);
                    pass.setScissorRect(0,0,target?target.width:canvas.width,target?target.height:canvas.height);
                    for(let i=0;i<commands.length;i+=12){const mesh=meshes.get(commands[i]),entry=textures.get(commands[i+4]);if(!mesh||!entry||entry===target)continue;const materialEntries=[];let valid=true;for(let slot=0;slot<8;slot++){const texture=textures.get(commands[i+4+slot]);if(!texture||texture===target){valid=false;break;}materialEntries.push({binding:slot*2,resource:texture.sampler||sampler},{binding:slot*2+1,resource:texture.view});}if(!valid)continue;const materialGroup=device.createBindGroup({layout:shaderTextureLayout,entries:materialEntries});const custom=shaders.get(commands[i+3]);pass.setPipeline(custom&&custom.materialPipeline?custom.materialPipeline:pipeline3d);
                        pass.setVertexBuffer(0,mesh.vertexBuffer);pass.setVertexBuffer(1,instanceBuffer3d,commands[i+1]*128,commands[i+2]*128);pass.setBindGroup(0,entry.group);pass.setBindGroup(1,custom?custom.uniformGroup:defaultUniformGroup3d);pass.setBindGroup(2,materialGroup);pass.setBindGroup(3,sceneGroup3d);
                        if(mesh.indexBuffer){pass.setIndexBuffer(mesh.indexBuffer,"uint16");pass.drawIndexed(mesh.indexCount,commands[i+2],0,0,0);}else pass.draw(mesh.vertexCount,commands[i+2],0,0);
                    }
                }
                if (count) {
                    device.queue.writeBuffer(buffer, 0, new Uint8Array(wasm.memory.buffer, vertices, count*24));
                    pass.setVertexBuffer(0, buffer, 0, count*24);
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
                        if(shader){pass.setBindGroup(1,shader.uniformGroup);pass.setBindGroup(2,shader.textureGroup);}
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
        function sargpuKey(code) {
            if (/^Key[A-Z]$/.test(code)) return code.charCodeAt(3);
            if (/^Digit[0-9]$/.test(code)) return code.charCodeAt(5);
            if (/^Numpad[0-9]$/.test(code)) return 320 + Number(code.charAt(6));
            return keyCodes[code] || 0;
        }
        function keyboard(event) {
            if (event.type === "keydown") { retryFullscreen(); retryPointerLock(); }
            const key = sargpuKey(event.code);
            if (key) {
                if (event.type === "keydown" && audioContext && audioContext.state === "suspended") audioContext.resume();
                wasm.sargpu_key(key, event.type === "keydown" ? 1 : 0);
                if (event.type === "keydown" && !event.ctrlKey && !event.altKey && !event.metaKey) {
                    const characters=Array.from(event.key);
                    if (characters.length === 1) wasm.sargpu_char(characters[0].codePointAt(0));
                }
                event.preventDefault();
            }
        }
        window.addEventListener("keydown", keyboard, {signal: events.signal});
        window.addEventListener("keyup", keyboard, {signal: events.signal});
        window.addEventListener("resize", () => updateCanvasDisplay(true), {signal: events.signal});
        document.addEventListener("fullscreenchange", () => { if (!document.fullscreenElement) fullscreenMode = 0; }, {signal: events.signal});
        window.addEventListener("paste", event => { clipboardText = event.clipboardData ? event.clipboardData.getData("text") : clipboardText; }, {signal: events.signal});
        window.addEventListener("dragover", event => event.preventDefault(), {signal: events.signal});
        window.addEventListener("drop", event => {
            event.preventDefault();
            const files = Array.from(event.dataTransfer ? event.dataTransfer.files : []);
            Promise.all(files.map(async (file, index) => {
                let name = file.name || `dropped-${index}`;
                if (files.some((other, otherIndex) => otherIndex < index && other.name === file.name)) name = `${index}-${name}`;
                return {name, data:new Uint8Array(await file.arrayBuffer())};
            })).then(entries => { if (!stopped) { droppedNames = entries.map(entry => entry.name); for (const entry of entries) fileCache.set(entry.name, entry.data); } }).catch(() => {});
        }, {signal: events.signal});
        window.addEventListener("focus", () => wasm.sargpu_focus(1), {signal: events.signal});
        window.addEventListener("blur", () => { wasm.sargpu_focus(0); wasm.sargpu_blur(); }, {signal: events.signal});
        document.addEventListener("visibilitychange", () => {
            if (document.hidden) wasm.sargpu_blur();
            lastFrame = undefined;
        }, {signal: events.signal});
        const button = value => value === 2 ? 1 : value === 1 ? 2 : value === 0 ? 0 : value === 3 ? 3 : value === 4 ? 4 : -1;
        function mouse(event) {
            const bounds = canvas.getBoundingClientRect();
            if (!bounds.width || !bounds.height) return;
            const scaleX=canvas.width/bounds.width,scaleY=canvas.height/bounds.height;
            if(document.pointerLockElement===canvas){mouseX+=event.movementX*scaleX;mouseY+=event.movementY*scaleY;}
            else {mouseX=(event.clientX-bounds.left)*scaleX;mouseY=(event.clientY-bounds.top)*scaleY;}
            mouseX=Math.max(0,Math.min(canvas.width,mouseX));mouseY=Math.max(0,Math.min(canvas.height,mouseY));
            const action=event.type==="pointerdown"?0:(event.type==="pointerup"||event.type==="pointercancel"?2:1);
            if(event.pointerType==="touch"||event.pointerType==="pen") {
                wasm.sargpu_mouse(mouseX,mouseY,-1,0);wasm.sargpu_touch(event.pointerId,action,mouseX,mouseY);
            } else {
                const mapped=event.type==="pointermove"?-1:button(event.button);
                wasm.sargpu_mouse(mouseX,mouseY,mapped,event.type==="pointerdown"?1:0);
                if(event.button===0||event.type==="pointermove")wasm.sargpu_touch(-1,action,mouseX,mouseY);
            }
            if (event.type === "pointerdown") {
                retryFullscreen(); retryPointerLock();
                if (audioContext && audioContext.state === "suspended") audioContext.resume();
                canvas.focus(); canvas.setPointerCapture(event.pointerId);
            }
            if(event.pointerType==="touch"||event.pointerType==="pen")event.preventDefault();
        }
        for (const type of ["pointermove", "pointerdown", "pointerup", "pointercancel"]) canvas.addEventListener(type, mouse, {signal: events.signal});
        canvas.addEventListener("pointerenter",()=>{cursorOnCanvas=true;},{signal:events.signal});
        canvas.addEventListener("pointerleave",()=>{cursorOnCanvas=false;},{signal:events.signal});
        document.addEventListener("pointerlockchange",()=>{applyCursor();},{signal:events.signal});
        canvas.addEventListener("wheel", event => {
            wasm.sargpu_wheel(-Math.sign(event.deltaX), -Math.sign(event.deltaY));
            event.preventDefault();
        }, {signal: events.signal, passive: false});
        canvas.addEventListener("contextmenu", event => event.preventDefault(), {signal: events.signal});
        function frame(timestamp) {
            if (stopped) return;
            updateCanvasDisplay();
            const interval = targetFPS > 0 ? 1000/targetFPS : 0;
            if (lastFrame === undefined || timestamp-lastFrame >= interval-0.5) {
                lastFrame = timestamp;
                try { if (!wasm.sargpu_frame()) return; } catch (error) { fail(error); return; }
            }
            requestAnimationFrame(frame);
        }
        requestAnimationFrame(frame);
    } catch (error) { fail(error); }
})();
