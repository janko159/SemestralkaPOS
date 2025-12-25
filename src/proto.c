#include "proto.h"
#include <errno.h>

int proto_send(int fd, msg_type_t type, const void *payload, int len) {
    (void)fd; (void)type; (void)payload; (void)len;
    errno = ENOSYS;
    return -1;
}

int proto_recv(int fd, msg_type_t *type, void *buf, int cap, int *out_len) {
    (void)fd; (void)type; (void)buf; (void)cap; (void)out_len;
    errno = ENOSYS;
    return -1;
}

