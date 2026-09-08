#include "examples.h"

static Camera3D camera;
static float orbit;
static Model glbModel;
static ModelAnimation *glbAnimations;
static int glbAnimationCount;

void Basic3DUpdate(void)
{
    orbit += GetFrameTime()*0.35f;
    camera.position.x = Sin(orbit + 1.5707963f)*14.0f;
    camera.position.z = Sin(orbit)*14.0f;
    if (glbAnimationCount > 0 && glbAnimations[0].frameCount > 0 &&
        IsModelAnimationValid(glbModel, glbAnimations[0])) {
        int frame = (int)(GetTime()*60.0) % glbAnimations[0].frameCount;
        UpdateModelAnimation(glbModel, glbAnimations[0], frame);
    }

    Ray mouseRay = GetScreenToWorldRay(GetMousePosition(), camera);
    BoundingBox cubeBox = {{-1.5f, 0.0f, -1.5f}, {1.5f, 3.0f, 1.5f}};
    bool cubeSelected = GetRayCollisionBox(mouseRay, cubeBox).hit;

    BeginDrawing();
        ClearBackground((Color){12, 18, 30, 255});

        BeginMode3D(camera);
            DrawGrid(20, 1.0f);
            DrawCube((Vector3){0, 1.5f, 0}, 3, 3, 3, cubeSelected ? GOLD : BLUE);
            DrawCubeWires((Vector3){0, 1.5f, 0}, 3, 3, 3, WHITE);
            DrawSphere((Vector3){-4, 1.5f, 0}, 1.5f, RED);
            DrawSphereWires((Vector3){-4, 1.5f, 0}, 1.52f, 12, 18, MAROON);
            DrawCylinder((Vector3){4, 0, 0}, 0.5f, 1.5f, 3.0f, 24, GREEN);
            DrawCylinderWires((Vector3){4, 0, 0}, 0.5f, 1.5f, 3.0f, 24, DARKGREEN);
            if (IsModelValid(glbModel)) {
                DrawModelEx(glbModel, (Vector3){0, 1.0f, 4.5f},
                    (Vector3){0, 1, 0}, orbit*45.0f, (Vector3){0.8f, 0.8f, 0.8f}, WHITE);
            }
            DrawLine3D((Vector3){0, 0, 0}, (Vector3){3, 0, 0}, RED);
            DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 3, 0}, GREEN);
            DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, 3}, BLUE);
        EndMode3D();

        DrawRectangle(24, 24, 680, 150, (Color){0, 0, 0, 170});
        DrawText("RayGPU basic 3D", 44, 40, 36, RAYWHITE);
        DrawText("Move the pointer over the cube to highlight it", 44, 86, 22, LIGHTGRAY);
        DrawText(IsModelValid(glbModel) ? "Loaded assets/example.glb" : "assets/example.glb not found",
            44, 142, 20, IsModelValid(glbModel) ? GOLD : GRAY);
        if (glbAnimationCount > 0) DrawText("Playing the first model animation", 390, 142, 20, SKYBLUE);
        DrawExampleBackButton();
        DrawFPS(1800, 112);
    EndDrawing();
}

void Basic3DInit(void)
{
    orbit = 0;
    camera = (Camera3D){
        .position = {14, 10, 14},
        .target = {0, 1.5f, 0},
        .up = {0, 1, 0},
        .fovy = 45,
        .projection = CAMERA_PERSPECTIVE
    };
    glbModel = LoadModel("assets/example.glb");
    glbAnimations = LoadModelAnimations("assets/example.glb", &glbAnimationCount);
    SetWindowTitle(IsModelValid(glbModel) ?
        "RayGPU Examples - Basic 3D - GLB loaded" : "RayGPU Examples - Basic 3D");
}

void Basic3DShutdown(void)
{
    UnloadModelAnimations(glbAnimations, glbAnimationCount);
    if (IsModelValid(glbModel)) UnloadModel(glbModel);
    glbAnimations = (ModelAnimation *)0;
    glbAnimationCount = 0;
    glbModel = (Model){0};
    camera = (Camera3D){0};
    orbit = 0;
}
