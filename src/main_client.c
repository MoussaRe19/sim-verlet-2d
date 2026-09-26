#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <signal.h>

#define ENET_IMPLEMENTATION
#include "net/net_client.h"
#include "core/config.h"
#include "io/model_loader.h"
#include "client/renderer.h"
#include "client/input.h"

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
	// TODO: fix later require a Shared model
	// btw client and server about game config
	const int screenWidth = 1280;
	const int screenHeight = 720;
	const int max_particles = 1024;
	const int max_constraints = 2048;

	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	ClientConfig      config;
	ConfigParseResult result = client_config_parse(&config, argc, argv);
	if (result == CONFIG_PARSE_HELP) return 0;
	if (result == CONFIG_PARSE_ERROR) return 1;
	client_config_print(&config);

	if (enet_initialize() != 0) {
		fprintf(stderr, "enet_initialize failed\n");
		return 1;
	}
	atexit(enet_deinitialize);

	size_t required =
		render_world_arena_size_required(max_particles, max_constraints);
	Arena client_arena = arena_create(required);

	RenderWorld* render_world = render_world_create(
		&client_arena, 1024, 2048,
		(Vector4){0.0f, 0.0f, (float)screenWidth, (float)screenHeight});

	if (!render_world_load_json(render_world, config.model_path)) {
		fprintf(stderr, "[client][json] failed to load model: %s\n",
				config.model_path);
		return 1;
	}

	ClientContext* client =
		net_client_connect(config.address, config.port, render_world);
	if (!client) {
		fprintf(stderr, "[client] failed to connect to %s:%" PRIu16 "\n",
				config.address, config.port);
		return 1;
	}

	printf("[client] connecting to %s:%" PRIu16 " (press Ctrl+C to stop)...\n",
		   config.address, config.port);

	RenderConfig rcfg = renderer_default_config();
	Vector4      bounds = render_world_get_bounds(render_world);

	InitWindow(bounds.z ? bounds.z : screenWidth,
			   bounds.w ? bounds.w : screenHeight, "sim-verlt-2D");
	SetTargetFPS(60);

	double last = now_seconds();

	while (g_running && !WindowShouldClose()) {
		double current = now_seconds();
		double frame_dt = current - last;
		last = current;

		// 1. Drain incoming snapshot chunks from server
		net_client_update(client, frame_dt);

		// 2. Handle discrete user inputs (clicks)
		input_update(client, render_world_get_world(render_world), frame_dt);

		// 5. Render frame
		BeginDrawing();
		{
			ClearBackground(BLACK);
			renderer_draw_world(render_world, &rcfg, client->local_client_id);
		}

		// Clear the FPS oerlay area
		DrawRectangle(0, 0, 220, 60, BLACK);
		DrawFPS(10, 10);
		EndDrawing();
	}

	printf("\n[client] disconnecting...\n");
	net_client_disconnect(client);
	arena_destroy(&client_arena);

	CloseWindow();

	return 0;
}
