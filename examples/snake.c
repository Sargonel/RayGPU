/* Snake scene used by the single RayGPU example catalog. */
#include "examples.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define COLS 30
#define ROWS 20
#define CELL 24
#define BOARD_WIDTH (COLS*CELL)
#define BOARD_HEIGHT (ROWS*CELL)
#define MAX_SNAKE (COLS*ROWS)

typedef struct Cell { int x, y; } Cell;

static Cell snake[MAX_SNAKE];
static Cell food;
static int snakeLength;
static int directionX, directionY;
static int pendingX, pendingY;
static int score;
static float accumulator;
static bool paused;
static bool gameOver;
static bool turnQueued;
static RenderTexture2D board;
static Shader screenShader;
static int tintLocation;
static int timeLocation;
static Sound eatSound;
static Sound crashSound;
static Font uiFont;

static bool SameCell(Cell a, Cell b) { return a.x == b.x && a.y == b.y; }

static void GameText(const char *text, int x, int y, int size, Color color)
{
    if (IsFontValid(uiFont)) DrawTextEx(uiFont, text, (Vector2){ (float)x, (float)y }, (float)size, 1, color);
    else DrawText(text, x, y, size, color);
}

static void NumberText(char *text, int value)
{
    char reversed[12];
    int count = 0;
    if (value == 0) reversed[count++] = '0';
    while (value > 0 && count < 11) {
        reversed[count++] = (char)('0' + value%10);
        value /= 10;
    }
    for (int i = 0; i < count; i++) text[i] = reversed[count - i - 1];
    text[count] = '\0';
}

static void PlaceFood(void)
{
    bool occupied;
    do {
        food = (Cell){ GetRandomValue(0, COLS - 1), GetRandomValue(0, ROWS - 1) };
        occupied = false;
        for (int i = 0; i < snakeLength; i++) if (SameCell(food, snake[i])) occupied = true;
    } while (occupied);
}

static Sound MakeTone(float frequency, float seconds, float volume)
{
    enum { SAMPLE_RATE = 44100 };
    int frames = (int)(SAMPLE_RATE*seconds);
    short *samples = MemAlloc((unsigned int)(frames*sizeof(short)));
    if (!samples) return (Sound){0};
    for (int i = 0; i < frames; i++) {
        float fade = 1.0f - (float)i/frames;
        samples[i] = (short)(Sin(6.2831853f*frequency*i/SAMPLE_RATE)*volume*fade*32767.0f);
    }
    Wave wave = { (unsigned int)frames, SAMPLE_RATE, 16, 1, samples };
    Sound sound = LoadSoundFromWave(wave);
    MemFree(samples);
    return sound;
}

static void ResetGame(void)
{
    snakeLength = 5;
    for (int i = 0; i < snakeLength; i++) snake[i] = (Cell){ COLS/2 - i, ROWS/2 };
    directionX = pendingX = 1;
    directionY = pendingY = 0;
    score = 0;
    accumulator = 0;
    paused = false;
    gameOver = false;
    turnQueued = false;
    PlaceFood();
}

static void SetDirection(int x, int y)
{
    if (!turnQueued && (x != -directionX || y != -directionY)) {
        pendingX = x;
        pendingY = y;
        turnQueued = true;
    }
}

static void UpdateGame(void)
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) SetDirection(0, -1);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) SetDirection(0, 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) SetDirection(-1, 0);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) SetDirection(1, 0);
    if (IsKeyPressed(KEY_P) && !gameOver) paused = !paused;
    if (IsKeyPressed(KEY_ENTER) && gameOver) ResetGame();
    if (paused || gameOver) return;

    accumulator += GetFrameTime();
    float step = 0.115f - (score < 15 ? score*0.004f : 0.060f);
    while (accumulator >= step) {
        accumulator -= step;
        directionX = pendingX;
        directionY = pendingY;
        turnQueued = false;
        Cell next = { snake[0].x + directionX, snake[0].y + directionY };
        bool grow = SameCell(next, food);
        bool hit = next.x < 0 || next.y < 0 || next.x >= COLS || next.y >= ROWS;
        int collisionLength = snakeLength - (grow ? 0 : 1);
        for (int i = 0; i < collisionLength && !hit; i++) if (SameCell(next, snake[i])) hit = true;
        if (hit) {
            gameOver = true;
            PlaySound(crashSound);
            return;
        }

        int newLength = snakeLength + (grow && snakeLength < MAX_SNAKE ? 1 : 0);
        for (int i = newLength - 1; i > 0; i--) snake[i] = snake[i - 1];
        snake[0] = next;
        snakeLength = newLength;
        if (grow) {
            score++;
            SetSoundPitch(eatSound, 1.0f + score*0.025f);
            PlaySound(eatSound);
            PlaceFood();
        }
    }
}

