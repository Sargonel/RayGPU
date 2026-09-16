/* RayGPU: independent, raylib-inspired WebGPU library.
 * Copyright (c) 2026 Sargonel. Distributed under the zlib license.
 * Include this header and compile src/raygpu.c alongside your application.
 * Windows uses Dawn + Win32; web uses freestanding Clang WASM + raygpu.js.
 * The API is inspired by raylib by Ramon Santamaria (zlib license).
 */
#ifndef RAYGPU_H
#define RAYGPU_H
#include <stdbool.h>
#include <stdint.h>

#define RAYGPU_VERSION_MAJOR 0
#define RAYGPU_VERSION_MINOR 1
#define RAYGPU_VERSION_PATCH 0
#define RAYGPU_VERSION "0.1.0-dev"

/* Foundational public types intentionally match raylib's field layout. */
typedef struct Vector2 { float x,y; } Vector2;
typedef struct Vector3 { float x,y,z; } Vector3;
typedef struct Vector4 { float x,y,z,w; } Vector4;
typedef Vector4 Quaternion;
typedef struct Matrix {
    float m0,m4,m8,m12;
    float m1,m5,m9,m13;
    float m2,m6,m10,m14;
    float m3,m7,m11,m15;
} Matrix;
typedef struct Color { unsigned char r,g,b,a; } Color;
typedef struct Rectangle { float x,y,width,height; } Rectangle;
typedef struct Image { void *data; int width,height,mipmaps,format; } Image;
typedef struct Texture { unsigned int id; int width,height,mipmaps,format; } Texture;
typedef Texture Texture2D;
typedef Texture TextureCubemap;
typedef struct RenderTexture { unsigned int id; Texture texture,depth; } RenderTexture;
typedef RenderTexture RenderTexture2D;
typedef struct NPatchInfo { Rectangle source; int left,top,right,bottom,layout; } NPatchInfo;
typedef struct GlyphInfo { int value,offsetX,offsetY,advanceX; Image image; } GlyphInfo;
typedef struct Font { int baseSize,glyphCount,glyphPadding; Texture2D texture; Rectangle *recs; GlyphInfo *glyphs; } Font;
typedef struct Shader { unsigned int id; int *locs; } Shader;
typedef struct Wave { unsigned int frameCount,sampleRate,sampleSize,channels; void *data; } Wave;
typedef struct rAudioBuffer rAudioBuffer;
typedef struct rAudioProcessor rAudioProcessor;
typedef struct AudioStream {
    rAudioBuffer *buffer; rAudioProcessor *processor;
    unsigned int sampleRate,sampleSize,channels;
} AudioStream;
typedef struct Sound { AudioStream stream; unsigned int frameCount; } Sound;
typedef struct Music {
    AudioStream stream; unsigned int frameCount; bool looping; int ctxType; void *ctxData;
} Music;
typedef void (*AudioCallback)(void *bufferData,unsigned int frames);
typedef void (*ImageLoadCallback)(Image image,void *userData);
typedef struct Camera3D { Vector3 position,target,up; float fovy; int projection; } Camera3D;
typedef Camera3D Camera;
typedef struct Camera2D { Vector2 offset,target; float rotation,zoom; } Camera2D;
typedef struct MeshMorphTarget { float *vertices,*normals,*tangents; } MeshMorphTarget;
typedef struct ModelMorphData ModelMorphData;
typedef struct Mesh {
    int vertexCount,triangleCount;
    float *vertices,*texcoords,*texcoords2,*normals,*tangents;
    unsigned char *colors; unsigned short *indices;
    float *animVertices,*animNormals; unsigned char *boneIds; float *boneWeights;
    Matrix *boneMatrices; int boneCount; unsigned int vaoId,*vboId;
    int morphTargetCount; MeshMorphTarget *morphTargets; float *morphWeights;
    float *morphBaseVertices,*morphBaseNormals,*morphBaseTangents;
    int sourceNode;
} Mesh;
typedef struct MaterialMap { Texture2D texture; Color color; float value; } MaterialMap;
typedef struct Material { Shader shader; MaterialMap *maps; float params[4]; } Material;
typedef struct Transform { Vector3 translation; Quaternion rotation; Vector3 scale; } Transform;
typedef struct BoneInfo { char name[32]; int parent; } BoneInfo;
typedef struct Model {
    Matrix transform; int meshCount,materialCount; Mesh *meshes; Material *materials;
    int *meshMaterial; int boneCount; BoneInfo *bones; Transform *bindPose;
    ModelMorphData *morphData;
} Model;
typedef struct ModelAnimation {
    int boneCount,frameCount; BoneInfo *bones; Transform **framePoses; char name[32];
    int sourceAnimation;
} ModelAnimation;
typedef struct Ray { Vector3 position,direction; } Ray;
typedef struct RayCollision { bool hit; float distance; Vector3 point,normal; } RayCollision;
typedef struct BoundingBox { Vector3 min,max; } BoundingBox;
typedef enum CameraProjection {
    CAMERA_PERSPECTIVE=0, CAMERA_ORTHOGRAPHIC
} CameraProjection;
typedef enum MaterialMapIndex {
    MATERIAL_MAP_ALBEDO=0,MATERIAL_MAP_METALNESS,MATERIAL_MAP_NORMAL,
    MATERIAL_MAP_ROUGHNESS,MATERIAL_MAP_OCCLUSION,MATERIAL_MAP_EMISSION,
    MATERIAL_MAP_HEIGHT,MATERIAL_MAP_CUBEMAP,MATERIAL_MAP_IRRADIANCE,
    MATERIAL_MAP_PREFILTER,MATERIAL_MAP_BRDF
} MaterialMapIndex;
#define MATERIAL_MAP_DIFFUSE MATERIAL_MAP_ALBEDO
#define MATERIAL_MAP_SPECULAR MATERIAL_MAP_METALNESS

