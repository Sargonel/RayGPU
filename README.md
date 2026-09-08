# RayGPU

RayGPU is an experimental, single-header 2D C library with a raylib-style API
and a WebGPU renderer. The same C source can compile as a native Windows
program or as freestanding WebAssembly for a browser.

The web build does **not** use Emscripten, WASI, Node.js, or a package manager.
Native Windows rendering uses Dawn with D3D12. Browser rendering uses WebGPU
through the small `raygpu.js` platform bridge.

RayGPU currently matches 290 raylib function names. It is not yet a complete
drop-in raylib replacement, and 3D, models, meshes, materials, VR, and stereo
rendering are outside its current scope.

## Quick start

Define `RAYGPU_IMPLEMENTATION` in exactly one C source file:

```c
#define RAYGPU_IMPLEMENTATION
#include "raygpu.h"

static void GameFrame(void)
{
    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from RayGPU", 20, 20, 30, DARKBLUE);
        DrawCircle(400, 225, 50, SKYBLUE);
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
```

`RunMainLoop()` is required because native Windows uses a normal blocking loop,
while the browser uses `requestAnimationFrame()`.

## Requirements

Native Windows builds require:

- Clang with C17 support.
- A Dawn source checkout at `../dawn`.
- Dawn generated headers and libraries under `build/dawn-release`.
- Windows 10 or newer with a D3D12-capable GPU.

Web builds require:

- Clang with the `wasm32` target.
- A browser with WebGPU support.
- HTTPS when deployed. `localhost` is allowed for development.

The Dawn paths can be changed at the top of the root `makefile`. Dawn is only
needed for native builds; browsers provide their own WebGPU implementation.

## Commands

The root makefile exposes five commands:

```powershell
make native   # Build main.exe
make web      # Build the static site in build/web
make run      # Build and run main.exe
make server   # Build web and serve http://localhost:8000
make clean    # Remove generated native and web files
```

Stop the local server from its terminal with `Ctrl+C`.

## Snake example

[`examples/snake.c`](examples/snake.c) is a complete 1920x1080 Snake game made
with RayGPU. It demonstrates shapes, gradients, custom fonts, procedural sound,
keyboard input, timing, random values, collision logic, a 2D camera, render
textures, scissor rectangles, blend modes, texture drawing, and an animated
WGSL shader with uniforms.

Run it natively:

```powershell
cd examples
make run
```

Run the exact same source in a browser:

```powershell
cd examples
make server
```

Snake controls:

- `WASD` or arrow keys: move
- `P`: pause
- `Enter`: restart after losing

Snake uses `assets/font.ttf` when available and falls back to the built-in font
when it is absent. Its sound effects are generated in C and require no files.

## Assets

The root `assets/` directory is intentionally ignored by Git. Create it locally
and place your game files inside it. The current `main.c` looks for:

```text
assets/
|-- example.jpg
|-- font.ttf
`-- tone.wav
```

`make web` copies everything under `assets/` into `build/web/assets/`. Keep
asset paths relative, such as `LoadTexture("assets/player.png")`, so the same
source works natively and in the browser.

## Web hosting

`make web` produces a static site:

```text
build/web/
|-- index.html
|-- raygpu.js
|-- main.wasm
`-- assets/
```

Upload the contents of `build/web` to any static host. The host must:

- Use HTTPS in production.
- Serve `main.wasm` with the `application/wasm` MIME type.
- Preserve the relative paths under `assets/`.

The server does not need Emscripten, Dawn, Node.js, PowerShell, or a C compiler.
Opening `index.html` through a `file://` URL is not supported.

## Supported features

- Window creation, resizing, DPI-aware dimensions, focus state, timing, FPS
  limiting, title/position/size controls, and native minimize/maximize/restore.
- Complete raylib keyboard constants; press, repeat, release, Unicode character
  queues, mouse buttons, position, delta, wheel, offset, and scaling.
- Complete raylib 2D shapes, gradients, polygons, splines, 2D collisions, 2D
  cameras, and screen/world conversion.
- PNG, JPEG, BMP, TGA, and first-frame GIF decoding from files or memory.
- Image creation, copying, cropping, resizing, flipping, arbitrary rotation,
  alpha/color processing, palettes, blur, convolution, dithering, channel
  extraction, CPU drawing, and procedural gradients/noise/cellular images.
- Texture loading, partial/full updates, filtering, wrapping, source rectangles,
  scaling, rotation, nine-patch drawing, and three-patch drawing.
- Render textures, alpha/additive/multiplied/color/premultiplied blend modes, and
  scissor rectangles.
- Custom WGSL vertex and fragment shaders with float, vector, integer, array,
  and matrix uniform uploads.
- Built-in text plus TTF/OTF loading from files or memory, UTF-8 drawing,
  rotated text, measurement, and glyph lookup.
- WAV loading from files or memory and sound playback with pause, resume,
  volume, pitch, pan, and sample updates.
- Binary/text file loading, path inspection, memory allocation, random-number
  utilities, and common color/text helpers.
- Growable WebAssembly memory: 64 MB initially, up to 2 GB or the browser's
  available limit.

Image decoding and font rasterization use `stb_image` v2.30 and
`stb_truetype`, amalgamated into `raygpu.h`. Both use public-domain/MIT
licensing and are also bundled by raylib. Users do not install them separately.

## Custom WGSL contract

Custom shaders use `vs` and `fs` as their entry points. Vertex inputs are:

| Location | Type | Value |
|---:|---|---|
| 0 | `vec2f` | position |
| 1 | `vec2f` | texture coordinates |
| 2 | `vec4f` | RGBA8 vertex color |

Texture shaders use group 0:

| Binding | Resource |
|---:|---|
| 0 | filtering sampler |
| 1 | `texture_2d<f32>` |

Optional uniforms use a 2048-byte buffer at group 1, binding 0. Declare it as:

```wgsl
struct Uniforms { values: array<vec4f, 128> };
@group(1) @binding(0) var<uniform> uniforms: Uniforms;
```

Uniform location `n` starts at `uniforms.values[n*4]`, reserving 64 bytes for
that location. See the shader in [`main.c`](main.c) or
[`examples/snake.c`](examples/snake.c) for complete examples.

## Remaining 2D work

- OGG, MP3, and FLAC decoding, streamed music, procedural audio streams, and
  audio processors.
- Image and generated texture mipmaps, animated GIF frames, image exporting,
  screenshots, and GPU texture readback.
- Image-based fonts, font-data extraction, and font-atlas export helpers.
- Extra shader texture samplers and shader attribute reflection.
- Gamepads, vibration, touch input, gestures, and cursor management.
- Fullscreen, borderless mode, monitor selection/information, window icons,
  opacity, clipboard, and dropped files.
- File saving, directory listing, URL opening, compression, Base64, hashes,
  logging callbacks, and automation events.

## Current limits

- Native rendering currently supports Windows through Dawn/D3D12.
- Images and ordinary textures use RGBA8 internally.
- GIF loading currently returns only the first frame.
- The built-in font contains a small ASCII subset; use a TTF/OTF font for wider
  Unicode coverage.
- RayGPU supports 256 simultaneous texture slots, 32 custom shaders, and
  262,144 vertices per frame.

## Distribution

Keep `raygpu.h`, `raygpu.js`, and `shell.html` in the repository. Do not
commit the Dawn checkout or Dawn build output: Dawn is large and
platform-specific. Native users can build a pinned Dawn revision separately,
and releases can optionally provide prebuilt Windows Dawn binaries. Web users
do not need Dawn.
