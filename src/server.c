#define _DEFAULT_SOURCE
#include "net.h"
#include "proto.h"
#include "common.h"
#include "sim.h"
#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;

    int pasivny_socket;

    int *klientske_sockety;
    int pocet_klientov;
    int kapacita_klientov;

    int admin_socket;

    int konfiguracia_je;
    int simulacia_bezi;
    int stop_flag;

    int typ_svetu;
    int sirka;
    int vyska;
    int pocet_replikacii;
    int hodnota_k;
    int mod_simulacie;

    int start_x_trasa;
    int start_y_trasa;

    double pravd_hore;
    double pravd_dole;
    double pravd_vlavo;
    double pravd_vpravo;

    char *policka;

} server_stav_t;

static void broadcast(server_stav_t *stav, int typ, const void *data, int dlzka) {
    pthread_mutex_lock(&stav->mutex);

    for (int i = 0; i < stav->pocet_klientov; i++) {
        (void)proto_posli(stav->klientske_sockety[i], typ, data, dlzka);
    }

    pthread_mutex_unlock(&stav->mutex);
}

static void callback_priebeh(int aktualna, int celkovo, void *user) {
    server_stav_t *stav = (server_stav_t *)user;

    sprava_priebeh_t sprava;

    sprava.aktualna_replikacia = aktualna;
    sprava.celkovy_pocet_replikacii = celkovo;

    broadcast(stav, MSG_PRIEBEH, &sprava, (int)sizeof(sprava));
}

static void callback_trasa_text(const char *text, void *user) {
    server_stav_t *stav = (server_stav_t *)user;

    if (text == NULL) {
        return;
    }

    broadcast(stav, MSG_TEXT, text, (int)strlen(text));
}

static void posli_chybu(int socket, const char *text) {
    if (text == NULL) {
        text = "chyba";
    }

    (void)proto_posli(socket, MSG_CHYBA, text, (int)strlen(text));
}

static int rozdel_tokeny(char *text, char **tokeny, int max_tokenov) {
    int pocet = 0;

    char *p = text;

    while (p != NULL && *p != '\0' && pocet < max_tokenov) {
        tokeny[pocet] = p;
        pocet++;

        char *oddelovac = strchr(p, '|');

        if (oddelovac == NULL) {
            break;
        }

        *oddelovac = '\0';
        p = oddelovac + 1;
    }

    return pocet;
}

static int parse_int(const char *s, int *out) {
    if (s == NULL || out == NULL) {
        return -1;
    }

    char *koniec = NULL;

    long v = strtol(s, &koniec, 10);

    if (koniec == s) {
        return -1;
    }

    *out = (int)v;

    return 0;
}

static int parse_double(const char *s, double *out) {
    if (s == NULL || out == NULL) {
        return -1;
    }

    char *koniec = NULL;

    double v = strtod(s, &koniec);

    if (koniec == s) {
        return -1;
    }

    *out = v;

    return 0;
}

static void pridaj_klienta(server_stav_t *stav, int socket) {
    pthread_mutex_lock(&stav->mutex);

    if (stav->pocet_klientov == stav->kapacita_klientov) {
        int nova_kapacita = (stav->kapacita_klientov == 0) ? 8 : stav->kapacita_klientov * 2;

        int *novy = (int *)realloc(stav->klientske_sockety, (size_t)nova_kapacita * sizeof(int));

        if (novy == NULL) {
            pthread_mutex_unlock(&stav->mutex);
            close(socket);
            return;
        }

        stav->klientske_sockety = novy;
        stav->kapacita_klientov = nova_kapacita;
    }

    stav->klientske_sockety[stav->pocet_klientov] = socket;
    stav->pocet_klientov++;

    if (stav->admin_socket == -1) {
        stav->admin_socket = socket;
    }

    pthread_mutex_unlock(&stav->mutex);
}

