# RayGPU example catalog

The examples build together into one interactive application. `main.c` owns
the window and catalog screen; every demo keeps its own state and drawing code
in a separate C file.

Current catalog entries:

- `snake.c`: shapes, procedural sound, keyboard input, a 2D camera, render
  textures, scissor rectangles, blend modes, and a custom WGSL shader.
- `basic_3d.c`: perspective projection, depth buffering, solid and wireframe
  primitives, a grid, mouse-ray picking, `.gltf`/`.glb` model loading, and
  automatic playback when a model contains skeletal animation.

Build and open the native catalog:

```powershell
cd examples
make run
```

Build and open the same catalog in a browser:

```powershell
cd examples
make server
```

Click a card or press its number to open an example. Press `Escape` or click
`EXAMPLES` to return to the catalog without closing the application.
