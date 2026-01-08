#define _DEFAULT_SOURCE
#include "net.h"
#include "proto.h"
#include "common.h"
#include "persist.h"
#include "sim.h"
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

typedef struct {
    pthread_mutex_t mutex;

    int socket;
    int bezi;

    int zobrazenie;

    char vystupny_subor[256];

    char *posledny_vysledok_text;

} klient_stav_t;

static void vycisti_stdin(void) {
    int c;

    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

static void nacitaj_riadok(char *buffer, int kapacita) {
    if (fgets(buffer, kapacita, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    size_t n = strlen(buffer);

    while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
        buffer[n - 1] = '\0';
        n--;
    }
}

static void vykresli_vysledok_podla_zobrazenia(const char *text, int zobrazenie) {
    if (text == NULL) {
        return;
    }

    int vypisujem = 1;

    if (zobrazenie == ZOBRAZ_AVG) {
        vypisujem = 1;
    } else if (zobrazenie == ZOBRAZ_PROB) {
        vypisujem = 1;
    } else {
        vypisujem = 1;
    }

    if (zobrazenie == ZOBRAZ_OBE) {
        printf("%s", text);
        return;
    }

    const char *p = text;

    while (*p) {
        const char *riadok_koniec = strchr(p, '\n');

        size_t dlzka = (riadok_koniec == NULL) ? strlen(p) : (size_t)(riadok_koniec - p + 1);

        if (strncmp(p, "AVG_STEPS", 9) == 0) {
            if (zobrazenie == ZOBRAZ_AVG) {
                vypisujem = 1;
            } else {
                vypisujem = 0;
            }
        }

        if (strncmp(p, "PROB_K", 6) == 0) {
            if (zobrazenie == ZOBRAZ_PROB) {
                vypisujem = 1;
            } else {
                vypisujem = 0;
            }
        }

        if (strncmp(p, "GRID", 4) == 0) {
            vypisujem = 1;
        }

        if (vypisujem) {
            fwrite(p, 1, dlzka, stdout);
        }

        if (riadok_koniec == NULL) {
            break;
        }

        p = riadok_koniec + 1;
    }
}

static void *vlakno_prijem(void *arg) {
    klient_stav_t *stav = (klient_stav_t *)arg;

    while (1) {
        int typ = 0;

        char buffer[65536];

        int dlzka = 0;

        int rc = proto_prijmi(stav->socket, &typ, buffer, (int)sizeof(buffer) - 1, &dlzka);

        if (rc == 0) {
            printf("\nCLIENT: server zavrel spojenie\n");
            break;
        }

        if (rc < 0) {
            perror("client: proto_prijmi");
            break;
        }

        buffer[dlzka] = '\0';

        if (typ == MSG_PRIEBEH) {
            if (dlzka == (int)sizeof(sprava_priebeh_t)) {
                sprava_priebeh_t sprava;

                memcpy(&sprava, buffer, sizeof(sprava));

                printf("\rReplikacia %d/%d", sprava.aktualna_replikacia, sprava.celkovy_pocet_replikacii);
                fflush(stdout);
            }
        } else if (typ == MSG_TEXT) {
            printf("\n");

            if (strstr(buffer, "# RandomWalk result") != NULL) {
                pthread_mutex_lock(&stav->mutex);

                free(stav->posledny_vysledok_text);

                stav->posledny_vysledok_text = strdup(buffer);

                if (stav->posledny_vysledok_text != NULL) {
                    vykresli_vysledok_podla_zobrazenia(stav->posledny_vysledok_text, stav->zobrazenie);
                }

                if (stav->vystupny_subor[0] != '\0' && stav->posledny_vysledok_text != NULL) {
                    (void)persist_uloz_text_do_suboru(stav->vystupny_subor, stav->posledny_vysledok_text);
                    printf("\nCLIENT: ulozene do %s\n", stav->vystupny_subor);
                }

                pthread_mutex_unlock(&stav->mutex);
            } else {
                printf("%s", buffer);
            }
        } else if (typ == MSG_CHYBA) {
            fprintf(stderr, "\nCHYBA: %s", buffer);
        } else if (typ == MSG_HOTOVO) {
            printf("\nCLIENT: hotovo\n");
            break;
        }
    }

    pthread_mutex_lock(&stav->mutex);
    stav->bezi = 0;
    pthread_mutex_unlock(&stav->mutex);

    return NULL;
}

static void posli_prikaz(int socket, const char *text) {
    if (text == NULL) {
        return;
    }

    (void)proto_posli(socket, MSG_TEXT, text, (int)strlen(text));
}

static void *vlakno_vstup(void *arg) {
    klient_stav_t *stav = (klient_stav_t *)arg;

    while (1) {
        pthread_mutex_lock(&stav->mutex);

        int bezi = stav->bezi;

        pthread_mutex_unlock(&stav->mutex);

        if (!bezi) {
            break;
        }

        printf("\nPrikaz (avg/prob/obe/stop/quit): ");

        char riadok[128];

        nacitaj_riadok(riadok, (int)sizeof(riadok));

        if (strcmp(riadok, "avg") == 0) {
            pthread_mutex_lock(&stav->mutex);
            stav->zobrazenie = ZOBRAZ_AVG;
            if (stav->posledny_vysledok_text != NULL) {
                printf("\n");
                vykresli_vysledok_podla_zobrazenia(stav->posledny_vysledok_text, stav->zobrazenie);
            }
            pthread_mutex_unlock(&stav->mutex);
        } else if (strcmp(riadok, "prob") == 0) {
            pthread_mutex_lock(&stav->mutex);
            stav->zobrazenie = ZOBRAZ_PROB;
            if (stav->posledny_vysledok_text != NULL) {
                printf("\n");
                vykresli_vysledok_podla_zobrazenia(stav->posledny_vysledok_text, stav->zobrazenie);
            }
            pthread_mutex_unlock(&stav->mutex);
        } else if (strcmp(riadok, "obe") == 0) {
            pthread_mutex_lock(&stav->mutex);
            stav->zobrazenie = ZOBRAZ_OBE;
            if (stav->posledny_vysledok_text != NULL) {
                printf("\n");
                vykresli_vysledok_podla_zobrazenia(stav->posledny_vysledok_text, stav->zobrazenie);
            }
            pthread_mutex_unlock(&stav->mutex);
        } else if (strcmp(riadok, "stop") == 0) {
            posli_prikaz(stav->socket, "STOP;");
        } else if (strcmp(riadok, "quit") == 0) {
            shutdown(stav->socket, SHUT_RDWR);
            close(stav->socket);
            break;
        }
    }

    return NULL;
}

static int spusti_server_proces(int port) {
    pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        char port_text[32];

        snprintf(port_text, sizeof(port_text), "%d", port);

        execl("./server", "server", "--port", port_text, (char *)NULL);

        _exit(1);
    }

    return (int)pid;
}

static int vyber_port_pre_server(void) {
    int port = 5000 + (getpid() % 20000);
    return port;
}

static void vytvor_novu_simulaciu(void) {
    int typ_svetu = 0;

    printf("Typ sveta (0=torus bez prekazok, 1=svet s prekazkami): ");
    scanf("%d", &typ_svetu);
    vycisti_stdin();

    int sirka = 0;
    int vyska = 0;

    char *policka = NULL;

    if (typ_svetu == SVET_TORUS) {
        printf("Sirka: ");
        scanf("%d", &sirka);
        printf("Vyska: ");
        scanf("%d", &vyska);
        vycisti_stdin();

        policka = (char *)malloc((size_t)(sirka * vyska));

        if (policka == NULL) {
            printf("Nedostatok pamate\n");
            return;
        }

        for (int i = 0; i < sirka * vyska; i++) {
            policka[i] = '.';
        }
    } else {
        printf("Svet s prekazkami: 1=nacitat zo suboru, 2=nahodne generovat: ");

        int volba = 0;

        scanf("%d", &volba);
        vycisti_stdin();

        if (volba == 1) {
            char cesta[256];

            printf("Cesta k suboru sveta: ");
            nacitaj_riadok(cesta, (int)sizeof(cesta));

            if (persist_nacitaj_svet(cesta, &sirka, &vyska, &policka) != 0) {
                printf("Nepodarilo sa nacitat svet\n");
                return;
            }

            int ox = sirka / 2;
            int oy = vyska / 2;

            policka[oy * sirka + ox] = '.';
        } else {
            printf("Sirka: ");
            scanf("%d", &sirka);
            printf("Vyska: ");
            scanf("%d", &vyska);
            vycisti_stdin();

            printf("Percento prekazok (0-90): ");
            int percento = 0;
            scanf("%d", &percento);
            vycisti_stdin();

            srand((unsigned int)time(NULL));

            if (simulacia_vygeneruj_prekazky(sirka, vyska, percento, &policka) != 0) {
                printf("Nepodarilo sa vygenerovat svet s dosiahnutelnostou\n");
                return;
            }
        }
    }

    int pocet_replikacii = 0;
    int hodnota_k = 0;
    int mod = 0;

    double pravd_hore = 0.25;
    double pravd_dole = 0.25;
    double pravd_vlavo = 0.25;
    double pravd_vpravo = 0.25;

    printf("Pocet replikacii: ");
    scanf("%d", &pocet_replikacii);
    printf("Hodnota K: ");
    scanf("%d", &hodnota_k);
    printf("Mod (0=sumarny, 1=interaktivny): ");
    scanf("%d", &mod);
    vycisti_stdin();

    printf("Pravdepodobnost hore: ");
    scanf("%lf", &pravd_hore);
    printf("Pravdepodobnost dole: ");
    scanf("%lf", &pravd_dole);
    printf("Pravdepodobnost vlavo: ");
    scanf("%lf", &pravd_vlavo);
    printf("Pravdepodobnost vpravo: ");
    scanf("%lf", &pravd_vpravo);
    vycisti_stdin();

    double suma = pravd_hore + pravd_dole + pravd_vlavo + pravd_vpravo;

    if (suma < 0.999999 || suma > 1.000001) {
        printf("Suma pravdepodobnosti musi byt 1\n");
        free(policka);
        return;
    }

    int start_x_trasa = sirka / 2;
    int start_y_trasa = vyska / 2;

    if (mod == MOD_INTERAKTIVNY) {
        int dx = 0;
        int dy = 0;

        printf("Interaktivna trasa - start relativne ku stredu (dx dy): ");
        scanf("%d %d", &dx, &dy);
        vycisti_stdin();

        start_x_trasa = (sirka / 2) + dx;
        start_y_trasa = (vyska / 2) + dy;

        if (start_x_trasa < 0) {
            start_x_trasa = 0;
        }

        if (start_x_trasa >= sirka) {
            start_x_trasa = sirka - 1;
        }

        if (start_y_trasa < 0) {
            start_y_trasa = 0;
        }

        if (start_y_trasa >= vyska) {
            start_y_trasa = vyska - 1;
        }
    }

    char vystup[256];

    printf("Subor pre ulozenie vysledku: ");
    nacitaj_riadok(vystup, (int)sizeof(vystup));

    int port = vyber_port_pre_server();

    int pid = spusti_server_proces(port);

    if (pid < 0) {
        printf("Nepodarilo sa spustit server proces\n");
        free(policka);
        return;
    }

    sleep(1);

    int socket = net_pripoj_sa("127.0.0.1", port);

    if (socket < 0) {
        printf("Nepodarilo sa pripojit na server\n");
        free(policka);
        return;
    }

    klient_stav_t stav;

    memset(&stav, 0, sizeof(stav));

    pthread_mutex_init(&stav.mutex, NULL);

    stav.socket = socket;
    stav.bezi = 1;
    stav.zobrazenie = ZOBRAZ_AVG;

    snprintf(stav.vystupny_subor, sizeof(stav.vystupny_subor), "%s", vystup);

    posli_prikaz(socket, "HELLO;");

    int dlzka_mrizky = sirka * vyska;

    char *mrizka_text = (char *)malloc((size_t)dlzka_mrizky + 32);

    if (mrizka_text == NULL) {
        free(policka);
        close(socket);
        return;
    }

    for (int i = 0; i < dlzka_mrizky; i++) {
        mrizka_text[i] = policka[i];
    }

    mrizka_text[dlzka_mrizky] = '\0';

    char prikaz_create[512];

    snprintf(
        prikaz_create,
        sizeof(prikaz_create),
        "CREATE|%d|%d|%d|%d|%d|%.10f|%.10f|%.10f|%.10f|%d|%d|%d;",
        typ_svetu,
        sirka,
        vyska,
        pocet_replikacii,
        hodnota_k,
        pravd_hore,
        pravd_dole,
        pravd_vlavo,
        pravd_vpravo,
        mod,
        start_x_trasa,
        start_y_trasa
    );

    posli_prikaz(socket, prikaz_create);

    char prikaz_world_prefix[8] = "WORLD|";

    char *prikaz_world = (char *)malloc((size_t)dlzka_mrizky + 16);

    if (prikaz_world == NULL) {
        free(mrizka_text);
        free(policka);
        close(socket);
        return;
    }

    strcpy(prikaz_world, prikaz_world_prefix);
    strcat(prikaz_world, mrizka_text);
    strcat(prikaz_world, ";");

    posli_prikaz(socket, prikaz_world);

    free(prikaz_world);
    free(mrizka_text);
    free(policka);

    posli_prikaz(socket, "START;");

    pthread_t t_prijem;
    pthread_t t_vstup;

    pthread_create(&t_prijem, NULL, vlakno_prijem, &stav);
    pthread_create(&t_vstup, NULL, vlakno_vstup, &stav);

    pthread_join(t_prijem, NULL);

    pthread_mutex_lock(&stav.mutex);
    stav.bezi = 0;
    pthread_mutex_unlock(&stav.mutex);

    pthread_join(t_vstup, NULL);

    pthread_mutex_destroy(&stav.mutex);

    free(stav.posledny_vysledok_text);

    close(socket);

    (void)waitpid((pid_t)pid, NULL, 0);
}

static void pripoj_sa_k_simulacii(void) {
    char host[128];

    printf("Host (napr. 127.0.0.1): ");
    nacitaj_riadok(host, (int)sizeof(host));

    int port = 0;

    printf("Port: ");
    scanf("%d", &port);
    vycisti_stdin();

    int socket = net_pripoj_sa(host, port);

    if (socket < 0) {
        printf("Nepodarilo sa pripojit\n");
        return;
    }

    klient_stav_t stav;

    memset(&stav, 0, sizeof(stav));

    pthread_mutex_init(&stav.mutex, NULL);

    stav.socket = socket;
    stav.bezi = 1;
    stav.zobrazenie = ZOBRAZ_AVG;

    stav.vystupny_subor[0] = '\0';

    posli_prikaz(socket, "HELLO;");

    pthread_t t_prijem;
    pthread_t t_vstup;

    pthread_create(&t_prijem, NULL, vlakno_prijem, &stav);
    pthread_create(&t_vstup, NULL, vlakno_vstup, &stav);

    pthread_join(t_prijem, NULL);

    pthread_mutex_lock(&stav.mutex);
    stav.bezi = 0;
    pthread_mutex_unlock(&stav.mutex);

    pthread_join(t_vstup, NULL);

    pthread_mutex_destroy(&stav.mutex);

    free(stav.posledny_vysledok_text);

    close(socket);
}

static void opatovne_spustenie_simulacie(void) {
    char vstup[256];

    printf("Subor s ulozenym vysledkom (z ktoreho sa nacta konfiguracia): ");
    nacitaj_riadok(vstup, (int)sizeof(vstup));

    int typ_svetu = 0;
    int sirka = 0;
    int vyska = 0;
    int hodnota_k = 0;

    double pravd_hore = 0.25;
    double pravd_dole = 0.25;
    double pravd_vlavo = 0.25;
    double pravd_vpravo = 0.25;

    char *policka = NULL;

    if (persist_nacitaj_konfiguraciu_zo_suboru_vysledku(
        vstup,
        &typ_svetu,
        &sirka,
        &vyska,
        &hodnota_k,
        &pravd_hore,
        &pravd_dole,
        &pravd_vlavo,
        &pravd_vpravo,
        &policka
    ) != 0) {
        printf("Nepodarilo sa nacitat konfiguraciu\n");
        return;
    }

    int pocet_replikacii = 0;

    printf("Novy pocet replikacii: ");
    scanf("%d", &pocet_replikacii);
    vycisti_stdin();

    char vystup[256];

    printf("Novy vystupny subor: ");
    nacitaj_riadok(vystup, (int)sizeof(vystup));

    int port = vyber_port_pre_server();

    int pid = spusti_server_proces(port);

    if (pid < 0) {
        free(policka);
        return;
    }

    sleep(1);

    int socket = net_pripoj_sa("127.0.0.1", port);

    if (socket < 0) {
        free(policka);
        return;
    }

    klient_stav_t stav;

    memset(&stav, 0, sizeof(stav));

    pthread_mutex_init(&stav.mutex, NULL);

    stav.socket = socket;
    stav.bezi = 1;
    stav.zobrazenie = ZOBRAZ_AVG;

    snprintf(stav.vystupny_subor, sizeof(stav.vystupny_subor), "%s", vystup);

    int mod = MOD_SUMARNY;

    int start_x_trasa = sirka / 2;
    int start_y_trasa = vyska / 2;

    char prikaz_create[512];

    snprintf(
        prikaz_create,
        sizeof(prikaz_create),
        "CREATE|%d|%d|%d|%d|%d|%.10f|%.10f|%.10f|%.10f|%d|%d|%d;",
        typ_svetu,
        sirka,
        vyska,
        pocet_replikacii,
        hodnota_k,
        pravd_hore,
        pravd_dole,
        pravd_vlavo,
        pravd_vpravo,
        mod,
        start_x_trasa,
        start_y_trasa
    );

    posli_prikaz(socket, prikaz_create);

    int dlzka_mrizky = sirka * vyska;

    char *mrizka_text = (char *)malloc((size_t)dlzka_mrizky + 1);

    if (mrizka_text == NULL) {
        free(policka);
        close(socket);
        return;
    }

    for (int i = 0; i < dlzka_mrizky; i++) {
        mrizka_text[i] = policka[i];
    }

    mrizka_text[dlzka_mrizky] = '\0';

    char *prikaz_world = (char *)malloc((size_t)dlzka_mrizky + 16);

    if (prikaz_world == NULL) {
        free(mrizka_text);
        free(policka);
        close(socket);
        return;
    }

    strcpy(prikaz_world, "WORLD|");
    strcat(prikaz_world, mrizka_text);
    strcat(prikaz_world, ";");

    posli_prikaz(socket, prikaz_world);

    free(prikaz_world);
    free(mrizka_text);
    free(policka);

    posli_prikaz(socket, "START;");

    pthread_t t_prijem;
    pthread_t t_vstup;

    pthread_create(&t_prijem, NULL, vlakno_prijem, &stav);
    pthread_create(&t_vstup, NULL, vlakno_vstup, &stav);

    pthread_join(t_prijem, NULL);

    pthread_mutex_lock(&stav.mutex);
    stav.bezi = 0;
    pthread_mutex_unlock(&stav.mutex);

    pthread_join(t_vstup, NULL);

    pthread_mutex_destroy(&stav.mutex);

    free(stav.posledny_vysledok_text);

    close(socket);

    (void)waitpid((pid_t)pid, NULL, 0);
}

int main(void) {
    while (1) {
        printf("\n=== MENU ===\n");
        printf("1) nova simulacia\n");
        printf("2) pripojenie k simulacii\n");
        printf("3) opatovne spustenie simulacie zo suboru\n");
        printf("4) koniec\n");
        printf("Volba: ");

        int volba = 0;

        scanf("%d", &volba);
        vycisti_stdin();

        if (volba == 1) {
            vytvor_novu_simulaciu();
        } else if (volba == 2) {
            pripoj_sa_k_simulacii();
        } else if (volba == 3) {
            opatovne_spustenie_simulacie();
        } else if (volba == 4) {
            break;
        }
    }

    return 0;
}

