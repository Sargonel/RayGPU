#include "sargpu.h"

static void GameFrame(void)
{
    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from SarGPU", 20, 20, 30, DARKBLUE);
    EndDrawing();
}

int main(void)
{
    InitWindow(800, 450, "SarGPU");
    if (!IsWindowReady()) return 1;

    SetTargetFPS(60);
    RunMainLoop(GameFrame);
    return SarGPUHadError() ? 1 : 0;
}
