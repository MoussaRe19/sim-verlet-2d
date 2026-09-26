#ifndef NET_SERVER_H
#define NET_SERVER_H

#include "net_router.h"
#include "core/world.h"
#include "core/world_sim.h"
#include "enet/enet.h"

#define NET_PHYSICS_TICK_HZ 60.0
#define NET_PHYSICS_TICK_DT (1.0 / NET_PHYSICS_TICK_HZ)
#define NET_SNAPSHOT_RATE_HZ 20.0
#define NET_SNAPSHOT_INTERVAL (1.0 / NET_SNAPSHOT_RATE_HZ)

#define NET_INVALID_PACKET_KICK_THRESHOLD 20

typedef struct ClientSlot {
	bool      active;
	ENetPeer* peer;
	uint8_t   client_id;
	uint32_t  invalid_packet_count;
	uint64_t  last_intent_tick[NET_MSG_COUNT];
	int32_t   held_particle_index; //(drag/auto-release) -1 = none.
} ClientSlot;

typedef struct ServerContex {
	ENetHost*  host;
	ClientSlot slots[NET_MAX_PLAYERS];
	uint8_t    active_count;

	World* world; // not owned
	Arena  frame_arena;

	double   snapshot_accumulator;
	uint64_t tick_index;
} ServerContext;

ServerContext* net_server_create(World* world, uint16_t port);
void           net_server_poll_events(ServerContext* ctx);
void           net_server_tick(ServerContext* ctx, double dt);
void           net_server_destroy(ServerContext* ctx);

ClientSlot* net_server_slot_for_peer(ServerContext* ctx, ENetPeer* peer);

// Hard reset; performs CLIENT_LEFT and held-particle cleanup.
void net_server_kick_slot(ServerContext* ctx, ClientSlot* slot);

#endif // NET_SERVER_H
