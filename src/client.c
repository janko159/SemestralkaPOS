#include "net.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    const char *host = "127.0.0.1";
    const int port = 5050;

    int fd = net_connect(host, port);
    if (fd < 0) {
        perror("client: net_connect");
        return 1;
    }

    printf("CLIENT: connected to %s:%u\n", host, (unsigned)port);
    close(fd);
    return 0;
}

