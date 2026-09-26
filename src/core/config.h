#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CONFIG_PATH_MAX 256
#define CONFIG_ADDR_MAX 64

typedef enum {
	CONFIG_PARSE_OK,
	CONFIG_PARSE_HELP,   // --help was passed; caller should exit 0
	CONFIG_PARSE_ERROR,  // bad/missing argument; caller should exit nonzero
} ConfigParseResult;

typedef struct {
	uint16_t port;
	uint32_t max_particles;
	uint32_t max_constraints;
	char     model_path[CONFIG_PATH_MAX];
} ServerConfig;

typedef struct {
	char     address[CONFIG_ADDR_MAX];
	uint16_t port;
	char     model_path[CONFIG_PATH_MAX];
} ClientConfig;

ServerConfig      server_config_default(void);
ConfigParseResult server_config_parse(ServerConfig* config, int argc,
									  char** argv);
void              server_config_print(const ServerConfig* config);

ClientConfig      client_config_default(void);
ConfigParseResult client_config_parse(ClientConfig* config, int argc,
									  char** argv);
void              client_config_print(const ClientConfig* config);

#endif // CONFIG_H
