#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/memory_arena.h"

#define NET_PROTOCOL_VERSION 1
#define NET_HEADER_SIZE 2
#define NET_MAX_PLAYERS 16
#define NET_MAX_PACKET_SIZE 1400
#define NET_MAX_SNAPSHOT_PARTICLES                                     \
	((NET_MAX_PACKET_SIZE - NET_HEADER_SIZE - (int)sizeof(uint16_t)) / \
	 (2 * (int)sizeof(float)))

#define NET_SNAPSHOT_CHUNK_HEADER_SIZE \
	((int)sizeof(uint32_t) + 4 * (int)sizeof(uint16_t))
#define NET_MAX_PARTICLES_PER_CHUNK           \
	((NET_MAX_PACKET_SIZE - NET_HEADER_SIZE - \
	  NET_SNAPSHOT_CHUNK_HEADER_SIZE) /       \
	 (2 * (int)sizeof(float)))

// Simulation-level particle limit used to bound snapshot chunks.
// Increase only after re-checking the 20Hz bandwidth cost.
#define NET_MAX_WORLD_PARTICLES 2000

#define NET_MAX_SNAPSHOT_CHUNKS                                    \
	((NET_MAX_WORLD_PARTICLES + NET_MAX_PARTICLES_PER_CHUNK - 1) / \
	 NET_MAX_PARTICLES_PER_CHUNK)

#define NET_CHANNEL_RELIABLE 0
#define NET_CHANNEL_UNRELIABLE 1
#define NET_CHANNEL_COUNT 2

typedef enum {
	NET_MSG_NONE = 0,
	NET_MSG_ACCEPT_CLIENT = 1,    // Server -> Client (Assign Slot ID)
	NET_MSG_CLIENT_JOINED = 2,    // Server -> All Clients
	NET_MSG_CLIENT_LEFT = 3,      // Server -> All Clients
	NET_MSG_ROSTER_SNAPSHOT = 4,  // Server -> Client after accept
	NET_MSG_WORLD_SNAPSHOT = 5, // DEPRECATED: Legacy single-packet (max ~174
								// particles). Kept for unit tests.

	NET_MSG_WORLD_SNAPSHOT_CHUNK = 6, // Server -> All Clients (Pos state)

	NET_MSG_INTENT_GRAB = 7, // Client -> Server
	NET_MSG_INTENT_DRAG = 8, // Client -> Server
	NET_MSG_INTENT_RELEASE = 9, // Client -> Server
	NET_MSG_INTENT_TOGGLE_PIN = 10, // Client -> Server
	NET_MSG_PARTICLE_OWNERSHIP = 11, // Server -> Client
	NET_MSG_COUNT
} NetMessageType;

typedef struct {
	uint8_t* buffer;
	size_t   capacity;
	size_t   cursor;
} NetWriter;

typedef struct {
	const uint8_t* buffer;
	size_t         size;
	size_t         cursor;
} NetReader;

typedef struct {
	uint8_t client_id;
} NetAcceptClientPayload;

typedef struct {
	uint8_t client_id;
} NetClientJoinedPayload;

typedef struct {
	uint8_t count;
	uint8_t client_ids[NET_MAX_PLAYERS];
} NetRosterSnapshotPayload;

typedef struct NetSnapshotChunkPayload {
	uint32_t tick_index; // monotonic — used for our own staleness rejection
	uint16_t chunk_index;
	uint16_t total_chunks;
	uint16_t particle_start_index; // offset into the world's particle arrays
	uint16_t particle_count;
	float*   positions_x;
	float*   positions_y;
} NetSnapshotChunkPayload;

typedef struct {
	uint8_t client_id;
} NetClientLeftPayload;

typedef struct {
	uint16_t particle_index;
	float    target_x;
	float    target_y;
} NetIntentDragPayload;

typedef struct {
	uint16_t particle_count;
	float*   positions_x;
	float*   positions_y;
} NetSnapshotPayload;

typedef struct {
	uint16_t particle_index;
	float    target_x;
	float    target_y;
} NetIntentGrabPayload;

typedef struct {
	uint16_t particle_index;
} NetIntentReleasePayload;

typedef struct {
	uint16_t particle_index;
} NetIntentTogglePinPayload;

typedef struct NetParticleOwnershipPayload {
	uint16_t particle_index;
	uint8_t  owner_id; // WORLD_UNOWNED (0xFF) means released
} NetParticleOwnershipPayload;

void net_writer_init(NetWriter* w, uint8_t* buffer, size_t capacity);
void net_reader_init(NetReader* r, const uint8_t* buffer, size_t size);

bool net_write_u8(NetWriter* w, uint8_t val);
bool net_write_u16(NetWriter* w, uint16_t val);
bool net_write_u32(NetWriter* w, uint32_t val);
bool net_write_f32(NetWriter* w, float val);
bool net_write_header(NetWriter* w, NetMessageType type);

bool net_read_u8(NetReader* r, uint8_t* out_val);
bool net_read_u16(NetReader* r, uint16_t* out_val);
bool net_read_u32(NetReader* r, uint32_t* out_val);
bool net_read_f32(NetReader* r, float* out_val);

bool net_pack_accept_client(NetWriter*                    w,
							const NetAcceptClientPayload* payload);
bool net_unpack_accept_client(NetReader*              r,
							  NetAcceptClientPayload* out_payload);

bool net_pack_client_joined(NetWriter*                    w,
							const NetClientJoinedPayload* payload);
bool net_unpack_client_joined(NetReader*              r,
							  NetClientJoinedPayload* out_payload);

bool net_pack_roster_snapshot(NetWriter*                      w,
							  const NetRosterSnapshotPayload* payload);
bool net_unpack_roster_snapshot(NetReader*                r,
								NetRosterSnapshotPayload* out_payload);

bool net_pack_client_left(NetWriter* w, const NetClientLeftPayload* payload);
bool net_unpack_client_left(NetReader* r, NetClientLeftPayload* out_payload);

bool net_pack_intent_drag(NetWriter* w, const NetIntentDragPayload* payload);
bool net_unpack_intent_drag(NetReader* r, NetIntentDragPayload* out_payload);

bool net_pack_snapshot(NetWriter* w, const NetSnapshotPayload* payload);
bool net_unpack_snapshot_arena(Arena* arena, NetReader* r,
							   NetSnapshotPayload* out_payload);

bool net_pack_snapshot_chunk(NetWriter*                     w,
							 const NetSnapshotChunkPayload* payload);
bool net_unpack_snapshot_chunk_arena(Arena* arena, NetReader* r,
									 NetSnapshotChunkPayload* out_payload);

bool net_pack_intent_grab(NetWriter* w, const NetIntentGrabPayload* payload);
bool net_unpack_intent_grab(NetReader* r, NetIntentGrabPayload* out_payload);

bool net_pack_intent_release(NetWriter*                     w,
							 const NetIntentReleasePayload* payload);
bool net_unpack_intent_release(NetReader*               r,
							   NetIntentReleasePayload* out_payload);

bool net_pack_intent_toggle_pin(NetWriter*                       w,
								const NetIntentTogglePinPayload* payload);
bool net_unpack_intent_toggle_pin(NetReader*                 r,
								  NetIntentTogglePinPayload* out_payload);

bool net_pack_particle_ownership(NetWriter*                         w,
								 const NetParticleOwnershipPayload* payload);
bool net_unpack_particle_ownership(NetReader*                   r,
								   NetParticleOwnershipPayload* out_payload);

#endif // NET_PROTOCOL_H
