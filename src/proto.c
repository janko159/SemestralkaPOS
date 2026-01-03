#include "proto.h"
#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

int proto_send(int fd, msg_type_t type, const void *payload, uint32_t len) {
    msg_hdr_t hdr;
    hdr.type = htonl((uint32_t)type);
    hdr.len  = htonl(len);

    if (net_write_full(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
        return -1;
    }

    if (len > 0) {
        if (!payload) {
            errno = EINVAL;
            return -1;
        }
        if (net_write_full(fd, payload, len) != (ssize_t)len) {
            return -1;
        }
    }

    return 0;
}

int proto_recv(int fd, msg_type_t *type, void *buf, uint32_t cap, uint32_t *out_len) {
    if (!type || !out_len) {
        errno = EINVAL;
        return -1;
    }

    msg_hdr_t hdr;
    ssize_t r = net_read_full(fd, &hdr, sizeof(hdr));
    if (r == 0) {
        // peer closed cleanly
        errno = 0;
        return 0;
    }
    if (r != (ssize_t)sizeof(hdr)) {
        return -1;
    }

    uint32_t n_type = ntohl(hdr.type);
    uint32_t n_len  = ntohl(hdr.len);

    *type = (msg_type_t)n_type;

    if (n_len == 0) {
        *out_len = 0;
        return 1;
    }

    if (n_len > cap) {
        errno = EMSGSIZE;
        return -1;
    }

    if (!buf) {
        errno = EINVAL;
        return -1;
    }

    if (net_read_full(fd, buf, n_len) != (ssize_t)n_len) {
        return -1;
    }

    *out_len = n_len;
    return 1;
}