/* raylib-compatible color constants. */
#define LIGHTGRAY  ((Color){200,200,200,255})
#define GRAY       ((Color){130,130,130,255})
#define DARKGRAY   ((Color){80,80,80,255})
#define YELLOW     ((Color){253,249,0,255})
#define GOLD       ((Color){255,203,0,255})
#define ORANGE     ((Color){255,161,0,255})
#define PINK       ((Color){255,109,194,255})
#define RED        ((Color){230,41,55,255})
#define MAROON     ((Color){190,33,55,255})
#define GREEN      ((Color){0,228,48,255})
#define LIME       ((Color){0,158,47,255})
#define DARKGREEN  ((Color){0,117,44,255})
#define SKYBLUE    ((Color){102,191,255,255})
#define BLUE       ((Color){0,121,241,255})
#define DARKBLUE   ((Color){0,82,172,255})
#define PURPLE     ((Color){200,122,255,255})
#define VIOLET     ((Color){135,60,190,255})
#define DARKPURPLE ((Color){112,31,126,255})
#define BEIGE      ((Color){211,176,131,255})
#define BROWN      ((Color){127,106,79,255})
#define DARKBROWN  ((Color){76,63,47,255})
#define WHITE      ((Color){255,255,255,255})
#define BLACK      ((Color){0,0,0,255})
#define BLANK      ((Color){0,0,0,0})
#define MAGENTA    ((Color){255,0,255,255})
#define RAYWHITE   ((Color){245,245,245,255})
typedef enum KeyboardKey {
    KEY_NULL=0,
    KEY_BACK=4, KEY_MENU=5, KEY_VOLUME_UP=24, KEY_VOLUME_DOWN=25,
    KEY_SPACE=32, KEY_APOSTROPHE=39, KEY_COMMA=44, KEY_MINUS=45,
    KEY_PERIOD=46, KEY_SLASH=47,
    KEY_ZERO=48, KEY_ONE=49, KEY_TWO=50, KEY_THREE=51, KEY_FOUR=52,
    KEY_FIVE=53, KEY_SIX=54, KEY_SEVEN=55, KEY_EIGHT=56, KEY_NINE=57,
    KEY_SEMICOLON=59, KEY_EQUAL=61,
    KEY_A=65, KEY_B=66, KEY_C=67, KEY_D=68, KEY_E=69, KEY_F=70,
    KEY_G=71, KEY_H=72, KEY_I=73, KEY_J=74, KEY_K=75, KEY_L=76,
    KEY_M=77, KEY_N=78, KEY_O=79, KEY_P=80, KEY_Q=81, KEY_R=82,
    KEY_S=83, KEY_T=84, KEY_U=85, KEY_V=86, KEY_W=87, KEY_X=88,
    KEY_Y=89, KEY_Z=90,
    KEY_LEFT_BRACKET=91, KEY_BACKSLASH=92, KEY_RIGHT_BRACKET=93, KEY_GRAVE=96,
    KEY_ESCAPE=256, KEY_ENTER=257, KEY_TAB=258, KEY_BACKSPACE=259,
    KEY_INSERT=260, KEY_DELETE=261, KEY_RIGHT=262, KEY_LEFT=263,
    KEY_DOWN=264, KEY_UP=265, KEY_PAGE_UP=266, KEY_PAGE_DOWN=267,
    KEY_HOME=268, KEY_END=269, KEY_CAPS_LOCK=280, KEY_SCROLL_LOCK=281,
    KEY_NUM_LOCK=282, KEY_PRINT_SCREEN=283, KEY_PAUSE=284,
    KEY_F1=290, KEY_F2=291, KEY_F3=292, KEY_F4=293, KEY_F5=294,
    KEY_F6=295, KEY_F7=296, KEY_F8=297, KEY_F9=298, KEY_F10=299,
    KEY_F11=300, KEY_F12=301,
    KEY_KP_0=320, KEY_KP_1=321, KEY_KP_2=322, KEY_KP_3=323,
    KEY_KP_4=324, KEY_KP_5=325, KEY_KP_6=326, KEY_KP_7=327,
    KEY_KP_8=328, KEY_KP_9=329, KEY_KP_DECIMAL=330, KEY_KP_DIVIDE=331,
    KEY_KP_MULTIPLY=332, KEY_KP_SUBTRACT=333, KEY_KP_ADD=334,
    KEY_KP_ENTER=335, KEY_KP_EQUAL=336,
    KEY_LEFT_SHIFT=340, KEY_LEFT_CONTROL=341, KEY_LEFT_ALT=342,
    KEY_LEFT_SUPER=343, KEY_RIGHT_SHIFT=344, KEY_RIGHT_CONTROL=345,
    KEY_RIGHT_ALT=346, KEY_RIGHT_SUPER=347, KEY_KB_MENU=348
} KeyboardKey;
typedef enum TextureFilter {
    TEXTURE_FILTER_POINT=0, TEXTURE_FILTER_BILINEAR, TEXTURE_FILTER_TRILINEAR,
    TEXTURE_FILTER_ANISOTROPIC_4X, TEXTURE_FILTER_ANISOTROPIC_8X, TEXTURE_FILTER_ANISOTROPIC_16X
} TextureFilter;
typedef enum PixelFormat {
    PIXELFORMAT_UNCOMPRESSED_GRAYSCALE=1,PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,
    PIXELFORMAT_UNCOMPRESSED_R5G6B5,PIXELFORMAT_UNCOMPRESSED_R8G8B8,
    PIXELFORMAT_UNCOMPRESSED_R5G5B5A1,PIXELFORMAT_UNCOMPRESSED_R4G4B4A4,
    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
} PixelFormat;
typedef enum TextureWrap {
    TEXTURE_WRAP_REPEAT=0, TEXTURE_WRAP_CLAMP, TEXTURE_WRAP_MIRROR_REPEAT, TEXTURE_WRAP_MIRROR_CLAMP
} TextureWrap;
typedef enum NPatchLayout {
    NPATCH_NINE_PATCH=0, NPATCH_THREE_PATCH_VERTICAL, NPATCH_THREE_PATCH_HORIZONTAL
} NPatchLayout;
typedef enum BlendMode {
    BLEND_ALPHA=0, BLEND_ADDITIVE, BLEND_MULTIPLIED, BLEND_ADD_COLORS,
    BLEND_SUBTRACT_COLORS, BLEND_ALPHA_PREMULTIPLY, BLEND_CUSTOM, BLEND_CUSTOM_SEPARATE
} BlendMode;
typedef enum ShaderUniformDataType {
    SHADER_UNIFORM_FLOAT=0,SHADER_UNIFORM_VEC2,SHADER_UNIFORM_VEC3,SHADER_UNIFORM_VEC4,
    SHADER_UNIFORM_INT,SHADER_UNIFORM_IVEC2,SHADER_UNIFORM_IVEC3,SHADER_UNIFORM_IVEC4,SHADER_UNIFORM_SAMPLER2D
} ShaderUniformDataType;
enum { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE };
void InitWindow(int width,int height,const char *title);
void RunMainLoop(void (*updateDraw)(void));
float Sin(float radians);
bool IsWindowReady(void);
bool WindowShouldClose(void);
void CloseWindow(void);
bool RayGPUHadError(void);
bool IsWindowResized(void);
bool IsWindowFocused(void);
bool IsWindowMinimized(void);
bool IsWindowMaximized(void);
int GetScreenWidth(void);
int GetScreenHeight(void);
int GetRenderWidth(void);
int GetRenderHeight(void);
Vector2 GetWindowPosition(void);
Vector2 GetWindowScaleDPI(void);
void SetWindowTitle(const char *title);
void SetWindowPosition(int x,int y);
void SetWindowSize(int width,int height);
void SetWindowMinSize(int width,int height);
void SetWindowMaxSize(int width,int height);
void MinimizeWindow(void);
void MaximizeWindow(void);
void RestoreWindow(void);
void SetExitKey(int key);
void SetTargetFPS(int fps);
float GetFrameTime(void);
double GetTime(void);
int GetFPS(void);
void WaitTime(double seconds);
void SetRandomSeed(unsigned int seed);
int GetRandomValue(int min,int max);
int *LoadRandomSequence(unsigned int count,int min,int max);
void UnloadRandomSequence(int *sequence);
void *MemAlloc(unsigned int size);
void *MemRealloc(void *ptr,unsigned int size);
void MemFree(void *ptr);
bool IsKeyDown(int key);
bool IsKeyPressed(int key);
bool IsKeyPressedRepeat(int key);
bool IsKeyReleased(int key);
bool IsKeyUp(int key);
int GetKeyPressed(void);
int GetCharPressed(void);
bool IsMouseButtonDown(int button);
bool IsMouseButtonPressed(int button);
bool IsMouseButtonReleased(int button);
bool IsMouseButtonUp(int button);
int GetMouseX(void);
int GetMouseY(void);
Vector2 GetMousePosition(void);
Vector2 GetMouseDelta(void);
float GetMouseWheelMove(void);
Vector2 GetMouseWheelMoveV(void);
void SetMousePosition(int x,int y);
void SetMouseOffset(int offsetX,int offsetY);
void SetMouseScale(float scaleX,float scaleY);
void BeginDrawing(void);
void ClearBackground(Color color);
void EndDrawing(void);
void BeginBlendMode(int mode);
void EndBlendMode(void);
void BeginScissorMode(int x,int y,int width,int height);
void EndScissorMode(void);
Shader LoadShader(const char *vsFileName,const char *fsFileName);
Shader LoadShaderFromMemory(const char *vsCode,const char *fsCode);
bool IsShaderValid(Shader shader);
void UnloadShader(Shader shader);
void BeginShaderMode(Shader shader);
void EndShaderMode(void);
int GetShaderLocation(Shader shader,const char *uniformName);
void SetShaderValue(Shader shader,int locIndex,const void *value,int uniformType);
void SetShaderValueV(Shader shader,int locIndex,const void *value,int uniformType,int count);
void SetShaderValueMatrix(Shader shader,int locIndex,Matrix mat);
void BeginMode3D(Camera3D camera);
void EndMode3D(void);
Ray GetScreenToWorldRay(Vector2 position,Camera camera);
Ray GetScreenToWorldRayEx(Vector2 position,Camera camera,int width,int height);
Vector2 GetWorldToScreen(Vector3 position,Camera camera);
Vector2 GetWorldToScreenEx(Vector3 position,Camera camera,int width,int height);
Matrix GetCameraMatrix(Camera camera);
void BeginMode2D(Camera2D camera);
void EndMode2D(void);
Vector2 GetWorldToScreen2D(Vector2 position,Camera2D camera);
Vector2 GetScreenToWorld2D(Vector2 position,Camera2D camera);
Matrix GetCameraMatrix2D(Camera2D camera);
void SetShapesTexture(Texture2D texture,Rectangle source);
Texture2D GetShapesTexture(void);
Rectangle GetShapesTextureRectangle(void);
void DrawLine3D(Vector3 startPos,Vector3 endPos,Color color);
void DrawPoint3D(Vector3 position,Color color);
void DrawCircle3D(Vector3 center,float radius,Vector3 rotationAxis,float rotationAngle,Color color);
void DrawTriangle3D(Vector3 v1,Vector3 v2,Vector3 v3,Color color);
void DrawTriangleStrip3D(const Vector3 *points,int pointCount,Color color);
void DrawCube(Vector3 position,float width,float height,float length,Color color);
void DrawCubeV(Vector3 position,Vector3 size,Color color);
void DrawCubeTexture(Texture2D texture,Vector3 position,float width,float height,float length,Color color);
void DrawCubeTextureRec(Texture2D texture,Rectangle source,Vector3 position,float width,float height,float length,Color color);
void DrawCubeWires(Vector3 position,float width,float height,float length,Color color);
void DrawCubeWiresV(Vector3 position,Vector3 size,Color color);
void DrawSphere(Vector3 centerPos,float radius,Color color);
void DrawSphereEx(Vector3 centerPos,float radius,int rings,int slices,Color color);
void DrawSphereWires(Vector3 centerPos,float radius,int rings,int slices,Color color);
void DrawCylinder(Vector3 position,float radiusTop,float radiusBottom,float height,int slices,Color color);
void DrawCylinderEx(Vector3 startPos,Vector3 endPos,float startRadius,float endRadius,int sides,Color color);
void DrawCylinderWires(Vector3 position,float radiusTop,float radiusBottom,float height,int slices,Color color);
void DrawCylinderWiresEx(Vector3 startPos,Vector3 endPos,float startRadius,float endRadius,int sides,Color color);
void DrawCapsule(Vector3 startPos,Vector3 endPos,float radius,int slices,int rings,Color color);
void DrawCapsuleWires(Vector3 startPos,Vector3 endPos,float radius,int slices,int rings,Color color);
void DrawPlane(Vector3 centerPos,Vector2 size,Color color);
void DrawRay(Ray ray,Color color);
void DrawGrid(int slices,float spacing);
Model LoadModel(const char *fileName);
/* glTF scene index; -1 uses the default scene (or the first scene). */
Model LoadModelFromScene(const char *fileName,int sceneIndex);
/* Weights may be negative or exceed 1; count must equal morphTargetCount. */
void SetMeshMorphWeights(Mesh mesh,const float *weights,int count);
/* Evaluate glTF weight animation by its original index, in seconds (clamped). */
void UpdateModelMorphAnimation(Model model,int animationIndex,float time);
/* Load an MTL material library. UnloadMaterial() each entry, then MemFree() the array. */
Material *LoadMaterials(const char *fileName,int *materialCount);
ModelAnimation *LoadModelAnimations(const char *fileName,int *animCount);
ModelAnimation *LoadModelAnimationsFromScene(const char *fileName,int sceneIndex,int *animCount);
void UpdateModelAnimation(Model model,ModelAnimation anim,int frame);
void UpdateModelAnimationBones(Model model,ModelAnimation anim,int frame);
void UnloadModelAnimation(ModelAnimation anim);
void UnloadModelAnimations(ModelAnimation *animations,int animCount);
bool IsModelAnimationValid(Model model,ModelAnimation anim);
Model LoadModelFromMesh(Mesh mesh);
bool IsModelValid(Model model);
void UnloadModel(Model model);
BoundingBox GetModelBoundingBox(Model model);
void DrawModel(Model model,Vector3 position,float scale,Color tint);
void DrawModelEx(Model model,Vector3 position,Vector3 rotationAxis,float rotationAngle,Vector3 scale,Color tint);
void DrawModelWires(Model model,Vector3 position,float scale,Color tint);
void DrawModelWiresEx(Model model,Vector3 position,Vector3 rotationAxis,float rotationAngle,Vector3 scale,Color tint);
void DrawModelPoints(Model model,Vector3 position,float scale,Color tint);
void DrawModelPointsEx(Model model,Vector3 position,Vector3 rotationAxis,float rotationAngle,Vector3 scale,Color tint);
void DrawBillboard(Camera camera,Texture2D texture,Vector3 position,float scale,Color tint);
void DrawBillboardRec(Camera camera,Texture2D texture,Rectangle source,Vector3 position,Vector2 size,Color tint);
void DrawBillboardPro(Camera camera,Texture2D texture,Rectangle source,Vector3 position,Vector3 up,Vector2 size,Vector2 origin,float rotation,Color tint);
void DrawBoundingBox(BoundingBox box,Color color);
void UploadMesh(Mesh *mesh,bool dynamic);
void UpdateMeshBuffer(Mesh mesh,int index,const void *data,int dataSize,int offset);
void UnloadMesh(Mesh mesh);
bool ExportMesh(Mesh mesh,const char *fileName);
bool ExportMeshAsCode(Mesh mesh,const char *fileName);
void DrawMesh(Mesh mesh,Material material,Matrix transform);
void DrawMeshInstanced(Mesh mesh,Material material,const Matrix *transforms,int instances);
BoundingBox GetMeshBoundingBox(Mesh mesh);
void GenMeshTangents(Mesh *mesh);
Mesh GenMeshPoly(int sides,float radius);
Mesh GenMeshPlane(float width,float length,int resX,int resZ);
Mesh GenMeshCube(float width,float height,float length);
Mesh GenMeshSphere(float radius,int rings,int slices);
Mesh GenMeshHemiSphere(float radius,int rings,int slices);
Mesh GenMeshCylinder(float radius,float height,int slices);
Mesh GenMeshCone(float radius,float height,int slices);
Mesh GenMeshTorus(float radius,float size,int radSeg,int sides);
Mesh GenMeshKnot(float radius,float size,int radSeg,int sides);
Mesh GenMeshHeightmap(Image heightmap,Vector3 size);
Mesh GenMeshCubicmap(Image cubicmap,Vector3 cubeSize);
Material LoadMaterialDefault(void);
bool IsMaterialValid(Material material);
void UnloadMaterial(Material material);
void SetMaterialTexture(Material *material,int mapType,Texture2D texture);
void SetModelMeshMaterial(Model *model,int meshId,int materialId);
bool CheckCollisionSpheres(Vector3 center1,float radius1,Vector3 center2,float radius2);
bool CheckCollisionBoxes(BoundingBox box1,BoundingBox box2);
bool CheckCollisionBoxSphere(BoundingBox box,Vector3 center,float radius);
RayCollision GetRayCollisionSphere(Ray ray,Vector3 center,float radius);
RayCollision GetRayCollisionBox(Ray ray,BoundingBox box);
RayCollision GetRayCollisionTriangle(Ray ray,Vector3 p1,Vector3 p2,Vector3 p3);
RayCollision GetRayCollisionQuad(Ray ray,Vector3 p1,Vector3 p2,Vector3 p3,Vector3 p4);
RayCollision GetRayCollisionMesh(Ray ray,Mesh mesh,Matrix transform);
void DrawTriangle(Vector2 a,Vector2 b,Vector2 c,Color color);
void DrawRectangle(int x,int y,int width,int height,Color color);
void DrawCircle(int x,int y,float radius,Color color);
void DrawLineEx(Vector2 start,Vector2 end,float thickness,Color color);
void DrawPixel(int posX,int posY,Color color);
void DrawPixelV(Vector2 position,Color color);
void DrawLine(int startPosX,int startPosY,int endPosX,int endPosY,Color color);
void DrawLineV(Vector2 startPos,Vector2 endPos,Color color);
void DrawLineStrip(const Vector2 *points,int pointCount,Color color);
void DrawLineBezier(Vector2 startPos,Vector2 endPos,float thick,Color color);
void DrawCircleSector(Vector2 center,float radius,float startAngle,float endAngle,int segments,Color color);
void DrawCircleSectorLines(Vector2 center,float radius,float startAngle,float endAngle,int segments,Color color);
void DrawCircleGradient(int centerX,int centerY,float radius,Color inner,Color outer);
void DrawCircleV(Vector2 center,float radius,Color color);
void DrawCircleLines(int centerX,int centerY,float radius,Color color);
void DrawCircleLinesV(Vector2 center,float radius,Color color);
void DrawEllipse(int centerX,int centerY,float radiusH,float radiusV,Color color);
void DrawEllipseLines(int centerX,int centerY,float radiusH,float radiusV,Color color);
void DrawRing(Vector2 center,float innerRadius,float outerRadius,float startAngle,float endAngle,int segments,Color color);
void DrawRingLines(Vector2 center,float innerRadius,float outerRadius,float startAngle,float endAngle,int segments,Color color);
void DrawRectangleV(Vector2 position,Vector2 size,Color color);
void DrawRectangleRec(Rectangle rec,Color color);
void DrawRectanglePro(Rectangle rec,Vector2 origin,float rotation,Color color);
void DrawRectangleGradientV(int posX,int posY,int width,int height,Color top,Color bottom);
void DrawRectangleGradientH(int posX,int posY,int width,int height,Color left,Color right);
void DrawRectangleGradientEx(Rectangle rec,Color topLeft,Color bottomLeft,Color topRight,Color bottomRight);
void DrawRectangleLines(int posX,int posY,int width,int height,Color color);
void DrawRectangleLinesEx(Rectangle rec,float lineThick,Color color);
void DrawRectangleRounded(Rectangle rec,float roundness,int segments,Color color);
void DrawRectangleRoundedLines(Rectangle rec,float roundness,int segments,Color color);
void DrawRectangleRoundedLinesEx(Rectangle rec,float roundness,int segments,float lineThick,Color color);
void DrawTriangleLines(Vector2 v1,Vector2 v2,Vector2 v3,Color color);
void DrawTriangleFan(const Vector2 *points,int pointCount,Color color);
void DrawTriangleStrip(const Vector2 *points,int pointCount,Color color);
void DrawPoly(Vector2 center,int sides,float radius,float rotation,Color color);
void DrawPolyLines(Vector2 center,int sides,float radius,float rotation,Color color);
void DrawPolyLinesEx(Vector2 center,int sides,float radius,float rotation,float lineThick,Color color);
void DrawSplineLinear(const Vector2 *points,int pointCount,float thick,Color color);
void DrawSplineBasis(const Vector2 *points,int pointCount,float thick,Color color);
void DrawSplineCatmullRom(const Vector2 *points,int pointCount,float thick,Color color);
void DrawSplineBezierQuadratic(const Vector2 *points,int pointCount,float thick,Color color);
void DrawSplineBezierCubic(const Vector2 *points,int pointCount,float thick,Color color);
void DrawSplineSegmentLinear(Vector2 p1,Vector2 p2,float thick,Color color);
void DrawSplineSegmentBasis(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float thick,Color color);
void DrawSplineSegmentCatmullRom(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float thick,Color color);
void DrawSplineSegmentBezierQuadratic(Vector2 p1,Vector2 c2,Vector2 p3,float thick,Color color);
void DrawSplineSegmentBezierCubic(Vector2 p1,Vector2 c2,Vector2 c3,Vector2 p4,float thick,Color color);
Vector2 GetSplinePointLinear(Vector2 startPos,Vector2 endPos,float t);
Vector2 GetSplinePointBasis(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float t);
Vector2 GetSplinePointCatmullRom(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float t);
Vector2 GetSplinePointBezierQuad(Vector2 p1,Vector2 c2,Vector2 p3,float t);
Vector2 GetSplinePointBezierCubic(Vector2 p1,Vector2 c2,Vector2 c3,Vector2 p4,float t);
bool CheckCollisionRecs(Rectangle rec1,Rectangle rec2);
bool CheckCollisionCircles(Vector2 center1,float radius1,Vector2 center2,float radius2);
bool CheckCollisionCircleRec(Vector2 center,float radius,Rectangle rec);
bool CheckCollisionCircleLine(Vector2 center,float radius,Vector2 p1,Vector2 p2);
bool CheckCollisionPointRec(Vector2 point,Rectangle rec);
bool CheckCollisionPointCircle(Vector2 point,Vector2 center,float radius);
bool CheckCollisionPointTriangle(Vector2 point,Vector2 p1,Vector2 p2,Vector2 p3);
bool CheckCollisionPointLine(Vector2 point,Vector2 p1,Vector2 p2,int threshold);
bool CheckCollisionPointPoly(Vector2 point,const Vector2 *points,int pointCount);
bool CheckCollisionLines(Vector2 startPos1,Vector2 endPos1,Vector2 startPos2,Vector2 endPos2,Vector2 *collisionPoint);
Rectangle GetCollisionRec(Rectangle rec1,Rectangle rec2);
void DrawText(const char *text,int x,int y,int fontSize,Color color);
void DrawFPS(int x,int y);
int MeasureText(const char *text,int fontSize);
Font GetFontDefault(void);
Font LoadFont(const char *fileName);
Font LoadFontEx(const char *fileName,int fontSize,int *codepoints,int codepointCount);
Font LoadFontFromMemory(const char *fileType,const unsigned char *fileData,int dataSize,int fontSize,int *codepoints,int codepointCount);
bool IsFontValid(Font font);
void UnloadFont(Font font);
void DrawTextEx(Font font,const char *text,Vector2 position,float fontSize,float spacing,Color tint);
void DrawTextPro(Font font,const char *text,Vector2 position,Vector2 origin,float rotation,float fontSize,float spacing,Color tint);
void DrawTextCodepoint(Font font,int codepoint,Vector2 position,float fontSize,Color tint);
void DrawTextCodepoints(Font font,const int *codepoints,int codepointCount,Vector2 position,float fontSize,float spacing,Color tint);
Vector2 MeasureTextEx(Font font,const char *text,float fontSize,float spacing);
int GetGlyphIndex(Font font,int codepoint);
GlyphInfo GetGlyphInfo(Font font,int codepoint);
Rectangle GetGlyphAtlasRec(Font font,int codepoint);
void SetTextLineSpacing(int spacing);
bool ColorIsEqual(Color col1,Color col2);
Color Fade(Color color,float alpha);
int ColorToInt(Color color);
Vector4 ColorNormalize(Color color);
Color ColorFromNormalized(Vector4 normalized);
Vector3 ColorToHSV(Color color);
Color ColorFromHSV(float hue,float saturation,float value);
Color ColorTint(Color color,Color tint);
Color ColorBrightness(Color color,float factor);
Color ColorContrast(Color color,float contrast);
Color ColorAlpha(Color color,float alpha);
Color ColorAlphaBlend(Color dst,Color src,Color tint);
Color ColorLerp(Color color1,Color color2,float factor);
Color GetColor(unsigned int hexValue);
char *LoadUTF8(const int *codepoints,int length);
void UnloadUTF8(char *text);
int *LoadCodepoints(const char *text,int *count);
void UnloadCodepoints(int *codepoints);
int GetCodepointCount(const char *text);
int GetCodepoint(const char *text,int *codepointSize);
int GetCodepointNext(const char *text,int *codepointSize);
int GetCodepointPrevious(const char *text,int *codepointSize);
const char *CodepointToUTF8(int codepoint,int *utf8Size);
int TextCopy(char *dst,const char *src);
bool TextIsEqual(const char *text1,const char *text2);
unsigned int TextLength(const char *text);
const char *TextSubtext(const char *text,int position,int length);
void TextAppend(char *text,const char *append,int *position);
int TextFindIndex(const char *text,const char *find);
const char *TextToUpper(const char *text);
const char *TextToLower(const char *text);
int TextToInteger(const char *text);
float TextToFloat(const char *text);
unsigned char *LoadFileData(const char *fileName,int *dataSize);
void UnloadFileData(unsigned char *data);
bool SaveFileData(const char *fileName,void *data,int dataSize);
char *LoadFileText(const char *fileName);
void UnloadFileText(char *text);
bool SaveFileText(const char *fileName,char *text);
char *EncodeDataBase64(const unsigned char *data,int dataSize,int *outputSize);
unsigned char *DecodeDataBase64(const unsigned char *data,int *outputSize);
unsigned int ComputeCRC32(unsigned char *data,int dataSize);
unsigned int *ComputeMD5(unsigned char *data,int dataSize);
unsigned int *ComputeSHA1(unsigned char *data,int dataSize);
bool FileExists(const char *fileName);
bool DirectoryExists(const char *dirPath);
bool IsFileExtension(const char *fileName,const char *ext);
bool IsPathFile(const char *path);
int GetFileLength(const char *fileName);
const char *GetFileExtension(const char *fileName);
const char *GetFileName(const char *filePath);
const char *GetFileNameWithoutExt(const char *filePath);
const char *GetDirectoryPath(const char *filePath);
const char *GetPrevDirectoryPath(const char *dirPath);
const char *GetWorkingDirectory(void);
const char *GetApplicationDirectory(void);
Image LoadImage(const char *fileName);
Image LoadImageFromMemory(const char *fileType,const unsigned char *fileData,int dataSize);
Image LoadImageRaw(const char *fileName,int width,int height,int format,int headerSize);
/* Animated images contain consecutive RGBA8 frames; delays are discarded like raylib.
 * UpdateTexture() with data + frame*width*height*4; UnloadImage() frees all frames. */
