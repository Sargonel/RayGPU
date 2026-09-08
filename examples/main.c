#define RAYGPU_IMPLEMENTATION
#include "examples.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

typedef enum ExampleScreen {
    EXAMPLE_CATALOG,
    EXAMPLE_SNAKE,
    EXAMPLE_BASIC_3D
} ExampleScreen;

static ExampleScreen currentScreen;
static Rectangle backButton = {1660, 28, 220, 62};

static bool Clicked(Rectangle bounds)
{
    return CheckCollisionPointRec(GetMousePosition(), bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int FitTextSize(const char *text, int preferredSize, int maxWidth)
{
    int size = preferredSize;
    while (size > 12 && MeasureText(text, size) > maxWidth) size--;
    return size;
}

static void DrawFittedText(const char *text, int x, int y, int preferredSize,
    int maxWidth, Color color)
{
    DrawText(text, x, y, FitTextSize(text, preferredSize, maxWidth), color);
}

void DrawExampleBackButton(void)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), backButton);
    DrawRectangleRounded(backButton, 0.28f, 8, hover ? (Color){54, 78, 112, 245} : (Color){25, 38, 60, 230});
    DrawRectangleRoundedLinesEx(backButton, 0.28f, 8, 2, hover ? SKYBLUE : (Color){80, 110, 145, 255});
    const char *label = "<  EXAMPLES";
    int size = FitTextSize(label, 24, (int)backButton.width - 32);
    int width = MeasureText(label, size);
    DrawText(label, (int)(backButton.x + (backButton.width - width)/2),
        (int)(backButton.y + (backButton.height - size)/2), size, RAYWHITE);
}

static void OpenExample(ExampleScreen screen)
{
    currentScreen = screen;
    if (screen == EXAMPLE_SNAKE) SnakeInit();
    else if (screen == EXAMPLE_BASIC_3D) Basic3DInit();
}

static void CloseExample(void)
{
    if (currentScreen == EXAMPLE_SNAKE) SnakeShutdown();
    else if (currentScreen == EXAMPLE_BASIC_3D) Basic3DShutdown();
    currentScreen = EXAMPLE_CATALOG;
    SetWindowTitle("RayGPU Examples");
}

static void DrawCard(Rectangle card, const char *number, const char *title,
    const char *description1, const char *description2, Color accent)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), card);
    DrawRectangleRounded(card, 0.06f, 12, hover ? (Color){29, 44, 68, 255} : (Color){20, 31, 49, 255});
    DrawRectangleRoundedLinesEx(card, 0.06f, 12, hover ? 4 : 2, hover ? accent : (Color){54, 75, 103, 255});
    DrawCircle((int)card.x + 72, (int)card.y + 72, 34, accent);
    int numberWidth = MeasureText(number, 32);
    DrawText(number, (int)card.x + 72 - numberWidth/2, (int)card.y + 54, 32, (Color){8, 13, 22, 255});
    DrawFittedText(title, (int)card.x + 128, (int)card.y + 40, 38,
        (int)card.width - 164, RAYWHITE);
    DrawFittedText(description1, (int)card.x + 128, (int)card.y + 94, 22,
        (int)card.width - 164, LIGHTGRAY);
    DrawFittedText(description2, (int)card.x + 128, (int)card.y + 126, 22,
        (int)card.width - 164, LIGHTGRAY);
    DrawFittedText(hover ? "CLICK TO OPEN  >" : "OPEN EXAMPLE",
        (int)card.x + 36, (int)(card.y + card.height - 62), 22,
        (int)card.width - 72, hover ? accent : GRAY);
}

static void DrawCatalog(void)
{
    Rectangle snakeCard = {150, 300, 760, 470};
    Rectangle threeDCard = {1010, 300, 760, 470};

    if (Clicked(snakeCard) || IsKeyPressed(KEY_ONE)) { OpenExample(EXAMPLE_SNAKE); SnakeUpdate(); return; }
    if (Clicked(threeDCard) || IsKeyPressed(KEY_TWO)) { OpenExample(EXAMPLE_BASIC_3D); Basic3DUpdate(); return; }

    BeginDrawing();
        ClearBackground((Color){8, 13, 23, 255});
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
            (Color){18, 35, 60, 255}, (Color){5, 9, 17, 255});
        DrawFittedText("RAYGPU EXAMPLES", 150, 100, 64, SCREEN_WIDTH - 300, RAYWHITE);
        DrawFittedText("Choose a demo. Everything runs inside this one application.",
            154, 185, 28, SCREEN_WIDTH - 308, LIGHTGRAY);

        DrawCard(snakeCard, "1", "SNAKE", "2D game, sound and shader",
            "Render texture and keyboard input", (Color){76, 255, 174, 255});
        DrawCard(threeDCard, "2", "BASIC 3D", "Depth, camera and GLB model",
            "Primitives, wireframes and picking", (Color){102, 191, 255, 255});

        DrawText("Click a card or press 1 / 2 to enter", 150, 850, 26, GRAY);
        DrawText("ESC or the EXAMPLES button returns here", 150, 895, 22, DARKGRAY);
        DrawFPS(1800, 24);
    EndDrawing();
}

static void AppFrame(void)
{
    if (currentScreen != EXAMPLE_CATALOG &&
        (IsKeyPressed(KEY_ESCAPE) || Clicked(backButton))) {
        CloseExample();
        DrawCatalog();
        return;
    }

    if (currentScreen == EXAMPLE_SNAKE) SnakeUpdate();
    else if (currentScreen == EXAMPLE_BASIC_3D) Basic3DUpdate();
    else DrawCatalog();
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "RayGPU Examples");
    if (!IsWindowReady()) return 1;

    SetWindowMinSize(1280, 720);
    SetExitKey(KEY_NULL);
    SetTargetFPS(120);
    InitAudioDevice();
    RunMainLoop(AppFrame);
    return RayGPUHadError() ? 1 : 0;
}