static void odstran_klienta(server_stav_t *stav, int socket) {
    pthread_mutex_lock(&stav->mutex);

    for (int i = 0; i < stav->pocet_klientov; i++) {
        if (stav->klientske_sockety[i] == socket) {
            stav->klientske_sockety[i] = stav->klientske_sockety[stav->pocet_klientov - 1];
            stav->pocet_klientov--;
            break;
        }
    }

    pthread_mutex_unlock(&stav->mutex);
}

static void *vlakno_simulacia(void *arg) {
    server_stav_t *stav = (server_stav_t *)arg;

    vysledok_simulacie_t vysledok;

    memset(&vysledok, 0, sizeof(vysledok));

    int rc = simulacia_spusti(
        stav->typ_svetu,
        stav->sirka,
        stav->vyska,
        stav->policka,
        stav->pocet_replikacii,
        stav->hodnota_k,
        stav->pravd_hore,
        stav->pravd_dole,
        stav->pravd_vlavo,
        stav->pravd_vpravo,
        stav->mod_simulacie,
        stav->start_x_trasa,
        stav->start_y_trasa,
        &stav->stop_flag,
        callback_priebeh,
        callback_trasa_text,
        stav,
        &vysledok
    );

    if (rc != 0) {
        broadcast(stav, MSG_CHYBA, "Simulacia zlyhala\n", (int)strlen("Simulacia zlyhala\n"));
        broadcast(stav, MSG_HOTOVO, NULL, 0);
        exit(0);
    }

    char *text_vysledku = persist_vytvor_text_vysledku(
        stav->typ_svetu,
        vysledok.sirka,
        vysledok.vyska,
        stav->hodnota_k,
        stav->pocet_replikacii,
        stav->pravd_hore,
        stav->pravd_dole,
        stav->pravd_vlavo,
        stav->pravd_vpravo,
        vysledok.policka,
        vysledok.priemerne_kroky,
        vysledok.pravdepodobnosti_k
    );

    if (text_vysledku != NULL) {
        broadcast(stav, MSG_TEXT, text_vysledku, (int)strlen(text_vysledku));
        free(text_vysledku);
    }

    simulacia_uvolni_vysledok(&vysledok);

    broadcast(stav, MSG_HOTOVO, NULL, 0);

    exit(0);
}

