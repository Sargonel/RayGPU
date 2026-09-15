/* Private platform definitions and shared state. */
#ifndef RAYGPU_INTERNAL_H
#define RAYGPU_INTERNAL_H
#include "raygpu.h"
#include <stddef.h>
#if defined(__wasm__)
#define MR_IMPORT(name) __attribute__((import_module("raygpu"), import_name(name)))
#define MR_EXPORT(name) __attribute__((export_name(name)))
MR_IMPORT("log") void mr_log(const char *text);
MR_IMPORT("now") double mr_web_now(void);
MR_IMPORT("init") void mr_web_init(int width,int height,const char *title);
MR_IMPORT("window_command") void mr_web_window_command(int command,int a,int b,const char *text);
MR_IMPORT("present") void mr_web_present(const void *vertices,int vertexCount,const void *batches,int batchCount,const void *draws3d,int drawCount,const void *instances3d,int instanceCount,int color,unsigned int target);
MR_IMPORT("mesh_upload") void mr_web_mesh_upload(unsigned int id,const void *vertices,int vertexCount,const void *indices,int indexCount);
MR_IMPORT("mesh_update") void mr_web_mesh_update(unsigned int id,const void *vertices,int vertexCount);
MR_IMPORT("mesh_unload") void mr_web_mesh_unload(unsigned int id);
MR_IMPORT("texture") void mr_web_texture(unsigned int id,const void *pixels,int width,int height);
MR_IMPORT("render_texture") void mr_web_render_texture(unsigned int id,int width,int height);
MR_IMPORT("shader_load") int mr_web_shader_load(unsigned int id,const char *vsCode,const char *fsCode);
MR_IMPORT("shader_unload") void mr_web_shader_unload(unsigned int id);
MR_IMPORT("shader_uniform") void mr_web_shader_uniform(unsigned int id,int location,const void *data,int size);
MR_IMPORT("texture_update") void mr_web_texture_update(unsigned int id,int x,int y,int width,int height,const void *pixels);
MR_IMPORT("texture_params") void mr_web_texture_params(unsigned int id,int filter,int wrap);
MR_IMPORT("unload") void mr_web_unload(unsigned int id);
MR_IMPORT("audio_init") int mr_web_audio_init(void);
MR_IMPORT("audio_close") void mr_web_audio_close(void);
MR_IMPORT("audio_load") void mr_web_audio_load(unsigned int id,const void *data,unsigned int frames,unsigned int rate,unsigned int bits,unsigned int channels);
MR_IMPORT("audio_unload") void mr_web_audio_unload(unsigned int id);
MR_IMPORT("audio_command") void mr_web_audio_command(unsigned int id,int command,float value);
MR_IMPORT("audio_playing") int mr_web_audio_playing(unsigned int id);
MR_IMPORT("close") void mr_web_close(void);
MR_IMPORT("fps") void mr_web_fps(int fps);
MR_IMPORT("file_size") int mr_web_file_size(const char *fileName);
MR_IMPORT("file_read") int mr_web_file_read(const char *fileName,void *data,int size);
MR_IMPORT("file_write") int mr_web_file_write(const char *fileName,const void *data,int size);
MR_IMPORT("sin") float sinf(float x);
MR_IMPORT("cos") float cosf(float x);
static float sqrtf(float x) { return __builtin_sqrtf(x); }
/* Freestanding compiler support: no libc or WASI runtime is linked. */
void *memset(void *destination,int value,size_t size) {
    unsigned char *p=destination; for (size_t i=0;i<size;i++) p[i]=(unsigned char)value; return destination;
}
void *memcpy(void *destination,const void *source,size_t size) {
    unsigned char *d=destination; const unsigned char *p=source;
    for (size_t i=0;i<size;i++) d[i]=p[i]; return destination;
}
int memcmp(const void *left,const void *right,size_t size) {
    const unsigned char *a=left,*b=right;
    for(size_t i=0;i<size;i++) if(a[i]!=b[i]) return a[i]<b[i]?-1:1;
    return 0;
}
static int puts(const char *s) { mr_log(s); return 0; }
#elif defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <webgpu/webgpu.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define CloseWindow Win32CloseWindow
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif
#undef CloseWindow
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
#define MR_CALLBACK_MODE WGPUCallbackMode_AllowProcessEvents
/* VS Code's Clang IntelliSense mode can reject the Windows SDK's ui64
 * literals when Dawn expands its default descriptors. Use equivalent C17
 * constants for the editor only; actual compiler definitions stay untouched. */
#ifdef __INTELLISENSE__
#undef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#undef UINT64_MAX
#define UINT64_MAX UINT64_C(18446744073709551615)
#endif
#else
#error "raygpu currently supports Windows and wasm32."
#endif

