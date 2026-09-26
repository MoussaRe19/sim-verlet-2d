#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include "core/world.h"
#include "client/renderer.h"

bool world_load_json(World* w, const char* filepath);
bool render_world_load_json(RenderWorld* rw, const char* filepath);

#endif
