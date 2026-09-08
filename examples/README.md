# RayGPU examples

## Snake

`snake.c` is a complete asset-free game. It uses generated shapes, procedural
sound, keyboard input, timing, random values, collision logic, a 2D camera,
render textures, scissor rectangles, blend modes, texture drawing, and a WGSL
shader with animated uniforms.

If `assets/font.ttf` exists, the game uses it for the interface. It falls back
to RayGPU's built-in font when the file is absent.

Run it from the examples folder on Windows:

```powershell
cd examples
make run
```

Run the same source in the browser:

```powershell
cd examples
make server
```
