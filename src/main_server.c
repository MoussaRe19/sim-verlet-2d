#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <signal.h>

#define ENET_IMPLEMENTATION
#include "net/net_server.h"
#include "core/config.h"
#include "io/model_loader.h"

static volatile bool g_running = true;

static void handle_sigint(int sig) {
	(void)sig;
	g_running = false;
}

static double now_seconds(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char** argv) {
	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	ServerConfig      config;
	ConfigParseResult result = server_config_parse(&config, argc, argv);
	if (result == CONFIG_PARSE_HELP) return 0;
	if (result == CONFIG_PARSE_ERROR) return 1;
	server_config_print(&config);

	if (enet_initialize() != 0) {
		fprintf(stderr, "enet_initialize failed\n");
		return 1;
	}
	atexit(enet_deinitialize);

	size_t required_bytes =
		world_arena_size_required(config.max_particles, config.max_constraints);
	Arena world_arena = arena_create(required_bytes);

	Vector4 bounds = (Vector4){0.0f, 0.0f, 1280.0f, 720.0f};
	World*  world = world_create(&world_arena, 1024, 2048, bounds);

	if (!world_load_json(world, config.model_path)) {
		fprintf(stderr, "[server][json] failed to load model: %s\n", config.model_path);
		return 1;
	}

	ServerContext* server = net_server_create(world, config.port);
	if (!server) {
		fprintf(stderr, "failed to start server\n");
		return 1;
	}

	printf("[server] listening on port %" PRIu16 "\n", config.port);

	double last = now_seconds();
	double accumulator = 0.0;

	while (g_running) {
		double current = now_seconds();
		double frame_dt = current - last;
		last = current;
		// Prevents the Spiral of death
		if (frame_dt > 0.25) frame_dt = 0.25;
		accumulator += frame_dt;

		net_server_poll_events(server);

		while (accumulator >= NET_PHYSICS_TICK_DT) {
			net_server_tick(server, NET_PHYSICS_TICK_DT);
			accumulator -= NET_PHYSICS_TICK_DT;
		}
		const double SLEEP_MARGIN_S = 0.001;
		const double SLEEP_THRESHOLD_S = 0.002;

		double sleep_s = NET_PHYSICS_TICK_DT - accumulator;
		if (sleep_s > SLEEP_THRESHOLD_S) {
			double          target_sleep = sleep_s - SLEEP_MARGIN_S;
			struct timespec req = {
				.tv_sec = (time_t)target_sleep,
				.tv_nsec =
					(long)((target_sleep - (double)(time_t)target_sleep) * 1e9),
			};
			nanosleep(&req, NULL);
		}
	}

	printf("\n[server] shutting down...\n");
	net_server_destroy(server);
	arena_destroy(&world_arena);

	return 0;
}
