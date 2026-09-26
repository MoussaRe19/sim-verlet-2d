#ifndef INPUT_H
#define INPUT_H

#include "core/world.h"
#include "net/net_client.h"

#define INPUT_GRAB_RADIUS 14.0f

bool input_find_nearest_draggable(World* w, Vector2 point, float max_dist,
								  uint16_t* out_index);

bool input_find_nearest_particle(World* w, Vector2 point, float max_dist,
								 uint16_t* out_index);

void input_update(ClientContext* ctx, World* w, double dt);

#endif
