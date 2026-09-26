#ifndef NET_ROUTER_H
#define NET_ROUTER_H

#include "net_protocol.h"
#include "core/memory_arena.h"

typedef struct {
	uint8_t  client_id;      // server: sender's slot id. client: our id.
	void*    peer_handle;    // ENetPeer*
	void*    world_state;    // server: World*. client: RenderWorld*.
	Arena*   frame_arena;
	void*    user_data;      // server: ClientSlot*. client: NULL.
	uint64_t server_tick;    // server: current tick. client: unused.

	// client: points to the client's id. server: NULL.
	uint8_t* out_local_client_id;
} NetContext;

typedef bool (*NetMessageHandler)(NetContext* ctx, NetReader* reader);

// Implemented separately by server and client.
// Only the role-specific implementation should be linked.
bool net_dispatch_message(NetContext* ctx, const uint8_t* buffer, size_t size);

#endif
