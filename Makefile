CC      ?= gcc
CFLAGS  := -Wall -Wextra -std=c99 -O2 -D_GNU_SOURCE -Isrc -Ithird_party
LDFLAGS := -lm

RAYLIB_LDFLAGS := -lraylib -lGL -lpthread -ldl -lrt -lX11

BUILD_DIR := build

# Source groups
CORE_SRCS  := $(wildcard src/core/*.c)
IO_SRCS    := $(wildcard src/io/*.c) third_party/yyjson/yyjson.c
UTILS_SRCS := $(wildcard src/utils/*.c)
CLIENT_SRCS:= $(wildcard src/client/*.c)

NET_COMMON_SRCS := src/net/net_protocol.c
NET_SERVER_SRCS := src/net/net_server.c src/net/net_router_server.c
NET_CLIENT_SRCS := src/net/net_client.c src/net/net_router_client.c

# Executable targets
SERVER_BIN := sim_server
CLIENT_BIN := sim_client

# Object files mapping
SERVER_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, \
    $(CORE_SRCS) $(IO_SRCS) $(UTILS_SRCS) $(NET_COMMON_SRCS) $(NET_SERVER_SRCS) src/main_server.c)
CLIENT_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, \
    $(CORE_SRCS) $(IO_SRCS) $(UTILS_SRCS) $(NET_COMMON_SRCS) $(NET_CLIENT_SRCS) $(CLIENT_SRCS) src/main_client.c)

ALL_OBJS := $(sort $(SERVER_OBJS) $(CLIENT_OBJS))
DEPS     := $(ALL_OBJS:.o=.d)

.PHONY: all clean bear bear-full

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS) $(RAYLIB_LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS) $(RAYLIB_LDFLAGS)

-include $(DEPS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Bear integration for compile_commands.json generation
bear:
	bear -a $(MAKE) all

bear-full: clean
	bear $(MAKE) all

clean:
	rm -rf $(BUILD_DIR) $(SERVER_BIN) $(CLIENT_BIN)
