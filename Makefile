CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -I./src -MMD -MP
LDFLAGS = -pthread

OBJS_SPOLOCNE = src/net.o src/proto.o src/sim.o src/persist.o
OBJS_CLIENT = src/client.o $(OBJS_SPOLOCNE)
OBJS_SERVER = src/server.o $(OBJS_SPOLOCNE)

.PHONY: all clean client server

all: client server

client: $(OBJS_CLIENT)
	$(CC) -o client $(OBJS_CLIENT) $(LDFLAGS)

server: $(OBJS_SERVER)
	$(CC) -o server $(OBJS_SERVER) $(LDFLAGS)

clean:
	rm -f client server src/*.o src/*.d

-include src/*.d

