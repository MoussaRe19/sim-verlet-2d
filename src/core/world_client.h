#ifndef WORLD_CLIENT_H
#define WORLD_CLIENT_H

#include "world.h"

bool world_set_particle_position(World* w, uint16_t index, Vector2 position);

void world_set_particle_owner_unchecked(World* world, uint16_t index,
										uint8_t owner_id);

#endif
