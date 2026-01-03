#include "net.h"
#include "proto.h"
#include "common.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h> 
#include <errno.h>

static uint32_t nacitaj_k(void) {
    char riadok[64];

    while (1) {
        printf("Zadaj K (max pocet krokov) (>0): ");
        fflush(stdout);

        if (fgets(riadok, (int)sizeof(riadok), stdin) == NULL) {
            return 0;
        }

        errno = 0;
        char *koniec = NULL;
        unsigned long x = strtoul(riadok, &koniec, 10);

        if (errno == 0 && koniec != riadok && x > 0 && x <= 100000000UL) {
            return (uint32_t)x;
        }

        printf("Neplatne cislo. Skus znova.\n");
    }
}


static uint32_t nacitaj_replikacie(void) {
    char riadok[64];

    while (1) {
        printf("Zadaj pocet replikacii (>0): ");
        fflush(stdout);

        if (fgets(riadok, (int)sizeof(riadok), stdin) == NULL) {
            return 0; // EOF / chyba
        }

        errno = 0;
        char *koniec = NULL;
        unsigned long x = strtoul(riadok, &koniec, 10);

        if (errno == 0 && koniec != riadok && x > 0 && x <= 1000000UL) {
            return (uint32_t)x;
        }

        printf("Neplatne cislo. Skus znova.\n");
    }
}


int main(void) {
    const char *host = "127.0.0.1";
    uint16_t port = 5050;

    int spojenie = net_connect(host, port);
    if (spojenie < 0) {
        perror("client: net_connect");
        return 1;
    }

    // Premenne pouzivame stale tie iste (ziadne redeklaracie)
    msg_type_t typ;
    char buffer[64];
    uint32_t dlzka = 0;
    int rc;

    // 1) HELLO
    const char *msg = "ahoj";
    if (proto_send(spojenie, MSG_HELLO, msg, (uint32_t)strlen(msg)) != 0) {
        perror("client: proto_send HELLO");
        close(spojenie);
        return 1;
    }

    // 2) HELLO_ACK
    dlzka = 0;
    rc = proto_recv(spojenie, &typ, buffer, (uint32_t)sizeof(buffer), &dlzka);
    if (rc <= 0 || typ != MSG_HELLO_ACK) {
        fprintf(stderr, "CLIENT: ocakaval som HELLO_ACK\n");
        close(spojenie);
        return 1;
    }

    // 3) START: posli pocet replikacii (zatial natvrdo)
    uint32_t kolko = nacitaj_replikacie();
if (kolko == 0) {
    fprintf(stderr, "CLIENT: nepodarilo sa nacitat pocet replikacii\n");
    close(spojenie);
    return 1;
}

uint32_t k = nacitaj_k();
if (k == 0) {
    fprintf(stderr, "CLIENT: nepodarilo sa nacitat K\n");
    close(spojenie);
    return 1;
}

start_msg_t start;
start.replikacie = htonl(kolko);
start.k_max      = htonl(k);

    if (proto_send(spojenie, MSG_START, &start, (uint32_t)sizeof(start)) != 0) {
        perror("client: proto_send START");
        close(spojenie);
        return 1;
    }

    // 4) START_ACK
    dlzka = 0;
    rc = proto_recv(spojenie, &typ, buffer, (uint32_t)sizeof(buffer), &dlzka);
    if (rc <= 0 || typ != MSG_START_ACK) {
        fprintf(stderr, "CLIENT: ocakaval som START_ACK\n");
        close(spojenie);
        return 1;
    }

    // 5) Prijimaj PROGRESS az po DONE
    while (1) {
        dlzka = 0;
        rc = proto_recv(spojenie, &typ, buffer, (uint32_t)sizeof(buffer), &dlzka);

        if (rc == 0) {
            printf("\nCLIENT: server zavrel spojenie\n");
            break;
        }
        if (rc < 0) {
            perror("client: proto_recv");
            break;
        }

        if (typ == MSG_PROGRESS) {
            if (dlzka != sizeof(progress_msg_t)) {
                continue;
            }

            progress_msg_t sprava;
            memcpy(&sprava, buffer, sizeof(sprava));

            uint32_t aktualna = ntohl(sprava.aktualna);
            uint32_t celkovo  = ntohl(sprava.celkovo);

            printf("\rReplikacia %u/%u", (unsigned)aktualna, (unsigned)celkovo);
            fflush(stdout);
        } else if (typ == MSG_DONE) {
            printf("\nHotovo.\n");
            break;
        }
    }

    close(spojenie);
    return 0;
}

