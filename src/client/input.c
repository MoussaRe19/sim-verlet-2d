#include "client/input.h"
#include <float.h>
#include <math.h>

static bool find_nearest_particle_impl(World* w, Vector2 point, float max_dist,
                                       bool ignore_pinned, uint16_t* out_index) {
    float best_dist_sq = max_dist * max_dist;
    uint16_t best_index = 0;
    bool found = false;

    size_t count = world_particle_count(w);
    for (uint16_t i = 0; i < (uint16_t)count; i++) {
        if (ignore_pinned && world_particle_pinned(w, i)) continue;

        Vector2 pos = world_particle_position(w, i);
        float dx = pos.x - point.x;
        float dy = pos.y - point.y;
        float dist_sq = dx * dx + dy * dy;

        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_index = i;
            found = true;
        }
    }

    if (found) *out_index = best_index;
    return found;
}
bool input_find_nearest_particle(World* w, Vector2 point, float max_dist,
								 uint16_t* out_index) {
	return find_nearest_particle_impl(w, point, max_dist, false, out_index);
}

bool input_find_nearest_draggable(World* w, Vector2 point, float max_dist,
								  uint16_t* out_index) {
	return find_nearest_particle_impl(w, point, max_dist, true, out_index);
}

void input_update(ClientContext* ctx, World* w, double dt) {
	Vector2 mouse = GetMousePosition();

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		uint16_t idx;
		if (input_find_nearest_draggable(w, mouse, INPUT_GRAB_RADIUS, &idx)) {
			net_client_send_intent_grab(ctx, idx, mouse.x, mouse.y);
		}
	}

	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ctx->held_particle_index >= 0) {
		net_client_update_drag(ctx, dt, mouse.x, mouse.y);
	}

	if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
		ctx->held_particle_index >= 0) {
		net_client_send_intent_release(ctx, (uint16_t)ctx->held_particle_index);
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
		uint16_t idx;
		if (input_find_nearest_particle(w, mouse, INPUT_GRAB_RADIUS, &idx)) {
			net_client_send_intent_toggle_pin(ctx, idx);
		}
	}
}
