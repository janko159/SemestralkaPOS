CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -I./src
LDLIBS := -pthread

CLIENT := client
SERVER := server

COMMON_SRCS := src/net.c src/proto.c src/sim.c src/persist.c
CLIENT_SRCS := src/client.c $(COMMON_SRCS)
SERVER_SRCS := src/server.c $(COMMON_SRCS)

CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)
DEPS 				:= $(CLIENT_OBJS:.o=.d) $(SERVER_OBJS:.o=.d)

.PHONY: all clean client server

all: client server

client: $(CLIENT_OBJS)
	$(CC) -o $(CLIENT) $(CLIENT_OBJS) $(LDLIBS)

server: $(SERVER_OBJS)
	$(CC) -o $(SERVER) $(SERVER_OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	$(RM) $(CLIENT) $(SERVER) $(CLIENT_OBJS) $(SERVER_OBJS) $(DEPS)