Image LoadImageAnim(const char *fileName,int *frames);
Image LoadImageAnimFromMemory(const char *fileType,const unsigned char *fileData,int dataSize,int *frames);
bool IsImageValid(Image image);
void UnloadImage(Image image);
void ImageMipmaps(Image *image);
bool ExportImage(Image image,const char *fileName);
unsigned char *ExportImageToMemory(Image image,const char *fileType,int *fileSize);
bool ExportImageAsCode(Image image,const char *fileName);
Image LoadImageFromTexture(Texture2D texture);
/* Async readback callbacks never run before the request function returns.
 * WindowShouldClose() or BeginDrawing() dispatches completed callbacks.
 * The callback owns image.data and must call UnloadImage(image). */
bool LoadImageFromTextureAsync(Texture2D texture,ImageLoadCallback callback,void *userData);
bool LoadImageFromScreenAsync(ImageLoadCallback callback,void *userData);
void TakeScreenshot(const char *fileName);
Image GenImageColor(int width,int height,Color color);
Image GenImageGradientLinear(int width,int height,int direction,Color start,Color end);
Image GenImageGradientRadial(int width,int height,float density,Color inner,Color outer);
Image GenImageGradientSquare(int width,int height,float density,Color inner,Color outer);
Image GenImagePerlinNoise(int width,int height,int offsetX,int offsetY,float scale);
Image GenImageCellular(int width,int height,int tileSize);
Image GenImageChecked(int width,int height,int checksX,int checksY,Color col1,Color col2);
Image GenImageWhiteNoise(int width,int height,float factor);
Image ImageCopy(Image image);
Image ImageFromImage(Image image,Rectangle rec);
void ImageCrop(Image *image,Rectangle crop);
void ImageAlphaCrop(Image *image,float threshold);
void ImageAlphaClear(Image *image,Color color,float threshold);
void ImageAlphaMask(Image *image,Image alphaMask);
void ImageAlphaPremultiply(Image *image);
void ImageResize(Image *image,int newWidth,int newHeight);
void ImageResizeNN(Image *image,int newWidth,int newHeight);
void ImageResizeCanvas(Image *image,int newWidth,int newHeight,int offsetX,int offsetY,Color fill);
void ImageFlipVertical(Image *image);
void ImageFlipHorizontal(Image *image);
void ImageRotateCW(Image *image);
void ImageRotateCCW(Image *image);
void ImageRotate(Image *image,int degrees);
void ImageBlurGaussian(Image *image,int blurSize);
void ImageKernelConvolution(Image *image,const float *kernel,int kernelSize);
void ImageDither(Image *image,int rBpp,int gBpp,int bBpp,int aBpp);
void ImageToPOT(Image *image,Color fill);
Image ImageFromChannel(Image image,int selectedChannel);
void ImageColorTint(Image *image,Color color);
void ImageColorInvert(Image *image);
void ImageColorGrayscale(Image *image);
void ImageColorContrast(Image *image,float contrast);
void ImageColorBrightness(Image *image,int brightness);
void ImageColorReplace(Image *image,Color color,Color replace);
Color *LoadImageColors(Image image);
Color *LoadImagePalette(Image image,int maxPaletteSize,int *colorCount);
void UnloadImageColors(Color *colors);
void UnloadImagePalette(Color *colors);
Rectangle GetImageAlphaBorder(Image image,float threshold);
Color GetImageColor(Image image,int x,int y);
void ImageClearBackground(Image *dst,Color color);
void ImageDrawPixel(Image *dst,int posX,int posY,Color color);
void ImageDrawPixelV(Image *dst,Vector2 position,Color color);
void ImageDrawLine(Image *dst,int startPosX,int startPosY,int endPosX,int endPosY,Color color);
void ImageDrawLineV(Image *dst,Vector2 start,Vector2 end,Color color);
void ImageDrawCircle(Image *dst,int centerX,int centerY,int radius,Color color);
void ImageDrawCircleV(Image *dst,Vector2 center,int radius,Color color);
void ImageDrawRectangle(Image *dst,int posX,int posY,int width,int height,Color color);
void ImageDrawRectangleV(Image *dst,Vector2 position,Vector2 size,Color color);
void ImageDrawRectangleRec(Image *dst,Rectangle rec,Color color);
void ImageDrawRectangleLines(Image *dst,Rectangle rec,int thick,Color color);
void ImageDraw(Image *dst,Image src,Rectangle srcRec,Rectangle dstRec,Color tint);
Texture2D LoadTextureRGBA(const unsigned char *pixels,int width,int height);
Texture2D LoadTexture(const char *fileName);
Texture2D LoadTextureFromImage(Image image);
bool IsTextureValid(Texture2D texture);
void UnloadTexture(Texture2D texture);
void UpdateTexture(Texture2D texture,const void *pixels);
void UpdateTextureRec(Texture2D texture,Rectangle rec,const void *pixels);
void SetTextureFilter(Texture2D texture,int filter);
void SetTextureWrap(Texture2D texture,int wrap);
void GenTextureMipmaps(Texture2D *texture);
void DrawTexture(Texture2D texture,int x,int y,Color tint);
void DrawTextureV(Texture2D texture,Vector2 position,Color tint);
void DrawTextureEx(Texture2D texture,Vector2 position,float rotation,float scale,Color tint);
void DrawTextureRec(Texture2D texture,Rectangle source,Vector2 position,Color tint);
void DrawTexturePro(Texture2D texture,Rectangle source,Rectangle dest,Vector2 origin,float rotation,Color tint);
void DrawTextureNPatch(Texture2D texture,NPatchInfo nPatchInfo,Rectangle dest,Vector2 origin,float rotation,Color tint);
RenderTexture2D LoadRenderTexture(int width,int height);
bool IsRenderTextureValid(RenderTexture2D target);
void UnloadRenderTexture(RenderTexture2D target);
void BeginTextureMode(RenderTexture2D target);
void EndTextureMode(void);
void InitAudioDevice(void);
void CloseAudioDevice(void);
bool IsAudioDeviceReady(void);
void SetMasterVolume(float volume);
float GetMasterVolume(void);
Wave LoadWave(const char *fileName);
Wave LoadWaveFromMemory(const char *fileType,const unsigned char *fileData,int dataSize);
bool IsWaveValid(Wave wave);
Wave WaveCopy(Wave wave);
void WaveCrop(Wave *wave,int initFrame,int finalFrame);
float *LoadWaveSamples(Wave wave);
void UnloadWaveSamples(float *samples);
void UnloadWave(Wave wave);
bool ExportWave(Wave wave,const char *fileName);
bool ExportWaveAsCode(Wave wave,const char *fileName);
Sound LoadSound(const char *fileName);
Sound LoadSoundFromWave(Wave wave);
bool IsSoundValid(Sound sound);
void UpdateSound(Sound sound,const void *data,int sampleCount);
void UnloadSound(Sound sound);
void PlaySound(Sound sound);
void StopSound(Sound sound);
void PauseSound(Sound sound);
void ResumeSound(Sound sound);
bool IsSoundPlaying(Sound sound);
void SetSoundVolume(Sound sound,float volume);
void SetSoundPitch(Sound sound,float pitch);
void SetSoundPan(Sound sound,float pan);
Music LoadMusicStream(const char *fileName);
Music LoadMusicStreamFromMemory(const char *fileType,const unsigned char *data,int dataSize);
bool IsMusicValid(Music music);
void UnloadMusicStream(Music music);
void PlayMusicStream(Music music);
bool IsMusicStreamPlaying(Music music);
void UpdateMusicStream(Music music);
void StopMusicStream(Music music);
void PauseMusicStream(Music music);
void ResumeMusicStream(Music music);
void SeekMusicStream(Music music,float position);
void SetMusicVolume(Music music,float volume);
void SetMusicPitch(Music music,float pitch);
void SetMusicPan(Music music,float pan);
float GetMusicTimeLength(Music music);
float GetMusicTimePlayed(Music music);
AudioStream LoadAudioStream(unsigned int sampleRate,unsigned int sampleSize,unsigned int channels);
bool IsAudioStreamValid(AudioStream stream);
void UnloadAudioStream(AudioStream stream);
void UpdateAudioStream(AudioStream stream,const void *data,int frameCount);
bool IsAudioStreamProcessed(AudioStream stream);
void PlayAudioStream(AudioStream stream);
void PauseAudioStream(AudioStream stream);
void ResumeAudioStream(AudioStream stream);
bool IsAudioStreamPlaying(AudioStream stream);
void StopAudioStream(AudioStream stream);
void SetAudioStreamVolume(AudioStream stream,float volume);
void SetAudioStreamPitch(AudioStream stream,float pitch);
void SetAudioStreamPan(AudioStream stream,float pan);
void SetAudioStreamBufferSizeDefault(int size);
void SetAudioStreamCallback(AudioStream stream,AudioCallback callback);
void AttachAudioStreamProcessor(AudioStream stream,AudioCallback processor);
void DetachAudioStreamProcessor(AudioStream stream,AudioCallback processor);
void AttachAudioMixedProcessor(AudioCallback processor);
void DetachAudioMixedProcessor(AudioCallback processor);

#endif /* RAYGPU_H */
