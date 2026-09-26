#include "io/model_loader.h"
#include "core/world_client.h"
#include "core/memory_arena.h"
#include "client/renderer.h"

struct RenderWorld {
	World* world;

	uint8_t  roster[WORLD_MAX_PLAYERS];
	uint16_t roster_count;

	uint32_t last_applied_tick;
};

size_t render_world_arena_size_required(size_t max_particles,
										size_t max_constraints) {
	return sizeof(RenderWorld) +
		   world_arena_size_required(max_particles, max_constraints);
}

RenderWorld* render_world_create(Arena* arena, size_t max_particles,
								 size_t max_constraints, Vector4 bounds) {
	if (!arena) return NULL;

	RenderWorld* rw = ARENA_PUSH_TYPE(arena, RenderWorld);
	if (!rw) return NULL;

	rw->world = world_create(arena, max_particles, max_constraints, bounds);
	if (!rw->world) return NULL;

	rw->roster_count = 0;
	memset(rw->roster, 0, sizeof(rw->roster));
	rw->last_applied_tick = 0;

	return rw;
}

bool render_world_load_json(RenderWorld* rw, const char* filepath) {
	if (!rw) return false;
	return world_load_json(rw->world, filepath);
}

World* render_world_get_world(const RenderWorld* rw) {
	return rw ? rw->world : NULL;
}

Vector4 render_world_get_bounds(RenderWorld *rw) {
    return rw ? world_bounds(rw->world) : Vector4Zero();
}

void render_world_add_player(RenderWorld* rw, uint8_t client_id) {
	if (!rw) return;
	for (uint16_t i = 0; i < rw->roster_count; i++) {
		if (rw->roster[i] == client_id) return;
	}
	if (rw->roster_count < WORLD_MAX_PLAYERS) {
		rw->roster[rw->roster_count++] = client_id;
	}
}

void render_world_remove_player(RenderWorld* rw, uint8_t client_id) {
	if (!rw) return;
	for (uint16_t i = 0; i < rw->roster_count; i++) {
		if (rw->roster[i] == client_id) {
			rw->roster[i] = rw->roster[--rw->roster_count];
			return;
		}
	}
}

void render_world_apply_snapshot(RenderWorld* rw, const float* xs,
								 const float* ys, uint16_t count) {
	render_world_apply_snapshot_chunk(rw, 0, count, xs, ys);
}

void render_world_apply_snapshot_chunk(RenderWorld* rw, uint16_t start_index,
									   uint16_t count, const float* xs,
									   const float* ys) {
	if (!rw || !rw->world || !xs || !ys) return;

	size_t total = world_particle_count(rw->world);
	for (uint16_t i = 0; i < count && (start_index + i) < total; i++) {
		world_set_particle_position(rw->world, start_index + i,
									(Vector2){xs[i], ys[i]});
	}
}

uint32_t render_world_get_last_applied_tick(const RenderWorld* rw) {
	return rw ? rw->last_applied_tick : 0;
}

void render_world_set_last_applied_tick(RenderWorld* rw, uint32_t tick) {
	if (rw) rw->last_applied_tick = tick;
}

void render_world_set_particle_owner(RenderWorld* rw, uint16_t particle_index,
									 uint8_t owner_id) {
	world_set_particle_owner_unchecked(rw->world, particle_index, owner_id);
}

// --- drawing -------------------------------------------------------------

RenderConfig renderer_default_config(void) {
	return (RenderConfig){
		.particle_color = WHITE,
		.pinned_color = RED,
		.held_by_me_color = GREEN,
		.held_by_other_color = ORANGE,
		.constraint_color = GRAY,
		.particle_radius = 4.0f,
		.line_thickness = 1.5f,
		.draw_hidden_constraints = false,
	};
}

static Color particle_color_for(const World* w, uint16_t idx,
								uint8_t             local_client_id,
								const RenderConfig* cfg) {
	if (world_particle_pinned(w, idx)) return cfg->pinned_color;

	uint8_t owner = world_particle_owner(w, idx);
	if (owner == WORLD_UNOWNED) return cfg->particle_color;
	return (owner == local_client_id) ? cfg->held_by_me_color
									  : cfg->held_by_other_color;
}

void renderer_draw_world(const RenderWorld* rw, const RenderConfig* cfg,
						 uint8_t local_client_id) {
	if (!rw || !cfg) return;
	World* w = render_world_get_world(rw);
	if (!w) return;

	size_t ccount = world_constraint_count(w);
	for (size_t i = 0; i < ccount; i++) {
		if (world_constraint_hidden(w, (uint16_t)i) &&
			!cfg->draw_hidden_constraints)
			continue;
		Vector2 a, b;
		world_constraint_endpoints(w, (uint16_t)i, &a, &b);
		DrawLineEx(a, b, cfg->line_thickness, cfg->constraint_color);
	}

	size_t pcount = world_particle_count(w);
	for (size_t i = 0; i < pcount; i++) {
		Vector2 pos = world_particle_position(w, (uint16_t)i);
		Color color = particle_color_for(w, (uint16_t)i, local_client_id, cfg);
		DrawCircleV(pos, cfg->particle_radius, color);
	}
}
