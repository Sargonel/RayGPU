/* RayGPU core module. Compiled through raygpu.c; do not compile separately. */
static void mr_error(const char *s) { puts(s); mr.error=true; mr.close=true; }
#ifdef _WIN32
static WGPUStringView mr_string(const char *s) { return (WGPUStringView){s,WGPU_STRLEN}; }
static void mr_message(const char *prefix,WGPUStringView message) {
    fprintf(stderr,"raygpu: %s: ",prefix);
    if (message.data) fwrite(message.data,1,message.length==WGPU_STRLEN ? strlen(message.data) : message.length,stderr);
    fputc('\n',stderr);
}
#endif
static double mr_clock(void) {
#ifdef __wasm__
    return mr_web_now()/1000.0;
#else
    LARGE_INTEGER ticks,frequency; QueryPerformanceCounter(&ticks); QueryPerformanceFrequency(&frequency);
    return (double)ticks.QuadPart/(double)frequency.QuadPart;
#endif
}
#ifdef _WIN32
static void mr_yield(int ms) { Sleep((DWORD)ms); }
static void mr_limit_frame(void) {
    if (!mr.softwareFrameLimit || mr.fps<=0) return;
    const double deadline=mr.frameStart+1.0/mr.fps;
    /* Sleep for the coarse part and spin only for the final fraction. This
     * keeps accurate pacing without burning a CPU core while the game idles. */
    for (;;) {
        double remaining=deadline-mr_clock();if(remaining<=0)break;
        if(remaining>0.002)Sleep((DWORD)((remaining-0.001)*1000.0));
        else YieldProcessor();
    }
}
#endif
static void mr_key(int key,bool down) {
    if (key<=KEY_NULL || key>=512) return;
    if (down && !mr.keys[key]) {
        mr.pressed[key]=true;
        if (mr.keyQueueCount<16) mr.keyQueue[mr.keyQueueCount++]=key;
    }
    else if (down) mr.repeated[key]=true;
    if (!down && mr.keys[key]) mr.released[key]=true;
    mr.keys[key]=down;
}
static void mr_button(int button,bool down) {
    if (button<0 || button>=3) return;
    if (down && !mr.buttons[button]) mr.clicked[button]=true;
    if (!down && mr.buttons[button]) mr.buttonReleased[button]=true;
    mr.buttons[button]=down;
}
static void mr_mouse(float x,float y) {
    x=x*mr.mouseScale.x+mr.mouseOffset.x; y=y*mr.mouseScale.y+mr.mouseOffset.y;
    mr.mouseDelta.x+=x-mr.mouse.x; mr.mouseDelta.y+=y-mr.mouse.y;
    mr.mouse=(Vector2){x,y};
}
static void mr_clear_input(void) {
    memset(mr.keys,0,sizeof mr.keys); memset(mr.pressed,0,sizeof mr.pressed);
    memset(mr.repeated,0,sizeof mr.repeated); memset(mr.released,0,sizeof mr.released);
    memset(mr.buttons,0,sizeof mr.buttons); memset(mr.clicked,0,sizeof mr.clicked);
    memset(mr.buttonReleased,0,sizeof mr.buttonReleased);
    mr.mouseDelta=(Vector2){0}; mr.wheel=(Vector2){0};
    mr.keyQueueCount=0; mr.charQueueCount=0;
}
static void mr_finish_input_frame(void) {
    memset(mr.pressed,0,sizeof mr.pressed); memset(mr.repeated,0,sizeof mr.repeated);
    memset(mr.released,0,sizeof mr.released); memset(mr.clicked,0,sizeof mr.clicked);
    memset(mr.buttonReleased,0,sizeof mr.buttonReleased);
    mr.mouseDelta=(Vector2){0}; mr.wheel=(Vector2){0};
    mr.keyQueueCount=0; mr.charQueueCount=0; mr.resized=false;
}
#ifdef _WIN32
static int mr_translate_key(WPARAM key,LPARAM detail) {
    if ((key>='0' && key<='9') || (key>='A' && key<='Z')) return (int)key;
    if (key>=VK_F1 && key<=VK_F12) return KEY_F1+(int)(key-VK_F1);
    if (key>=VK_NUMPAD0 && key<=VK_NUMPAD9) return KEY_KP_0+(int)(key-VK_NUMPAD0);
    switch (key) {
    case VK_SPACE: return KEY_SPACE; case VK_ESCAPE: return KEY_ESCAPE;
    case VK_RETURN: return (detail&(1L<<24)) ? KEY_KP_ENTER : KEY_ENTER;
    case VK_TAB: return KEY_TAB; case VK_BACK: return KEY_BACKSPACE;
    case VK_INSERT: return KEY_INSERT; case VK_DELETE: return KEY_DELETE;
    case VK_RIGHT: return KEY_RIGHT; case VK_LEFT: return KEY_LEFT;
    case VK_DOWN: return KEY_DOWN; case VK_UP: return KEY_UP;
    case VK_PRIOR: return KEY_PAGE_UP; case VK_NEXT: return KEY_PAGE_DOWN;
    case VK_HOME: return KEY_HOME; case VK_END: return KEY_END;
    case VK_CAPITAL: return KEY_CAPS_LOCK; case VK_SCROLL: return KEY_SCROLL_LOCK;
    case VK_NUMLOCK: return KEY_NUM_LOCK; case VK_SNAPSHOT: return KEY_PRINT_SCREEN;
    case VK_PAUSE: return KEY_PAUSE;
    case VK_SHIFT: {
        UINT translated=MapVirtualKeyA((UINT)((detail>>16)&0xff),MAPVK_VSC_TO_VK_EX);
        return translated==VK_RSHIFT ? KEY_RIGHT_SHIFT : KEY_LEFT_SHIFT;
    }
    case VK_LSHIFT: return KEY_LEFT_SHIFT; case VK_RSHIFT: return KEY_RIGHT_SHIFT;
    case VK_CONTROL: return (detail&(1L<<24)) ? KEY_RIGHT_CONTROL : KEY_LEFT_CONTROL;
    case VK_LCONTROL: return KEY_LEFT_CONTROL; case VK_RCONTROL: return KEY_RIGHT_CONTROL;
    case VK_MENU: return (detail&(1L<<24)) ? KEY_RIGHT_ALT : KEY_LEFT_ALT;
    case VK_LMENU: return KEY_LEFT_ALT; case VK_RMENU: return KEY_RIGHT_ALT;
    case VK_LWIN: return KEY_LEFT_SUPER; case VK_RWIN: return KEY_RIGHT_SUPER;
    case VK_APPS: return KEY_KB_MENU;
    case VK_DECIMAL: return KEY_KP_DECIMAL; case VK_DIVIDE: return KEY_KP_DIVIDE;
    case VK_MULTIPLY: return KEY_KP_MULTIPLY; case VK_SUBTRACT: return KEY_KP_SUBTRACT;
    case VK_ADD: return KEY_KP_ADD;
    case VK_OEM_7: return KEY_APOSTROPHE; case VK_OEM_COMMA: return KEY_COMMA;
    case VK_OEM_MINUS: return KEY_MINUS; case VK_OEM_PERIOD: return KEY_PERIOD;
    case VK_OEM_2: return KEY_SLASH; case VK_OEM_1: return KEY_SEMICOLON;
    case VK_OEM_PLUS: return KEY_EQUAL; case VK_OEM_4: return KEY_LEFT_BRACKET;
    case VK_OEM_5: return KEY_BACKSLASH; case VK_OEM_6: return KEY_RIGHT_BRACKET;
    case VK_OEM_3: return KEY_GRAVE;
    case VK_VOLUME_UP: return KEY_VOLUME_UP; case VK_VOLUME_DOWN: return KEY_VOLUME_DOWN;
    default: return KEY_NULL;
    }
}
static LRESULT CALLBACK mr_window_proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_CLOSE: mr.close=true; return 0;
    case WM_SIZE: mr.width=LOWORD(l); mr.height=HIWORD(l); mr.resized=true; return 0;
    case WM_SETFOCUS: mr.focused=true; return 0;
    case WM_KILLFOCUS: mr.focused=false; mr_clear_input(); return 0;
    case WM_CHAR:
        if ((unsigned int)w>=32 && mr.charQueueCount<16) mr.charQueue[mr.charQueueCount++]=(int)w;
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits=(MINMAXINFO *)l;
        RECT minRect={0,0,mr.minWidth,mr.minHeight},maxRect={0,0,mr.maxWidth,mr.maxHeight};
        if (mr.minWidth>0 && mr.minHeight>0) { AdjustWindowRect(&minRect,WS_OVERLAPPEDWINDOW,FALSE); limits->ptMinTrackSize=(POINT){minRect.right-minRect.left,minRect.bottom-minRect.top}; }
        if (mr.maxWidth>0 && mr.maxHeight>0) { AdjustWindowRect(&maxRect,WS_OVERLAPPEDWINDOW,FALSE); limits->ptMaxTrackSize=(POINT){maxRect.right-maxRect.left,maxRect.bottom-maxRect.top}; }
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
        int key=mr_translate_key(w,l);
        mr_key(key,message==WM_KEYDOWN || message==WM_SYSKEYDOWN); return 0;
    }
    case WM_MOUSEMOVE: mr_mouse((float)GET_X_LPARAM(l),(float)GET_Y_LPARAM(l)); return 0;
    case WM_MOUSEWHEEL: mr.wheel.y+=(float)GET_WHEEL_DELTA_WPARAM(w)/(float)WHEEL_DELTA; return 0;
    case WM_MOUSEHWHEEL: mr.wheel.x+=(float)GET_WHEEL_DELTA_WPARAM(w)/(float)WHEEL_DELTA; return 0;
    case WM_LBUTTONDOWN: mr_button(0,true); SetCapture(window); return 0;
    case WM_RBUTTONDOWN: mr_button(1,true); SetCapture(window); return 0;
    case WM_MBUTTONDOWN: mr_button(2,true); SetCapture(window); return 0;
    case WM_LBUTTONUP: mr_button(0,false); if (!mr.buttons[1] && !mr.buttons[2]) ReleaseCapture(); return 0;
    case WM_RBUTTONUP: mr_button(1,false); if (!mr.buttons[0] && !mr.buttons[2]) ReleaseCapture(); return 0;
    case WM_MBUTTONUP: mr_button(2,false); if (!mr.buttons[0] && !mr.buttons[1]) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED: memset(mr.buttons,0,sizeof mr.buttons); return 0;
    }
    return DefWindowProcA(window,message,w,l);
}
static void mr_pump(void) {
    MSG message;
    while (PeekMessageA(&message,NULL,0,0,PM_REMOVE)) {
        if (message.message==WM_QUIT) mr.close=true;
        TranslateMessage(&message); DispatchMessageA(&message);
    }
    if (mr.instance) wgpuInstanceProcessEvents(mr.instance);
}
static void mr_adapter(WGPURequestAdapterStatus status,WGPUAdapter adapter,WGPUStringView message,void *a,void *b) {
    (void)a; (void)b;
    if (status==WGPURequestAdapterStatus_Success) mr.adapter=adapter;
    else { mr_message("adapter request failed",message); mr_error("No WebGPU adapter available"); }
    mr.adapterDone=true;
}
static void mr_device(WGPURequestDeviceStatus status,WGPUDevice device,WGPUStringView message,void *a,void *b) {
    (void)a; (void)b;
    if (status==WGPURequestDeviceStatus_Success) mr.device=device;
    else { mr_message("device request failed",message); mr_error("Could not create device"); }
    mr.deviceDone=true;
}
static void mr_gpu_error(WGPUDevice const *device,WGPUErrorType type,WGPUStringView message,void *a,void *b) {
    (void)device; (void)type; (void)a; (void)b;
    mr_message("WebGPU error",message); mr.error=true; mr.close=true;
}
static void mr_device_lost(WGPUDevice const *device,WGPUDeviceLostReason reason,WGPUStringView message,void *a,void *b) {
    (void)device; (void)a; (void)b;
    if (reason==WGPUDeviceLostReason_Destroyed || reason==WGPUDeviceLostReason_CallbackCancelled) return;
    mr_message("device lost",message); mr.error=true; mr.close=true;
}
#else
static void mr_pump(void) {}
#endif
static MRTexture *mr_texture(unsigned int id) {
    if (!id) return NULL;
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (mr.textures[i].id==id) return &mr.textures[i];
    return NULL;
}
#ifdef _WIN32
Texture2D LoadTextureRGBA(const unsigned char *pixels,int width,int height) {
    if (!mr.device || !pixels || width<=0 || height<=0 || width>8192 || height>8192) return (Texture2D){0};
    MRTexture *t=NULL;
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (!mr.textures[i].id) { t=&mr.textures[i]; break; }
    if (!t) { fprintf(stderr,"raygpu: texture limit reached\n"); return (Texture2D){0}; }
    WGPUTextureDescriptor desc=WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.size=(WGPUExtent3D){(uint32_t)width,(uint32_t)height,1};
    desc.dimension=WGPUTextureDimension_2D; desc.format=WGPUTextureFormat_RGBA8Unorm;
    desc.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc;
    t->texture=wgpuDeviceCreateTexture(mr.device,&desc);
    t->view=wgpuTextureCreateView(t->texture,NULL);
    WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; destination.texture=t->texture;
    /* Explicit initialization avoids an IntelliSense bug with the Windows
     * SDK's UINT32_MAX literal used by Dawn's initializer macro. */
    WGPUTexelCopyBufferLayout layout={0};
    layout.bytesPerRow=(uint32_t)width*4; layout.rowsPerImage=(uint32_t)height;
    wgpuQueueWriteTexture(mr.queue,&destination,pixels,(size_t)width*height*4,&layout,&desc.size);
    WGPUBindGroupEntry entries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding=0; entries[0].sampler=mr.sampler;
    entries[1].binding=1; entries[1].textureView=t->view;
    WGPUBindGroupDescriptor group=WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    group.layout=mr.textureLayout; group.entryCount=2; group.entries=entries;
    t->group=wgpuDeviceCreateBindGroup(mr.device,&group);
    t->pixels=MemAlloc((unsigned int)((size_t)width*height*4));if(!t->pixels){wgpuBindGroupRelease(t->group);wgpuTextureViewRelease(t->view);wgpuTextureRelease(t->texture);memset(t,0,sizeof*t);return(Texture2D){0};}memcpy(t->pixels,pixels,(size_t)width*height*4);t->width=width;t->height=height;t->mipmaps=1;t->id=++mr.nextTexture;
    return (Texture2D){t->id,width,height,1,7};
}
#else
Texture2D LoadTextureRGBA(const unsigned char *pixels,int width,int height) {
    if (!mr.ready || !pixels || width<=0 || height<=0 || width>8192 || height>8192) return (Texture2D){0};
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (!mr.textures[i].id) {
        unsigned int id=++mr.nextTexture;
        mr.textures[i].pixels=MemAlloc((unsigned int)((size_t)width*height*4));if(!mr.textures[i].pixels)return(Texture2D){0};memcpy(mr.textures[i].pixels,pixels,(size_t)width*height*4);mr.textures[i].width=width;mr.textures[i].height=height;mr.textures[i].mipmaps=1;mr.textures[i].id=id; mr_web_texture(id,pixels,width,height);
        return (Texture2D){id,width,height,1,7};
    }
    puts("raygpu: texture limit reached"); return (Texture2D){0};
}
#endif
void UnloadTexture(Texture2D texture) {
    MRTexture *t=mr_texture(texture.id); if (!t || texture.id==mr.white) return;
    if (mr.drawing) { puts("raygpu: unload textures outside BeginDrawing/EndDrawing"); return; }
#ifdef _WIN32
    wgpuBindGroupRelease(t->group); if (t->customSampler) wgpuSamplerRelease(t->customSampler);
    if (t->depthView) wgpuTextureViewRelease(t->depthView);
    if (t->depthTexture) wgpuTextureRelease(t->depthTexture);
    wgpuTextureViewRelease(t->view); wgpuTextureRelease(t->texture);
#else
    mr_web_unload(t->id);
#endif
    if (texture.id==mr.shapesTexture.id) {
        mr.shapesTexture=(Texture2D){mr.white,1,1,1,7}; mr.shapesSource=(Rectangle){0,0,1,1};
    }
    MemFree(t->pixels);memset(t,0,sizeof *t);
}
static MRShaderEntry *mr_shader(unsigned int id) {
    if(!id)return NULL;for(int i=0;i<32;i++)if(mr.shaders[i].id==id)return &mr.shaders[i];return NULL;
}
void UpdateTextureRec(Texture2D texture,Rectangle rec,const void *pixels) {
    MRTexture *t=mr_texture(texture.id); if (!t || !pixels) return;
    int x=(int)rec.x,y=(int)rec.y,width=(int)rec.width,height=(int)rec.height;
    if (x<0 || y<0 || width<=0 || height<=0 || x+width>texture.width || y+height>texture.height) return;
    if(t->pixels&&!t->renderTarget)for(int row=0;row<height;row++)memcpy(t->pixels+((size_t)(y+row)*t->width+x)*4,(const unsigned char*)pixels+(size_t)row*width*4,(size_t)width*4);
#ifdef _WIN32
    WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture=t->texture; destination.origin=(WGPUOrigin3D){(uint32_t)x,(uint32_t)y,0};
    WGPUTexelCopyBufferLayout layout={0}; layout.bytesPerRow=(uint32_t)width*4; layout.rowsPerImage=(uint32_t)height;
    WGPUExtent3D extent={(uint32_t)width,(uint32_t)height,1};
    wgpuQueueWriteTexture(mr.queue,&destination,pixels,(size_t)width*height*4,&layout,&extent);
#else
    mr_web_texture_update(t->id,x,y,width,height,pixels);
#endif
}
void UpdateTexture(Texture2D texture,const void *pixels) {
    UpdateTextureRec(texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},pixels);
}
static void mr_apply_texture_params(MRTexture *texture) {
#ifdef _WIN32
    WGPUSamplerDescriptor descriptor=WGPU_SAMPLER_DESCRIPTOR_INIT;
    bool linear=texture->filter!=TEXTURE_FILTER_POINT;
    descriptor.magFilter=linear?WGPUFilterMode_Linear:WGPUFilterMode_Nearest;
    descriptor.minFilter=linear?WGPUFilterMode_Linear:WGPUFilterMode_Nearest;
    descriptor.mipmapFilter=linear?WGPUMipmapFilterMode_Linear:WGPUMipmapFilterMode_Nearest;
    WGPUAddressMode address=WGPUAddressMode_Repeat;
    if(texture->wrap==TEXTURE_WRAP_CLAMP || texture->wrap==TEXTURE_WRAP_MIRROR_CLAMP) address=WGPUAddressMode_ClampToEdge;
    else if(texture->wrap==TEXTURE_WRAP_MIRROR_REPEAT) address=WGPUAddressMode_MirrorRepeat;
    descriptor.addressModeU=address; descriptor.addressModeV=address;
    if(texture->filter>=TEXTURE_FILTER_ANISOTROPIC_4X) descriptor.maxAnisotropy=(uint16_t)(4u<<(texture->filter-TEXTURE_FILTER_ANISOTROPIC_4X));
    WGPUSampler sampler=wgpuDeviceCreateSampler(mr.device,&descriptor); if(!sampler)return;
    WGPUBindGroupEntry entries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding=0;entries[0].sampler=sampler;entries[1].binding=1;entries[1].textureView=texture->view;
    WGPUBindGroupDescriptor groupDescriptor=WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    groupDescriptor.layout=mr.textureLayout;groupDescriptor.entryCount=2;groupDescriptor.entries=entries;
    WGPUBindGroup group=wgpuDeviceCreateBindGroup(mr.device,&groupDescriptor);if(!group){wgpuSamplerRelease(sampler);return;}
    wgpuBindGroupRelease(texture->group);if(texture->customSampler)wgpuSamplerRelease(texture->customSampler);
    texture->group=group;texture->customSampler=sampler;
#else
    mr_web_texture_params(texture->id,texture->filter,texture->wrap);
#endif
}
void SetTextureFilter(Texture2D texture,int filter) {
    MRTexture *entry=mr_texture(texture.id);if(!entry)return;
    if(filter<TEXTURE_FILTER_POINT)filter=TEXTURE_FILTER_POINT;if(filter>TEXTURE_FILTER_ANISOTROPIC_16X)filter=TEXTURE_FILTER_ANISOTROPIC_16X;
    entry->filter=filter;mr_apply_texture_params(entry);
}
void SetTextureWrap(Texture2D texture,int wrap) {
    MRTexture *entry=mr_texture(texture.id);if(!entry)return;
    if(wrap<TEXTURE_WRAP_REPEAT)wrap=TEXTURE_WRAP_REPEAT;if(wrap>TEXTURE_WRAP_MIRROR_CLAMP)wrap=TEXTURE_WRAP_MIRROR_CLAMP;
    entry->wrap=wrap;mr_apply_texture_params(entry);
}
void SetShapesTexture(Texture2D texture,Rectangle source) {
    if (!mr_texture(texture.id) || texture.width<=0 || texture.height<=0) return;
    mr.shapesTexture=texture; mr.shapesSource=source;
}
Texture2D GetShapesTexture(void) { return mr.shapesTexture; }
Rectangle GetShapesTextureRectangle(void) { return mr.shapesSource; }
static const char *mr_default_vertex_wgsl="struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)color:vec4f};@vertex fn vs(@location(0)p:vec2f,@location(1)uv:vec2f,@location(2)c:vec4f,@location(3)z:f32)->V{var o:V;o.position=vec4f(p,z,1);o.uv=uv;o.color=c;return o;}";
static const char *mr_default_fragment_wgsl="struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)color:vec4f};@group(0)@binding(0)var smp:sampler;@group(0)@binding(1)var tex:texture_2d<f32>;@fragment fn fs(v:V)->@location(0)vec4f{return textureSample(tex,smp,v.uv)*v.color;}";
#ifdef _WIN32
static bool mr_make_shader_pipelines(const char *vsCode,const char *fsCode,WGPURenderPipeline output[6]) {
    WGPUShaderSourceWGSL vsSource=WGPU_SHADER_SOURCE_WGSL_INIT,fsSource=WGPU_SHADER_SOURCE_WGSL_INIT;vsSource.code=mr_string(vsCode);fsSource.code=mr_string(fsCode);
    WGPUShaderModuleDescriptor vsDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT,fsDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;vsDesc.nextInChain=&vsSource.chain;fsDesc.nextInChain=&fsSource.chain;
    WGPUShaderModule vs=wgpuDeviceCreateShaderModule(mr.device,&vsDesc),fs=wgpuDeviceCreateShaderModule(mr.device,&fsDesc);if(!vs||!fs)return false;
    WGPUBindGroupLayout layouts[2]={mr.textureLayout,mr.uniformLayout};WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;ld.bindGroupLayoutCount=2;ld.bindGroupLayouts=layouts;WGPUPipelineLayout layout=wgpuDeviceCreatePipelineLayout(mr.device,&ld);
    WGPUVertexAttribute attributes[4]={WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT};attributes[0].format=WGPUVertexFormat_Float32x2;attributes[1].format=WGPUVertexFormat_Float32x2;attributes[1].offset=offsetof(MRVertex,u);attributes[1].shaderLocation=1;attributes[2].format=WGPUVertexFormat_Unorm8x4;attributes[2].offset=offsetof(MRVertex,r);attributes[2].shaderLocation=2;attributes[3].format=WGPUVertexFormat_Float32;attributes[3].offset=offsetof(MRVertex,z);attributes[3].shaderLocation=3;
    WGPUVertexBufferLayout vl=WGPU_VERTEX_BUFFER_LAYOUT_INIT;vl.arrayStride=sizeof(MRVertex);vl.stepMode=WGPUVertexStepMode_Vertex;vl.attributeCount=4;vl.attributes=attributes;
    WGPUBlendState blend=WGPU_BLEND_STATE_INIT;WGPUColorTargetState target=WGPU_COLOR_TARGET_STATE_INIT;target.format=mr.config.format;target.blend=&blend;WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=fs;fragment.entryPoint=mr_string("fs");fragment.targetCount=1;fragment.targets=&target;
    WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;depth.format=WGPUTextureFormat_Depth24Plus;depth.depthWriteEnabled=WGPUOptionalBool_True;depth.depthCompare=WGPUCompareFunction_LessEqual;
    WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.layout=layout;pd.vertex.module=vs;pd.vertex.entryPoint=mr_string("vs");pd.vertex.bufferCount=1;pd.vertex.buffers=&vl;pd.primitive.topology=WGPUPrimitiveTopology_TriangleList;pd.fragment=&fragment;pd.depthStencil=&depth;
    bool ok=true;for(int mode=0;mode<6;mode++){blend.color.operation=mode==BLEND_SUBTRACT_COLORS?WGPUBlendOperation_ReverseSubtract:WGPUBlendOperation_Add;blend.alpha.operation=WGPUBlendOperation_Add;blend.color.srcFactor=(mode==BLEND_ALPHA_PREMULTIPLY||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS)?WGPUBlendFactor_One:mode==BLEND_MULTIPLIED?WGPUBlendFactor_Dst:WGPUBlendFactor_SrcAlpha;blend.color.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;blend.alpha.srcFactor=WGPUBlendFactor_One;blend.alpha.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;output[mode]=wgpuDeviceCreateRenderPipeline(mr.device,&pd);if(!output[mode])ok=false;}
    wgpuPipelineLayoutRelease(layout);wgpuShaderModuleRelease(vs);wgpuShaderModuleRelease(fs);return ok;
}
#endif
static unsigned int mr_name_hash(const char *name){unsigned int hash=2166136261u;if(name)while(*name){hash^=(unsigned char)*name++;hash*=16777619u;}return hash?hash:1;}
static bool mr_text_starts(const char *text,const char *prefix){while(*prefix)if(*text++!=*prefix++)return false;return true;}
static void mr_shader_parse_locations(MRShaderEntry *entry,const char *code){
    const char *tag="@raygpu_uniform";if(!code)return;
    for(const char *p=code;*p;p++)if(*p=='@'&&mr_text_starts(p,tag)){
        p+=15;while(*p==' '||*p=='\t')p++;char name[64];int n=0;while((*p=='_'||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9'))&&n<63)name[n++]=*p++;name[n]=0;
        while(*p==' '||*p=='\t')p++;int slot=0,hasDigit=false;while(*p>='0'&&*p<='9'){hasDigit=true;slot=slot*10+(*p++-'0');}
        if(!n||!hasDigit||slot<0||slot>=32)continue;entry->explicitLocations=true;unsigned int hash=mr_name_hash(name);bool exists=false;for(int i=0;i<entry->locationCount;i++)if(entry->nameHashes[i]==hash){exists=true;break;}if(!exists&&entry->locationCount<32){entry->nameHashes[entry->locationCount]=hash;entry->nameSlots[entry->locationCount]=(unsigned char)slot;entry->locationCount++;}
    }
}
Shader LoadShaderFromMemory(const char *vsCode,const char *fsCode){if(!mr.ready)return(Shader){0};if(!vsCode)vsCode=mr_default_vertex_wgsl;if(!fsCode)fsCode=mr_default_fragment_wgsl;MRShaderEntry*entry=NULL;for(int i=0;i<32;i++)if(!mr.shaders[i].id){entry=&mr.shaders[i];break;}if(!entry)return(Shader){0};memset(entry,0,sizeof *entry);mr_shader_parse_locations(entry,vsCode);mr_shader_parse_locations(entry,fsCode);unsigned int id=++mr.nextShader;
#ifdef _WIN32
    if(!mr_make_shader_pipelines(vsCode,fsCode,entry->pipelines)){for(int i=0;i<6;i++)if(entry->pipelines[i])wgpuRenderPipelineRelease(entry->pipelines[i]);memset(entry,0,sizeof *entry);return(Shader){0};}
    WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=sizeof entry->uniforms;bd.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;entry->uniformBuffer=wgpuDeviceCreateBuffer(mr.device,&bd);
    WGPUBindGroupEntry be=WGPU_BIND_GROUP_ENTRY_INIT;be.binding=0;be.buffer=entry->uniformBuffer;be.size=sizeof entry->uniforms;WGPUBindGroupDescriptor gd=WGPU_BIND_GROUP_DESCRIPTOR_INIT;gd.layout=mr.uniformLayout;gd.entryCount=1;gd.entries=&be;entry->uniformGroup=wgpuDeviceCreateBindGroup(mr.device,&gd);
#else
    if(!mr_web_shader_load(id,vsCode,fsCode))return(Shader){0};
#endif
    entry->id=id;return(Shader){id,NULL};}
Shader LoadShader(const char*vsFile,const char*fsFile){char*vs=vsFile?LoadFileText(vsFile):NULL,*fs=fsFile?LoadFileText(fsFile):NULL;if((vsFile&&!vs)||(fsFile&&!fs)){UnloadFileText(vs);UnloadFileText(fs);return(Shader){0};}Shader shader=LoadShaderFromMemory(vs,fs);UnloadFileText(vs);UnloadFileText(fs);return shader;}
bool IsShaderValid(Shader shader){return shader.id&&mr_shader(shader.id)!=NULL;}
void UnloadShader(Shader shader){MRShaderEntry*entry=mr_shader(shader.id);if(!entry)return;if(mr.currentShader==shader.id)mr.currentShader=0;
#ifdef _WIN32
    for(int i=0;i<6;i++)if(entry->pipelines[i])wgpuRenderPipelineRelease(entry->pipelines[i]);if(entry->uniformGroup)wgpuBindGroupRelease(entry->uniformGroup);if(entry->uniformBuffer)wgpuBufferRelease(entry->uniformBuffer);
#else
    mr_web_shader_unload(shader.id);
#endif
    memset(entry,0,sizeof *entry);}
void BeginShaderMode(Shader shader){mr.currentShader=IsShaderValid(shader)?shader.id:0;}
void EndShaderMode(void){mr.currentShader=0;}
int GetShaderLocation(Shader shader,const char *name){MRShaderEntry*entry=mr_shader(shader.id);if(!entry||!name)return-1;unsigned int hash=mr_name_hash(name);for(int i=0;i<entry->locationCount;i++)if(entry->nameHashes[i]==hash)return entry->nameSlots[i];if(entry->explicitLocations||entry->locationCount>=32)return-1;int slot=entry->locationCount;entry->nameHashes[entry->locationCount]=hash;entry->nameSlots[entry->locationCount]=(unsigned char)slot;entry->locationCount++;return slot;}
static int mr_uniform_components(int type){switch(type){case SHADER_UNIFORM_VEC2:case SHADER_UNIFORM_IVEC2:return 2;case SHADER_UNIFORM_VEC3:case SHADER_UNIFORM_IVEC3:return 3;case SHADER_UNIFORM_VEC4:case SHADER_UNIFORM_IVEC4:return 4;default:return 1;}}
void SetShaderValueV(Shader shader,int location,const void*value,int type,int count){MRShaderEntry*entry=mr_shader(shader.id);if(!entry||!value||location<0||location>=32||count<=0)return;int itemBytes=mr_uniform_components(type)*4;if(count>4)count=4;memset(entry->uniforms[location],0,64);for(int i=0;i<count;i++)memcpy(entry->uniforms[location]+i*16,(const unsigned char*)value+i*itemBytes,(size_t)itemBytes);
#ifdef _WIN32
    wgpuQueueWriteBuffer(mr.queue,entry->uniformBuffer,(uint64_t)location*64,entry->uniforms[location],64);
#else
    mr_web_shader_uniform(shader.id,location,entry->uniforms[location],64);
#endif
}
void SetShaderValue(Shader shader,int location,const void*value,int type){SetShaderValueV(shader,location,value,type,1);}
void SetShaderValueMatrix(Shader shader,int location,Matrix matrix){SetShaderValueV(shader,location,&matrix,SHADER_UNIFORM_VEC4,4);}
#ifdef _WIN32
static bool mr_renderer(void) {
    WGPUSurfaceCapabilities caps=WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(mr.surface,mr.adapter,&caps)!=WGPUStatus_Success || !caps.formatCount || !caps.alphaModeCount) {
        wgpuSurfaceCapabilitiesFreeMembers(caps); mr_error("Cannot query surface capabilities"); return false;
    }
    WGPUSurfaceConfiguration config=WGPU_SURFACE_CONFIGURATION_INIT;
    mr.config=config;
    mr.config.device=mr.device; mr.config.format=caps.formats[0];
    for (size_t i=0;i<caps.formatCount;i++) if (caps.formats[i]==WGPUTextureFormat_BGRA8Unorm || caps.formats[i]==WGPUTextureFormat_RGBA8Unorm) { mr.config.format=caps.formats[i]; break; }
    mr.config.alphaMode=caps.alphaModes[0];
    mr.config.presentMode=WGPUPresentMode_Fifo;
    /* SetTargetFPS is a software cap, like raylib's default behavior. FIFO
     * already waits for vertical sync; combining both waits can halve 60 to
     * 30 FPS. Prefer Immediate and apply exactly one precise frame limit. */
    for (size_t i=0;i<caps.presentModeCount;i++) {
        if (caps.presentModes[i]==WGPUPresentMode_Immediate) {
            mr.config.presentMode=WGPUPresentMode_Immediate;
            mr.softwareFrameLimit=true;
            break;
        }
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    WGPUBindGroupLayoutEntry entries[2]={WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding=0; entries[0].visibility=WGPUShaderStage_Fragment; entries[0].sampler.type=WGPUSamplerBindingType_Filtering;
    entries[1].binding=1; entries[1].visibility=WGPUShaderStage_Fragment;
    entries[1].texture.sampleType=WGPUTextureSampleType_Float; entries[1].texture.viewDimension=WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor bindLayout=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindLayout.entryCount=2; bindLayout.entries=entries;
    mr.textureLayout=wgpuDeviceCreateBindGroupLayout(mr.device,&bindLayout);
    WGPUBindGroupLayoutEntry uniformEntry=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;uniformEntry.binding=0;uniformEntry.visibility=WGPUShaderStage_Vertex|WGPUShaderStage_Fragment;uniformEntry.buffer.type=WGPUBufferBindingType_Uniform;uniformEntry.buffer.minBindingSize=32*64;
    WGPUBindGroupLayoutDescriptor uniformDesc=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;uniformDesc.entryCount=1;uniformDesc.entries=&uniformEntry;mr.uniformLayout=wgpuDeviceCreateBindGroupLayout(mr.device,&uniformDesc);
    WGPUSamplerDescriptor sampler=WGPU_SAMPLER_DESCRIPTOR_INIT;
    sampler.magFilter=WGPUFilterMode_Nearest; sampler.minFilter=WGPUFilterMode_Nearest;
    sampler.addressModeU=WGPUAddressMode_Repeat; sampler.addressModeV=WGPUAddressMode_Repeat;
    mr.sampler=wgpuDeviceCreateSampler(mr.device,&sampler);
    WGPUPipelineLayoutDescriptor layout=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    layout.bindGroupLayoutCount=1; layout.bindGroupLayouts=&mr.textureLayout;
    WGPUPipelineLayout pipelineLayout=wgpuDeviceCreatePipelineLayout(mr.device,&layout);
    const char *wgsl=
        "struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };\n"
        "@group(0) @binding(0) var smp: sampler;\n"
        "@group(0) @binding(1) var tex: texture_2d<f32>;\n"
        "@vertex fn vs(@location(0) p: vec2f, @location(1) uv: vec2f, @location(2) c: vec4f, @location(3) z: f32) -> V {\n"
        " var o: V; o.position=vec4f(p,z,1); o.uv=uv; o.color=c; return o; }\n"
        "@fragment fn fs(v: V) -> @location(0) vec4f { return textureSample(tex,smp,v.uv)*v.color; }\n";
    WGPUShaderSourceWGSL source=WGPU_SHADER_SOURCE_WGSL_INIT; source.code=mr_string(wgsl);
    WGPUShaderModuleDescriptor shaderDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT; shaderDesc.nextInChain=&source.chain;
    WGPUShaderModule shader=wgpuDeviceCreateShaderModule(mr.device,&shaderDesc);
    WGPUVertexAttribute attributes[4]={WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].format=WGPUVertexFormat_Float32x2;
    attributes[1].format=WGPUVertexFormat_Float32x2; attributes[1].offset=offsetof(MRVertex,u); attributes[1].shaderLocation=1;
    attributes[2].format=WGPUVertexFormat_Unorm8x4; attributes[2].offset=offsetof(MRVertex,r); attributes[2].shaderLocation=2;
    attributes[3].format=WGPUVertexFormat_Float32; attributes[3].offset=offsetof(MRVertex,z); attributes[3].shaderLocation=3;
    WGPUVertexBufferLayout vertexLayout=WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexLayout.arrayStride=sizeof(MRVertex); vertexLayout.stepMode=WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount=4; vertexLayout.attributes=attributes;
    WGPUBlendState blend=WGPU_BLEND_STATE_INIT;
    WGPUColorTargetState target=WGPU_COLOR_TARGET_STATE_INIT; target.format=mr.config.format; target.blend=&blend;
    WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;
    fragment.module=shader; fragment.entryPoint=mr_string("fs"); fragment.targetCount=1; fragment.targets=&target;
    WGPURenderPipelineDescriptor pipeline=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipeline.layout=pipelineLayout; pipeline.vertex.module=shader; pipeline.vertex.entryPoint=mr_string("vs");
    pipeline.vertex.bufferCount=1; pipeline.vertex.buffers=&vertexLayout;
    WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT; depth.format=WGPUTextureFormat_Depth24Plus;
    depth.depthWriteEnabled=WGPUOptionalBool_True; depth.depthCompare=WGPUCompareFunction_LessEqual;
    pipeline.primitive.topology=WGPUPrimitiveTopology_TriangleList; pipeline.fragment=&fragment; pipeline.depthStencil=&depth;
    for (int mode=0;mode<6;mode++) {
        blend.color.operation=mode==BLEND_SUBTRACT_COLORS?WGPUBlendOperation_ReverseSubtract:WGPUBlendOperation_Add;
        blend.alpha.operation=WGPUBlendOperation_Add;
        blend.color.srcFactor=(mode==BLEND_ALPHA_PREMULTIPLY||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS)?WGPUBlendFactor_One:
            mode==BLEND_MULTIPLIED?WGPUBlendFactor_Dst:WGPUBlendFactor_SrcAlpha;
        blend.color.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;
        blend.alpha.srcFactor=WGPUBlendFactor_One;
        blend.alpha.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;
        mr.pipelines[mode]=wgpuDeviceCreateRenderPipeline(mr.device,&pipeline);
    }
    const char *wgsl3d=
        "struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)color:vec4f,@location(2)normal:vec3f,@location(3)world:vec3f,@location(4)camera:vec3f};\n"
        "@group(0)@binding(0)var smp:sampler;@group(0)@binding(1)var tex:texture_2d<f32>;\n"
        "@vertex fn vs(@location(0)p:vec3f,@location(1)n:vec3f,@location(2)uv:vec2f,@location(3)c:vec4f,"
        "@location(4)m0:vec4f,@location(5)m1:vec4f,@location(6)m2:vec4f,@location(7)m3:vec4f,"
        "@location(8)v0:vec4f,@location(9)v1:vec4f,@location(10)v2:vec4f,@location(11)v3:vec4f,@location(12)tint:vec4f,@location(13)camera:vec3f)->V{"
        "let model=mat4x4f(m0,m1,m2,m3);let vp=mat4x4f(v0,v1,v2,v3);let world=vec4f(p,1)*model;var o:V;o.position=world*vp;o.uv=uv;o.color=c*tint;"
        "let c0=cross(m1.xyz,m2.xyz);let c1=cross(m2.xyz,m0.xyz);let c2=cross(m0.xyz,m1.xyz);let handed=select(-1.0,1.0,dot(m0.xyz,c0)>=0);o.normal=normalize(vec3f(dot(n,c0),dot(n,c1),dot(n,c2))*handed);o.world=world.xyz;o.camera=camera;return o;}\n"
        "@fragment fn fs(v:V,@builtin(front_facing)front:bool)->@location(0)vec4f{var normal=normalize(v.normal);if(!front){normal=-normal;}"
        "let light=normalize(vec3f(0.45,0.85,0.35));let diffuse=max(dot(normal,light),0);let view=normalize(v.camera-v.world);let halfVector=normalize(light+view);"
        "let specular=pow(max(dot(normal,halfVector),0),32)*0.22;let albedo=textureSample(tex,smp,v.uv)*v.color;let lighting=0.22+0.78*diffuse;return vec4f(albedo.rgb*lighting+specular*albedo.a,albedo.a);}";
    WGPUShaderSourceWGSL source3d=WGPU_SHADER_SOURCE_WGSL_INIT;source3d.code=mr_string(wgsl3d);WGPUShaderModuleDescriptor shaderDesc3d=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;shaderDesc3d.nextInChain=&source3d.chain;WGPUShaderModule shader3d=wgpuDeviceCreateShaderModule(mr.device,&shaderDesc3d);
    WGPUVertexAttribute meshAttributes[4]={WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT};
    meshAttributes[0].format=WGPUVertexFormat_Float32x3;meshAttributes[0].offset=offsetof(MRGpuVertex,x);meshAttributes[0].shaderLocation=0;
    meshAttributes[1].format=WGPUVertexFormat_Float32x3;meshAttributes[1].offset=offsetof(MRGpuVertex,nx);meshAttributes[1].shaderLocation=1;
    meshAttributes[2].format=WGPUVertexFormat_Float32x2;meshAttributes[2].offset=offsetof(MRGpuVertex,u);meshAttributes[2].shaderLocation=2;
    meshAttributes[3].format=WGPUVertexFormat_Unorm8x4;meshAttributes[3].offset=offsetof(MRGpuVertex,r);meshAttributes[3].shaderLocation=3;
    WGPUVertexAttribute instanceAttributes[10];memset(instanceAttributes,0,sizeof instanceAttributes);for(int i=0;i<8;i++){instanceAttributes[i].format=WGPUVertexFormat_Float32x4;instanceAttributes[i].offset=(uint64_t)i*16;instanceAttributes[i].shaderLocation=(uint32_t)i+4;}instanceAttributes[8].format=WGPUVertexFormat_Unorm8x4;instanceAttributes[8].offset=offsetof(MRInstance3D,tint);instanceAttributes[8].shaderLocation=12;instanceAttributes[9].format=WGPUVertexFormat_Float32x3;instanceAttributes[9].offset=offsetof(MRInstance3D,camera);instanceAttributes[9].shaderLocation=13;
    WGPUVertexBufferLayout layouts3d[2]={WGPU_VERTEX_BUFFER_LAYOUT_INIT,WGPU_VERTEX_BUFFER_LAYOUT_INIT};layouts3d[0].arrayStride=sizeof(MRGpuVertex);layouts3d[0].stepMode=WGPUVertexStepMode_Vertex;layouts3d[0].attributeCount=4;layouts3d[0].attributes=meshAttributes;layouts3d[1].arrayStride=sizeof(MRInstance3D);layouts3d[1].stepMode=WGPUVertexStepMode_Instance;layouts3d[1].attributeCount=10;layouts3d[1].attributes=instanceAttributes;
    WGPUBlendState blend3d=WGPU_BLEND_STATE_INIT;blend3d.color.srcFactor=WGPUBlendFactor_SrcAlpha;blend3d.color.dstFactor=WGPUBlendFactor_OneMinusSrcAlpha;blend3d.color.operation=WGPUBlendOperation_Add;blend3d.alpha.srcFactor=WGPUBlendFactor_One;blend3d.alpha.dstFactor=WGPUBlendFactor_OneMinusSrcAlpha;blend3d.alpha.operation=WGPUBlendOperation_Add;
    WGPUColorTargetState target3d=WGPU_COLOR_TARGET_STATE_INIT;target3d.format=mr.config.format;target3d.blend=&blend3d;WGPUFragmentState fragment3d=WGPU_FRAGMENT_STATE_INIT;fragment3d.module=shader3d;fragment3d.entryPoint=mr_string("fs");fragment3d.targetCount=1;fragment3d.targets=&target3d;
    WGPURenderPipelineDescriptor pipeline3d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pipeline3d.layout=pipelineLayout;pipeline3d.vertex.module=shader3d;pipeline3d.vertex.entryPoint=mr_string("vs");pipeline3d.vertex.bufferCount=2;pipeline3d.vertex.buffers=layouts3d;pipeline3d.fragment=&fragment3d;pipeline3d.primitive.topology=WGPUPrimitiveTopology_TriangleList;pipeline3d.primitive.cullMode=WGPUCullMode_None;pipeline3d.depthStencil=&depth;mr.pipeline3d=wgpuDeviceCreateRenderPipeline(mr.device,&pipeline3d);
    wgpuShaderModuleRelease(shader3d);
    wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(pipelineLayout);
    WGPUBufferDescriptor buffer=WGPU_BUFFER_DESCRIPTOR_INIT;
    buffer.size=sizeof mr.vertices; buffer.usage=WGPUBufferUsage_Vertex|WGPUBufferUsage_CopyDst;
    mr.buffer=wgpuDeviceCreateBuffer(mr.device,&buffer);
    buffer.size=sizeof mr.instances3d;mr.instanceBuffer3d=wgpuDeviceCreateBuffer(mr.device,&buffer);
    const unsigned char white[4]={255,255,255,255}; mr.white=LoadTextureRGBA(white,1,1).id;
    mr.shapesTexture=(Texture2D){mr.white,1,1,1,7}; mr.shapesSource=(Rectangle){0,0,1,1};
    return mr.pipelines[0] && mr.pipeline3d && mr.buffer && mr.instanceBuffer3d && mr.white && !mr.error;
}
static bool mr_resize_depth(int width,int height) {
    if(width<=0||height<=0)return false;
    if(mr.depthView&&mr.depthWidth==width&&mr.depthHeight==height)return true;
    if(mr.depthView)wgpuTextureViewRelease(mr.depthView);
    if(mr.depthTexture)wgpuTextureRelease(mr.depthTexture);
    mr.depthView=NULL;mr.depthTexture=NULL;mr.depthWidth=mr.depthHeight=0;
    WGPUTextureDescriptor descriptor=WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.size=(WGPUExtent3D){(uint32_t)width,(uint32_t)height,1};
    descriptor.dimension=WGPUTextureDimension_2D;descriptor.format=WGPUTextureFormat_Depth24Plus;
    descriptor.usage=WGPUTextureUsage_RenderAttachment;
    mr.depthTexture=wgpuDeviceCreateTexture(mr.device,&descriptor);
    if(!mr.depthTexture)return false;
    mr.depthView=wgpuTextureCreateView(mr.depthTexture,NULL);
    if(!mr.depthView){wgpuTextureRelease(mr.depthTexture);mr.depthTexture=NULL;return false;}
    mr.depthWidth=width;mr.depthHeight=height;return true;
}
void InitWindow(int width,int height,const char *title) {
    if (mr.instance) { fprintf(stderr,"raygpu: only one window is supported\n"); return; }
    memset(&mr,0,sizeof mr); mr.fps=60; mr.dt=1.0f/60; mr.clear=BLACK; mr.exitKey=KEY_ESCAPE; mr.focused=true; mr.mouseScale=(Vector2){1,1};
    mr.start=mr.previous=mr_clock();
    if (width<=0 || height<=0 || width>8192 || height>8192) { mr_error("Invalid window size"); return; }
    mr.width=width; mr.height=height;
    /* Keep window, framebuffer and mouse coordinates in physical pixels.
     * Without DPI awareness Windows turns 1920x1080 into 3840x2160 when the
     * desktop uses 200% display scaling. This must happen before any HWND is
     * created. */
    SetProcessDPIAware();
    HINSTANCE module=GetModuleHandleA(NULL);
    WNDCLASSA wc={0}; wc.lpfnWndProc=mr_window_proc; wc.hInstance=module;
    wc.lpszClassName="RaygpuWebGPU"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    if (!RegisterClassA(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) { mr_error("Cannot register window"); return; }
    RECT rect={0,0,width,height}; AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    mr.window=CreateWindowExA(0,wc.lpszClassName,title,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
        rect.right-rect.left,rect.bottom-rect.top,NULL,NULL,module,NULL);
    if (!mr.window) { mr_error("Cannot create window"); return; }
    ShowWindow(mr.window,SW_SHOW);
    mr.instance=wgpuCreateInstance(NULL);
    if (!mr.instance) { mr_error("Cannot create WebGPU instance"); CloseWindow(); return; }
    WGPUSurfaceDescriptor surface=WGPU_SURFACE_DESCRIPTOR_INIT;
    WGPUSurfaceSourceWindowsHWND hwnd=WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
    hwnd.hinstance=module; hwnd.hwnd=mr.window; surface.nextInChain=&hwnd.chain;
    mr.surface=wgpuInstanceCreateSurface(mr.instance,&surface);
    if (!mr.surface) { mr_error("Cannot create WebGPU surface"); CloseWindow(); return; }
    WGPURequestAdapterOptions options=WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.backendType=WGPUBackendType_D3D12;
    options.compatibleSurface=mr.surface; options.powerPreference=WGPUPowerPreference_HighPerformance;
    WGPURequestAdapterCallbackInfo adapterInfo=WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapterInfo.mode=MR_CALLBACK_MODE; adapterInfo.callback=mr_adapter;
    wgpuInstanceRequestAdapter(mr.instance,&options,adapterInfo);
    while (!mr.adapterDone) { mr_pump(); mr_yield(1); }
    if (!mr.adapter || mr.close) { CloseWindow(); return; }
    WGPUDeviceDescriptor device=WGPU_DEVICE_DESCRIPTOR_INIT;
    device.uncapturedErrorCallbackInfo.callback=mr_gpu_error;
    device.deviceLostCallbackInfo.mode=MR_CALLBACK_MODE; device.deviceLostCallbackInfo.callback=mr_device_lost;
    WGPURequestDeviceCallbackInfo deviceInfo=WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    deviceInfo.mode=MR_CALLBACK_MODE; deviceInfo.callback=mr_device;
    wgpuAdapterRequestDevice(mr.adapter,&device,deviceInfo);
    while (!mr.deviceDone) { mr_pump(); mr_yield(1); }
    if (!mr.device || mr.close) { CloseWindow(); return; }
    mr.queue=wgpuDeviceGetQueue(mr.device); mr.ready=mr_renderer();
    if (!mr.ready) { CloseWindow(); return; }
    mr.previous=mr_clock(); puts("raygpu: WebGPU renderer ready");
}
#else
void InitWindow(int width,int height,const char *title) {
    if (mr.ready) return;
    memset(&mr,0,sizeof mr); mr.fps=60; mr.dt=1.0f/60; mr.clear=BLACK; mr.exitKey=KEY_ESCAPE; mr.focused=true; mr.mouseScale=(Vector2){1,1};
    if (width<=0 || height<=0 || width>8192 || height>8192) { mr_error("Invalid window size"); return; }
    mr.width=width; mr.height=height; mr.start=mr.previous=mr_clock();
    mr_web_init(width,height,title); mr.ready=true;
    const unsigned char white[4]={255,255,255,255}; mr.white=LoadTextureRGBA(white,1,1).id;
    mr.shapesTexture=(Texture2D){mr.white,1,1,1,7}; mr.shapesSource=(Rectangle){0,0,1,1};
    puts("raygpu: WebGPU renderer ready");
}
#endif
bool IsWindowReady(void) { return mr.ready; }
bool RayGPUHadError(void) { return mr.error; }
bool WindowShouldClose(void) {
    mr_pump();
    mr_dispatch_readbacks();
    if (mr.exitKey>KEY_NULL && IsKeyPressed(mr.exitKey)) mr.close=true;
    double now=mr_clock(); mr.dt=(float)(now-mr.previous); mr.previous=now; mr.frameStart=now;
    if (mr.dt>0.1f) mr.dt=0.1f;
    return mr.close || !mr.ready;
}
int GetScreenWidth(void) { return mr.width; }
int GetScreenHeight(void) { return mr.height; }
int GetRenderWidth(void) { return mr.width; }
int GetRenderHeight(void) { return mr.height; }
bool IsWindowResized(void) { return mr.resized; }
bool IsWindowFocused(void) { return mr.focused; }
bool IsWindowMinimized(void) {
#ifdef _WIN32
    return mr.window && IsIconic(mr.window);
#else
    return false;
#endif
}
bool IsWindowMaximized(void) {
#ifdef _WIN32
    return mr.window && IsZoomed(mr.window);
#else
    return false;
#endif
}
Vector2 GetWindowPosition(void) {
#ifdef _WIN32
    RECT rect={0}; if (mr.window && GetWindowRect(mr.window,&rect)) return (Vector2){(float)rect.left,(float)rect.top};
#endif
    return (Vector2){0};
}
Vector2 GetWindowScaleDPI(void) {
#ifdef _WIN32
    if (mr.window) { float scale=GetDpiForWindow(mr.window)/96.0f; if (scale>0) return (Vector2){scale,scale}; }
#endif
    return (Vector2){1,1};
}
void SetWindowTitle(const char *title) {
    if (!title) return;
#ifdef _WIN32
    if (mr.window) SetWindowTextA(mr.window,title);
#else
    mr_web_window_command(0,0,0,title);
#endif
}
void SetWindowPosition(int x,int y) {
#ifdef _WIN32
    if (mr.window) SetWindowPos(mr.window,NULL,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
#else
    (void)x; (void)y;
#endif
}
void SetWindowSize(int width,int height) {
    if (width<=0 || height<=0 || width>8192 || height>8192) return;
#ifdef _WIN32
    if (mr.window) { RECT rect={0,0,width,height}; AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE); SetWindowPos(mr.window,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE); }
#else
    mr.width=width; mr.height=height; mr.resized=true; mr_web_window_command(1,width,height,NULL);
#endif
}
void SetWindowMinSize(int width,int height) { mr.minWidth=width>0?width:0; mr.minHeight=height>0?height:0; }
void SetWindowMaxSize(int width,int height) { mr.maxWidth=width>0?width:0; mr.maxHeight=height>0?height:0; }
void MinimizeWindow(void) {
#ifdef _WIN32
    if (mr.window) ShowWindow(mr.window,SW_MINIMIZE);
#endif
}
void MaximizeWindow(void) {
#ifdef _WIN32
    if (mr.window) ShowWindow(mr.window,SW_MAXIMIZE);
#endif
}
void RestoreWindow(void) {
#ifdef _WIN32
    if (mr.window) ShowWindow(mr.window,SW_RESTORE);
#endif
}
void SetExitKey(int key) { mr.exitKey=(key>=KEY_NULL && key<512)?key:KEY_NULL; }
void SetTargetFPS(int fps) {
    mr.fps=fps>0 ? fps : 0;
#ifdef __wasm__
    mr_web_fps(mr.fps);
#endif
}
float GetFrameTime(void) { return mr.dt; }
double GetTime(void) { return mr_clock()-mr.start; }
int GetFPS(void) { return mr.dt>0.000001f ? (int)(1.0f/mr.dt+0.5f) : 0; }
void WaitTime(double seconds) {
    if (seconds<=0) return;
    double end=mr_clock()+seconds;
#ifdef _WIN32
    while (mr_clock()+0.002<end) Sleep(1);
#endif
    while (mr_clock()<end) { }
}
#ifdef __wasm__
typedef struct MRMemoryBlock { size_t size; struct MRMemoryBlock *next; bool free; } MRMemoryBlock;
extern unsigned char __heap_base;
static MRMemoryBlock *mr_memory_head;
static size_t mr_align_size(size_t size) {
    const size_t alignment=_Alignof(max_align_t);
    return (size+alignment-1)&~(alignment-1);
}
static bool mr_grow_memory(size_t required) {
    const size_t pageSize=65536;
    size_t pages=(required+sizeof(MRMemoryBlock)+pageSize-1)/pageSize;
    size_t oldPages=__builtin_wasm_memory_size(0);
    size_t result=__builtin_wasm_memory_grow(0,pages);
    if (result==(size_t)-1) return false;
    MRMemoryBlock *tail=mr_memory_head;
    while (tail && tail->next) tail=tail->next;
    unsigned char *oldEnd=(unsigned char *)(oldPages*pageSize);
    if (tail && tail->free && (unsigned char *)(tail+1)+tail->size==oldEnd) tail->size+=pages*pageSize;
    else {
        MRMemoryBlock *block=(MRMemoryBlock *)oldEnd;
        block->size=pages*pageSize-sizeof(MRMemoryBlock); block->next=NULL; block->free=true;
        if (tail) tail->next=block; else mr_memory_head=block;
    }
    return true;
}
void *MemAlloc(unsigned int requested) {
    if (!requested) return NULL;
    if (!mr_memory_head) {
        uintptr_t start=((uintptr_t)&__heap_base+_Alignof(max_align_t)-1)&~(uintptr_t)(_Alignof(max_align_t)-1);
        size_t end=__builtin_wasm_memory_size(0)*65536u;
        if (end<=start+sizeof(MRMemoryBlock)) return NULL;
        mr_memory_head=(MRMemoryBlock *)start;
        mr_memory_head->size=end-start-sizeof(MRMemoryBlock);
        mr_memory_head->next=NULL; mr_memory_head->free=true;
    }
    size_t size=mr_align_size(requested);
    for (MRMemoryBlock *block=mr_memory_head;block;block=block->next) if (block->free && block->size>=size) {
        if (block->size>=size+sizeof(MRMemoryBlock)+_Alignof(max_align_t)) {
            MRMemoryBlock *next=(MRMemoryBlock *)((unsigned char *)(block+1)+size);
            next->size=block->size-size-sizeof(MRMemoryBlock); next->next=block->next; next->free=true;
            block->next=next; block->size=size;
        }
        block->free=false; return block+1;
    }
    return mr_grow_memory(size) ? MemAlloc(requested) : NULL;
}
void MemFree(void *pointer) {
    if (!pointer) return;
    MRMemoryBlock *block=(MRMemoryBlock *)pointer-1; block->free=true;
    for (MRMemoryBlock *it=mr_memory_head;it && it->next;) {
        if (it->free && it->next->free) { it->size+=sizeof(MRMemoryBlock)+it->next->size; it->next=it->next->next; }
        else it=it->next;
    }
}
void *MemRealloc(void *pointer,unsigned int size) {
    if (!pointer) return MemAlloc(size);
    if (!size) { MemFree(pointer); return NULL; }
    MRMemoryBlock *old=(MRMemoryBlock *)pointer-1;
    if (old->size>=size) return pointer;
    void *replacement=MemAlloc(size);
    if (replacement) { memcpy(replacement,pointer,old->size); MemFree(pointer); }
    return replacement;
}
#else
void *MemAlloc(unsigned int size) { return size ? malloc(size) : NULL; }
void *MemRealloc(void *pointer,unsigned int size) { return realloc(pointer,size); }
void MemFree(void *pointer) { free(pointer); }
#endif
unsigned char *LoadFileData(const char *fileName,int *dataSize) {
    if (dataSize) *dataSize=0; if (!fileName) return NULL;
#ifdef __wasm__
    int size=mr_web_file_size(fileName); if (size<=0) return NULL;
    unsigned char *data=MemAlloc((unsigned int)size); if (!data) return NULL;
    int read=mr_web_file_read(fileName,data,size); if (read!=size) { MemFree(data); return NULL; }
#else
    FILE *file=NULL; if (fopen_s(&file,fileName,"rb")!=0 || !file) return NULL;
    if (fseek(file,0,SEEK_END)!=0) { fclose(file); return NULL; }
    long length=ftell(file); if (length<=0 || length>0x7fffffffL || fseek(file,0,SEEK_SET)!=0) { fclose(file); return NULL; }
    int size=(int)length; unsigned char *data=MemAlloc((unsigned int)size);
    if (!data || fread(data,1,(size_t)size,file)!=(size_t)size) { fclose(file); MemFree(data); return NULL; }
    fclose(file);
#endif
    if (dataSize) *dataSize=size; return data;
}
void UnloadFileData(unsigned char *data) { MemFree(data); }
bool SaveFileData(const char *fileName,void *data,int dataSize) {
    if (!fileName || !data || dataSize<0) return false;
#ifdef __wasm__
    return mr_web_file_write(fileName,data,dataSize)!=0;
#else
    FILE *file=NULL; if (fopen_s(&file,fileName,"wb")!=0 || !file) return false;
    bool saved=fwrite(data,1,(size_t)dataSize,file)==(size_t)dataSize;
    if (fclose(file)!=0) saved=false;
    return saved;
#endif
}
int GetFileLength(const char *fileName) {
#ifdef __wasm__
    return fileName ? mr_web_file_size(fileName) : 0;
#else
    if (!fileName) return 0; FILE *file=NULL; if (fopen_s(&file,fileName,"rb")!=0 || !file) return 0;
    if (fseek(file,0,SEEK_END)!=0) { fclose(file); return 0; } long length=ftell(file); fclose(file);
    return length>0 && length<=0x7fffffffL ? (int)length : 0;
#endif
}
bool FileExists(const char *fileName) { return GetFileLength(fileName)>0; }
char *LoadFileText(const char *fileName) {
    int size=0; unsigned char *data=LoadFileData(fileName,&size); if (!data) return NULL;
    char *text=MemAlloc((unsigned int)size+1); if (text) { memcpy(text,data,(size_t)size); text[size]='\0'; }
    UnloadFileData(data); return text;
}
void UnloadFileText(char *text) { MemFree(text); }
bool SaveFileText(const char *fileName,char *text) {
    if (!text) return false;
    int length=0; while (text[length]) length++;
    return SaveFileData(fileName,text,length);
}
char *EncodeDataBase64(const unsigned char *data,int dataSize,int *outputSize) {
    static const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if(outputSize)*outputSize=0;if(dataSize<0||(!data&&dataSize>0))return NULL;
    if(dataSize>0x5ffffffd)return NULL;int length=((dataSize+2)/3)*4;
    char *encoded=MemAlloc((unsigned int)length+1);if(!encoded)return NULL;
    int input=0,output=0;while(input<dataSize){unsigned int a=data[input++],b=input<dataSize?data[input++]:0,c=input<dataSize?data[input++]:0,triple=(a<<16)|(b<<8)|c;encoded[output++]=table[(triple>>18)&63];encoded[output++]=table[(triple>>12)&63];encoded[output++]=table[(triple>>6)&63];encoded[output++]=table[triple&63];}
    int remainder=dataSize%3;if(remainder==1){encoded[length-2]='=';encoded[length-1]='=';}else if(remainder==2)encoded[length-1]='=';encoded[length]=0;if(outputSize)*outputSize=length;return encoded;
}
static int mr_base64_value(unsigned char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return-1;}
unsigned char *DecodeDataBase64(const unsigned char *data,int *outputSize) {
    if(outputSize)*outputSize=0;if(!data)return NULL;int symbols=0,padding=0;
    for(int i=0;data[i];i++){unsigned char c=data[i];if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;if(c=='='){padding++;symbols++;}else{if(mr_base64_value(c)<0||padding)return NULL;symbols++;}}
    if(symbols%4||padding>2)return NULL;int length=(symbols/4)*3-padding;unsigned char *decoded=MemAlloc((unsigned int)(length>0?length:1));if(!decoded)return NULL;
    int values[4],count=0,output=0;for(int i=0;data[i];i++){unsigned char c=data[i];if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;values[count++]=c=='='?0:mr_base64_value(c);if(count==4){unsigned int triple=((unsigned int)values[0]<<18)|((unsigned int)values[1]<<12)|((unsigned int)values[2]<<6)|(unsigned int)values[3];if(output<length)decoded[output++]=(unsigned char)(triple>>16);if(output<length)decoded[output++]=(unsigned char)(triple>>8);if(output<length)decoded[output++]=(unsigned char)triple;count=0;}}
    if(outputSize)*outputSize=length;return decoded;
}
unsigned int ComputeCRC32(unsigned char *data,int dataSize) {
    if(!data||dataSize<=0)return 0;unsigned int crc=0xffffffffu;
    for(int i=0;i<dataSize;i++){crc^=data[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&((unsigned int)-(int)(crc&1u)));}
    return~crc;
}
static uint32_t mr_rotate_left32(uint32_t value,unsigned int shift){return(value<<shift)|(value>>(32-shift));}
unsigned int *ComputeMD5(unsigned char *data,int dataSize) {
    static unsigned int hash[4];
    static const uint32_t shifts[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    static const uint32_t constants[64]={0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    hash[0]=0x67452301u;hash[1]=0xefcdab89u;hash[2]=0x98badcfeu;hash[3]=0x10325476u;if(dataSize<0||(!data&&dataSize>0)){memset(hash,0,sizeof hash);return hash;}
    uint64_t bitLength=(uint64_t)(unsigned int)dataSize*8u;uint32_t blocks=(uint32_t)(dataSize/64)+((dataSize%64)<=55?1u:2u);
    for(uint32_t blockIndex=0;blockIndex<blocks;blockIndex++){unsigned char block[64]={0};uint64_t start=(uint64_t)blockIndex*64u;for(int i=0;i<64;i++){uint64_t position=start+(unsigned int)i;if(position<(uint64_t)dataSize)block[i]=data[(int)position];else if(position==(uint64_t)dataSize)block[i]=0x80;}if(blockIndex==blocks-1)for(int i=0;i<8;i++)block[56+i]=(unsigned char)(bitLength>>(i*8));uint32_t words[16];for(int i=0;i<16;i++)words[i]=(uint32_t)block[i*4]|((uint32_t)block[i*4+1]<<8)|((uint32_t)block[i*4+2]<<16)|((uint32_t)block[i*4+3]<<24);uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3];for(int i=0;i<64;i++){uint32_t f,g;if(i<16){f=(b&c)|(~b&d);g=(uint32_t)i;}else if(i<32){f=(d&b)|(~d&c);g=(uint32_t)(5*i+1)%16;}else if(i<48){f=b^c^d;g=(uint32_t)(3*i+5)%16;}else{f=c^(b|~d);g=(uint32_t)(7*i)%16;}uint32_t next=d;d=c;c=b;b+=mr_rotate_left32(a+f+constants[i]+words[g],shifts[i]);a=next;}hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;}
    return hash;
}
unsigned int *ComputeSHA1(unsigned char *data,int dataSize) {
    static unsigned int hash[5];hash[0]=0x67452301u;hash[1]=0xefcdab89u;hash[2]=0x98badcfeu;hash[3]=0x10325476u;hash[4]=0xc3d2e1f0u;if(dataSize<0||(!data&&dataSize>0)){memset(hash,0,sizeof hash);return hash;}
    uint64_t bitLength=(uint64_t)(unsigned int)dataSize*8u;uint32_t blocks=(uint32_t)(dataSize/64)+((dataSize%64)<=55?1u:2u);
    for(uint32_t blockIndex=0;blockIndex<blocks;blockIndex++){unsigned char block[64]={0};uint64_t start=(uint64_t)blockIndex*64u;for(int i=0;i<64;i++){uint64_t position=start+(unsigned int)i;if(position<(uint64_t)dataSize)block[i]=data[(int)position];else if(position==(uint64_t)dataSize)block[i]=0x80;}if(blockIndex==blocks-1)for(int i=0;i<8;i++)block[63-i]=(unsigned char)(bitLength>>(i*8));uint32_t words[80]={0};for(int i=0;i<16;i++)words[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];for(int i=16;i<80;i++)words[i]=mr_rotate_left32(words[i-3]^words[i-8]^words[i-14]^words[i-16],1);uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3],e=hash[4];for(int i=0;i<80;i++){uint32_t f,k;if(i<20){f=(b&c)|(~b&d);k=0x5a827999u;}else if(i<40){f=b^c^d;k=0x6ed9eba1u;}else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdcu;}else{f=b^c^d;k=0xca62c1d6u;}uint32_t next=mr_rotate_left32(a,5)+f+e+k+words[i];e=d;d=c;c=mr_rotate_left32(b,30);b=a;a=next;}hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;hash[4]+=e;}
    return hash;
}
bool DirectoryExists(const char *path) {
#ifdef _WIN32
    DWORD attributes=path?GetFileAttributesA(path):INVALID_FILE_ATTRIBUTES;
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
#else
    (void)path; return false;
#endif
}
bool IsPathFile(const char *path) {
#ifdef _WIN32
    DWORD attributes=path?GetFileAttributesA(path):INVALID_FILE_ATTRIBUTES;
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)==0;
#else
    return FileExists(path);
#endif
}
const char *GetFileExtension(const char *fileName) {
    if (!fileName) return ""; const char *extension="";
    for (const char *p=fileName;*p;p++) { if (*p=='.') extension=p; else if (*p=='/' || *p=='\\') extension=""; }
    return extension;
}
static int mr_path_lower(int c) { return c>='A'&&c<='Z'?c+('a'-'A'):c; }
bool IsFileExtension(const char *fileName,const char *extensions) {
    if (!fileName || !extensions) return false; const char *actual=GetFileExtension(fileName);
    for (const char *start=extensions;*start;) {
        while (*start==';' || *start==' ' || *start==',') start++;
        const char *end=start; while (*end && *end!=';' && *end!=',' && *end!=' ') end++;
        const char *a=actual,*b=start; while (b<end && *a && mr_path_lower(*a)==mr_path_lower(*b)) { a++; b++; }
        if (b==end && !*a) return true; start=end;
    }
    return false;
}
const char *GetFileName(const char *path) {
    if (!path) return ""; const char *name=path;
    for (const char *p=path;*p;p++) if (*p=='/' || *p=='\\') name=p+1;
    return name;
}
const char *GetFileNameWithoutExt(const char *path) {
    static char result[1024]; const char *name=GetFileName(path),*extension=GetFileExtension(name);
    unsigned int length=*extension?(unsigned int)(extension-name):TextLength(name); if (length>=sizeof result) length=sizeof result-1;
    memcpy(result,name,length); result[length]='\0'; return result;
}
const char *GetDirectoryPath(const char *path) {
    static char result[1024]; if (!path || !*path) return "."; const char *last=NULL;
    for (const char *p=path;*p;p++) if (*p=='/' || *p=='\\') last=p;
    if (!last) return "."; unsigned int length=(unsigned int)(last-path); if (length==0) length=1;
    else if (length==2 && path[1]==':') length=3;
    if (length>=sizeof result) length=sizeof result-1; memcpy(result,path,length); result[length]='\0'; return result;
}
const char *GetPrevDirectoryPath(const char *path) {
    static char result[1024]; if (!path || !*path) return "."; unsigned int length=TextLength(path);
    while (length>1 && (path[length-1]=='/' || path[length-1]=='\\') && !(length==3 && path[1]==':')) length--;
    while (length>0 && path[length-1]!='/' && path[length-1]!='\\') length--;
    while (length>1 && (path[length-1]=='/' || path[length-1]=='\\') && !(length==3 && path[1]==':')) length--;
    if (!length) return "."; if (length>=sizeof result) length=sizeof result-1;
    memcpy(result,path,length); result[length]='\0'; return result;
}
const char *GetWorkingDirectory(void) {
    static char result[1024];
#ifdef _WIN32
    DWORD length=GetCurrentDirectoryA((DWORD)sizeof result,result); if (length>0 && length<sizeof result) return result;
#endif
    result[0]='.'; result[1]='\0'; return result;
}
const char *GetApplicationDirectory(void) {
    static char result[1024];
#ifdef _WIN32
    DWORD length=GetModuleFileNameA(NULL,result,(DWORD)sizeof result); if (length>0 && length<sizeof result) {
        while (length>0 && result[length-1]!='/' && result[length-1]!='\\') length--;
        if (length>0) result[length]='\0'; return result;
    }
#endif
    result[0]='.'; result[1]='\0'; return result;
}
static unsigned int mr_random_state=0x12345678u;
void SetRandomSeed(unsigned int seed) { mr_random_state=seed ? seed : 0x12345678u; }
static unsigned int mr_random(void) {
    unsigned int x=mr_random_state; x^=x<<13; x^=x>>17; x^=x<<5; return mr_random_state=x;
}
int GetRandomValue(int min,int max) {
    if (min>max) { int swap=min; min=max; max=swap; }
    unsigned int range=(unsigned int)((long long)max-(long long)min)+1u;
    return range ? min+(int)(mr_random()%range) : (int)mr_random();
}
int *LoadRandomSequence(unsigned int count,int min,int max) {
    if (min>max) { int swap=min; min=max; max=swap; }
    unsigned int available=(unsigned int)((long long)max-(long long)min)+1u;
    if (!count || count>available || !available) return NULL;
    int *pool=MemAlloc(available*sizeof *pool); if (!pool) return NULL;
    for (unsigned int i=0;i<available;i++) pool[i]=min+(int)i;
    for (unsigned int i=0;i<count;i++) {
        unsigned int pick=i+mr_random()%(available-i);
        int swap=pool[i]; pool[i]=pool[pick]; pool[pick]=swap;
    }
    if (count<available) {
        int *result=MemAlloc(count*sizeof *result);
        if (result) memcpy(result,pool,count*sizeof *result);
        MemFree(pool); return result;
    }
    return pool;
}
void UnloadRandomSequence(int *sequence) { MemFree(sequence); }
bool IsKeyDown(int key) { return key>=0 && key<512 && mr.keys[key]; }
bool IsKeyPressed(int key) { return key>=0 && key<512 && mr.pressed[key]; }
bool IsKeyPressedRepeat(int key) { return key>=0 && key<512 && mr.repeated[key]; }
bool IsKeyReleased(int key) { return key>=0 && key<512 && mr.released[key]; }
bool IsKeyUp(int key) { return !IsKeyDown(key); }
int GetKeyPressed(void) {
    if (mr.keyQueueCount<=0) return 0;
    int key=mr.keyQueue[0];
    for (int i=1;i<mr.keyQueueCount;i++) mr.keyQueue[i-1]=mr.keyQueue[i];
    mr.keyQueueCount--; return key;
}
int GetCharPressed(void) {
    if (mr.charQueueCount<=0) return 0;
    int codepoint=mr.charQueue[0];
    for (int i=1;i<mr.charQueueCount;i++) mr.charQueue[i-1]=mr.charQueue[i];
    mr.charQueueCount--; return codepoint;
}
bool IsMouseButtonDown(int b) { return b>=0 && b<3 && mr.buttons[b]; }
bool IsMouseButtonPressed(int b) { return b>=0 && b<3 && mr.clicked[b]; }
bool IsMouseButtonReleased(int b) { return b>=0 && b<3 && mr.buttonReleased[b]; }
bool IsMouseButtonUp(int b) { return !IsMouseButtonDown(b); }
int GetMouseX(void) { return (int)mr.mouse.x; }
int GetMouseY(void) { return (int)mr.mouse.y; }
Vector2 GetMousePosition(void) { return mr.mouse; }
Vector2 GetMouseDelta(void) { return mr.mouseDelta; }
Vector2 GetMouseWheelMoveV(void) { return mr.wheel; }
float GetMouseWheelMove(void) {
    float x=mr.wheel.x<0 ? -mr.wheel.x : mr.wheel.x;
    float y=mr.wheel.y<0 ? -mr.wheel.y : mr.wheel.y;
    return x>y ? mr.wheel.x : mr.wheel.y;
}
void SetMousePosition(int x,int y) {
#ifdef _WIN32
    if (mr.window) {
        float sx=mr.mouseScale.x!=0?mr.mouseScale.x:1,sy=mr.mouseScale.y!=0?mr.mouseScale.y:1;
        POINT point={(LONG)((x-mr.mouseOffset.x)/sx),(LONG)((y-mr.mouseOffset.y)/sy)};
        ClientToScreen(mr.window,&point); SetCursorPos(point.x,point.y);
    }
#endif
    mr.mouseDelta.x+=(float)x-mr.mouse.x; mr.mouseDelta.y+=(float)y-mr.mouse.y; mr.mouse=(Vector2){(float)x,(float)y};
}
void SetMouseOffset(int x,int y) { mr.mouseOffset=(Vector2){(float)x,(float)y}; }
void SetMouseScale(float x,float y) { mr.mouseScale=(Vector2){x,y}; }
void BeginDrawing(void) { mr_dispatch_readbacks();mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=0; mr.overflow=false; mr.renderTarget=0; mr.targetWidth=mr.width; mr.targetHeight=mr.height; mr.drawing=mr.ready; }
void ClearBackground(Color color) { mr.clear=color; mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=0; }
void BeginBlendMode(int mode) { mr.blendMode=(mode>=0 && mode<6)?mode:BLEND_ALPHA; }
void EndBlendMode(void) { mr.blendMode=BLEND_ALPHA; }
void BeginScissorMode(int x,int y,int width,int height) {
    if(x<0){width+=x;x=0;}if(y<0){height+=y;y=0;}
    mr.scissorActive=width>0 && height>0; mr.scissor=(Rectangle){(float)x,(float)y,(float)width,(float)height};
}
void EndScissorMode(void) { mr.scissorActive=false; }
Vector2 GetWorldToScreen2D(Vector2 position,Camera2D camera) {
    float angle=camera.rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    float x=position.x-camera.target.x,y=position.y-camera.target.y;
    return (Vector2){camera.offset.x+(x*c-y*s)*camera.zoom,camera.offset.y+(x*s+y*c)*camera.zoom};
}
Vector2 GetScreenToWorld2D(Vector2 position,Camera2D camera) {
    if (camera.zoom==0) return camera.target;
    float angle=-camera.rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    float x=(position.x-camera.offset.x)/camera.zoom,y=(position.y-camera.offset.y)/camera.zoom;
    return (Vector2){camera.target.x+x*c-y*s,camera.target.y+x*s+y*c};
}
Matrix GetCameraMatrix2D(Camera2D camera) {
    float angle=camera.rotation*MR_DEG2RAD,c=cosf(angle)*camera.zoom,s=sinf(angle)*camera.zoom;
    Matrix result={0}; result.m0=c; result.m4=-s; result.m1=s; result.m5=c; result.m10=1; result.m15=1;
    result.m12=c*(-camera.target.x)-s*(-camera.target.y)+camera.offset.x;
    result.m13=s*(-camera.target.x)+c*(-camera.target.y)+camera.offset.y; return result;
}
void BeginMode2D(Camera2D camera) { mr.camera2d=camera; mr.camera2dActive=true; }
void EndMode2D(void) { mr.camera2dActive=false; }
