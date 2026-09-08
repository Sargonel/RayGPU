/* The same source builds and runs on native Windows and the web. */
#define RAYGPU_IMPLEMENTATION
#include "raygpu.h"

#define WIDTH 1280
#define HEIGHT 720

static Texture2D picture;
static RenderTexture2D badge;
static Shader tintShader;
static int tintLocation;
static Font customFont;
static Sound effect;
static Vector2 position = { 500.0f, 260.0f };
static float rotation;

static void GameFrame(void)
{
    float movement = 300.0f*GetFrameTime();
    rotation += 25.0f*GetFrameTime();

    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) position.x += movement;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) position.x -= movement;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) position.y += movement;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) position.y -= movement;
    if (IsKeyPressed(KEY_SPACE)) PlaySound(effect);

    if (position.x < 0) position.x = 0;
    if (position.y < 110) position.y = 110;
    if (position.x > GetScreenWidth() - picture.width) position.x = GetScreenWidth() - picture.width;
    if (position.y > GetScreenHeight() - picture.height) position.y = GetScreenHeight() - picture.height;

    BeginTextureMode(badge);
        ClearBackground((Color){ 8, 18, 45, 255 });
        BeginScissorMode(8, 8, 224, 104);
            BeginBlendMode(BLEND_ADDITIVE);
                DrawCircle(75, 60, 44, (Color){ 30, 120, 255, 180 });
                DrawCircle(118, 60, 44, (Color){ 255, 40, 130, 180 });
            EndBlendMode();
        EndScissorMode();
        DrawText("RENDER TEXTURE", 18, 92, 14, WHITE);
    EndTextureMode();

    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("RayGPU image example", 24, 20, 32, DARKBLUE);
        DrawText("Image loaded with LoadImage + LoadTextureFromImage", 24, 62, 18, GRAY);
        DrawText("Move: WASD/arrows   Play sound: SPACE", 24, 86, 18, GRAY);
        float shaderTint[4] = { 0.75f + 0.25f*Sin((float)GetTime()), 1.0f, 1.0f, 1.0f };
        SetShaderValue(tintShader, tintLocation, shaderTint, SHADER_UNIFORM_VEC4);
        BeginShaderMode(tintShader);
            DrawTextureRec(badge.texture, (Rectangle){ 0, 120, 240, -120 }, (Vector2){ 24, 120 }, WHITE);
        EndShaderMode();

        if (IsFontValid(customFont)) {
            DrawTextEx(customFont, "Custom TTF font works!", (Vector2){ 300, 145 }, 42, 2, DARKBLUE);
            DrawTextEx(customFont, "Türkçe: ç Ç ğ Ğ ı İ ö Ö ş Ş ü Ü",
                (Vector2){ 300, 195 }, 34, 1, BLACK);
            DrawTextPro(customFont, "Rotated custom text", (Vector2){ 420, 270 },
                (Vector2){ 110, 20 }, -12, 32, 1, MAROON);
        }
        else DrawText("Could not load assets/font.ttf", 300, 150, 20, RED);

        if (IsTextureValid(picture)) {
            Rectangle source = { 0, 0, (float)picture.width, (float)picture.height };
            Rectangle destination = { position.x + picture.width/2.0f,
                position.y + picture.height/2.0f, (float)picture.width, (float)picture.height };
            DrawTexturePro(picture, source, destination,
                (Vector2){ picture.width/2.0f, picture.height/2.0f }, rotation, WHITE);
        }
        else DrawText("Could not load assets/example.jpg", 24, 140, 20, RED);

        DrawFPS(GetScreenWidth() - 100, 24);
    EndDrawing();
}

int main(void)
{
    InitWindow(WIDTH, HEIGHT, "RayGPU image example");
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(640, 360);
    badge = LoadRenderTexture(240, 120);
    tintShader = LoadShaderFromMemory(NULL,
        "struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };"
        "struct Uniforms { values: array<vec4f, 128> };"
        "@group(0) @binding(0) var smp: sampler; @group(0) @binding(1) var tex: texture_2d<f32>;"
        "@group(1) @binding(0) var<uniform> uniforms: Uniforms;"
        "@fragment fn fs(v: V) -> @location(0) vec4f {"
        " return textureSample(tex, smp, v.uv)*v.color*uniforms.values[0]; }");
    tintLocation = GetShaderLocation(tintShader, "tint");

    int fontCodepoints[107];
    for (int i = 0; i < 95; i++) fontCodepoints[i] = 32 + i;
    const int turkishCodepoints[12] = {
        0x00E7, 0x00C7, 0x011F, 0x011E, 0x0131, 0x0130,
        0x00F6, 0x00D6, 0x015F, 0x015E, 0x00FC, 0x00DC
    };
    for (int i = 0; i < 12; i++) fontCodepoints[95 + i] = turkishCodepoints[i];
    customFont = LoadFontEx("assets/font.ttf", 48, fontCodepoints, 107);

    InitAudioDevice();
    effect = LoadSound("assets/tone.wav");

    Image image = LoadImage("assets/example.jpg");
    if (IsImageValid(image)) {
        picture = LoadTextureFromImage(image);
        SetTextureFilter(picture, TEXTURE_FILTER_BILINEAR);
        UnloadImage(image);       /* CPU pixels are no longer needed. */
    }

    SetTargetFPS(60);
    RunMainLoop(GameFrame);       /* RayGPU releases GPU and audio resources on exit. */
    return RayGPUHadError() ? 1 : 0;
}
