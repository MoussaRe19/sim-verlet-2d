#include "net_protocol.h"
#include <string.h>

void net_writer_init(NetWriter* w, uint8_t* buffer, size_t capacity) {
	w->buffer = buffer;
	w->capacity = capacity;
	w->cursor = 0;
}

void net_reader_init(NetReader* r, const uint8_t* buffer, size_t size) {
	r->buffer = buffer;
	r->size = size;
	r->cursor = 0;
}

// Serialization (Explicit Big-Endian / Network Byte Order)
bool net_write_u8(NetWriter* w, uint8_t val) {
	if (w->cursor + 1 > w->capacity) return false;
	w->buffer[w->cursor++] = val;
	return true;
}

bool net_write_u16(NetWriter* w, uint16_t val) {
	if (w->cursor + 2 > w->capacity) return false;
	w->buffer[w->cursor++] = (val >> 8) & 0xFF;
	w->buffer[w->cursor++] = val & 0xFF;
	return true;
}

bool net_write_u32(NetWriter* w, uint32_t val) {
    if (w->cursor + 4 > w->capacity) return false;
    w->buffer[w->cursor++] = (val >> 24) & 0xFF;
    w->buffer[w->cursor++] = (val >> 16) & 0xFF;
    w->buffer[w->cursor++] = (val >> 8) & 0xFF;
    w->buffer[w->cursor++] = val & 0xFF;
    return true;
}

bool net_write_f32(NetWriter* w, float val) {
	uint32_t bits;
	memcpy(&bits, &val, sizeof(float)); // Preserve bit pattern safely
	if (w->cursor + 4 > w->capacity) return false;
	w->buffer[w->cursor++] = (bits >> 24) & 0xFF;
	w->buffer[w->cursor++] = (bits >> 16) & 0xFF;
	w->buffer[w->cursor++] = (bits >> 8) & 0xFF;
	w->buffer[w->cursor++] = bits & 0xFF;
	return true;
}

bool net_write_header(NetWriter* w, NetMessageType type) {
	if (!net_write_u8(w, NET_PROTOCOL_VERSION)) return false;
	if (!net_write_u8(w, (uint8_t)type)) return false;
	return true;
}

bool net_read_u8(NetReader* r, uint8_t* out_val) {
	if (r->cursor + 1 > r->size) return false;
	*out_val = r->buffer[r->cursor++];
	return true;
}

bool net_read_u16(NetReader* r, uint16_t* out_val) {
	if (r->cursor + 2 > r->size) return false;
	*out_val = ((uint16_t)r->buffer[r->cursor] << 8) |
			   ((uint16_t)r->buffer[r->cursor + 1]);
	r->cursor += 2;
	return true;
}

bool net_read_u32(NetReader* r, uint32_t* out_val) {
    if (r->cursor + 4 > r->size) return false;
    *out_val = ((uint32_t)r->buffer[r->cursor]     << 24) |
               ((uint32_t)r->buffer[r->cursor + 1] << 16) |
               ((uint32_t)r->buffer[r->cursor + 2] << 8)  |
               ((uint32_t)r->buffer[r->cursor + 3]);
    r->cursor += 4;
    return true;
}

bool net_read_f32(NetReader* r, float* out_val) {
	if (r->cursor + 4 > r->size) return false;
	uint32_t bits = ((uint32_t)r->buffer[r->cursor] << 24) |
					((uint32_t)r->buffer[r->cursor + 1] << 16) |
					((uint32_t)r->buffer[r->cursor + 2] << 8) |
					((uint32_t)r->buffer[r->cursor + 3]);
	r->cursor += 4;
	memcpy(out_val, &bits, sizeof(float));
	return true;
}

bool net_pack_accept_client(NetWriter*                    w,
							const NetAcceptClientPayload* payload) {
	if (!net_write_header(w, NET_MSG_ACCEPT_CLIENT)) return false;
	if (!net_write_u8(w, payload->client_id)) return false;
	return true;
}

bool net_unpack_accept_client(NetReader*              r,
							  NetAcceptClientPayload* out_payload) {
	if (!net_read_u8(r, &out_payload->client_id)) return false;
	return true;
}

