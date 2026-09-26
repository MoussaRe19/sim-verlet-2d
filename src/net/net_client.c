#include "net_client.h"
#include <stdio.h>

static ENetPacket* build_packet(size_t max_size, bool reliable) {
	return enet_packet_create(NULL, max_size,
							  reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
}

ClientContext* net_client_connect(const char* address, uint16_t port,
								  RenderWorld* render_world) {
	ClientContext* ctx = (ClientContext*)calloc(1, sizeof(ClientContext));
	if (!ctx) return NULL;

	ctx->host = enet_host_create(NULL, 1, NET_CHANNEL_COUNT, 0, 0);
	if (!ctx->host) {
		fprintf(stderr, "[net][client] failed to create ENet host\n");
		free(ctx);
		return NULL;
	}

	ENetAddress enet_addr;
	if (enet_address_set_host(&enet_addr, address) != 0) {
		fprintf(stderr, "[net][client] failed to resolve host '%s'\n", address);
		enet_host_destroy(ctx->host);
		free(ctx);
		return NULL;
	}

	enet_addr.port = port;

	ctx->server_peer =
		enet_host_connect(ctx->host, &enet_addr, NET_CHANNEL_COUNT, 0);
	if (!ctx->server_peer) {
		fprintf(
			stderr,
			"[net][client] enet_host_connect failed (no peers available)\n");
		enet_host_destroy(ctx->host);
		free(ctx);
		return NULL;
	}
	ctx->local_client_id = NET_CLIENT_SENTINEL_ID;
	ctx->render_world = render_world;
	ctx->frame_arena = arena_create((size_t)NET_MAX_SNAPSHOT_PARTICLES * 2 *
									sizeof(float) * 2);
	ctx->held_particle_index = -1;
	ctx->drag_send_accumulator = 0.0;

	return ctx;
}

bool net_client_is_accepted(const ClientContext* ctx) {
	return ctx && ctx->local_client_id != NET_CLIENT_SENTINEL_ID;
}

void net_client_update(ClientContext* ctx, double dt) {
	(void)dt;
	ENetEvent event;

	while (enet_host_service(ctx->host, &event, 0) > 0) {
		switch (event.type) {
		case ENET_EVENT_TYPE_RECEIVE: {
			NetContext netctx = {
				.client_id = ctx->local_client_id,
				.peer_handle = ctx->server_peer,
				.world_state = ctx->render_world,
				.frame_arena = &ctx->frame_arena,
				.user_data = NULL,
				.server_tick = 0,
				.out_local_client_id = &ctx->local_client_id,
			};

			bool ok = net_dispatch_message(&netctx, event.packet->data,
										   event.packet->dataLength);
			enet_packet_destroy(event.packet);

			if (!ok) {
				fprintf(stderr,
						"[net][client] rejected a message from the server\n");
			}

			break;
		}

		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
			fprintf(stderr, "[net][client] disconnected from server\n");
			ctx->local_client_id = NET_CLIENT_SENTINEL_ID;
			ctx->server_peer = NULL;
			ctx->held_particle_index = -1;
			break;
		}

		default:
			break;
		}
	}
}

void net_client_disconnect(ClientContext* ctx) {
	if (!ctx) return;

	if (ctx->server_peer) {
		enet_peer_disconnect(ctx->server_peer, 0);
		ENetEvent event;
		uint32_t  waited = 0;
		while (waited < 200 && enet_host_service(ctx->host, &event, 20) >= 0) {
			if (event.type == ENET_EVENT_TYPE_RECEIVE) {
				enet_packet_destroy(event.packet);
			}
			if (event.type == ENET_EVENT_TYPE_DISCONNECT) break;
			waited += 20;
		}
	}

	arena_destroy(&ctx->frame_arena);
	enet_host_destroy(ctx->host);
	free(ctx);
}

void net_client_send_intent_grab(ClientContext* ctx, uint16_t particle_index,
								 float target_x, float target_y) {
	if (!net_client_is_accepted(ctx)) return;

	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetIntentGrabPayload payload = {.particle_index = particle_index,
									.target_x = target_x,
									.target_y = target_y};

	if (!net_pack_intent_grab(&w, &payload)) {
		enet_packet_destroy(packet);
		return;
	}
	packet->dataLength = w.cursor;
	enet_peer_send(ctx->server_peer, NET_CHANNEL_RELIABLE, packet);
	ctx->held_particle_index =
		(int32_t)particle_index; // TODO: optimistic — server stays
														// authoritative on contention
}

void net_client_send_intent_drag(ClientContext* ctx, uint16_t particle_index,
								 float target_x, float target_y) {
	if (!net_client_is_accepted(ctx)) return;
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, false);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetIntentDragPayload payload = {.particle_index = particle_index,
									.target_x = target_x,
									.target_y = target_y};
	if (!net_pack_intent_drag(&w, &payload)) {
		enet_packet_destroy(packet);
		return;
	}
	packet->dataLength = w.cursor;
	enet_peer_send(ctx->server_peer, NET_CHANNEL_UNRELIABLE, packet);
}

void net_client_update_drag(ClientContext* ctx, double dt, float target_x,
							float target_y) {
	if (ctx->held_particle_index < 0) return;

	ctx->drag_send_accumulator += dt;
	if (ctx->drag_send_accumulator < NET_DRAG_SEND_INTERVAL) return;
	ctx->drag_send_accumulator -= NET_DRAG_SEND_INTERVAL;

	net_client_send_intent_drag(ctx, (uint16_t)ctx->held_particle_index,
								target_x, target_y);
}

void net_client_send_intent_release(ClientContext* ctx,
									uint16_t       particle_index) {
	if (!net_client_is_accepted(ctx)) return;
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetIntentReleasePayload payload = {.particle_index = particle_index};
	if (!net_pack_intent_release(&w, &payload)) {
		enet_packet_destroy(packet);
		return;
	}
	packet->dataLength = w.cursor;
	enet_peer_send(ctx->server_peer, NET_CHANNEL_RELIABLE, packet);

	// Optimistic
	if (ctx->held_particle_index == (int32_t)particle_index)
		ctx->held_particle_index = -1;
}

void net_client_send_intent_toggle_pin(ClientContext* ctx,
									   uint16_t       particle_index) {
	if (!net_client_is_accepted(ctx)) return;
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetIntentTogglePinPayload payload = {.particle_index = particle_index};
	if (!net_pack_intent_toggle_pin(&w, &payload)) {
		enet_packet_destroy(packet);
		return;
	}
	packet->dataLength = w.cursor;
	enet_peer_send(ctx->server_peer, NET_CHANNEL_RELIABLE, packet);
}