static int spracuj_prikaz(server_stav_t *stav, int socket, char *prikaz) {
    if (prikaz == NULL) {
        return -1;
    }

    char *koniec = strchr(prikaz, ';');

    if (koniec != NULL) {
        *koniec = '\0';
    }

    char *tokeny[16];

    int pocet = rozdel_tokeny(prikaz, tokeny, 16);

    if (pocet <= 0) {
        return -1;
    }

    if (strcmp(tokeny[0], "HELLO") == 0) {
        (void)proto_posli(socket, MSG_TEXT, "HELLO_ACK\n", (int)strlen("HELLO_ACK\n"));
        return 0;
    }

    if (strcmp(tokeny[0], "STOP") == 0) {
        pthread_mutex_lock(&stav->mutex);

        if (socket == stav->admin_socket) {
            stav->stop_flag = 1;
        }

        pthread_mutex_unlock(&stav->mutex);

        (void)proto_posli(socket, MSG_TEXT, "STOP_ACK\n", (int)strlen("STOP_ACK\n"));
        return 0;
    }

    if (strcmp(tokeny[0], "CREATE") == 0) {
        if (pocet < 11) {
            posli_chybu(socket, "CREATE: malo parametrov\n");
            return -1;
        }

        int typ_svetu = 0;
        int sirka = 0;
        int vyska = 0;
        int pocet_replikacii = 0;
        int hodnota_k = 0;
        int mod_simulacie = 0;
        int start_x_trasa = 0;
        int start_y_trasa = 0;

        double pravd_hore = 0.0;
        double pravd_dole = 0.0;
        double pravd_vlavo = 0.0;
        double pravd_vpravo = 0.0;

        if (parse_int(tokeny[1], &typ_svetu) != 0) {
            posli_chybu(socket, "CREATE: typ_svetu\n");
            return -1;
        }

        if (parse_int(tokeny[2], &sirka) != 0) {
            posli_chybu(socket, "CREATE: sirka\n");
            return -1;
        }

        if (parse_int(tokeny[3], &vyska) != 0) {
            posli_chybu(socket, "CREATE: vyska\n");
            return -1;
        }

        if (parse_int(tokeny[4], &pocet_replikacii) != 0) {
            posli_chybu(socket, "CREATE: replikacie\n");
            return -1;
        }

        if (parse_int(tokeny[5], &hodnota_k) != 0) {
            posli_chybu(socket, "CREATE: K\n");
            return -1;
        }

        if (parse_double(tokeny[6], &pravd_hore) != 0) {
            posli_chybu(socket, "CREATE: pravd_hore\n");
            return -1;
        }

        if (parse_double(tokeny[7], &pravd_dole) != 0) {
            posli_chybu(socket, "CREATE: pravd_dole\n");
            return -1;
        }

        if (parse_double(tokeny[8], &pravd_vlavo) != 0) {
            posli_chybu(socket, "CREATE: pravd_vlavo\n");
            return -1;
        }

        if (parse_double(tokeny[9], &pravd_vpravo) != 0) {
            posli_chybu(socket, "CREATE: pravd_vpravo\n");
            return -1;
        }

        if (parse_int(tokeny[10], &mod_simulacie) != 0) {
            posli_chybu(socket, "CREATE: mod\n");
            return -1;
        }

        if (pocet >= 13) {
            (void)parse_int(tokeny[11], &start_x_trasa);
            (void)parse_int(tokeny[12], &start_y_trasa);
        }

        double suma = pravd_hore + pravd_dole + pravd_vlavo + pravd_vpravo;

        if (suma < 0.999999 || suma > 1.000001) {
            posli_chybu(socket, "Suma pravdepodobnosti musi byt 1\n");
            return -1;
        }

        pthread_mutex_lock(&stav->mutex);

        if (stav->simulacia_bezi) {
            pthread_mutex_unlock(&stav->mutex);
            posli_chybu(socket, "Simulacia uz bezi\n");
            return -1;
        }

        free(stav->policka);

        stav->typ_svetu = typ_svetu;
        stav->sirka = sirka;
        stav->vyska = vyska;
        stav->pocet_replikacii = pocet_replikacii;
        stav->hodnota_k = hodnota_k;
        stav->mod_simulacie = mod_simulacie;

        stav->start_x_trasa = start_x_trasa;
        stav->start_y_trasa = start_y_trasa;

        stav->pravd_hore = pravd_hore;
        stav->pravd_dole = pravd_dole;
        stav->pravd_vlavo = pravd_vlavo;
        stav->pravd_vpravo = pravd_vpravo;

        stav->policka = (char *)malloc((size_t)(sirka * vyska));

        if (stav->policka == NULL) {
            pthread_mutex_unlock(&stav->mutex);
            posli_chybu(socket, "Nedostatok pamate na svet\n");
            return -1;
        }

        for (int i = 0; i < sirka * vyska; i++) {
            stav->policka[i] = '.';
        }

        stav->konfiguracia_je = 1;

        pthread_mutex_unlock(&stav->mutex);

        (void)proto_posli(socket, MSG_TEXT, "CREATE_ACK\n", (int)strlen("CREATE_ACK\n"));
        return 0;
    }

    if (strcmp(tokeny[0], "WORLD") == 0) {
        if (pocet < 2) {
            posli_chybu(socket, "WORLD: chyba\n");
            return -1;
        }

        pthread_mutex_lock(&stav->mutex);

        int sirka = stav->sirka;
        int vyska = stav->vyska;

        if (stav->policka == NULL || sirka <= 0 || vyska <= 0) {
            pthread_mutex_unlock(&stav->mutex);
            posli_chybu(socket, "WORLD: nie je CREATE\n");
            return -1;
        }

        int treba = sirka * vyska;

        if ((int)strlen(tokeny[1]) < treba) {
            pthread_mutex_unlock(&stav->mutex);
            posli_chybu(socket, "WORLD: zla dlzka\n");
            return -1;
        }

        for (int i = 0; i < treba; i++) {
            char c = tokeny[1][i];

            if (c == '#') {
                stav->policka[i] = '#';
            } else {
                stav->policka[i] = '.';
            }
        }

        pthread_mutex_unlock(&stav->mutex);

        (void)proto_posli(socket, MSG_TEXT, "WORLD_ACK\n", (int)strlen("WORLD_ACK\n"));
        return 0;
    }

    if (strcmp(tokeny[0], "START") == 0) {
        pthread_mutex_lock(&stav->mutex);

        if (!stav->konfiguracia_je) {
            pthread_mutex_unlock(&stav->mutex);
            posli_chybu(socket, "START: nie je CREATE\n");
            return -1;
        }

        if (stav->simulacia_bezi) {
            pthread_mutex_unlock(&stav->mutex);
            posli_chybu(socket, "START: uz bezi\n");
            return -1;
        }

        stav->simulacia_bezi = 1;
        stav->stop_flag = 0;

        pthread_mutex_unlock(&stav->mutex);

        (void)proto_posli(socket, MSG_TEXT, "START_ACK\n", (int)strlen("START_ACK\n"));

        pthread_t t;

        pthread_create(&t, NULL, vlakno_simulacia, stav);
        pthread_detach(t);

        return 0;
    }

    posli_chybu(socket, "Neznamy prikaz\n");
    return -1;
}

