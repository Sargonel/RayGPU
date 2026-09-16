/* RayGPU implementation entry point. Compile once alongside your application.
 * Copyright (c) 2026 Sargonel. Distributed under the zlib license.
 * Modules share private state within this translation unit. */
#include "raygpu_internal.h"
#include "raygpu_core.c"
#include "raygpu_audio.c"
#include "raygpu_textures.c"
#include "raygpu_image_formats.c"
#include "raygpu_models.c"
#include "raygpu_model_formats.c"
#include "raygpu_shapes.c"
#include "raygpu_text.c"
#include "raygpu_renderer.c"
