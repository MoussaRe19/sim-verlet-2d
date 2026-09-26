#include "model_loader.h"
#include "yyjson/yyjson.h"

static inline float json_float(yyjson_val* obj, const char* key,
							   float fallback) {
	yyjson_val* val = yyjson_obj_get(obj, key);
	return (val && yyjson_is_num(val)) ? (float)yyjson_get_num(val) : fallback;
}

static inline bool json_bool(yyjson_val* obj, const char* key, bool fallback) {
	yyjson_val* val = yyjson_obj_get(obj, key);
	return val ? yyjson_get_bool(val) : fallback;
}

static inline bool json_index(yyjson_val* obj, const char* key, uint16_t* out) {
	yyjson_val* val = yyjson_obj_get(obj, key);
	if (!val || !yyjson_is_num(val)) return false;
	uint64_t n = yyjson_get_uint(val);
	if (n > UINT16_MAX) return false;
	*out = (uint16_t)n;
	return true;
}

bool world_load_json(World* world, const char* filepath) {
	yyjson_read_err err;
	yyjson_doc*     doc = yyjson_read_file(filepath, 0, NULL, &err);
	if (!doc) {
		fprintf(stderr, "[JSON] %s: %s (pos %zu)\n", filepath, err.msg,
				err.pos);
		return false;
	}

	yyjson_val* root = yyjson_doc_get_root(doc);
	if (!yyjson_is_obj(root)) {
		fprintf(stderr, "[JSON] %s: root is not an object\n", filepath);
		yyjson_doc_free(doc);
		return false;
	}

	bool success = false;

	yyjson_val* gravity = yyjson_obj_get(root, "gravity");
	if (gravity) {
		world_set_gravity(world, (Vector2){
									 json_float(gravity, "x", 0.0f),
									 json_float(gravity, "y", 980.0f),
								 });
	}

	yyjson_val* bounds = yyjson_obj_get(root, "bounds");
	if (bounds) {
		yyjson_val* min_x = yyjson_obj_get(bounds, "x");
		yyjson_val* min_y = yyjson_obj_get(bounds, "y");
		yyjson_val* max_x = yyjson_obj_get(bounds, "z");
		yyjson_val* max_y = yyjson_obj_get(bounds, "w");

		if (min_x && min_y && max_x && max_y) {
			world_set_bounds(world, (Vector4){
										(float)yyjson_get_num(min_x),
										(float)yyjson_get_num(min_y),
										(float)yyjson_get_num(max_x),
										(float)yyjson_get_num(max_y),
									});
		} else {
			fprintf(stderr, "[JSON] %s: incomplete bounds, ignoring override\n",
					filepath);
		}
	}

	if (yyjson_obj_get(root, "damping"))
		world_set_damping(world, json_float(root, "damping", 1.0f));

	if (yyjson_obj_get(root, "bounce"))
		world_set_bounce(world, json_float(root, "bounce", 0.9f));

	if (yyjson_obj_get(root, "sub_iterations"))
		world_set_sub_iterations(world,
								 (int)json_float(root, "sub_iterations", 8));

	yyjson_val* points = yyjson_obj_get(root, "points");
	size_t      index, count;
	yyjson_val* point;
	yyjson_arr_foreach(points, index, count, point) {
		yyjson_val* x_val = yyjson_obj_get(point, "x");
		yyjson_val* y_val = yyjson_obj_get(point, "y");
		if (!x_val || !y_val) {
			fprintf(stderr, "[JSON] %s: point %zu missing x/y, aborting load\n",
					filepath, index);
			goto cleanup;
		}

		Vector2 position = {(float)yyjson_get_num(x_val),
							(float)yyjson_get_num(y_val)};
		float   old_x = json_float(point, "oldx", position.x);
		float   old_y = json_float(point, "oldy", position.y);
		bool    pinned = json_bool(point, "pinned", false);

		uint16_t particle_id = world_add_particle(world, position, pinned);
		if (particle_id == WORLD_INVALID_INDEX) {
			fprintf(stderr, "[JSON] %s: particle capacity exceeded at %zu\n",
					filepath, index);
			goto cleanup;
		}

		if (old_x != position.x || old_y != position.y) {
			world_set_particle_prev_position(world, particle_id,
											 (Vector2){old_x, old_y});
		}
	}

	yyjson_val* sticks = yyjson_obj_get(root, "sticks");
	yyjson_val* stick;
	yyjson_arr_foreach(sticks, index, count, stick) {
		uint16_t p0, p1;
		if (!json_index(stick, "p0", &p0) || !json_index(stick, "p1", &p1)) {
			fprintf(stderr,
					"[JSON] %s: stick %zu missing/invalid p0 or p1, skipping\n",
					filepath, index);
			continue;
		}

		if (p0 >= world_particle_count(world) ||
			p1 >= world_particle_count(world)) {
			fprintf(stderr,
					"[JSON] %s: stick %zu out-of-range point (%u, %u)\n",
					filepath, index, p0, p1);
			continue;
		}

		float rest_length = json_float(stick, "length", 0.0f);
		bool  hidden = json_bool(stick, "hidden", false);

		uint16_t constraint_id =
			world_add_constraint(world, p0, p1, rest_length, hidden);
		if (constraint_id == UINT16_MAX) {
			fprintf(stderr, "[JSON] %s: constraint capacity exceeded at %zu\n",
					filepath, index);
			goto cleanup;
		}
	}

	success = true;

cleanup:
	yyjson_doc_free(doc);
	return success;
}
