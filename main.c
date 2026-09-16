#include "raygpu.h"

static void GameFrame(void)
{
    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from RayGPU", 20, 20, 30, DARKBLUE);
    EndDrawing();
}

int main(void)
{
    InitWindow(800, 450, "RayGPU");
    if (!IsWindowReady()) return 1;

    SetTargetFPS(60);
    RunMainLoop(GameFrame);
    return RayGPUHadError() ? 1 : 0;
}
