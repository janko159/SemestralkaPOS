#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    const uint16_t port = 5050;

    int listen_fd = net_listen(port);
    if (listen_fd < 0) {
        perror("server: net_listen");
        return 1;
    }

    printf("SERVER: listening on port %u\n", (unsigned)port);

    int cfd = net_accept(listen_fd);
    if (cfd < 0) {
        perror("server: net_accept");
        close(listen_fd);
        return 1;
    }

    printf("SERVER: client connected\n");

    close(cfd);
    close(listen_fd);
    printf("SERVER: done\n");
    return 0;
}

