/* The same source builds and runs on native Windows and the web. */
#define RAYGPU_IMPLEMENTATION
#include "raygpu.h"

#define WIDTH 1280
#define HEIGHT 720

static Texture2D picture;
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

    if (position.x < 0) position.x = 0;
    if (position.y < 110) position.y = 110;
    if (position.x > GetScreenWidth() - picture.width) position.x = GetScreenWidth() - picture.width;
    if (position.y > GetScreenHeight() - picture.height) position.y = GetScreenHeight() - picture.height;

    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("RayGPU image example", 24, 20, 32, DARKBLUE);
        DrawText("Image loaded with LoadImage + LoadTextureFromImage", 24, 62, 18, GRAY);
        DrawText("Move it with WASD or the arrow keys", 24, 86, 18, GRAY);

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

    Image image = LoadImage("assets/example.jpg");
    if (IsImageValid(image)) {
        picture = LoadTextureFromImage(image);
        SetTextureFilter(picture, TEXTURE_FILTER_BILINEAR);
        UnloadImage(image);       /* CPU pixels are no longer needed. */
    }

    SetTargetFPS(60);
    RunMainLoop(GameFrame);       /* RayGPU releases GPU resources on exit. */
    return RayGPUHadError() ? 1 : 0;
}