bool net_pack_client_joined(NetWriter*                    w,
							const NetClientJoinedPayload* payload) {
	if (!net_write_header(w, NET_MSG_CLIENT_JOINED)) return false;
	if (!net_write_u8(w, payload->client_id)) return false;
	return true;
}

bool net_unpack_client_joined(NetReader*              r,
							  NetClientJoinedPayload* out_payload) {
	if (!net_read_u8(r, &out_payload->client_id)) return false;
	return true;
}

bool net_pack_roster_snapshot(NetWriter*                      w,
							  const NetRosterSnapshotPayload* payload) {
	if (!net_write_header(w, NET_MSG_ROSTER_SNAPSHOT)) return false;
	if (!net_write_u8(w, payload->count)) return false;
	for (uint8_t i = 0; i < payload->count; i++) {
		if (!net_write_u8(w, payload->client_ids[i])) return false;
	}
	return true;
}

bool net_unpack_roster_snapshot(NetReader*                r,
								NetRosterSnapshotPayload* out_payload) {
	if (!net_read_u8(r, &out_payload->count)) return false;
	if (out_payload->count > NET_MAX_PLAYERS) return false;
	for (uint8_t i = 0; i < out_payload->count; i++) {
		if (!net_read_u8(r, &out_payload->client_ids[i])) return false;
	}
	return true;
}

bool net_pack_client_left(NetWriter* w, const NetClientLeftPayload* payload) {
	if (!net_write_header(w, NET_MSG_CLIENT_LEFT)) return false;
	if (!net_write_u8(w, payload->client_id)) return false;
	return true;
}

bool net_unpack_client_left(NetReader* r, NetClientLeftPayload* out_payload) {
	if (!net_read_u8(r, &out_payload->client_id)) return false;
	return true;
}

bool net_pack_intent_drag(NetWriter* w, const NetIntentDragPayload* payload) {
	if (!net_write_header(w, NET_MSG_INTENT_DRAG)) return false;
	if (!net_write_u16(w, payload->particle_index)) return false;
	if (!net_write_f32(w, payload->target_x)) return false;
	if (!net_write_f32(w, payload->target_y)) return false;
	return true;
}

bool net_unpack_intent_drag(NetReader* r, NetIntentDragPayload* out_payload) {
	if (!net_read_u16(r, &out_payload->particle_index)) return false;
	if (!net_read_f32(r, &out_payload->target_x)) return false;
	if (!net_read_f32(r, &out_payload->target_y)) return false;
	return true;
}

bool net_pack_snapshot(NetWriter* w, const NetSnapshotPayload* payload) {
	if (!net_write_header(w, NET_MSG_WORLD_SNAPSHOT)) return false;
	if (!net_write_u16(w, payload->particle_count)) return false;

	for (uint16_t i = 0; i < payload->particle_count; i++) {
		if (!net_write_f32(w, payload->positions_x[i])) return false;
		if (!net_write_f32(w, payload->positions_y[i])) return false;
	}

	return true;
}

bool net_unpack_snapshot_arena(Arena* arena, NetReader* r,
							   NetSnapshotPayload* out_payload) {
	if (!net_read_u16(r, &out_payload->particle_count)) return false;
	if (out_payload->particle_count > NET_MAX_SNAPSHOT_PARTICLES) return false;

	size_t bytes_needed =
		(size_t)out_payload->particle_count * 2 * sizeof(float);
	if ((r->size - r->cursor) < bytes_needed) return false;

	out_payload->positions_x =
		ARENA_PUSH_ARRAY(arena, float, out_payload->particle_count);
	out_payload->positions_y =
		ARENA_PUSH_ARRAY(arena, float, out_payload->particle_count);

	for (uint16_t i = 0; i < out_payload->particle_count; i++) {
		if (!net_read_f32(r, &out_payload->positions_x[i])) return false;
		if (!net_read_f32(r, &out_payload->positions_y[i])) return false;
	}

	return true;
}

