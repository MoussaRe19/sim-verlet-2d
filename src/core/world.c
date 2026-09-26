#include "world.h"
#include "world_internal.h"
#include "world_sim.h"

size_t world_arena_size_required(size_t max_particles, size_t max_constraints) {
	size_t p_cap = max_particles > 0 ? max_particles : WORLD_DEFAULT_CAPACITY;
	size_t c_cap =
		max_constraints > 0 ? max_constraints : WORLD_DEFAULT_CAPACITY;

	return sizeof(World) + (p_cap * sizeof(Particle)) +
		   (c_cap * sizeof(Constraint)) + KB(1);
}

World* world_create(Arena* arena, size_t max_particles, size_t max_constraints,
					Vector4 bounds) {

	if (max_particles > WORLD_MAX_CAPACITY) max_particles = WORLD_MAX_CAPACITY;
	if (max_constraints > WORLD_MAX_CAPACITY)
		max_constraints = WORLD_MAX_CAPACITY;

	size_t p_cap = max_particles > 0 ? max_particles : WORLD_DEFAULT_CAPACITY;
	size_t c_cap =
		max_constraints > 0 ? max_constraints : WORLD_DEFAULT_CAPACITY;

	World* w = ARENA_PUSH_TYPE(arena, World);
	if (!w) return NULL;

	w->particles = ARENA_PUSH_ARRAY(arena, Particle, p_cap);
	w->constraints = ARENA_PUSH_ARRAY(arena, Constraint, c_cap);
	if (!w->particles || !w->constraints) return NULL;

	w->particle_count = 0;
	w->particle_capacity = p_cap;
	w->constraint_count = 0;
	w->constraint_capacity = c_cap;

	w->gravity = (Vector2){0.0f, 980.0f};
	w->bounds = bounds;
	w->sub_iterations = 8;
	w->damping = 1.0f; // No by default
	w->bounce = 0.9f;  // Default

	return w;
}

uint16_t world_add_particle(World* w, Vector2 position, bool pinned) {
	if (w->particle_count >= w->particle_capacity ||
		w->particle_count >= WORLD_MAX_CAPACITY) {
		return WORLD_INVALID_INDEX;
	}

	uint16_t  idx = (uint16_t)w->particle_count++;
	Particle* p = &w->particles[idx];
	p->pos = position;
	p->prev_pos = position;
	p->acc = Vector2Zero();
	p->drag_target = Vector2Zero();
	p->pinned = pinned;
	p->dragging = false;
	p->owner_id = WORLD_UNOWNED;

	return idx;
}

uint16_t world_add_constraint(World* w, uint16_t p0_index, uint16_t p1_index,
							  float rest_length, bool hidden) {
	if (p0_index >= w->particle_count || p1_index >= w->particle_count)
		return UINT16_MAX;
	if (w->constraint_count >= w->constraint_capacity) return UINT16_MAX;

	uint16_t    idx = (uint16_t)w->constraint_count++;
	Constraint* c = &w->constraints[idx];
	c->p0_index = p0_index;
	c->p1_index = p1_index;
	c->rest_length = (rest_length > 0.0f)
						 ? rest_length
						 : Vector2Distance(w->particles[p0_index].pos,
										   w->particles[p1_index].pos);
	c->hidden = hidden;

	return idx;
}

size_t world_particle_count(World* w) {
	return w->particle_count;
}

size_t world_constraint_count(World* w) {
	return w->constraint_count;
}

Vector4 world_bounds(World *w) {
    return w ? w->bounds : Vector4Zero();
}

Vector2 world_particle_position(const World* w, uint16_t index) {
	return w->particles[index].pos;
}

bool world_particle_pinned(const World* w, uint16_t index) {
	return w->particles[index].pinned;
}

uint8_t world_particle_owner(const World* w, uint16_t index) {
	return w->particles[index].owner_id;
}

void world_constraint_endpoints(const World* w, uint16_t index, Vector2* out_a,
								Vector2* out_b) {
	const Constraint* c = &w->constraints[index];

	*out_a = w->particles[c->p0_index].pos;
	*out_b = w->particles[c->p1_index].pos;
}

void world_set_gravity(World* w, Vector2 gravity) {
	w->gravity = gravity;
}

void world_set_bounds(World* w, Vector4 bounds) {
	w->bounds = bounds;
}
void world_set_sub_iterations(World* w, int iterations) {
	w->sub_iterations = iterations;
}
void world_set_damping(World* w, float damping) {
	w->damping = damping;
}

void world_set_bounce(World* w, float bounce) {
	w->bounce = bounce;
}

void world_set_particle_prev_position(World* w, uint16_t index,
									  Vector2 old_pos) {
	if (index >= w->particle_count) return;
	Particle* p = &w->particles[index];
	p->prev_pos = old_pos;
}

bool world_try_claim_particle(World* w, uint16_t idx, uint8_t client_id) {
	if (idx >= w->particle_count) return false;
	Particle* p = &w->particles[idx];
	if (p->owner_id != WORLD_UNOWNED) return false;
	p->owner_id = client_id;
	// Start dragging now; hold position until the first mouse target update.
	p->dragging = true;
	p->drag_target = p->pos;
	return true;
}

void world_release_particle(World* w, uint16_t idx, uint8_t client_id) {
	if (idx >= w->particle_count) return;
	Particle* p = &w->particles[idx];
	if (p->owner_id == client_id) {
		p->owner_id = WORLD_UNOWNED;
		p->dragging = false;
	}
}

void world_set_drag_target(World* w, uint16_t idx, float x, float y) {
	if (idx >= w->particle_count) return;
	w->particles[idx].drag_target = (Vector2){x, y};
}

void world_toggle_pin(World* w, uint16_t idx) {
	if (idx >= w->particle_count) return;
	w->particles[idx].pinned = !w->particles[idx].pinned;
}

void world_copy_positions(const World* w, uint16_t start, uint16_t count,
						  float* out_xs, float* out_ys) {
	for (uint16_t i = 0; i < count; i++) {
		Vector2 pos = w->particles[start + i].pos;
		out_xs[i] = pos.x;
		out_ys[i] = pos.y;
	}
}

void world_apply_force(World* w, uint16_t index, Vector2 force) {
	if (index >= w->particle_count) return;
	Particle* p = &w->particles[index];
	if (p->pinned || p->dragging) return;

	p->acc.x += force.x;
	p->acc.y += force.y;
}

void world_apply_force_all(World* w, Vector2 force) {
	for (uint16_t i = 0; i < w->particle_count; i++) {
		world_apply_force(w, i, force);
	}
}

bool world_constraint_hidden(const World* w, uint16_t index) {
	return w->constraints[index].hidden;
}

// --- client only -----------------------
bool world_set_particle_position(World* w, uint16_t index, Vector2 position) {
    if (index >= w->particle_count) return false;
    w->particles[index].pos = position;
    return true;
}

void world_set_particle_owner_unchecked(World* w, uint16_t index, uint8_t owner_id) {
    if (index >= w->particle_count) return;
    w->particles[index].owner_id = owner_id;
}
