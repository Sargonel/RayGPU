# RayGPU

A small raylib-style 2D library using WebGPU. Write your game in **one C file**
and compile that same file for Windows and the web.

**No Emscripten, WASI SDK, npm packages, GLFW, or SDL.**
The web build needs only Clang and `wasm-ld`, both already installed here.
Windows uses your existing Dawn library and the Windows SDK. A native WebGPU
implementation is still necessary; this project is not dependency-free on Windows.

## Build and run

From this folder in PowerShell:

```powershell
make run       # Compile main.c and run the native Windows app
make serve     # Compile the same main.c to WASM and serve the web demo
```

Open **http://127.0.0.1:8000** after `make serve`. Stop the server with Ctrl+C.
The server uses Windows' built-in PowerShell/.NET; no server package is needed.
Use a browser with WebGPU enabled. Browser WebGPU requires HTTPS or localhost,
so opening the HTML with `file://` will not work.

Build without running:

```powershell
make native    # main.exe + d3dcompiler_47.dll
make web       # build/web/index.html + raygpu.js + main.wasm
```

The native make target copies the installed Windows shader compiler beside the
executable because Dawn searches there. It downloads nothing. Keep that DLL
beside `main.exe`. Native builds use the optimized Dawn library generated at
`build/dawn-release`; the Dawn source remains at `../dawn`. Paths are configurable:

```powershell
make native DAWN=../dawn DAWN_BUILD=build/dawn-release
```

## Your game

```c
#define RAYGPU_IMPLEMENTATION
#include "raygpu.c"

static void UpdateDraw(void) {
    BeginDrawing();
    ClearBackground(BLACK);
    DrawRectangle(40, 40, 120, 80, SKYBLUE);
    DrawText("HELLO WEBGPU", 40, 150, 28, WHITE);
    EndDrawing();
}

int main(void) {
    InitWindow(800, 600, "My game");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(60);
    RunMainLoop(UpdateDraw);
    return RayGPUHadError() ? 1 : 0;
}
```

`RunMainLoop` calls your frame function until Escape or window close. It owns
window and GPU cleanup. Call `CloseWindow()` to quit from a frame callback.
Store persistent game state globally or in static variables: the browser returns
from `main` before it calls the frame function. Do not put cleanup after
`RunMainLoop` that your web frames still depend on.

Windows runs a normal loop with a precise frame limiter. At high target rates
the final wait uses CPU time so Windows cannot round it down to 60 FPS. The browser uses
`requestAnimationFrame`; blocking inside a `while` loop would prevent browser
rendering and input. `SetTargetFPS` caps frames, while the browser's display
refresh rate remains the upper limit. Requesting 120 FPS on a 60 Hz browser
display therefore still presents at 60 FPS.
`GetFrameTime()` is in seconds and caps long stalls at 0.1 seconds.

## Files and architecture

- `main.c`: the game, with no platform-specific branches.
- `raygpu.h`: the single-header C library. Define `RAYGPU_IMPLEMENTATION` and
  include it in exactly one C file.
- `raygpu.js`: the browser platform bridge, using WebGPU directly.
- `shell.html`: the canvas and script loader.

C owns game state, input state, shapes, text geometry, and ordered texture batches.
Windows submits those batches through Dawn/D3D12. WebAssembly passes the same
vertex and batch layouts to the JavaScript bridge. The browser API needs this
bridge; WASM cannot call browser WebGPU directly.

## Implemented scope

- The complete 66-function raylib shapes module: pixels, lines, sectors,
  circles, ellipses, rings, rectangles, gradients, rounded rectangles,
  polygons, splines, spline evaluation, and 2D collision queries.
- Images and textures from PNG, JPEG, BMP, TGA, and GIF files or memory,
  decoded to RGBA8 by an amalgamated stb_image implementation. Raw RGBA8
  textures remain available through `LoadTextureRGBA`.
- Image generation, copying, cropping, nearest/bilinear resizing, canvas
  resizing, flipping, 90-degree rotation, alpha processing, color processing,
  palettes, alpha borders, and basic CPU image drawing.
- Full/source-rectangle/rotated texture drawing, complete and partial GPU
  texture updates, bilinear/point/anisotropic filtering, and wrap modes.
- A built-in 5x7 font, text measurement, UTF-8/codepoint conversion, string
  helpers, `DrawText`, and `DrawFPS`.
- Keyboard pressed/repeated/down/released/up states; mouse button states,
  position, delta and wheel input; focus reset.
- 2D camera drawing plus world/screen conversion and camera matrices.
- Complete raylib color conversion and adjustment helpers.
- Timing, random numbers and unique random sequences, plus portable memory
  allocation for native and freestanding WebAssembly.
- Native window resizing; the web canvas scales to the available page width.
- Timing, frame limiting, resource cleanup, and GPU error reporting.

This is an independent **2D library under active development**, not yet a full
raylib drop-in replacement. 3D and VR are intentionally out of scope. Audio,
custom fonts, shaders, render textures, gamepads, touch,
gestures, and filesystem APIs are still being implemented.
Textures take raw RGBA pixels; unload them outside a drawing frame. Texture
scaling uses nearest filtering. The font covers A-Z, digits, spaces, newlines,
and `: . - / + ? !`; lowercase maps to uppercase. The native backend is Windows
D3D12 only. There are 256 texture slots (one reserved) and 262,144 vertices per
frame; excess geometry is skipped with a message. Devices lost during execution
stop the demo and require a restart.
