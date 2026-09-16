# Bundled external code

RayGPU vendors these source files so applications do not need separate image,
font, audio, or model-decoder installations. Each file retains its original
copyright and license notice.

| File | Upstream license |
|---|---|
| `stb_image.h`, `stb_image_write.h`, `stb_truetype.h`, `stb_vorbis.c` | Public domain or MIT |
| `qoi.h` | MIT |
| `qoa.h` | MIT |
| `dr_mp3.h`, `dr_flac.h` | Public domain or MIT-0 |
| `jar_xm.h`, `jar_mod.h` | WTFPL |
| `m3d.h` | MIT |
| `meshopt_decode.h` | MIT |

Small conditional-compilation changes let the decoders use RayGPU's allocator
and compile in its freestanding WebAssembly build. Those integration changes
do not replace or remove the upstream license notices.
