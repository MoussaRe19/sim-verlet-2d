#include "net/net_server.h"
#include "core/physics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ENetPacket* build_packet(size_t max_size, bool reliable) {
	return enet_packet_create(NULL, max_size,
							  reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
}

static ClientSlot* find_free_slot(ServerContext* ctx) {
	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		if (!ctx->slots[i].active) return &ctx->slots[i];
	}
	return NULL;
}

static void server_send_accept_client(ClientSlot* target, uint8_t assigned_id) {
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetAcceptClientPayload payload = {.client_id = assigned_id};
	net_pack_accept_client(&w, &payload);
	packet->dataLength = w.cursor;
	enet_peer_send(target->peer, NET_CHANNEL_RELIABLE, packet);
}

static void server_send_roster_snapshot(ServerContext* ctx,
										ClientSlot*    target) {
	NetRosterSnapshotPayload payload = {0};
	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		ClientSlot* other = &ctx->slots[i];
		if (other->active && other != target) {
			payload.client_ids[payload.count++] = other->client_id;
		}
	}

	// Always sent, even when empty, to signal no roster yet.
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	if (!net_pack_roster_snapshot(&w, &payload)) {
		fprintf(stderr, "[net][server] roster snapshot pack failed\n");
		enet_packet_destroy(packet);
		return;
	}
	packet->dataLength = w.cursor;
	enet_peer_send(target->peer, NET_CHANNEL_RELIABLE, packet);
}

static void server_broadcast_client_joined(ServerContext* ctx,
										   uint8_t        joined_id,
										   ClientSlot*    except) {
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetClientJoinedPayload payload = {.client_id = joined_id};
	net_pack_client_joined(&w, &payload);
	packet->dataLength = w.cursor;

	bool sent = false;
	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		ClientSlot* slot = &ctx->slots[i];
		if (!slot->active || slot == except) continue;
		enet_peer_send(slot->peer, NET_CHANNEL_RELIABLE, packet);
		sent = true;
	}

	if (!sent) enet_packet_destroy(packet);
}

static void server_broadcast_client_left(ServerContext* ctx, uint8_t left_id,
										 ClientSlot* except) {
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetClientLeftPayload payload = {.client_id = left_id};
	net_pack_client_left(&w, &payload);
	packet->dataLength = w.cursor;

	bool sent = false;
	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		ClientSlot* slot = &ctx->slots[i];
		if (!slot->active || slot == except) continue;
		enet_peer_send(slot->peer, NET_CHANNEL_RELIABLE, packet);
		sent = true;
	}

	if (!sent) enet_packet_destroy(packet);
}

static void server_broadcast_particle_ownership(ServerContext* ctx,
												uint16_t       particle_index,
												uint8_t        owner_id) {
	ENetPacket* packet = build_packet(NET_MAX_PACKET_SIZE, true);
	NetWriter   w;
	net_writer_init(&w, packet->data, packet->dataLength);
	NetParticleOwnershipPayload payload = {.particle_index = particle_index,
										   .owner_id = owner_id};
	net_pack_particle_ownership(&w, &payload);
	packet->dataLength = w.cursor;

	bool sent = false;
	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		ClientSlot* slot = &ctx->slots[i];
		if (!slot->active) continue;
		enet_peer_send(slot->peer, NET_CHANNEL_RELIABLE, packet);
		sent = true;
	}
	if (!sent) enet_packet_destroy(packet);
}