static void DrawBoard(void)
{
    Camera2D camera = {0};
    camera.zoom = 1.0f;
    if (gameOver) camera.offset = (Vector2){ (float)GetRandomValue(-2, 2), (float)GetRandomValue(-2, 2) };

    BeginTextureMode(board);
        ClearBackground((Color){ 8, 13, 25, 255 });
        BeginScissorMode(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
            BeginMode2D(camera);
                for (int x = 0; x <= COLS; x++) DrawLine(x*CELL, 0, x*CELL, BOARD_HEIGHT, (Color){ 24, 38, 58, 255 });
                for (int y = 0; y <= ROWS; y++) DrawLine(0, y*CELL, BOARD_WIDTH, y*CELL, (Color){ 24, 38, 58, 255 });

                BeginBlendMode(BLEND_ADDITIVE);
                    DrawCircle(food.x*CELL + CELL/2, food.y*CELL + CELL/2,
                        10.0f + 2.0f*Sin((float)GetTime()*6.0f), (Color){ 255, 55, 90, 210 });
                    DrawCircle(food.x*CELL + CELL/2, food.y*CELL + CELL/2, 5, (Color){ 255, 210, 80, 255 });
                EndBlendMode();

                for (int i = snakeLength - 1; i >= 0; i--) {
                    float shade = 1.0f - i/(float)(snakeLength + 6);
                    Color color = ColorLerp((Color){ 24, 110, 95, 255 }, (Color){ 70, 255, 175, 255 }, shade);
                    DrawRectangleRounded((Rectangle){ snake[i].x*CELL + 2, snake[i].y*CELL + 2, CELL - 4, CELL - 4 }, 0.35f, 6, color);
                }
                int eyeX = snake[0].x*CELL + CELL/2 + directionX*5;
                int eyeY = snake[0].y*CELL + CELL/2 + directionY*5;
                DrawCircle(eyeX, eyeY, 3, BLACK);
            EndMode2D();
        EndScissorMode();
    EndTextureMode();
}

void SnakeUpdate(void)
{
    UpdateGame();
    DrawBoard();

    float tint[4] = { 0.88f + 0.12f*Sin((float)GetTime()*1.7f), 1.0f, 1.0f, 1.0f };
    float time = (float)GetTime();
    SetShaderValue(screenShader, tintLocation, tint, SHADER_UNIFORM_VEC4);
    SetShaderValue(screenShader, timeLocation, &time, SHADER_UNIFORM_FLOAT);

    BeginDrawing();
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
            (Color){ 12, 22, 42, 255 }, (Color){ 3, 7, 15, 255 });
        GameText("RAYGPU", 48, 40, 50, (Color){ 90, 255, 185, 255 });
        GameText("SNAKE", 48, 96, 50, (Color){ 90, 255, 185, 255 });
        GameText("WASD / ARROWS", 48, 180, 22, LIGHTGRAY);
        GameText("P  PAUSE", 48, 214, 22, LIGHTGRAY);
        GameText("ENTER  RESTART", 48, 248, 22, LIGHTGRAY);

        BeginShaderMode(screenShader);
            DrawTexturePro(board.texture, (Rectangle){ 0, 0, BOARD_WIDTH, BOARD_HEIGHT },
                (Rectangle){ 420, 80, 1440, 960 }, (Vector2){ 0 }, 0, WHITE);
        EndShaderMode();

        char number[12];
        NumberText(number, score);
        GameText("SCORE", 48, 340, 24, GRAY);
        GameText(number, 48, 374, 48, WHITE);
        NumberText(number, snakeLength);
        GameText("LENGTH", 48, 475, 24, GRAY);
        GameText(number, 48, 509, 42, WHITE);
        DrawFPS(1800, 30);

        if (paused) {
            DrawRectangle(760, 450, 400, 130, Fade(BLACK, 0.82f));
            GameText("PAUSED", 850, 486, 48, YELLOW);
        }
        if (gameOver) {
            DrawRectangle(670, 415, 580, 190, Fade(BLACK, 0.88f));
            GameText("GAME OVER", 795, 444, 54, RED);
            GameText("PRESS ENTER TO RESTART", 735, 530, 26, WHITE);
        }
        DrawExampleBackButton();
    EndDrawing();
}

void SnakeInit(void)
{
    SetWindowTitle("RayGPU Examples - Snake");
    SetRandomSeed((unsigned int)(GetTime()*1000000.0) + 0x51A9u);

    board = LoadRenderTexture(BOARD_WIDTH, BOARD_HEIGHT);
    screenShader = LoadShaderFromMemory(0,
        "// @raygpu_uniform tint 0\n"
        "// @raygpu_uniform time 1\n"
        "struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };"
        "struct Uniforms { values: array<vec4f, 128> };"
        "@group(0) @binding(0) var smp: sampler; @group(0) @binding(1) var tex: texture_2d<f32>;"
        "@group(1) @binding(0) var<uniform> uniforms: Uniforms;"
        "@fragment fn fs(v: V) -> @location(0) vec4f {"
        " let pixel = textureSample(tex, smp, v.uv)*v.color;"
        " let scan = 0.96 + 0.04*sin(v.position.y*0.45 + uniforms.values[4].x*4.0);"
        " return vec4f(pixel.rgb*uniforms.values[0].rgb*scan, pixel.a); }");
    tintLocation = GetShaderLocation(screenShader, "tint");
    timeLocation = GetShaderLocation(screenShader, "time");

    eatSound = MakeTone(720.0f, 0.09f, 0.32f);
    crashSound = MakeTone(115.0f, 0.38f, 0.42f);
    SetSoundPan(eatSound, 0.55f);
    uiFont = LoadFont("assets/font.ttf");

    ResetGame();
}

void SnakeShutdown(void)
{
    if (IsRenderTextureValid(board)) UnloadRenderTexture(board);
    if (IsShaderValid(screenShader)) UnloadShader(screenShader);
    if (IsSoundValid(eatSound)) UnloadSound(eatSound);
    if (IsSoundValid(crashSound)) UnloadSound(crashSound);
    if (IsFontValid(uiFont)) UnloadFont(uiFont);
    board = (RenderTexture2D){0};
    screenShader = (Shader){0};
    eatSound = crashSound = (Sound){0};
    uiFont = (Font){0};
}
