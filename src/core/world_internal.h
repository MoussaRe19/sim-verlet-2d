#ifndef WORLD_INTERNAL
#define WORLD_INTERNAL

#include "world.h"

typedef struct Particle {
	Vector2 pos;
	Vector2 prev_pos;
	Vector2 acc;
	Vector2 drag_target;
	bool    pinned;
	bool    dragging;
	uint8_t owner_id;
} Particle;

typedef struct Constraint {
	uint16_t p0_index;
	uint16_t p1_index;
	float    rest_length;
	bool     hidden;
} Constraint;

struct World {
	Particle* particles;
	size_t    particle_count;
	size_t    particle_capacity;

	Constraint* constraints;
	size_t      constraint_count;
	size_t      constraint_capacity;

	Vector2 gravity;
	Vector4 bounds;
	int     sub_iterations;
	float   damping;
	float   bounce;
};

#endif
