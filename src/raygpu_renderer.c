/* RayGPU renderer module. Compiled through raygpu.c; do not compile separately. */
void BeginTextureMode(RenderTexture2D target) {
    if (!IsRenderTextureValid(target)) return;
    mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=0; mr.overflow=false; mr.renderTarget=target.id;
    mr.targetWidth=target.texture.width; mr.targetHeight=target.texture.height; mr.drawing=true;
}
#ifdef _WIN32
static void mr_render_3d_commands(WGPURenderPassEncoder pass,MRTexture *target,int width,int height){
    if(!mr.drawCount3d||!mr.instanceCount3d||!mr.pipeline3d||!mr.instanceBuffer3d)return;wgpuQueueWriteBuffer(mr.queue,mr.instanceBuffer3d,0,mr.instances3d,(size_t)mr.instanceCount3d*sizeof(MRInstance3D));wgpuRenderPassEncoderSetPipeline(pass,mr.pipeline3d);wgpuRenderPassEncoderSetScissorRect(pass,0,0,(uint32_t)width,(uint32_t)height);
    for(unsigned int i=0;i<mr.drawCount3d;i++){MRDraw3D command=mr.draws3d[i];MRMeshEntry *mesh=mr_mesh_entry(command.mesh);MRTexture *texture=mr_texture(command.texture);if(!mesh||!mesh->vertexBuffer||!texture||texture==target||!command.instanceCount)continue;wgpuRenderPassEncoderSetVertexBuffer(pass,0,mesh->vertexBuffer,0,(uint64_t)mesh->vertexCount*sizeof(MRGpuVertex));wgpuRenderPassEncoderSetVertexBuffer(pass,1,mr.instanceBuffer3d,(uint64_t)command.firstInstance*sizeof(MRInstance3D),(uint64_t)command.instanceCount*sizeof(MRInstance3D));wgpuRenderPassEncoderSetBindGroup(pass,0,texture->group,0,NULL);if(mesh->indexed&&mesh->indexBuffer){wgpuRenderPassEncoderSetIndexBuffer(pass,mesh->indexBuffer,WGPUIndexFormat_Uint16,0,(uint64_t)mesh->indexCount*sizeof(unsigned short));wgpuRenderPassEncoderDrawIndexed(pass,(uint32_t)mesh->indexCount,command.instanceCount,0,0,0);}else wgpuRenderPassEncoderDraw(pass,(uint32_t)mesh->vertexCount,command.instanceCount,0,0);}
}
static void mr_render_texture_pass(MRTexture *target,int width,int height) {
    WGPUCommandEncoder encoder=wgpuDeviceCreateCommandEncoder(mr.device,NULL);
    WGPURenderPassColorAttachment attachment={0}; attachment.depthSlice=(uint32_t)-1; attachment.view=target->view;
    attachment.loadOp=WGPULoadOp_Clear; attachment.storeOp=WGPUStoreOp_Store;
    attachment.clearValue=(WGPUColor){mr.clear.r/255.0,mr.clear.g/255.0,mr.clear.b/255.0,mr.clear.a/255.0};
    WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;depth.view=target->depthView;depth.depthLoadOp=WGPULoadOp_Clear;depth.depthStoreOp=WGPUStoreOp_Store;depth.depthClearValue=1.0f;depth.stencilLoadOp=WGPULoadOp_Undefined;depth.stencilStoreOp=WGPUStoreOp_Undefined;
    WGPURenderPassDescriptor desc=WGPU_RENDER_PASS_DESCRIPTOR_INIT; desc.colorAttachmentCount=1; desc.colorAttachments=&attachment;desc.depthStencilAttachment=&depth;
    WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(encoder,&desc);
    mr_render_3d_commands(pass,target,width,height);
    if (mr.vertexCount) {
        wgpuQueueWriteBuffer(mr.queue,mr.buffer,0,mr.vertices,mr.vertexCount*sizeof(MRVertex));
        wgpuRenderPassEncoderSetVertexBuffer(pass,0,mr.buffer,0,mr.vertexCount*sizeof(MRVertex));
        for (unsigned int i=0;i<mr.batchCount;i++) {
            MRBatch batch=mr.batches[i]; MRTexture *texture=mr_texture(batch.texture); if (!texture || texture==target) continue;
            unsigned int x=batch.x<(unsigned int)width?batch.x:(unsigned int)width,y=batch.y<(unsigned int)height?batch.y:(unsigned int)height;
            unsigned int w=batch.width; if(w>(unsigned int)width-x)w=(unsigned int)width-x;
            unsigned int h=batch.height;if(h>(unsigned int)height-y)h=(unsigned int)height-y;if(!w||!h)continue;
            MRShaderEntry *shader=mr_shader(batch.shader);
            wgpuRenderPassEncoderSetPipeline(pass,shader?shader->pipelines[batch.blend<6?batch.blend:0]:mr.pipelines[batch.blend<6?batch.blend:0]);
            if(shader){wgpuRenderPassEncoderSetBindGroup(pass,1,shader->uniformGroup,0,NULL);wgpuRenderPassEncoderSetBindGroup(pass,2,shader->textureGroup,0,NULL);}
            wgpuRenderPassEncoderSetScissorRect(pass,x,y,w,h);wgpuRenderPassEncoderSetBindGroup(pass,0,texture->group,0,NULL);
            wgpuRenderPassEncoderDraw(pass,batch.count,1,batch.first,0);
        }
    }
    wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer commands=wgpuCommandEncoderFinish(encoder,NULL);wgpuQueueSubmit(mr.queue,1,&commands);
    wgpuCommandBufferRelease(commands);wgpuCommandEncoderRelease(encoder);
}
void EndTextureMode(void) {
    if (!mr.drawing || !mr.renderTarget) return; MRTexture *target=mr_texture(mr.renderTarget);
    if (target) mr_render_texture_pass(target,mr.targetWidth,mr.targetHeight);
    mr.drawing=false;mr.renderTarget=0;mr.targetWidth=mr.width;mr.targetHeight=mr.height;mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=0;
}
void EndDrawing(void) {
    if (!mr.drawing) return;
    mr.drawing=false;
    if (mr.width>0 && mr.height>0 && !mr.close) {
        if (mr.config.width!=(uint32_t)mr.width || mr.config.height!=(uint32_t)mr.height) {
            mr.config.width=(uint32_t)mr.width; mr.config.height=(uint32_t)mr.height;
            wgpuSurfaceConfigure(mr.surface,&mr.config);
        }
        if(!mr_resize_depth(mr.width,mr.height)){mr_error("Could not create depth buffer");return;}
        WGPUSurfaceTexture surface=WGPU_SURFACE_TEXTURE_INIT;
        wgpuSurfaceGetCurrentTexture(mr.surface,&surface);
        if (surface.status==WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal || surface.status==WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
            WGPUTextureView view=wgpuTextureCreateView(surface.texture,NULL);
            WGPUCommandEncoder encoder=wgpuDeviceCreateCommandEncoder(mr.device,NULL);
            WGPURenderPassColorAttachment attachment={0};
            attachment.depthSlice=(uint32_t)-1;
            attachment.view=view; attachment.loadOp=WGPULoadOp_Clear; attachment.storeOp=WGPUStoreOp_Store;
            attachment.clearValue=(WGPUColor){mr.clear.r/255.0,mr.clear.g/255.0,mr.clear.b/255.0,mr.clear.a/255.0};
            WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;depth.view=mr.depthView;depth.depthLoadOp=WGPULoadOp_Clear;depth.depthStoreOp=WGPUStoreOp_Store;depth.depthClearValue=1.0f;depth.stencilLoadOp=WGPULoadOp_Undefined;depth.stencilStoreOp=WGPUStoreOp_Undefined;
            WGPURenderPassDescriptor desc=WGPU_RENDER_PASS_DESCRIPTOR_INIT; desc.colorAttachmentCount=1; desc.colorAttachments=&attachment;desc.depthStencilAttachment=&depth;
            WGPURenderPassEncoder pass=wgpuCommandEncoderBeginRenderPass(encoder,&desc);
            mr_render_3d_commands(pass,NULL,mr.width,mr.height);
            if (mr.vertexCount) {
                wgpuQueueWriteBuffer(mr.queue,mr.buffer,0,mr.vertices,mr.vertexCount*sizeof(MRVertex));
                wgpuRenderPassEncoderSetVertexBuffer(pass,0,mr.buffer,0,mr.vertexCount*sizeof(MRVertex));
                for (unsigned int i=0;i<mr.batchCount;i++) {
                    MRBatch batch=mr.batches[i]; MRTexture *texture=mr_texture(batch.texture);
                    if (!texture) continue;
                    unsigned int x=batch.x<((unsigned int)mr.width)?batch.x:(unsigned int)mr.width;
                    unsigned int y=batch.y<((unsigned int)mr.height)?batch.y:(unsigned int)mr.height;
                    unsigned int width=batch.width; if (width>(unsigned int)mr.width-x) width=(unsigned int)mr.width-x;
                    unsigned int height=batch.height; if (height>(unsigned int)mr.height-y) height=(unsigned int)mr.height-y;
                    if (!width || !height) continue;
                    MRShaderEntry *shader=mr_shader(batch.shader);
                    wgpuRenderPassEncoderSetPipeline(pass,shader?shader->pipelines[batch.blend<6?batch.blend:0]:mr.pipelines[batch.blend<6?batch.blend:0]);
                    if(shader){wgpuRenderPassEncoderSetBindGroup(pass,1,shader->uniformGroup,0,NULL);wgpuRenderPassEncoderSetBindGroup(pass,2,shader->textureGroup,0,NULL);}
                    wgpuRenderPassEncoderSetScissorRect(pass,x,y,width,height);
                    wgpuRenderPassEncoderSetBindGroup(pass,0,texture->group,0,NULL);
                    wgpuRenderPassEncoderDraw(pass,batch.count,1,batch.first,0);
                }
            }
            wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass);
            WGPUCommandBuffer commands=wgpuCommandEncoderFinish(encoder,NULL);
            wgpuQueueSubmit(mr.queue,1,&commands);
            wgpuSurfacePresent(mr.surface);
            wgpuCommandBufferRelease(commands); wgpuCommandEncoderRelease(encoder); wgpuTextureViewRelease(view);
            if (surface.status==WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) mr.config.width=0;
        } else if (surface.status==WGPUSurfaceGetCurrentTextureStatus_Outdated || surface.status==WGPUSurfaceGetCurrentTextureStatus_Lost) {
            mr.config.width=0;
        } else if (surface.status!=WGPUSurfaceGetCurrentTextureStatus_Timeout) mr_error("Could not acquire surface texture");
        if (surface.texture) wgpuTextureRelease(surface.texture);
    }
    mr_finish_input_frame();
    /* The browser uses requestAnimationFrame; native uses this precise cap. */
    mr_limit_frame();
}
void CloseWindow(void) {
    if (mr_audio_ready) CloseAudioDevice();
    for(int gamepad=0;gamepad<MR_MAX_GAMEPADS;gamepad++)SetGamepadVibration(gamepad,0,0,0);
    mr_discard_readbacks();
    UnloadDroppedFiles(mr.droppedFiles);mr.droppedFiles=(FilePathList){0};MemFree(mr.clipboardText);mr.clipboardText=NULL;
    mr.ready=false; mr.drawing=false; mr.close=true;
    if (mr.surface && mr.config.height) wgpuSurfaceUnconfigure(mr.surface);
    mr.white=0;
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (mr.textures[i].id) UnloadTexture((Texture2D){mr.textures[i].id,0,0,0,0});
    for (int i=0;i<32;i++) if (mr.shaders[i].id) UnloadShader((Shader){mr.shaders[i].id,NULL});
    for(int i=0;i<MR_MAX_MESHES;i++)if(mr.meshes[i].id){if(mr.meshes[i].vertexBuffer)wgpuBufferRelease(mr.meshes[i].vertexBuffer);if(mr.meshes[i].indexBuffer)wgpuBufferRelease(mr.meshes[i].indexBuffer);memset(&mr.meshes[i],0,sizeof mr.meshes[i]);}
    if (mr.buffer) wgpuBufferRelease(mr.buffer);
    if (mr.instanceBuffer3d) wgpuBufferRelease(mr.instanceBuffer3d);
    if (mr.depthView) wgpuTextureViewRelease(mr.depthView);
    if (mr.depthTexture) wgpuTextureRelease(mr.depthTexture);
    for (int i=0;i<6;i++) if (mr.pipelines[i]) wgpuRenderPipelineRelease(mr.pipelines[i]);
    if(mr.pipeline3d)wgpuRenderPipelineRelease(mr.pipeline3d);
    if (mr.sampler) wgpuSamplerRelease(mr.sampler);
    if (mr.textureLayout) wgpuBindGroupLayoutRelease(mr.textureLayout);
    if (mr.uniformLayout) wgpuBindGroupLayoutRelease(mr.uniformLayout);
    if (mr.shaderTextureLayout) wgpuBindGroupLayoutRelease(mr.shaderTextureLayout);
    if (mr.queue) wgpuQueueRelease(mr.queue);
    if (mr.device) { wgpuDeviceDestroy(mr.device); wgpuDeviceRelease(mr.device); }
    if (mr.surface) wgpuSurfaceRelease(mr.surface);
    if (mr.adapter) wgpuAdapterRelease(mr.adapter);
    if (mr.instance) wgpuInstanceRelease(mr.instance);
#ifdef _WIN32
    ClipCursor(NULL);
    if (mr.window) DestroyWindow(mr.window);
    if(mr.bigIcon)DestroyIcon(mr.bigIcon);if(mr.smallIcon)DestroyIcon(mr.smallIcon);
    mr.bigIcon=mr.smallIcon=NULL;
    mr.window=NULL;
#endif
    mr.buffer=NULL;mr.instanceBuffer3d=NULL;mr.pipeline3d=NULL;mr.depthView=NULL;mr.depthTexture=NULL;memset(mr.pipelines,0,sizeof mr.pipelines); mr.sampler=NULL; mr.textureLayout=NULL;mr.uniformLayout=NULL;mr.shaderTextureLayout=NULL; mr.queue=NULL;
    mr.device=NULL; mr.surface=NULL; mr.adapter=NULL; mr.instance=NULL;
}
#else
void EndTextureMode(void) {
    if (!mr.drawing || !mr.renderTarget) return;
    uint32_t color=(uint32_t)mr.clear.r|((uint32_t)mr.clear.g<<8)|((uint32_t)mr.clear.b<<16)|((uint32_t)mr.clear.a<<24);
    mr_web_present(mr.vertices,(int)mr.vertexCount,mr.batches,(int)mr.batchCount,mr.draws3d,(int)mr.drawCount3d,mr.instances3d,(int)mr.instanceCount3d,(int)color,mr.renderTarget);
    mr.drawing=false;mr.renderTarget=0;mr.targetWidth=mr.width;mr.targetHeight=mr.height;mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=0;
}
void EndDrawing(void) {
    if (!mr.drawing) return;
    mr.drawing=false;
    if (!mr.close) {
        uint32_t color=(uint32_t)mr.clear.r | ((uint32_t)mr.clear.g<<8) | ((uint32_t)mr.clear.b<<16) | ((uint32_t)mr.clear.a<<24);
        mr_web_present(mr.vertices,(int)mr.vertexCount,mr.batches,(int)mr.batchCount,mr.draws3d,(int)mr.drawCount3d,mr.instances3d,(int)mr.instanceCount3d,(int)color,0);
    }
    mr_finish_input_frame();
}
void CloseWindow(void) {
    if (!mr.ready) return;
    if (mr_audio_ready) CloseAudioDevice();
    for(int gamepad=0;gamepad<MR_MAX_GAMEPADS;gamepad++)SetGamepadVibration(gamepad,0,0,0);
    mr_discard_readbacks();
    UnloadDroppedFiles(mr.droppedFiles);mr.droppedFiles=(FilePathList){0};MemFree(mr.clipboardText);mr.clipboardText=NULL;mr_web_drop_clear();
    mr.ready=false; mr.close=true; mr.drawing=false; mr.white=0;
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (mr.textures[i].id) UnloadTexture((Texture2D){mr.textures[i].id,0,0,0,0});
    for (int i=0;i<32;i++) if (mr.shaders[i].id) UnloadShader((Shader){mr.shaders[i].id,NULL});
    for(int i=0;i<MR_MAX_MESHES;i++)if(mr.meshes[i].id){mr_web_mesh_unload(mr.meshes[i].id);memset(&mr.meshes[i],0,sizeof mr.meshes[i]);}
    mr_web_close();
}
MR_EXPORT("raygpu_key") void raygpu_key(int key,int down) { mr_key(key,down!=0); }
MR_EXPORT("raygpu_char") void raygpu_char(int codepoint) {
    if (codepoint>=32 && codepoint<=0x10ffff && mr.charQueueCount<16) mr.charQueue[mr.charQueueCount++]=codepoint;
}
MR_EXPORT("raygpu_focus") void raygpu_focus(int focused) {
    mr.focused=focused!=0; if (!mr.focused) mr_clear_input();
}
MR_EXPORT("raygpu_mouse") void raygpu_mouse(float x,float y,int button,int down) {
    mr_mouse(x,y); if (button>=0) mr_button(button,down!=0);
}
MR_EXPORT("raygpu_touch") void raygpu_touch(int id,int action,float x,float y) { mr_touch_event(id,action,x,y); }
MR_EXPORT("raygpu_wheel") void raygpu_wheel(float x,float y) { mr.wheel.x+=x; mr.wheel.y+=y; }
MR_EXPORT("raygpu_blur") void raygpu_blur(void) { mr_clear_input(); }
MR_EXPORT("raygpu_frame") int raygpu_frame(void) {
    if (WindowShouldClose() || !mr.updateDraw) { CloseWindow(); return 0; }
    mr.updateDraw();
    if (mr.close) { CloseWindow(); return 0; }
    return 1;
}
#endif
float Sin(float radians) { return sinf(radians); }
void RunMainLoop(void (*updateDraw)(void)) {
    if (!updateDraw || !mr.ready) return;
    mr.updateDraw=updateDraw;
#ifdef _WIN32
    while (!WindowShouldClose()) updateDraw();
    CloseWindow();
#endif
}