typedef struct {
    server_stav_t *stav;
    int socket;
} klient_vlakno_data_t;

static void *vlakno_klient(void *arg) {
    klient_vlakno_data_t *data = (klient_vlakno_data_t *)arg;

    server_stav_t *stav = data->stav;
    int socket = data->socket;

    free(data);

    while (1) {
        int typ = 0;
        char buffer[65536];
        int dlzka = 0;

        int rc = proto_prijmi(socket, &typ, buffer, (int)sizeof(buffer) - 1, &dlzka);

        if (rc == 0) {
            break;
        }

        if (rc < 0) {
            break;
        }

        buffer[dlzka] = '\0';

        if (typ == MSG_TEXT) {
            (void)spracuj_prikaz(stav, socket, buffer);
        }
    }

    odstran_klienta(stav, socket);

    close(socket);

    return NULL;
}

int main(int argc, char **argv) {
    int port = 5050;

    if (argc >= 3 && strcmp(argv[1], "--port") == 0) {
        port = atoi(argv[2]);
    }

    server_stav_t stav;

    memset(&stav, 0, sizeof(stav));

    pthread_mutex_init(&stav.mutex, NULL);

    stav.admin_socket = -1;

    stav.pasivny_socket = net_pocuvaj(port);

    if (stav.pasivny_socket < 0) {
        perror("server: net_pocuvaj");
        return 1;
    }

    printf("SERVER: pocuvam na porte %d\n", port);

    while (1) {
        int klient = net_prijmi_klienta(stav.pasivny_socket);

        if (klient < 0) {
            break;
        }

        printf("SERVER: klient pripojeny\n");

        pridaj_klienta(&stav, klient);

        klient_vlakno_data_t *data = (klient_vlakno_data_t *)malloc(sizeof(klient_vlakno_data_t));

        if (data == NULL) {
            close(klient);
            continue;
        }

        data->stav = &stav;
        data->socket = klient;

        pthread_t t;

        pthread_create(&t, NULL, vlakno_klient, data);
        pthread_detach(t);
    }

    close(stav.pasivny_socket);

    free(stav.klientske_sockety);
    free(stav.policka);

    pthread_mutex_destroy(&stav.mutex);

    return 0;
}