static void server_broadcast_snapshot(ServerContext* ctx) {
	if (ctx->active_count == 0) return;

	size_t particle_count = world_particle_count(ctx->world);
	if (particle_count > NET_MAX_SNAPSHOT_PARTICLES) {
		fprintf(stderr,
				"[net][server] world has %zu particles, exceeds "
				"NET_MAX_SNAPSHOT_PARTICLES (%d) — truncating\n",
				particle_count, NET_MAX_SNAPSHOT_PARTICLES);
		particle_count = NET_MAX_SNAPSHOT_PARTICLES;
	}
	if (particle_count == 0) return;

	uint16_t total_chunks =
		(uint16_t)((particle_count + NET_MAX_PARTICLES_PER_CHUNK - 1) /
				   NET_MAX_PARTICLES_PER_CHUNK);

	for (uint16_t chunk = 0; chunk < total_chunks; chunk++) {
		uint16_t start = (uint16_t)(chunk * NET_MAX_PARTICLES_PER_CHUNK);
		size_t   remaining = particle_count - start;
		uint16_t count = (uint16_t)(remaining < NET_MAX_PARTICLES_PER_CHUNK
										? remaining
										: NET_MAX_PARTICLES_PER_CHUNK);

		float xs[NET_MAX_PARTICLES_PER_CHUNK], ys[NET_MAX_PARTICLES_PER_CHUNK];
		world_copy_positions(ctx->world, start, count, xs, ys);

		NetSnapshotChunkPayload payload = {
			.tick_index = (uint32_t)ctx->tick_index,
			.chunk_index = chunk,
			.total_chunks = total_chunks,
			.particle_start_index = start,
			.particle_count = count,
			.positions_x = xs,
			.positions_y = ys,
		};

		// UNSEQUENCED
		ENetPacket* packet = enet_packet_create(NULL, NET_MAX_PACKET_SIZE,
												ENET_PACKET_FLAG_UNSEQUENCED);
		NetWriter   w;
		net_writer_init(&w, packet->data, packet->dataLength);

		if (!net_pack_snapshot_chunk(&w, &payload)) {
			fprintf(stderr,
					"[net][server] snapshot chunk pack failed (chunk %u/%u)\n",
					chunk, total_chunks);
			enet_packet_destroy(packet);
			continue;
		}

		packet->dataLength = w.cursor;

		bool sent = false;
		for (int i = 0; i < NET_MAX_PLAYERS; i++) {
			ClientSlot* slot = &ctx->slots[i];
			if (!slot->active) continue;
			enet_peer_send(slot->peer, NET_CHANNEL_UNRELIABLE, packet);
			sent = true;
		}

		if (!sent) enet_packet_destroy(packet);
	}
}

static void server_reset_slot(ServerContext* ctx, ClientSlot* slot,
							  ENetPeer* peer) {
	if (slot->held_particle_index >= 0) {
		world_release_particle(ctx->world, (uint16_t)slot->held_particle_index,
							   slot->client_id);
		server_broadcast_particle_ownership(
			ctx, (uint16_t)slot->held_particle_index, WORLD_UNOWNED);
		slot->held_particle_index = -1;
	}

	slot->active = false;
	slot->peer = NULL;
	if (peer) peer->data = NULL;
	ctx->active_count--;

	slot->invalid_packet_count = 0;
	for (int t = 0; t < NET_MSG_COUNT; t++) {
		slot->last_intent_tick[t] = 0;
	}
}

ClientSlot* net_server_slot_for_peer(ServerContext* ctx, ENetPeer* peer) {
	(void)ctx;
	return (ClientSlot*)peer->data;
}

ServerContext* net_server_create(World* world, uint16_t port) {
	ServerContext* ctx = (ServerContext*)calloc(1, sizeof(ServerContext));
	if (!ctx) return NULL;

	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = port;

	// No bandwidth limits until real traffic is measured.
	ctx->host =
		enet_host_create(&address, NET_MAX_PLAYERS, NET_CHANNEL_COUNT, 0, 0);
	if (!ctx->host) {
		fprintf(stderr, "[net][server] failed to create ENet host on port %u\n",
				port);
		free(ctx);
		return NULL;
	}

	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		ctx->slots[i].active = false;
		ctx->slots[i].peer = NULL;
		ctx->slots[i].client_id = (uint8_t)i;
		ctx->slots[i].invalid_packet_count = 0;
		ctx->slots[i].held_particle_index = -1;
		for (int t = 0; t < NET_MSG_COUNT; t++) {
			ctx->slots[i].last_intent_tick[t] = 0;
		}
	}

	ctx->world = world;
	ctx->frame_arena = arena_create(KB(4));
	ctx->snapshot_accumulator = 0.0;
	ctx->tick_index = 0;
	ctx->active_count = 0;

	return ctx;
}

void net_server_destroy(ServerContext* ctx) {
	if (!ctx) return;

	for (int i = 0; i < NET_MAX_PLAYERS; i++) {
		if (ctx->slots[i].active && ctx->slots[i].peer) {
			enet_peer_disconnect(ctx->slots[i].peer, 0);
		}
	}

	// Give disconnects a brief chance to flush before tearing the host down.
	ENetEvent event;
	uint32_t  waited = 0;
	while (waited < 200 && enet_host_service(ctx->host, &event, 20) > 0) {
		if (event.type == ENET_EVENT_TYPE_RECEIVE) {
			enet_packet_destroy(event.packet);
		}
		waited += 20;
	}

	arena_destroy(&ctx->frame_arena);
	enet_host_destroy(ctx->host);
	free(ctx);
}

