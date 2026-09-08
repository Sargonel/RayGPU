# RayGPU

RayGPU is an experimental, single-header 2D/3D C library with a raylib-style API
and a WebGPU renderer. The same C source can compile as a native Windows
program or as freestanding WebAssembly for a browser.

The web build does **not** use Emscripten, WASI, Node.js, or a package manager.
Native Windows rendering uses Dawn with D3D12. Browser rendering uses WebGPU
through the small `raygpu.js` platform bridge.

RayGPU is not yet a complete drop-in raylib replacement. It includes meshes,
models, glTF 2.0 materials, and basic skeletal animation, while advanced 3D
rendering is still being built. VR and stereo rendering are outside its scope.

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

## Example catalog

All examples are linked into one interactive 1920x1080 application. The
catalog lives in [`examples/main.c`](examples/main.c), while each example keeps
its implementation in a separate C file. Click a card or press its number to
open it; press `Escape` or click `EXAMPLES` to return to the catalog and choose
another.

Run the native catalog:

```powershell
cd examples
make run
```

Run the same catalog in a browser:

```powershell
cd examples
make server
```

The catalog currently contains:

- [`examples/snake.c`](examples/snake.c): a complete Snake game demonstrating
  shapes, gradients, custom fonts, procedural sound, keyboard input, timing,
  random values, collision logic, a 2D camera, render textures, scissor
  rectangles, blend modes, texture drawing, and animated shader uniforms.
- [`examples/basic_3d.c`](examples/basic_3d.c): the shared native/browser depth
  buffer, perspective camera, 3D primitives, wireframes, grid drawing, and
  mouse-ray box picking.

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
- Perspective and orthographic 3D cameras, depth buffering, screen/world rays,
  world-to-screen projection, cubes, spheres, cylinders, planes, grids, 3D
  lines/triangles, wireframes, and basic 3D collision queries.
- Raylib-compatible `Mesh`, `Material`, and `Model` structures; mesh upload and
  updates, solid/wire/point model drawing, instancing, mesh/model bounds, mesh
  ray collisions, and plane/cube/sphere mesh generators.
- Dependency-free glTF 2.0 (`.gltf` and `.glb`) loading with external, data-URI,
  and embedded buffers/textures; flattened node transforms and instances;
  positions, normals, tangents, two UV sets, vertex colors, and 8/16/32-bit
  indices; metallic/roughness, normal, occlusion, and emissive material maps;
  and one-armature skeletal animation with step, linear, and cubic channels.
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
| 3 | `f32` | normalized depth; optional for custom vertex shaders |

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
that location.

For stable, validated names, declare each location in a WGSL comment:

```wgsl
// @raygpu_uniform tint 0
// @raygpu_uniform time 1
```

Then ordinary raylib-style code resolves those declared names:

```c
int tintLoc = GetShaderLocation(shader, "tint");  // 0
int timeLoc = GetShaderLocation(shader, "time");  // 1
int typoLoc = GetShaderLocation(shader, "tiem");  // -1
```

Declarations can appear in either shader stage and do not depend on the order
of `GetShaderLocation()` calls. If a shader contains no declarations, RayGPU
keeps the earlier compatibility behavior and assigns locations in first-use
order. New shaders should use declarations so misspellings are detected and C
and WGSL stay synchronized. See the shader in [`main.c`](main.c) or
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

## Remaining 3D work

- Persistent GPU mesh buffers, mesh downloads, tangent generation, and the
  remaining procedural mesh generators.
- OBJ loading, retained node/scene hierarchies, sparse accessors, morph targets,
  and compressed mesh extensions.
- Lighting, fog, full PBR map shading, and additional samplers.
- Textured 3D primitives, billboards, heightmaps, cubic maps, and skyboxes.
- GPU skinning and support for multiple armatures.
- The remaining mesh, model, and ray collision helpers.

## Current limits

- Native rendering currently supports Windows through Dawn/D3D12.
- Images and ordinary textures use RGBA8 internally.
- Like raylib 5.5, glTF loading accepts triangle primitives, flattens node
  transforms into mesh data, ignores scene selection, uses one armature and
  four joints per vertex, and stores indices as 16-bit values. Morph targets,
  sparse accessors, Draco/meshopt compression, and extended PBR materials are
  not supported.
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
