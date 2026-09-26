#include "net_server.h"
#include <stdio.h>
#include <stdbool.h>

static bool net_validate_particle_index(NetContext* ctx, World* world,
										uint32_t    particle_index,
										const char* action_name) {
	if (particle_index >= world_particle_count(world)) {
		fprintf(stderr,
				"[net][server] client %u sent out-of-range particle_index %u "
				"(%s)\n",
				ctx->client_id, particle_index, action_name);
		return false;
	}
	return true;
}

static bool handle_intent_grab(NetContext* ctx, NetReader* r) {
	NetIntentGrabPayload p;
	if (!net_unpack_intent_grab(r, &p)) {
		fprintf(stderr, "[net][server] malformed INTENT_GRAB from client %u\n",
				ctx->client_id);
		return false;
	}

	World* world = (World*)ctx->world_state;
	if (!net_validate_particle_index(ctx, world, p.particle_index, "grab")) {
		return false;
	}

	ClientSlot* slot = (ClientSlot*)ctx->user_data;

	if (world_try_claim_particle(world, p.particle_index, ctx->client_id)) {
		if (slot) slot->held_particle_index = (int32_t)p.particle_index;
	}

	return true;
}

static bool handle_intent_drag(NetContext* ctx, NetReader* r) {
	NetIntentDragPayload p;
	if (!net_unpack_intent_drag(r, &p)) {
		fprintf(stderr, "[net][server] malformed INTENT_DRAG from client %u\n",
				ctx->client_id);
		return false;
	}

	World* world = (World *) ctx->world_state;

	if (!net_validate_particle_index(ctx, world, p.particle_index, "drag")) {
		return false;
	}

	ClientSlot* slot = (ClientSlot*)ctx->user_data;
	if (slot) {
		if (slot->last_intent_tick[NET_MSG_INTENT_DRAG] == ctx->server_tick) {
			return true; // silently dropped
		}
		slot->last_intent_tick[NET_MSG_INTENT_DRAG] = ctx->server_tick;

		if (slot->held_particle_index != (int32_t)p.particle_index) {
			return true; // ignored - not hodling it
		}
	}
	world_set_drag_target(world, p.particle_index, p.target_x, p.target_y);
	return true;
}

static bool handle_intent_release(NetContext* ctx, NetReader* r) {
	NetIntentReleasePayload p;
	if (!net_unpack_intent_release(r, &p)) {
		fprintf(stderr,
				"[net][server] malformed INTENT_RELEASE from client %u\n",
				ctx->client_id);
		return false;
	}

	World* world = (World*)ctx->world_state;
	if (!net_validate_particle_index(ctx, world, p.particle_index, "release")) {
		return false;
	}

	ClientSlot* slot = (ClientSlot*)ctx->user_data;
	if (slot && slot->held_particle_index == (int32_t)p.particle_index) {
		world_release_particle(world, p.particle_index, ctx->client_id);
		slot->held_particle_index = -1;
	}

	return true;
}

static bool handle_intent_toggle_pin(NetContext* ctx, NetReader* r) {
	NetIntentTogglePinPayload p;
	if (!net_unpack_intent_toggle_pin(r, &p)) {
		fprintf(stderr,
				"[net][server] malformed INTENT_TOGGLE_PIN from client %u\n",
				ctx->client_id);
		return false;
	}

	World* world = (World*)ctx->world_state;
	if (p.particle_index >= world_particle_count(world)) {
		fprintf(stderr,
				"[net][server] client %u sent out-of-range particle_index %u "
				"(toggle_pin)\n",
				ctx->client_id, p.particle_index);
		return false;
	}

	world_toggle_pin(world, p.particle_index);
	return true;
}

// Client --> Server
static const NetMessageHandler SERVER_DISPATCH_TABLE[NET_MSG_COUNT] = {
	[NET_MSG_NONE] = NULL,
	[NET_MSG_ACCEPT_CLIENT] = NULL,
	[NET_MSG_CLIENT_JOINED] = NULL,
	[NET_MSG_CLIENT_LEFT] = NULL,
	[NET_MSG_ROSTER_SNAPSHOT] = NULL,
	[NET_MSG_WORLD_SNAPSHOT] = NULL,
	[NET_MSG_WORLD_SNAPSHOT_CHUNK] = NULL,
	[NET_MSG_INTENT_GRAB] = handle_intent_grab,
	[NET_MSG_INTENT_DRAG] = handle_intent_drag,
	[NET_MSG_INTENT_RELEASE] = handle_intent_release,
	[NET_MSG_INTENT_TOGGLE_PIN] = handle_intent_toggle_pin,
	[NET_MSG_PARTICLE_OWNERSHIP] = NULL,
};

bool net_dispatch_message(NetContext* ctx, const uint8_t* buffer, size_t size) {
	NetReader r;
	net_reader_init(&r, buffer, size);

	uint8_t version, msg_type;
	if (!net_read_u8(&r, &version) || version != NET_PROTOCOL_VERSION) {
		fprintf(stderr, "[net][server] rejected packet: bad version\n");
		return false;
	}
	if (!net_read_u8(&r, &msg_type) || msg_type >= NET_MSG_COUNT) {
		fprintf(stderr,
				"[net][server] rejected packet: bad/out-of-range msg_type %u\n",
				msg_type);
		return false;
	}

	NetMessageHandler handler = SERVER_DISPATCH_TABLE[msg_type];
	if (!handler) {
		fprintf(stderr,
				"[net][server] rejected msg_type %u from client %u: no "
				"server-side handler "
				"(wrong direction or unimplemented)\n",
				msg_type, ctx->client_id);
		return false;
	}
	bool ok = false;
	MA_TempScope(ctx->frame_arena) {
		ok = handler(ctx, &r);
	}

	return ok;
}
