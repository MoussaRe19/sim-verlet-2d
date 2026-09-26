#ifndef WORLD_H
#define WORLD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "utils/vec2.h"
#include "core/memory_arena.h"

#define WORLD_MAX_PLAYERS 16
#define WORLD_UNOWNED 0xFFu
#define WORLD_INVALID_INDEX UINT16_MAX
#define WORLD_MAX_CAPACITY (UINT16_MAX - 1u)
#define WORLD_DEFAULT_CAPACITY 16

typedef struct World World;

size_t world_arena_size_required(size_t max_particles, size_t max_constraints);
World* world_create(Arena* arena, size_t max_particles, size_t max_constraints,
					Vector4 bounds);

uint16_t world_add_particle(World* world, Vector2 position, bool pinned);
uint16_t world_add_constraint(World* world, uint16_t p0_index,
							  uint16_t p1_index, float rest_length,
							  bool hidden);
size_t   world_particle_count(World* world);
size_t   world_constraint_count(World* world);
Vector4  world_bounds(World* world);
Vector2  world_particle_position(const World* world, uint16_t index);
bool     world_particle_pinned(const World* world, uint16_t index);
uint8_t  world_particle_owner(const World* world, uint16_t index);
void     world_constraint_endpoints(const World* world, uint16_t index,
									Vector2* out_a, Vector2* out_b);
bool     world_constraint_hidden(const World* w, uint16_t index);

void world_set_gravity(World* world, Vector2 gravity);
void world_set_bounds(World* world, Vector4 bounds);
void world_set_sub_iterations(World* world, int iterations);
void world_set_damping(World* world, float damping);
void world_set_bounce(World* world, float bounce);
void world_set_particle_prev_position(World* world, uint16_t index,
									  Vector2 old_pos);

// Write / Interaction Accessors

void world_copy_positions(const World* world, uint16_t start, uint16_t count,
						  float* out_xs, float* out_ys);

#endif
