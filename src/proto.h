#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

typedef enum {
    MSG_HELLO = 1,
    MSG_HELLO_ACK = 2,

    MSG_START = 3,
    MSG_START_ACK = 4,

    MSG_PROGRESS = 5,
    MSG_DONE = 6
} msg_type_t;

typedef struct {
    uint32_t type;  // msg_type_t, encoded as uint32_t
    uint32_t len;   // payload length in bytes
} msg_hdr_t;

int proto_send(int fd, msg_type_t type, const void *payload, uint32_t len);
int proto_recv(int fd, msg_type_t *type, void *buf, uint32_t cap, uint32_t *out_len);

#endif

