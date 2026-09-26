#ifndef RENDERER_H
#define RENDERER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "core/memory_arena.h"
#include "utils/common.h"
#include "utils/vec2.h"
#include "core/world.h"

typedef struct RenderWorld RenderWorld;

size_t       render_world_arena_size_required(size_t max_particles,
											  size_t max_constraints);
RenderWorld* render_world_create(Arena* arena, size_t max_particle,
								 size_t max_constraints, Vector4 bounds);

World*  render_world_get_world(const RenderWorld* rw);
Vector4 render_world_get_bounds(RenderWorld* rw);

void render_world_apply_snapshot(RenderWorld* rw, const float* xs,
								 const float* ys, uint16_t count);

void render_world_apply_snapshot_chunk(RenderWorld* rw, uint16_t start_index,
									   uint16_t count, const float* xs,
									   const float* ys);

uint32_t render_world_get_last_applied_tick(const RenderWorld* rw);
void     render_world_set_last_applied_tick(RenderWorld* rw, uint32_t tick);

void render_world_add_player(RenderWorld* rw, uint8_t client_id);
void render_world_remove_player(RenderWorld* rw, uint8_t client_id);
void render_world_set_particle_owner(RenderWorld* rw, uint16_t particle_index,
									 uint8_t owner_id);

typedef struct RenderConfig {
	Color particle_color;
	Color pinned_color;
	Color held_by_me_color;
	Color held_by_other_color;
	Color constraint_color;
	float particle_radius;
	float line_thickness;
	bool  draw_hidden_constraints; // debug toggle
} RenderConfig;

RenderConfig renderer_default_config(void);

void renderer_draw_world(const RenderWorld* rw, const RenderConfig* cfg,
						 uint8_t local_client_id);

#endif
