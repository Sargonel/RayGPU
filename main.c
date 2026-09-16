#include "raygpu.h"

static Shader checkerShader;
static Texture2D checkerTexture;
static Texture2D panelTexture;

static void GameFrame(void)
{
    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from RayGPU", 20, 20, 30, DARKBLUE);
        DrawCircle(400, 225, 50, SKYBLUE);
        BeginShaderMode(checkerShader);
            DrawTexturePro(panelTexture, (Rectangle){ 0, 0, 2, 2 }, (Rectangle){ 520, 150, 220, 150 }, (Vector2){ 0, 0 }, 0, WHITE);
        EndShaderMode();
        DrawText("extra sampler", 545, 315, 20, DARKGRAY);
    EndDrawing();
}

int main(void)
{
    InitWindow(800, 450, "RayGPU");
    if (!IsWindowReady()) return 1;

    Image checker = GenImageChecked(64, 64, 8, 8, (Color){ 40, 90, 220, 255 }, (Color){ 245, 190, 45, 255 });
    checkerTexture = LoadTextureFromImage(checker);
    UnloadImage(checker);
    Image panel = GenImageColor(2, 2, WHITE);
    panelTexture = LoadTextureFromImage(panel);
    UnloadImage(panel);
    checkerShader = LoadShaderFromMemory(0,
        "// @raygpu_sampler checkerTexture 0\n"
        "struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };"
        "@group(0) @binding(0) var smp: sampler; @group(0) @binding(1) var tex: texture_2d<f32>;"
        "@group(2) @binding(0) var checkerSampler: sampler;"
        "@group(2) @binding(1) var checkerTexture: texture_2d<f32>;"
        "@fragment fn fs(v: V) -> @location(0) vec4f {"
        " let base = textureSample(tex, smp, v.uv)*v.color;"
        " return base*textureSample(checkerTexture, checkerSampler, v.uv); }");
    int checkerLocation = GetShaderLocation(checkerShader, "checkerTexture");
    if (!IsTextureValid(checkerTexture) || !IsTextureValid(panelTexture) || !IsShaderValid(checkerShader) || checkerLocation != 0 ||
        GetShaderLocationAttrib(checkerShader, "p") != 0 || GetShaderLocationAttrib(checkerShader, "uv") != 1 ||
        GetShaderLocationAttrib(checkerShader, "c") != 2 || GetShaderLocationAttrib(checkerShader, "z") != 3) {
        CloseWindow();
        return 1;
    }
    SetShaderValueTexture(checkerShader, checkerLocation, checkerTexture);

    SetTargetFPS(60);
    RunMainLoop(GameFrame);
    return RayGPUHadError() ? 1 : 0;
}