#define MR_MAX_VERTICES 262144
#define MR_MAX_TEXTURES 256
#define MR_MAX_MESHES 1024
#define MR_MAX_3D_DRAWS 4096
#define MR_MAX_3D_INSTANCES 8192
#define MR_PI 3.14159265358979323846f
#define MR_DEG2RAD (MR_PI/180.0f)
typedef struct MRVertex { float x,y,u,v,z; unsigned char r,g,b,a; } MRVertex;
typedef struct MRTexture {
#ifdef _WIN32
    WGPUTexture texture; WGPUTextureView view; WGPUBindGroup group; WGPUSampler customSampler;
    WGPUTexture depthTexture; WGPUTextureView depthView;
#endif
    unsigned int id; int filter,wrap;
} MRTexture;
typedef struct MRShaderEntry {
#ifdef _WIN32
    WGPURenderPipeline pipelines[6]; WGPUBuffer uniformBuffer; WGPUBindGroup uniformGroup;
#endif
    unsigned int id,nameHashes[32]; unsigned char nameSlots[32]; int locationCount; bool explicitLocations; unsigned char uniforms[32][64];
} MRShaderEntry;
typedef struct MRBatch { unsigned int first,count,texture,blend,shader,x,y,width,height; } MRBatch;
typedef struct MRGpuVertex { float x,y,z,nx,ny,nz,u,v; unsigned char r,g,b,a; } MRGpuVertex;
typedef struct MRInstance3D { Matrix model,viewProjection; Color tint; float padding[3]; Vector3 camera; float cameraPadding; } MRInstance3D;
typedef struct MRDraw3D { unsigned int mesh,texture,firstInstance,instanceCount; } MRDraw3D;
typedef struct MRMeshEntry {
#ifdef _WIN32
    WGPUBuffer vertexBuffer,indexBuffer;
#endif
    unsigned int id; int vertexCount,indexCount; bool indexed;
} MRMeshEntry;
_Static_assert(sizeof(MRVertex)==24,"Vertex layout must match raygpu.js");
_Static_assert(sizeof(MRBatch)==36,"Batch layout must match raygpu.js");
_Static_assert(sizeof(MRGpuVertex)==36,"3D vertex layout must match raygpu.js");
_Static_assert(sizeof(MRInstance3D)==160,"3D instance layout must match raygpu.js");
_Static_assert(sizeof(MRDraw3D)==16,"3D command layout must match raygpu.js");
static struct {
    void (*updateDraw)(void);
#ifdef _WIN32
    WGPUInstance instance; WGPUAdapter adapter; WGPUDevice device; WGPUQueue queue;
    WGPUSurface surface; WGPUSurfaceConfiguration config;
    WGPURenderPipeline pipelines[6],pipeline3d; WGPUBuffer buffer,instanceBuffer3d;
    WGPUTexture depthTexture; WGPUTextureView depthView; int depthWidth,depthHeight;
    WGPUBindGroupLayout textureLayout,uniformLayout; WGPUSampler sampler;
#endif
    MRTexture textures[MR_MAX_TEXTURES]; unsigned int nextTexture,white;
    MRMeshEntry meshes[MR_MAX_MESHES]; unsigned int nextMesh;
    MRShaderEntry shaders[32]; unsigned int nextShader,currentShader;
    MRVertex vertices[MR_MAX_VERTICES]; MRBatch batches[MR_MAX_VERTICES/3];
    MRDraw3D draws3d[MR_MAX_3D_DRAWS]; MRInstance3D instances3d[MR_MAX_3D_INSTANCES];
    unsigned int vertexCount,batchCount,drawCount3d,instanceCount3d;
    bool ready,close,error,drawing,adapterDone,deviceDone,overflow,softwareFrameLimit,resized,focused;
    bool keys[512],pressed[512],repeated[512],released[512];
    bool buttons[3],clicked[3],buttonReleased[3];
    int keyQueue[16],keyQueueCount,charQueue[16],charQueueCount,exitKey,minWidth,minHeight,maxWidth,maxHeight;
    Vector2 mouse,mouseDelta,wheel,mouseOffset,mouseScale; Color clear; Texture2D shapesTexture; Rectangle shapesSource;
    Camera2D camera2d; Camera3D camera3d; bool camera2dActive,camera3dActive,scissorActive; Rectangle scissor; int blendMode;
    unsigned int renderTarget; int targetWidth,targetHeight;
    int width,height,fps; double start,previous,frameStart; float dt;
#ifdef _WIN32
    HWND window;
#endif
} mr;
static float mr_clamp01(float value);
static unsigned char mr_byte(float value);
static float mr_min(float a,float b);
static float mr_max(float a,float b);
static Vector2 mr_rotate_point(Vector2 p,float c,float s,Vector2 translation);
static void mr_triangle(Vector2 a,Vector2 b,Vector2 c,Vector2 uvA,Vector2 uvB,Vector2 uvC,Color color,unsigned int texture);
static void mr_quad(float x,float y,float w,float h,Color color,unsigned int texture);
#endif
