# SarGPU

SarGPU is an independent, modular 2D/3D C library inspired by raylib and
built around WebGPU. The same C source can compile as a native Windows program
or as freestanding WebAssembly for a browser.

**[Run the example catalog in your browser](https://sargonel.github.io/SarGPU/)**

The web build does **not** use Emscripten, WASI, Node.js, or a package manager.
Native Windows rendering uses Dawn with D3D12. Browser rendering uses WebGPU
through the small `sargpu.js` platform bridge.

Its familiar C API covers windowing, input, 2D drawing, audio, images, fonts,
shaders, meshes, glTF 2.0 models, skeletal animation, configurable lighting,
fog, PBR materials, and skyboxes. VR and stereo rendering are outside its scope.

## Quick start

Include the public header in your game code. The makefiles compile `src/sargpu.c` separately; no implementation macro is needed:

```c
#include "sargpu.h"

static void GameFrame(void)
{
    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from SarGPU", 20, 20, 30, DARKBLUE);
        DrawCircle(400, 225, 50, SKYBLUE);
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
```

`RunMainLoop()` is required because native Windows uses a normal blocking loop,
while the browser uses `requestAnimationFrame()`.

Like raylib, native windows have a fixed size by default. Enable resizing before
creating the window:

```c
SetConfigFlags(FLAG_WINDOW_RESIZABLE);
InitWindow(800, 450, "Resizable SarGPU window");
```

## Source layout

`src/sargpu.h` is the public API. Add `src/` to the compiler include path and
compile `src/sargpu.c` once alongside your application. It assembles the core,
shapes, textures, text, audio, models, and renderer modules in one translation
unit so they can share private state. Do not compile the module `.c` files
individually or define `SARGPU_IMPLEMENTATION` in application code.

Third-party image/font code and its license notices live in `src/external/`.
The browser bridge is `src/sargpu.js`; web builds copy it beside `main.wasm`
and `index.html`. Keep the complete `src/` directory when using SarGPU.

## Requirements

Native Windows builds require:

- Clang with C17 support.
- Windows 10 or newer with a D3D12-capable GPU.

Install SarGPU's pinned Windows x64 Dawn SDK once, then build normally:

```powershell
.\setup-dawn.ps1
make run
```

The installer downloads
[`sargpu-dawn-sdk-windows-x64-be6033b.zip`](https://github.com/Sargonel/SarGPU/releases/download/dawn-sdk-be6033b/sargpu-dawn-sdk-windows-x64-be6033b.zip),
verifies the archive and library SHA-256 checksums, and extracts it to the
gitignored `.deps/dawn-sdk` directory. The package contains only the public and
generated headers, monolithic `webgpu_dawn.lib`, build metadata, and required
license notices. It is built from Dawn revision
`be6033b3c82301df3559d02f6b14671ba7b1ce08` for Windows x64 with Clang 22.1.8.

To use a manually installed compatible SDK, override the make variable:

```powershell
make run DAWN_SDK=C:/path/to/dawn-sdk
```

Developers rebuilding Dawn can create the same directory layout with
`include/dawn`, `include/webgpu`, and `lib/webgpu_dawn.lib`. The source and
generated headers must come from the same Dawn revision as the library.

Web builds require:

- Clang with the `wasm32` target.
- A browser with WebGPU support.
- HTTPS when deployed. `localhost` is allowed for development.

Dawn is only needed for native builds; browsers provide their own WebGPU
implementation and do not use the SDK.

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
  buffer, perspective camera, 3D primitives, generated meshes, billboards,
  wireframes, grid drawing, and mouse-ray box picking. Its small GLB model is stored in
  [`examples/assets/example.glb`](examples/assets/example.glb) and is published
  with the online catalog.

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
|-- sargpu.js
|-- main.wasm
`-- assets/
```

Upload the contents of `build/web` to any static host. The host must:

- Use HTTPS in production.
- Serve `main.wasm` with the `application/wasm` MIME type.
- Preserve the relative paths under `assets/`.

The server does not need Emscripten, Dawn, Node.js, PowerShell, or a C compiler.
Opening `index.html` through a `file://` URL is not supported.

The web bridge uses `window.devicePixelRatio` so one logical SarGPU pixel maps
to one physical display pixel when the canvas fits. If the browser viewport is
smaller, the canvas shrinks uniformly to the largest size that preserves its
aspect ratio. Browser zoom, window resizing, and monitor DPI changes update the
display size automatically.

Browser fullscreen uses the Fullscreen API and therefore follows the browser's
user-gesture rules. If a fullscreen request is rejected outside a gesture,
SarGPU retries it on the next key press or pointer press. Browsers expose one
current display through the synchronous monitor API; physical millimeter sizes
return zero when the browser does not provide them. Clipboard reads return the
latest text made available by a SarGPU write, the Clipboard API, or a paste
event. Dropped browser files receive virtual names that can be passed directly
to SarGPU file-loading functions.

## Supported features

- Window creation, resizing, DPI-aware dimensions, fullscreen and borderless
  modes, focus state, timing, FPS limiting, title/position/size controls,
  monitor queries and selection, window icons and opacity, clipboard text,
  dropped files, and native minimize/maximize/restore.
- Complete raylib keyboard constants; press, repeat, release, Unicode character
  queues, mouse buttons, position, delta, wheel, offset, scaling, cursor shapes,
  cursor visibility, and cursor locking.
- Four gamepad slots with raylib's standard button/axis layout, connection and
  name queries, per-frame button transitions, dual-motor timed vibration,
  up to ten simultaneous touch points, and tap, double-tap, hold, drag, swipe,
  and pinch gestures. Windows uses the system XInput DLL dynamically; browsers
  use the Gamepad, Pointer Events, Pointer Lock, and haptics APIs.
- Complete raylib 2D shapes, gradients, polygons, splines, 2D collisions, 2D
  cameras, and screen/world conversion.
- Perspective and orthographic 3D cameras, depth buffering, screen/world rays,
  world-to-screen projection, cubes, textured cubes, billboards, spheres,
  cylinders, planes, grids, 3D lines/triangles, wireframes, and basic 3D
  collision queries.
- Raylib-compatible `Mesh`, `Material`, and `Model` structures; mesh upload and
  updates, persistent WebGPU vertex/index buffers, solid/wire/point model
  drawing, real GPU instancing, mesh/model bounds, mesh ray collisions, and
  polygon, plane, cube, sphere, hemisphere, cylinder, cone, torus, knot,
  heightmap, and cubic-map mesh generators, tangent generation, OBJ export,
  and C header export.
- A dedicated GPU 3D pipeline with model/view/projection transforms in WGSL,
  hardware clipping, perspective-correct interpolation, depth testing, and GPU
  skeletal skinning. Materials support albedo, metallic, normal,
  metallic-roughness, occlusion, emission, and two custom texture slots.
  Cook-Torrance metallic/roughness PBR can be switched to diffuse lighting with
  `SetPBRMode()`. Scenes support eight directional, point, or spot lights,
  ambient light, linear/exponential fog, and tinted equirectangular skyboxes.
  The dynamic CPU triangle batch remains dedicated to 2D shapes, sprites, text,
  and lightweight debug geometry.
- Dependency-free glTF 2.0 (`.gltf` and `.glb`) loading with external, data-URI,
  and embedded buffers/textures; flattened node transforms and instances;
  positions, normals, tangents, two UV sets, vertex colors, and 8/16/32-bit
  indices; metallic/roughness, normal, occlusion, and emissive material maps;
  and one-armature skeletal animation with step, linear, and cubic channels.
- glTF sparse accessors (including zero-initialized bases), quantized attributes,
  default/explicit scene selection, retained position/normal/tangent morph
  targets, and step/linear/cubic morph-weight animations. `LoadModel()` loads
  the default scene; `LoadModelFromScene(path, index)` selects another scene.
  `SetMeshMorphWeights(mesh, weights, mesh.morphTargetCount)` changes weights;
  `UpdateModelMorphAnimation(model, animationIndex, timeInSeconds)` evaluates
  weight animation, including models without a skeleton. Skeletal
  `LoadModelAnimations()` / `UpdateModelAnimation()` also work for morph-only
  clips and evaluate matching weights alongside skeletal animation.
  `LoadModelAnimationsFromScene(path, index, &count)` matches an explicitly
  selected scene.
- `EXT_meshopt_compression` decoding for attributes, triangle indices, and
  index sequences, with octahedral, quaternion, and exponential filters.
- OBJ geometry with polygon triangulation, negative indices, groups, UVs,
  normals, and MTL libraries/textures. `LoadMaterials()` loads standalone MTL
  libraries; call `UnloadMaterial()` on each entry and `MemFree()` on the array.
- IQM v2 geometry and skeletal animations, including separate animation files;
  MagicaVoxel VOX v150/v200 colored surface meshes; and binary/compressed/ASCII
  M3D models, materials, embedded textures, bones, and interpolated animations.
  Large imported meshes are split into batches of at most 65,535 vertices.
- PNG, JPEG, BMP, TGA, GIF, QOI, and DDS decoding from files or memory, plus
  raw pixel loading. Animated GIF sequences are available through
  `LoadImageAnim()`/`LoadImageAnimFromMemory()`. DDS supports uncompressed
  RGBA, DXT1/BC1, DXT3/BC2, DXT5/BC3, BC4/ATI1, and BC5/ATI2, including DX10
  headers for the supported BC formats.
- Image creation, copying, cropping, resizing, flipping, arbitrary rotation,
  alpha/color processing, palettes, blur, convolution, dithering, channel
  extraction, CPU drawing, and procedural gradients/noise/cellular images.
- Image and texture mipmap generation; PNG, JPEG, BMP, TGA, QOI, and C-header
  image export; screenshots; texture readback; texture loading, partial/full
  updates, filtering, wrapping, source rectangles, scaling, rotation,
  nine-patch drawing, and three-patch drawing. `LoadImageFromTexture()` reads
  CPU-backed textures synchronously on every platform. Render textures and the
  screen use the portable callback APIs `LoadImageFromTextureAsync()` and
  `LoadImageFromScreenAsync()` because browser WebGPU readback is asynchronous.
  SarGPU queues callbacks on every platform, so they never run before the
  request function returns. The callback owns the returned `Image` and releases
  it with `UnloadImage()`. Browser screenshots download a PNG.

Async image callbacks are dispatched from SarGPU's main-loop processing during
`WindowShouldClose()` or `BeginDrawing()`. An application waiting for readback
must continue running one of those functions; a blocking loop that calls
neither cannot dispatch the callback. The callback owns `image.data`, including
an invalid image delivered after an asynchronous readback failure, and should
call `UnloadImage(image)` when finished:

```c
static void OnReadback(Image image, void *userData)
{
    (void)userData;
    if (IsImageValid(image)) ExportImage(image, "capture.png");
    UnloadImage(image);
}

LoadImageFromTextureAsync(target.texture, OnReadback, NULL);
```
- Render textures, alpha/additive/multiplied/color/premultiplied blend modes, and
  scissor rectangles.
- Custom WGSL vertex and fragment shaders with float, vector, integer, array,
  and matrix uniform uploads.
- Built-in text plus TTF/OTF and image-font loading, UTF-8 drawing, rotated
  text, measurement, and glyph lookup. `LoadFontData()` exposes bitmap or SDF
  glyph images, `GenImageFontAtlas()` packs them into an atlas, and
  `ExportFontAsCode()` exports atlas pixels, dimensions, rectangles, and glyph
  metrics. Release arrays returned by `LoadFontData()` with `UnloadFontData()`.
- WAV, OGG Vorbis, MP3, QOA, FLAC, XM, and MOD loading from files or memory;
  sound and music playback with pause, resume, seek, looping, volume, pitch,
  pan, and time queries; double-buffered music and procedural audio streams;
  stream and mixed processors; and WAV or C-header export. Music keeps its
  decoder state and incrementally decodes stream-sized chunks, so compressed
  tracks are not expanded into a full PCM copy at load time.
- Binary/text file loading and saving, path inspection, memory allocation,
  Base64 encoding/decoding, CRC32/MD5/SHA1 hashing, random-number utilities,
  and common color/text helpers. Browser saves use a normal file download.
- Growable WebAssembly memory: 64 MB initially, up to 2 GB or the browser's
  available limit.

Image decoding, encoding, and font rasterization use `stb_image` v2.30,
`stb_image_write`, `qoi.h`, and `stb_truetype`, bundled under `src/external/`
with their original public-domain/MIT licenses. Audio decoding uses bundled
`stb_vorbis`, `dr_mp3`, `dr_flac`, `qoa.h`, `jar_xm`, and `jar_mod` sources
under their original licenses. These are also used or bundled by raylib; users
do not install them separately.
M3D importing uses the bundled Model3D SDK in `src/external/m3d.h`, under its
original MIT license. All importers use SarGPU's file and memory APIs on native
and web builds; no additional installation is needed.
Meshopt decoding uses a scalar C17 port of meshoptimizer v0.22's decoders,
bundled in `src/external/meshopt_decode.h` with the original MIT license.

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

Custom shaders can bind up to `SARGPU_MAX_SHADER_TEXTURES` additional textures
through group 2. Each slot uses two bindings:

| Slot | Sampler binding | Texture binding |
|---:|---:|---:|
| 0 | 0 | 1 |
| 1 | 2 | 3 |
| ... | `slot*2` | `slot*2 + 1` |

Declare the pair in WGSL and give the texture a stable location name:

```wgsl
// @sargpu_sampler maskTexture 0
@group(2) @binding(0) var maskSampler: sampler;
@group(2) @binding(1) var maskTexture: texture_2d<f32>;
```

Then bind it with the raylib-style API:

```c
int maskLoc = GetShaderLocation(shader, "maskTexture");
SetShaderValueTexture(shader, maskLoc, mask);
```

SarGPU also reflects direct WGSL vertex parameters, so
`GetShaderLocationAttrib(shader, "position")` returns the number from
`@location(...)`. Shaders that pass a vertex-input structure can declare the
same metadata explicitly with `// @sargpu_attribute position 0`.

Optional uniforms use a 2048-byte buffer at group 1, binding 0. Declare it as:

```wgsl
struct Uniforms { values: array<vec4f, 128> };
@group(1) @binding(0) var<uniform> uniforms: Uniforms;
```

Uniform location `n` starts at `uniforms.values[n*4]`, reserving 64 bytes for
that location.

For stable, validated names, declare each location in a WGSL comment:

```wgsl
// @sargpu_uniform tint 0
// @sargpu_uniform time 1
```

Then ordinary raylib-style code resolves those declared names:

```c
int tintLoc = GetShaderLocation(shader, "tint");  // 0
int timeLoc = GetShaderLocation(shader, "time");  // 1
int typoLoc = GetShaderLocation(shader, "tiem");  // -1
```

Declarations can appear in either shader stage and do not depend on the order
of `GetShaderLocation()` calls. If a shader contains no declarations, SarGPU
keeps the earlier compatibility behavior and assigns locations in first-use
order. New shaders should use declarations so misspellings are detected and C
and WGSL stay synchronized. The sampler and uniform snippets above show the
complete binding contract.

### Custom 3D material shaders

Use `LoadMaterialShader()` or `LoadMaterialShaderFromMemory()` for a shader
assigned to `material.shader`. These shaders use the same uniform and named
sampler helpers as 2D shaders, but their vertex contract is fixed so meshes,
instancing, and GPU skinning remain portable:

| Locations | Value |
|---:|---|
| 0, 1, 2, 3 | position, normal, UV0, RGBA8 color |
| 4, 5 | four `u8` bone IDs and four `f32` bone weights |
| 6, 7 | tangent and UV1 |
| 8-11 | model matrix columns |
| 12, 13, 14 | tint, material parameters, emission color |
| 15 | `vec4u` skin data: bone offset, count, map flags, reserved |

The material texture pairs are group 2 slots 0 through 7: albedo, metallic,
normal, metallic-roughness, occlusion, emission, and two custom textures.
Group 1 binding 0 is the normal 2048-byte custom uniform buffer. Group 3
binding 0 contains the shared scene data (view-projection matrix, camera,
ambient light, fog, skybox camera basis, settings, and eight lights); binding 1
is the read-only bone-matrix storage buffer. Custom shaders must use `vs` and
`fs` entry points. `SetShaderValueTexture()` supplies either custom slot, or a
material map can populate it directly. `BeginShaderMode()` is for 2D shaders;
assign a material shader through `Material.shader`.

`DrawSkybox()` accepts a normal 2D equirectangular panorama. Call it between
`BeginMode3D()` and `EndMode3D()` before or after the scene geometry; SarGPU
always renders it behind the geometry.

## Remaining core, 2D, and audio work

- Directory listing, URL opening, compression, logging callbacks, and
  automation events.

## Remaining 3D work

- Draco (`KHR_draco_mesh_compression`) and newer compressed mesh extensions.

## Current limits

- Native rendering currently supports Windows through Dawn/D3D12.
- Native gamepads currently use XInput's standard four-controller layout.
  Browser gamepad vibration depends on the controller and browser exposing a
  haptics actuator. `SetGamepadMappings()` returns zero because XInput and the
  browser standard mapping do not accept SDL controller-database mappings.
- VOX loading uses voxel volumes and their palettes; scene-node transforms and
  animation are ignored, as in raylib's VOX loader. IQM/M3D skinning supports up
  to four influences per vertex and bone IDs that fit in one byte.
- Images and ordinary textures use RGBA8 internally.
- glTF loading accepts triangle primitives, flattens node transforms into mesh
  data, uses one armature per selected scene and four joints per vertex, and
  stores indices as 16-bit values. Required Draco compression and optional
  glTF material extensions are not supported. Meshopt supports the EXT codec
  versions used by meshoptimizer v0.22, rather than newer
  `KHR_meshopt_compression` codecs.
  Morph targets are evaluated on the CPU, with up to 256 targets per primitive.
  Appended morph fields extend the raylib-inspired mesh/model type layouts.
- `LoadImage()` returns the first GIF frame. `LoadImageAnim()` returns all
  frames as consecutive RGBA8 pixels; frame delays are discarded, like raylib.
  Upload a selected frame with `UpdateTexture(texture, (unsigned char *)image.data
  + (size_t)frame*image.width*image.height*4)` and free the complete sequence
  with `UnloadImage(image)`. Choose playback timing in your game code.
- The built-in font contains a small ASCII subset; use a TTF/OTF font for wider
  Unicode coverage.
- SarGPU supports 256 simultaneous texture slots, 32 custom shaders, eight
  material samplers, eight scene lights, 16,384 bone matrices per frame, and
  262,144 vertices per frame.

## License

SarGPU is distributed under the MIT license. See [`LICENSE`](LICENSE).
The stb, Model3D, and meshoptimizer licenses remain included in their headers
under `src/external/`.
