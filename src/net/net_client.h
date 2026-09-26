#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "net_router.h"
#include "client/renderer.h"
#include "enet/enet.h"

#define NET_CLIENT_SENTINEL_ID 0xFF
#define NET_DRAG_SEND_RATE_HZ 30.0
#define NET_DRAG_SEND_INTERVAL (1.0 / NET_DRAG_SEND_RATE_HZ)

typedef struct ClientContext {
	ENetHost* host;
	ENetPeer* server_peer;
	uint8_t   local_client_id;

	RenderWorld* render_world; // Not owned
	Arena        frame_arena;

	int32_t held_particle_index; // holding nothing = -1
	double  drag_send_accumulator;
} ClientContext;

ClientContext* net_client_connect(const char* address, uint16_t port,
								  RenderWorld* render_world);
void           net_client_update(ClientContext* ctx, double dt);
void           net_client_disconnect(ClientContext* ctx);
bool           net_client_is_accepted(const ClientContext* ctx);

void net_client_send_intent_grab(ClientContext* ctx, uint16_t particle_index,
								 float target_x, float target_y);
void net_client_send_intent_drag(ClientContext* ctx, uint16_t particle_index,
								 float target_x, float target_y);
void net_client_send_intent_release(ClientContext* ctx,
									uint16_t       particle_index);
void net_client_send_intent_toggle_pin(ClientContext* ctx,
									   uint16_t       particle_index);

// Sends drag updates while a particle is held; call every frame.
void net_client_update_drag(ClientContext* ctx, double dt, float target_x,
							float target_y);

#endif // NET_CLIENT_H
