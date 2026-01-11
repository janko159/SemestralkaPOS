#define _DEFAULT_SOURCE
#include "proto.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

typedef struct {
    int typ_siete;
    int dlzka_siete;
} hlavicka_t;

 int precitaj_presne(int socket, void *buffer, int dlzka) {
    int precitane = 0;

    while (precitane < dlzka) {
        ssize_t r = read(socket, (char *)buffer + precitane, (size_t)(dlzka - precitane));

        if (r == 0) {
            return 0;
        }

        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        precitane = precitane + (int)r;
    }

    return 1;
}

 int zapis_presne(int socket, const void *buffer, int dlzka) {
    int zapisane = 0;

    while (zapisane < dlzka) {
        ssize_t w = write(socket, (const char *)buffer + zapisane, (size_t)(dlzka - zapisane));

        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        zapisane = zapisane + (int)w;
    }

    return 0;
}

 int na_siet(int hodnota) {
    return (int)htonl((unsigned int)hodnota);
}

 int zo_siete(int hodnota) {
    return (int)ntohl((unsigned int)hodnota);
}

 int proto_posli(int socket, int typ_spravy, const void *data, int dlzka) {
    hlavicka_t hlavicka;

    hlavicka.typ_siete = na_siet(typ_spravy);
    hlavicka.dlzka_siete = na_siet(dlzka);

    if (zapis_presne(socket, &hlavicka, (int)sizeof(hlavicka)) != 0) {
        return -1;
    }

    if (dlzka > 0 && data != NULL) {
        if (zapis_presne(socket, data, dlzka) != 0) {
            return -1;
        }
    }

    return 0;
}

 int proto_prijmi(
    int socket,
    int *typ_spravy,
    void *buffer,
    int kapacita_buffera,
    int *dlzka_dat
) {
    hlavicka_t hlavicka;

    int rc = precitaj_presne(socket, &hlavicka, (int)sizeof(hlavicka));

    if (rc <= 0) {
        return rc;
    }

    int typ = zo_siete(hlavicka.typ_siete);
    int dlzka = zo_siete(hlavicka.dlzka_siete);

    if (typ_spravy != NULL) {
        *typ_spravy = typ;
    }

    if (dlzka_dat != NULL) {
        *dlzka_dat = dlzka;
    }

    if (dlzka <= 0) {
        return 1;
    }

    if (buffer != NULL && dlzka <= kapacita_buffera) {
        rc = precitaj_presne(socket, buffer, dlzka);
        return rc;
    }

    char odpad[512];

    int zostava = dlzka;

    while (zostava > 0) {
        int kus = zostava;

        if (kus > (int)sizeof(odpad)) {
            kus = (int)sizeof(odpad);
        }

        rc = precitaj_presne(socket, odpad, kus);

        if (rc <= 0) {
            return rc;
        }

        zostava = zostava - kus;
    }

    return 1;
}


