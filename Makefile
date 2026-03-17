CC := gcc
CFLAGS := -Wall -Werror -O3 -std=c11 -D_POSIX_C_SOURCE=200809
SRC_DIR := src
BIN_DIR := bin

SERVER := $(BIN_DIR)/pcc_server
CLIENT := $(BIN_DIR)/pcc_client

.PHONY: all clean test

all: $(SERVER) $(CLIENT)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(SERVER): $(SRC_DIR)/pcc_server.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(CLIENT): $(SRC_DIR)/pcc_client.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN_DIR)

test: all
	@set -e; \
	port=5555; \
	tmpfile=$$(mktemp); \
	echo "abc123" > $$tmpfile; \
	timeout 5s $(SERVER) $$port > /tmp/pcc_server.log 2>&1 & \
	srv_pid=$$!; \
	sleep 1; \
	$(CLIENT) 127.0.0.1 $$port $$tmpfile > /tmp/pcc_client.log; \
	kill -INT $$srv_pid; \
	wait $$srv_pid || true; \
	grep "# of printable characters: 6" /tmp/pcc_client.log; \
	rm -f $$tmpfile