bool net_pack_snapshot_chunk(NetWriter* w, const NetSnapshotChunkPayload* payload) {
    if (!net_write_header(w, NET_MSG_WORLD_SNAPSHOT_CHUNK)) return false;
    if (!net_write_u32(w, payload->tick_index)) return false;
    if (!net_write_u16(w, payload->chunk_index)) return false;
    if (!net_write_u16(w, payload->total_chunks)) return false;
    if (!net_write_u16(w, payload->particle_start_index)) return false;
    if (!net_write_u16(w, payload->particle_count)) return false;

    for (uint16_t i = 0; i < payload->particle_count; i++) {
        if (!net_write_f32(w, payload->positions_x[i])) return false;
        if (!net_write_f32(w, payload->positions_y[i])) return false;
    }
    return true;
}

bool net_unpack_snapshot_chunk_arena(Arena* arena, NetReader* r, NetSnapshotChunkPayload* out_payload) {
    if (!net_read_u32(r, &out_payload->tick_index)) return false;
    if (!net_read_u16(r, &out_payload->chunk_index)) return false;
    if (!net_read_u16(r, &out_payload->total_chunks)) return false;
    if (!net_read_u16(r, &out_payload->particle_start_index)) return false;
    if (!net_read_u16(r, &out_payload->particle_count)) return false;

    if (out_payload->total_chunks == 0) return false;
    if (out_payload->total_chunks > NET_MAX_SNAPSHOT_CHUNKS) return false;
    if (out_payload->chunk_index >= out_payload->total_chunks) return false;
    if (out_payload->particle_count > NET_MAX_PARTICLES_PER_CHUNK) return false;

    size_t bytes_needed = (size_t)out_payload->particle_count * 2 * sizeof(float);
    if ((r->size - r->cursor) < bytes_needed) return false;

    out_payload->positions_x = ARENA_PUSH_ARRAY(arena, float, out_payload->particle_count);
    out_payload->positions_y = ARENA_PUSH_ARRAY(arena, float, out_payload->particle_count);

    for (uint16_t i = 0; i < out_payload->particle_count; i++) {
        if (!net_read_f32(r, &out_payload->positions_x[i])) return false;
        if (!net_read_f32(r, &out_payload->positions_y[i])) return false;
    }
    return true;
}

bool net_pack_intent_grab(NetWriter* w, const NetIntentGrabPayload* payload) {
	if (!net_write_header(w, NET_MSG_INTENT_GRAB)) return false;
	if (!net_write_u16(w, payload->particle_index)) return false;
	if (!net_write_f32(w, payload->target_x)) return false;
	if (!net_write_f32(w, payload->target_y)) return false;
	return true;
}

bool net_unpack_intent_grab(NetReader* r, NetIntentGrabPayload* out_payload) {
	if (!net_read_u16(r, &out_payload->particle_index)) return false;
	if (!net_read_f32(r, &out_payload->target_x)) return false;
	if (!net_read_f32(r, &out_payload->target_y)) return false;
	return true;
}

bool net_pack_intent_release(NetWriter*                     w,
							 const NetIntentReleasePayload* payload) {
	if (!net_write_header(w, NET_MSG_INTENT_RELEASE)) return false;
	if (!net_write_u16(w, payload->particle_index)) return false;
	return true;
}

bool net_unpack_intent_release(NetReader*               r,
							   NetIntentReleasePayload* out_payload) {
	if (!net_read_u16(r, &out_payload->particle_index)) return false;
	return true;
}

bool net_pack_intent_toggle_pin(NetWriter*                       w,
								const NetIntentTogglePinPayload* payload) {
	if (!net_write_header(w, NET_MSG_INTENT_TOGGLE_PIN)) return false;
	if (!net_write_u16(w, payload->particle_index)) return false;
	return true;
}

bool net_unpack_intent_toggle_pin(NetReader*                 r,
								  NetIntentTogglePinPayload* out_payload) {
	if (!net_read_u16(r, &out_payload->particle_index)) return false;
	return true;
}

bool net_pack_particle_ownership(NetWriter* w, const NetParticleOwnershipPayload* payload) {
    if (!net_write_header(w, NET_MSG_PARTICLE_OWNERSHIP)) return false;
    if (!net_write_u16(w, payload->particle_index)) return false;
    if (!net_write_u8(w, payload->owner_id)) return false;
    return true;
}

bool net_unpack_particle_ownership(NetReader* r, NetParticleOwnershipPayload* out_payload) {
    if (!net_read_u16(r, &out_payload->particle_index)) return false;
    if (!net_read_u8(r, &out_payload->owner_id)) return false;
    return true;
}
