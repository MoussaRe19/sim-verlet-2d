#include "net_client.h"
#include <stdio.h>

static bool net_validate_client_id(uint8_t client_id, const char* action_name) {
	if (client_id >= NET_MAX_PLAYERS) {
		fprintf(stderr,
				"[net][client] received %s with out-of-bounds client_id %u (>= "
				"MAX_PLAYERS %d)\n",
				action_name, client_id, NET_MAX_PLAYERS);
		return false;
	}
	return true;
}

static bool handle_accept_client(NetContext* ctx, NetReader* r) {
	NetAcceptClientPayload p;
	if (!net_unpack_accept_client(r, &p)) {
		fprintf(stderr, "[net][client] malformed ACCEPT_CLIENT\n");
		return false;
		// TODO: handle invalid packet (e.g., disconnect client or ignore)
	}

	if (!net_validate_client_id(p.client_id, "CLIENT_ACCEPT")) {
		return false;
	}

	if (ctx->out_local_client_id) {
		*ctx->out_local_client_id = p.client_id;
	}

	fprintf(stderr, "[net][client] accepted, local client_id=%u\n",
			p.client_id);
	return true;
}

static bool handle_client_joined(NetContext* ctx, NetReader* r) {
	NetClientJoinedPayload p;
	if (!net_unpack_client_joined(r, &p)) {
		fprintf(stderr, "[net][client] malformed CLIENT_JOINED\n");
		return false;
	}

	if (!net_validate_client_id(p.client_id, "CLIENT_JOIND")) {
		return false;
	}

	render_world_add_player((RenderWorld*)ctx->world_state, p.client_id);
	return true;
}

static bool handle_roster_snapshot(NetContext* ctx, NetReader* r) {
	NetRosterSnapshotPayload p;
	if (!net_unpack_roster_snapshot(r, &p)) {
		fprintf(stderr, "[net][client] malformed ROSTER_SNAPSHOT\n");
		return false;
	}

	for (uint8_t i = 0; i < p.count; i++) {
		if (!net_validate_client_id(p.client_ids[i], "ROSTER_SNAPSHOT")) {
			return false;
		}

		render_world_add_player((RenderWorld*)ctx->world_state,
								p.client_ids[i]);
	}

	return true;
}

static bool handle_client_left(NetContext* ctx, NetReader* r) {
	NetClientLeftPayload p;
	if (!net_unpack_client_left(r, &p)) {
		fprintf(stderr, "[net][client] malformed CLIENT_LEFT\n");
		return false;
	}

	if (!net_validate_client_id(p.client_id, "CLIENT_LEFT")) {
		return false;
	}

	render_world_remove_player((RenderWorld*)ctx->world_state, p.client_id);
	return true;
}

static bool handle_world_snapshot(NetContext* ctx, NetReader* r) {
	NetSnapshotPayload p;

	if (!net_unpack_snapshot_arena(ctx->frame_arena, r, &p)) {
		fprintf(stderr, "[net][client] malformed/oversized WORLD_SNAPSHOT\n");
		return false;
	}

	// p.positions_x/y live in the temp-scoped arena and become invalid
	// the instant this handler returns — copy into RenderWorld's own
	// permanent storage now.
	render_world_apply_snapshot((RenderWorld*)ctx->world_state, p.positions_x,
								p.positions_y, p.particle_count);
	return true;
}

static bool handle_world_snapshot_chunk(NetContext* ctx, NetReader* r) {
	NetSnapshotChunkPayload p;
	if (!net_unpack_snapshot_chunk_arena(ctx->frame_arena, r, &p)) {
		fprintf(stderr, "[net][client] malformed WORLD_SNAPSHOT_CHUNK\n");
		return false;
	}

	RenderWorld* rw = (RenderWorld*)ctx->world_state;
	// Drop old snapshots, but allow chunks from the same tick.
	if (p.tick_index < render_world_get_last_applied_tick(rw)) {
		return true;
	}
	render_world_set_last_applied_tick(rw, p.tick_index);

	// Apply chunks directly; incomplete ticks are allowed.
	render_world_apply_snapshot_chunk(rw, p.particle_start_index,
									  p.particle_count, p.positions_x,
									  p.positions_y);

	return true;
}

static bool handle_particle_ownership(NetContext* ctx, NetReader* r) {
    NetParticleOwnershipPayload p;
    if (!net_unpack_particle_ownership(r, &p)) {
        fprintf(stderr, "[net][client] malformed PARTICLE_OWNERSHIP\n");
        return false;
    }

   	if (p.owner_id != WORLD_UNOWNED && !net_validate_client_id(p.owner_id, "PARTICLE_OWNERSHIP")) {
		return false;
	}

    render_world_set_particle_owner((RenderWorld*)ctx->world_state, p.particle_index, p.owner_id);
    return true;
}

// Server --> Client
static const NetMessageHandler CLIENT_DISPATCH_TABLE[NET_MSG_COUNT] = {
	[NET_MSG_NONE] = NULL,
	[NET_MSG_ACCEPT_CLIENT] = handle_accept_client,
	[NET_MSG_CLIENT_JOINED] = handle_client_joined,
	[NET_MSG_ROSTER_SNAPSHOT] = handle_roster_snapshot,
	[NET_MSG_CLIENT_LEFT] = handle_client_left,
	[NET_MSG_WORLD_SNAPSHOT] = handle_world_snapshot,
	[NET_MSG_WORLD_SNAPSHOT_CHUNK] = handle_world_snapshot_chunk,
	[NET_MSG_PARTICLE_OWNERSHIP] = handle_particle_ownership,
	[NET_MSG_INTENT_GRAB] = NULL,
	[NET_MSG_INTENT_DRAG] = NULL,
	[NET_MSG_INTENT_RELEASE] = NULL,
	[NET_MSG_INTENT_TOGGLE_PIN] = NULL,
};

bool net_dispatch_message(NetContext* ctx, const uint8_t* buffer, size_t size) {
	NetReader r;
	net_reader_init(&r, buffer, size);

	uint8_t version, msg_type;
	if (!net_read_u8(&r, &version) || version != NET_PROTOCOL_VERSION) {
		fprintf(stderr, "[net][client] rejected packet: bad version\n");
		return false;
	}
	if (!net_read_u8(&r, &msg_type) || msg_type >= NET_MSG_COUNT) {
		fprintf(stderr,
				"[net][client] rejected packet: bad/out-of-range msg_type %u\n",
				msg_type);
		return false;
	}

	NetMessageHandler handler = CLIENT_DISPATCH_TABLE[msg_type];
	if (!handler) {
		fprintf(stderr,
				"[net][client] rejected msg_type %u: no client-side handler "
				"(wrong direction or unimplemented)\n",
				msg_type);
		return false;
	}

	bool ok = false;
	MA_TempScope(ctx->frame_arena) {
		ok = handler(ctx, &r);
	}

	return ok;
}
