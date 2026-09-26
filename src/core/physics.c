#include "physics.h"
#include "world_internal.h"
#include "world_sim.h"

static void apply_global_forces(World* w) {
	world_apply_force_all(w, w->gravity);
}

static void verlet_integrate_particle(Particle* p, float damping, float dt_sq) {
	Vector2 accel = p->acc;
	p->acc = Vector2Zero();

	if (p->pinned) return;

	if (p->dragging) {
		p->prev_pos = p->pos;
		p->pos = p->drag_target;
		return;
	}

	Vector2 velocity = {
		(p->pos.x - p->prev_pos.x) * damping,
		(p->pos.y - p->prev_pos.y) * damping,
	};

	Vector2 new_pos = {
		p->pos.x + velocity.x + accel.x * dt_sq,
		p->pos.y + velocity.y + accel.y * dt_sq,
	};

	p->prev_pos = p->pos;
	p->pos = new_pos;
}

static void integrate(World* w, float dt) {
	float dt_sq = dt * dt;
	for (uint16_t i = 0; i < w->particle_count; i++) {
		verlet_integrate_particle(&w->particles[i], w->damping, dt_sq);
	}
}

static void relax_constraints_pass(World* w) {

	for (uint16_t i = 0; i < w->constraint_count; i++) {
		Constraint* c = &w->constraints[i];
		Particle*   p0 = &w->particles[c->p0_index];
		Particle*   p1 = &w->particles[c->p1_index];

		bool p0_locked = p0->pinned || p0->dragging;
		bool p1_locked = p1->pinned || p1->dragging;
		if (p0_locked && p1_locked) continue;

		Vector2 delta = Vector2Subtract(p1->pos, p0->pos);
		float   dist = Vector2Length(delta);
		if (dist < 1e-6f) continue;

		float diff = (dist - c->rest_length) / dist;

			// TODO: inv_mass (w = 1/m, 0 for pinned) replaces
			// p0_locked/p1_locked — constraint correction should split by
			// relative mass, not a hard on/off switch.
		if (p0_locked) {
			p1->pos = Vector2Subtract(p1->pos, Vector2Scale(delta, diff));
		} else if (p1_locked) {
			p0->pos = Vector2Add(p0->pos, Vector2Scale(delta, diff));
		} else {
			Vector2 correction = Vector2Scale(delta, diff * 0.5f);
			p1->pos = Vector2Subtract(p1->pos, correction);
			p0->pos = Vector2Add(p0->pos, correction);
		}
	}
}

static void bounds_pass(World* w) {
	Vector4 b = w->bounds;
	for (uint16_t i = 0; i < w->particle_count; i++) {
		Particle* p = &w->particles[i];
		if (p->pinned || p->dragging) continue;

		Vector2 vel = Vector2Subtract(p->pos, p->prev_pos);

		if (p->pos.x > b.z) {
			p->pos.x = b.z;
			p->prev_pos.x = p->pos.x + vel.x * w->bounce;
		} else if (p->pos.x < b.x) {
			p->pos.x = b.x;
			p->prev_pos.x = p->pos.x + vel.x * w->bounce;
		}

		if (p->pos.y > b.w) {
			p->pos.y = b.w;
			p->prev_pos.y = p->pos.y + vel.y * w->bounce;
		} else if (p->pos.y < b.y) {
			p->pos.y = b.y;
			p->prev_pos.y = p->pos.y + vel.y * w->bounce;
		}
	}
}

void physics_step(World* w, float dt) {
	apply_global_forces(w);
	integrate(w, dt);

	int iterations = (w->sub_iterations > 0) ? w->sub_iterations : 1;
	for (int i = 0; i < iterations; i++) {
		relax_constraints_pass(w);
		bounds_pass(w);
	}
}
