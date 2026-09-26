#ifndef WORLD_SIM
#define WORLD_SIM

#include "world.h"

bool world_try_claim_particle(World* w, uint16_t idx, uint8_t client_id);
void world_release_particle(World* w, uint16_t idx, uint8_t client_id);
void world_set_drag_target(World* w, uint16_t idx, float x, float y);
void world_toggle_pin(World* w, uint16_t idx);

void world_apply_force(World* world, uint16_t index, Vector2 force);
void world_apply_force_all(World* world, Vector2 force);

#endif
