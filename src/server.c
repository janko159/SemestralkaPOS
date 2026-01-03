#include "net.h"
#include "proto.h"
#include "common.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    uint16_t port = 5050;

    int pocuvaci_socket = net_listen(port);
    if (pocuvaci_socket < 0) {
        perror("server: net_listen");
        return 1;
    }

    printf("SERVER: pocuvam na porte %u\n", (unsigned)port);

    int spojenie = net_accept(pocuvaci_socket);
    if (spojenie < 0) {
        perror("server: net_accept");
        close(pocuvaci_socket);
        return 1;
    }

    // 1) Prijmi HELLO
    msg_type_t typ;
    char buffer[64];
    uint32_t dlzka = 0;

    int rc = proto_recv(spojenie, &typ, buffer, (uint32_t)sizeof(buffer), &dlzka);
    if (rc <= 0 || typ != MSG_HELLO) {
        fprintf(stderr, "SERVER: ocakaval som HELLO\n");
        close(spojenie);
        close(pocuvaci_socket);
        return 1;
    }

    // 2) Posli HELLO_ACK
    const char *odpoved = "ok";
    if (proto_send(spojenie, MSG_HELLO_ACK, odpoved, (uint32_t)strlen(odpoved)) != 0) {
        perror("server: proto_send HELLO_ACK");
        close(spojenie);
        close(pocuvaci_socket);
        return 1;
    }
// 3) Prijmi START (pocet replikacii)
start_msg_t start;
uint32_t dlzka_start = 0;
msg_type_t typ_start;

rc = proto_recv(spojenie, &typ_start, &start, (uint32_t)sizeof(start), &dlzka_start);
if (rc <= 0 || typ_start != MSG_START || dlzka_start != sizeof(start)) {
    fprintf(stderr, "SERVER: ocakaval som START\n");
    close(spojenie);
    close(pocuvaci_socket);
    return 1;
}


uint32_t celkovo = ntohl(start.replikacie);
uint32_t k_max   = ntohl(start.k_max);

if (celkovo == 0) {
    fprintf(stderr, "SERVER: pocet replikacii nesmie byt 0\n");
    close(spojenie);
    close(pocuvaci_socket);
    return 1;
}

if (k_max == 0) {
    fprintf(stderr, "SERVER: K nesmie byt 0\n");
    close(spojenie);
    close(pocuvaci_socket);
    return 1;
}

// len aby sme videli, ze server to realne dostal
printf("SERVER: replikacie=%u, K=%u\n", (unsigned)celkovo, (unsigned)k_max);
fflush(stdout);

// START_ACK
const char *ok = "ok";
if (proto_send(spojenie, MSG_START_ACK, ok, (uint32_t)strlen(ok)) != 0) {
    perror("server: proto_send START_ACK");
    close(spojenie);
    close(pocuvaci_socket);
    return 1;
}

    // 3) Posielaj PROGRESS 1..5
    for (uint32_t aktualna = 1; aktualna <= celkovo; aktualna++) {
        progress_msg_t sprava;
        sprava.aktualna = htonl(aktualna);
        sprava.celkovo  = htonl(celkovo);

        if (proto_send(spojenie, MSG_PROGRESS, &sprava, (uint32_t)sizeof(sprava)) != 0) {
            perror("server: proto_send PROGRESS");
            break;
        }

        sleep(2); // 200ms, aby bolo vidno postup
    }

    // 4) DONE
    proto_send(spojenie, MSG_DONE, NULL, 0);

    close(spojenie);
    close(pocuvaci_socket);
    printf("SERVER: koniec\n");
    return 0;
}