void net_server_poll_events(ServerContext* ctx) {
	ENetEvent event;

	while (enet_host_service(ctx->host, &event, 0) > 0) {
		switch (event.type) {
		case ENET_EVENT_TYPE_CONNECT: {
			ClientSlot* slot = find_free_slot(ctx);
			if (!slot) {
				fprintf(stderr,
						"[net][server] server full, rejecting connection\n");
				enet_peer_disconnect(event.peer, 0);
				break;
			}

			slot->active = true;
			slot->peer = event.peer;
			event.peer->data = slot;
			ctx->active_count++;

			server_send_accept_client(slot, slot->client_id);
			server_send_roster_snapshot(ctx, slot);
			for (int i = 0; i < NET_MAX_PLAYERS; i++) {
				ClientSlot* other = &ctx->slots[i];
				if (other->active && other != slot &&
					other->held_particle_index >= 0) {
					ENetPacket* packet =
						build_packet(NET_MAX_PACKET_SIZE, true);
					NetWriter w;
					net_writer_init(&w, packet->data, packet->dataLength);
					NetParticleOwnershipPayload payload = {
						.particle_index = (uint16_t)other->held_particle_index,
						.owner_id = other->client_id,
					};
					net_pack_particle_ownership(&w, &payload);
					packet->dataLength = w.cursor;
					enet_peer_send(slot->peer, NET_CHANNEL_RELIABLE, packet);
				}
			}
			server_broadcast_client_joined(ctx, slot->client_id, slot);

			break;
		}

		case ENET_EVENT_TYPE_RECEIVE: {
			ClientSlot* slot = (ClientSlot*)event.peer->data;
			if (!slot) {
				enet_packet_destroy(event.packet);
				break;
			}

			// State capture : TODO: FIX LATER
			int32_t held_before = slot->held_particle_index;

			NetContext netctx = {
				.client_id = slot->client_id,
				.peer_handle = event.peer,
				.world_state = ctx->world,
				.frame_arena = &ctx->frame_arena,
				.user_data = slot,
				.server_tick = ctx->tick_index,
				.out_local_client_id = NULL,
			};

			bool ok = net_dispatch_message(&netctx, event.packet->data,
										   event.packet->dataLength);
			enet_packet_destroy(event.packet);

			// State change Actions here TODO: Improuve later
			if (slot->held_particle_index != held_before) {
				if (held_before >= 0) {
					server_broadcast_particle_ownership(
						ctx, (uint16_t)held_before, WORLD_UNOWNED);
				}
				if (slot->held_particle_index >= 0) {
					server_broadcast_particle_ownership(
						ctx, (uint16_t)slot->held_particle_index,
						slot->client_id);
				}
			}

			if (!ok) {
				slot->invalid_packet_count++;
				if (slot->invalid_packet_count >=
					NET_INVALID_PACKET_KICK_THRESHOLD) {
					fprintf(stderr,
							"[net][server] client %u exceeded invalid packet "
							"threshold (%u), resetting\n",
							slot->client_id, slot->invalid_packet_count);
					net_server_kick_slot(ctx, slot);
				}
			}
			break;
		}

		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
			ClientSlot* slot = (ClientSlot*)event.peer->data;
			if (!slot) break;
			uint8_t left_id = slot->client_id;
			server_reset_slot(ctx, slot, event.peer);
			server_broadcast_client_left(ctx, left_id, NULL);
			break;
		}

		default:
			break;
		}
	}
}

void net_server_tick(ServerContext* ctx, double dt) {
	// server_apply_pending_inputs(ctx); // TODO : future
	physics_step(ctx->world, dt);     // pending World integration

	ctx->tick_index++;

	ctx->snapshot_accumulator += dt;
	if (ctx->snapshot_accumulator >= NET_SNAPSHOT_INTERVAL) {
		server_broadcast_snapshot(ctx);
		ctx->snapshot_accumulator = 0.0;
	}
}

void net_server_kick_slot(ServerContext* ctx, ClientSlot* slot) {
	if (!slot || !slot->active || !slot->peer) return;

	uint8_t   kicked_id = slot->client_id;
	ENetPeer* peer = slot->peer;

	server_reset_slot(ctx, slot, peer);
	enet_peer_reset(peer);

	server_broadcast_client_left(ctx, kicked_id, NULL);
}
