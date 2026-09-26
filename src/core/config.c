#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdbool.h>

static bool parse_u32(const char* str, uint32_t* out) {
	errno = 0;
	char*         end = NULL;
	unsigned long value = strtoul(str, &end, 10);
	if (errno != 0 || end == str || *end != '\0' || value > UINT32_MAX) {
		return false;
	}
	*out = (uint32_t)value;
	return true;
}

static bool parse_u16(const char* str, uint16_t* out) {
	uint32_t value;
	if (!parse_u32(str, &value) || value > UINT16_MAX) return false;
	*out = (uint16_t)value;
	return true;
}

// Server
ServerConfig server_config_default(void) {
	ServerConfig cfg = {0};
	cfg.port = 4221;
	cfg.max_particles = 1024;
	cfg.max_constraints = 2048;
	snprintf(cfg.model_path, sizeof(cfg.model_path), "assets/model.json");
	return cfg;
}

ConfigParseResult server_config_parse(ServerConfig* config, int argc,
									  char** argv) {
	*config = server_config_default();

	for (int i = 1; i < argc; i++) {
		bool has_next = (i + 1 < argc);

		if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0)) {
			if (!has_next || !parse_u16(argv[++i], &config->port)) {
				fprintf(stderr,
						"error: --port requires a valid 16-bit number\n");
				return CONFIG_PARSE_ERROR;
			}
		} else if ((strcmp(argv[i], "--model") == 0 ||
					strcmp(argv[i], "-m") == 0)) {
			if (!has_next) {
				fprintf(stderr, "error: --model requires a path\n");
				return CONFIG_PARSE_ERROR;
			}
			snprintf(config->model_path, sizeof(config->model_path), "%s",
					 argv[++i]);
		} else if (strcmp(argv[i], "--max-particles") == 0) {
			if (!has_next || !parse_u32(argv[++i], &config->max_particles)) {
				fprintf(stderr,
						"error: --max-particles requires a valid number\n");
				return CONFIG_PARSE_ERROR;
			}
			if (config->max_particles > UINT16_MAX) {
				fprintf(stderr,
						"error: --max-particles cannot exceed %u (particle "
						"indices are 16-bit)\n",
						UINT16_MAX);
				return CONFIG_PARSE_ERROR;
			}
		} else if (strcmp(argv[i], "--max-constraints") == 0) {
			if (!has_next || !parse_u32(argv[++i], &config->max_constraints)) {
				fprintf(stderr,
						"error: --max-constraints requires a valid number\n");
				return CONFIG_PARSE_ERROR;
			}
			if (config->max_constraints > UINT16_MAX) {
				fprintf(stderr,
						"error: --max-constraints cannot exceed %u (constraint "
						"indices are 16-bit)\n",
						UINT16_MAX);
				return CONFIG_PARSE_ERROR;
			}
		} else if (strcmp(argv[i], "--help") == 0 ||
				   strcmp(argv[i], "-h") == 0) {
			printf("Usage: %s [options]\n", argv[0]);
			printf("  %-30s Server port (default: 4221)\n",
				   "-p, --port <port>");
			printf("  %-30s Model JSON file (default: model.json)\n",
				   "-m, --model <path>");
			printf("  %-30s World particle capacity (default: 1024)\n",
				   "--max-particles <count>");
			printf("  %-30s World constraint capacity (default: 2048)\n",
				   "--max-constraints <count>");
			return CONFIG_PARSE_HELP;
		} else {
			fprintf(stderr, "error: unrecognized argument '%s'\n", argv[i]);
			return CONFIG_PARSE_ERROR;
		}
	}
	return CONFIG_PARSE_OK;
}

void server_config_print(const ServerConfig* config) {
	printf("[config] port:             %" PRIu16 "\n", config->port);
	printf("[config] max_particles:    %" PRIu32 "\n", config->max_particles);
	printf("[config] max_constraints:  %" PRIu32 "\n", config->max_constraints);
	printf("[config] model_path:       %s\n", config->model_path);
}

// Client
ClientConfig client_config_default(void) {
	ClientConfig cfg = {0};
	snprintf(cfg.address, sizeof(cfg.address), "127.0.0.1");
	cfg.port = 4221;
	snprintf(cfg.model_path, sizeof(cfg.model_path), "assets/model.json");
	return cfg;
}

ConfigParseResult client_config_parse(ClientConfig* config, int argc,
									  char** argv) {
	*config = client_config_default();

	for (int i = 1; i < argc; i++) {
		bool has_next = (i + 1 < argc);

		if ((strcmp(argv[i], "--addr") == 0 || strcmp(argv[i], "-a") == 0)) {
			if (!has_next) {
				fprintf(stderr, "error: --addr requires a value\n");
				return CONFIG_PARSE_ERROR;
			}
			snprintf(config->address, sizeof(config->address), "%s", argv[++i]);
		} else if ((strcmp(argv[i], "--port") == 0 ||
					strcmp(argv[i], "-p") == 0)) {
			if (!has_next || !parse_u16(argv[++i], &config->port)) {
				fprintf(stderr,
						"error: --port requires a valid 16-bit number\n");
				return CONFIG_PARSE_ERROR;
			}
		} else if ((strcmp(argv[i], "--model") == 0 ||
					strcmp(argv[i], "-m") == 0)) {
			if (!has_next) {
				fprintf(stderr, "error: --model requires a path\n");
				return CONFIG_PARSE_ERROR;
			}
			snprintf(config->model_path, sizeof(config->model_path), "%s",
					 argv[++i]);
		} else if (strcmp(argv[i], "--help") == 0 ||
				   strcmp(argv[i], "-h") == 0) {
			printf("Usage: %s [options]\n", argv[0]);
			printf("  %-30s Server address (default: 127.0.0.1)\n",
				   "-a, --addr <ip>");
			printf("  %-30s Server port (default: 4221)\n",
				   "-p, --port <port>");
			printf("  %-30s Topology model, must match server's "
				   "(default: model.json)\n",
				   "-m, --model <path>");

			return CONFIG_PARSE_HELP;
		} else {
			fprintf(stderr, "error: unrecognized argument '%s'\n", argv[i]);
			return CONFIG_PARSE_ERROR;
		}
	}
	return CONFIG_PARSE_OK;
}

void client_config_print(const ClientConfig* config) {
	printf("[config] server:      %s:%" PRIu16 "\n", config->address,
		   config->port);
	printf("[config] model_path:  %s\n", config->model_path);
}
