#define PROTO_H

#include <stdint.h>

typedef enum{
  MSG_HELLO = 1,
  MSG_HELLO_ACK = 2,
}msg_type_t;

int proto_send(int fd, msg_type_t type, const void *payload, int len);
int proto_recv(int fd, msg_type_t *type, void *buf, int cap, int *out_len);

