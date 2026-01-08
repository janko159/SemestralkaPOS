#define _DEFAULT_SOURCE
#include "net.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

int net_pocuvaj(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        return -1;
    }

    int jedna = 1;

    (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &jedna, (socklen_t)sizeof(jedna));

    struct sockaddr_in adresa;

    memset(&adresa, 0, sizeof(adresa));

    adresa.sin_family = AF_INET;
    adresa.sin_addr.s_addr = htonl(INADDR_ANY);
    adresa.sin_port = htons((uint16_t)port);

    if (bind(socket_fd, (struct sockaddr *)&adresa, sizeof(adresa)) < 0) {
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, 8) < 0) {
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

int net_prijmi_klienta(int pasivny_socket) {
    struct sockaddr_in klient;

    socklen_t dlzka = sizeof(klient);

    int c = accept(pasivny_socket, (struct sockaddr *)&klient, &dlzka);

    return c;
}

int net_pripoj_sa(const char *host, int port) {
    char port_text[32];

    snprintf(port_text, sizeof(port_text), "%d", port);

    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;

    int rc = getaddrinfo(host, port_text, &hints, &res);

    if (rc != 0 || res == NULL) {
        return -1;
    }

    int socket_fd = -1;

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (socket_fd < 0) {
            continue;
        }

        if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return socket_fd;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(res);

    return -1;
}


