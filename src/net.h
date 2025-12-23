#include <stdio.h>
#define NET_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

int net_listen(int port);
int net_accept(int listen_fd);
int net_connect(const char *host, int port);

ssize_t net_read_full(int fd, void *buf, size_t n);
ssize_t net_write_full(int fd, const void *buf, size_t n);
