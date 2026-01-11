#define _DEFAULT_SOURCE
#include "net.h"
#include "proto.h"
#include "common.h"
#include "persist.h"
#include "sim.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    pthread_mutex_t mutex;

    int socket;
    int bezi;

    int zobrazenie;
    int mod_simulacie;

    char vystupny_subor[256];

    char *posledny_vysledok_text;

    /* --- Interaktivne vykreslovanie --- */
    int typ_svetu;
    int sirka;
    int vyska;

    char *policka;              /* sirka*vyska, '.' alebo '#' */
    unsigned char *navstivene;  /* sirka*vyska, 0/1 */

    int ox;
    int oy;

    int akt_rep;
    int celk_rep;

    int trasa_rep;
    int krok;
    int curx;
    int cury;

    int trasa_aktivna; /* 0/1 */
} klient_stav_t;

void vycisti_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

void nacitaj_riadok(char *buffer, int kapacita) {
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

void sleep_ms(long ms) {
    if (ms <= 0) return;

    struct timespec ts;
    ts.tv_sec = ms / 1000L;
    ts.tv_nsec = (ms % 1000L) * 1000000L;

    while (nanosleep(&ts, &ts) == -1) {
        if (errno == EINTR) continue;
        break;
    }
}

void clear_screen(void) {
    
    printf("\033[H\033[J");
}

int idx(int x, int y, int w) {
    return y * w + x;
}

void vykresli_interaktivne(klient_stav_t *stav, const char *stavovy_text) {
    if (stav == NULL) return;

    pthread_mutex_lock(&stav->mutex);

    int w = stav->sirka;
    int h = stav->vyska;

    const char *grid = stav->policka;
    const unsigned char *vis = stav->navstivene;

    int ox = stav->ox;
    int oy = stav->oy;

    int curx = stav->curx;
    int cury = stav->cury;

    int rep = (stav->trasa_rep > 0) ? stav->trasa_rep : stav->akt_rep;
    int reps = stav->celk_rep;
    int krok = stav->krok;

    pthread_mutex_unlock(&stav->mutex);

    if (w <= 0 || h <= 0 || grid == NULL) {
        return;
    }

    clear_screen();

    printf("INTERAKTIVNY MOD  |  replikacia %d/%d  |  krok %d\n", rep, reps, krok);
    printf("Ciel: stred sveta (0,0). Prikazy: stop (zastavit simulaciu), quit (ukoncit klienta)\n");
    if (stavovy_text != NULL && stavovy_text[0] != '\0') {
        printf("%s\n", stavovy_text);
    }
    printf("\n");
    /*Vyjreslovanie mapy pri interaktivnom mode*/
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            char c = '.';

            if (x == curx && y == cury) {
                c = '@';
            } else if (x == ox && y == oy) {
                c = '0';
            } else {
                int i = idx(x, y, w);

                if (grid[i] == '#') {
                    c = '#';
                } else if (vis != NULL && vis[i]) {
                    c = '*';
                } else {
                    c = '.';
                }
            }

            putchar(c);
        }
        putchar('\n');
    }

    fflush(stdout);
}

 void vykresli_vysledok_podla_zobrazenia(const char *text, int zobrazenie) {
    if (text == NULL) {
        return;
    }

    if (zobrazenie == ZOBRAZ_OBE) {
        printf("%s", text);
        return;
    }

    int vypisujem = 1;
    const char *p = text;

    while (*p) {
        const char *riadok_koniec = strchr(p, '\n');
        size_t dlzka = (riadok_koniec == NULL) ? strlen(p) : (size_t)(riadok_koniec - p + 1);

        if (strncmp(p, "AVG_STEPS", 9) == 0) {
            vypisujem = (zobrazenie == ZOBRAZ_AVG) ? 1 : 0;
        }

        if (strncmp(p, "PROB_K", 6) == 0) {
            vypisujem = (zobrazenie == ZOBRAZ_PROB) ? 1 : 0;
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

 int parse_trasa_line(const char *s, int *rep, int *dx, int *dy) {
    if (s == NULL) return 0;
    int r = 0, x = 0, y = 0;

    if (sscanf(s, "TRASA rep=%d start=(%d,%d)", &r, &x, &y) == 3) {
        if (rep) *rep = r;
        if (dx) *dx = x;
        if (dy) *dy = y;
        return 1;
    }

    return 0;
}

 int parse_krok_line(const char *s, int *krok, int *dx, int *dy) {
    if (s == NULL) return 0;
    int k = 0, x = 0, y = 0;

    
    if (sscanf(s, " krok=%d pozicia=(%d,%d)", &k, &x, &y) == 3) {
        if (krok) *krok = k;
        if (dx) *dx = x;
        if (dy) *dy = y;
        return 1;
    }

    return 0;
}

int najdi_najblizsie_volne(int w, int h, const char *grid, int *x, int *y) {
    if (grid == NULL || x == NULL || y == NULL) return -1;

    int sx = *x;
    int sy = *y;

    if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
        if (grid[idx(sx, sy, w)] != '#') return 0;
    }

    /*Prve volne miesto bez prekazky*/
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            if (grid[idx(xx, yy, w)] != '#') {
                *x = xx;
                *y = yy;
                return 0;
            }
        }
    }

    return -1;
}

 void *vlakno_prijem(void *arg) {
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

                pthread_mutex_lock(&stav->mutex);
                stav->akt_rep = sprava.aktualna_replikacia;
                stav->celk_rep = sprava.celkovy_pocet_replikacii;
                int je_inter = (stav->mod_simulacie == MOD_INTERAKTIVNY);
                pthread_mutex_unlock(&stav->mutex);

                if (!je_inter) {
                    printf("\rReplikacia %d/%d", sprava.aktualna_replikacia, sprava.celkovy_pocet_replikacii);
                    fflush(stdout);
                }
            }
            continue;
        }

        if (typ == MSG_TEXT) {
            pthread_mutex_lock(&stav->mutex);
            int je_inter = (stav->mod_simulacie == MOD_INTERAKTIVNY);
            pthread_mutex_unlock(&stav->mutex);

            /* --- Interaktivne vykreslovanie z textovej trasy --- */
            if (je_inter) {
                int rep = 0, dx = 0, dy = 0;

                if (parse_trasa_line(buffer, &rep, &dx, &dy)) {
                    pthread_mutex_lock(&stav->mutex);

                    stav->trasa_rep = rep;
                    stav->krok = 0;
                    stav->trasa_aktivna = 1;

                    if (stav->navstivene != NULL && stav->sirka > 0 && stav->vyska > 0) {
                        memset(stav->navstivene, 0, (size_t)(stav->sirka * stav->vyska));
                    }

                    int x = stav->ox + dx;
                    int y = stav->oy + dy;

                    if (x < 0) x = 0;
                    if (x >= stav->sirka) x = stav->sirka - 1;
                    if (y < 0) y = 0;
                    if (y >= stav->vyska) y = stav->vyska - 1;

                    /* ak je start na prekazke, presun na volne policko */
                    if (stav->policka != NULL) {
                        (void)najdi_najblizsie_volne(stav->sirka, stav->vyska, stav->policka, &x, &y);
                    }

                    stav->curx = x;
                    stav->cury = y;

                    pthread_mutex_unlock(&stav->mutex);

                    vykresli_interaktivne(stav, "Nova trajektoria (start)");
                    /* aby bolo vidno start */
                    sleep_ms(50);
                    continue;
                }

                int krok = 0;
                if (parse_krok_line(buffer, &krok, &dx, &dy)) {
                    pthread_mutex_lock(&stav->mutex);

                    int w = stav->sirka;
                    int h = stav->vyska;

                    int oldx = stav->curx;
                    int oldy = stav->cury;

                    int newx = stav->ox + dx;
                    int newy = stav->oy + dy;

                    if (newx < 0) newx = 0;
                    if (newx >= w) newx = w - 1;
                    if (newy < 0) newy = 0;
                    if (newy >= h) newy = h - 1;

                    if (stav->navstivene != NULL && stav->policka != NULL) {
                        int oldi = idx(oldx, oldy, w);
                        if (oldi >= 0 && oldi < w * h) {
                            if (stav->policka[oldi] != '#') {
                                stav->navstivene[oldi] = 1;
                            }
                        }
                    }

                    stav->curx = newx;
                    stav->cury = newy;
                    stav->krok = krok;

                    int je_v_strede = (newx == stav->ox && newy == stav->oy);

                    pthread_mutex_unlock(&stav->mutex);

                    if (je_v_strede) {
                        vykresli_interaktivne(stav, "Dosiahnuty stred -> koniec replikacie");
                        sleep_ms(300);
                    } else {
                        vykresli_interaktivne(stav, NULL);
                        sleep_ms(50); /* 50 ms po KAZDOM kroku */
                    }

                    continue;
                }

                /* vysledok simulacie: ulokladanie do suboru */
                if (strstr(buffer, "# RandomWalk result") != NULL) {
                    pthread_mutex_lock(&stav->mutex);

                    free(stav->posledny_vysledok_text);
                    stav->posledny_vysledok_text = strdup(buffer);

                    if (stav->vystupny_subor[0] != '\0' && stav->posledny_vysledok_text != NULL) {
                        (void)persist_uloz_text_do_suboru(stav->vystupny_subor, stav->posledny_vysledok_text);
                    }

                    pthread_mutex_unlock(&stav->mutex);

                    /* po dokonceni nechaj cistu informaciu */
                    clear_screen();
                    printf("CLIENT: vysledok ulozeny do %s\n", stav->vystupny_subor);
                    fflush(stdout);
                    continue;
                }

                
                pthread_mutex_lock(&stav->mutex);
                int trasa = stav->trasa_aktivna;
                pthread_mutex_unlock(&stav->mutex);

                if (!trasa) {
                    printf("%s", buffer);
                    fflush(stdout);
                }

                continue;
            }

            /* --- Sumarny mod  --- */
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

            continue;
        }

        if (typ == MSG_CHYBA) {
            fprintf(stderr, "\nCHYBA: %s", buffer);
            continue;
        }

        if (typ == MSG_HOTOVO) {
            printf("\nCLIENT: hotovo\n");
            break;
        }
    }

    pthread_mutex_lock(&stav->mutex);
    stav->bezi = 0;
    pthread_mutex_unlock(&stav->mutex);

    return NULL;
}

 void posli_prikaz(int socket, const char *text) {
    if (text == NULL) return;
    (void)proto_posli(socket, MSG_TEXT, text, (int)strlen(text));
}
/*Pomoc s AI*/
 int nacitaj_prikaz_neblokujuci(char *out, int cap, int timeout_ms) {
    if (out == NULL || cap <= 0) return 0;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);

    if (rc <= 0) return 0;

    if (fgets(out, cap, stdin) == NULL) return 0;

    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        out[n - 1] = '\0';
        n--;
    }

    return 1;
}

 void *vlakno_vstup(void *arg) {
    klient_stav_t *stav = (klient_stav_t *)arg;

    pthread_mutex_lock(&stav->mutex);
    int je_inter = (stav->mod_simulacie == MOD_INTERAKTIVNY);
    pthread_mutex_unlock(&stav->mutex);

    if (je_inter) {
        printf("Interaktivny mod: zadaj 'stop' alebo 'quit' (bez nutnosti promptu).\n");
        fflush(stdout);
    }

    while (1) {
        pthread_mutex_lock(&stav->mutex);
        int bezi = stav->bezi;
        pthread_mutex_unlock(&stav->mutex);

        if (!bezi) break;

        if (!je_inter) {
            printf("\nPrikaz (avg/prob/obe/stop/quit): ");
            fflush(stdout);

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

            continue;
        }

        /* interaktivny: neblokuj (aby sa thread vedel korektne ukoncit) */
        char riadok[128];
        if (!nacitaj_prikaz_neblokujuci(riadok, (int)sizeof(riadok), 200)) {
            continue;
        }

        if (strcmp(riadok, "stop") == 0) {
            posli_prikaz(stav->socket, "STOP;");
        } else if (strcmp(riadok, "quit") == 0) {
            shutdown(stav->socket, SHUT_RDWR);
            close(stav->socket);
            break;
        }
    }

    return NULL;
}

 int spusti_server_proces(int port) {
    pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        /* aby server nerusil animaciu v konzole, presmeruj jeho vystup do suboru */
        int fd = open("server.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            (void)dup2(fd, STDOUT_FILENO);
            (void)dup2(fd, STDERR_FILENO);
            close(fd);
        }

        char port_text[32];
        snprintf(port_text, sizeof(port_text), "%d", port);

        execl("./server", "server", "--port", port_text, (char *)NULL);
        _exit(1);
    }

    return (int)pid;
}

 int vyber_port_pre_server(void) {
    int port = 5000 + (getpid() % 20000);
    return port;
}

 int nacitaj_dve_inty(const char *prompt, int *a, int *b) {
    char riadok[128];

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        nacitaj_riadok(riadok, (int)sizeof(riadok));

        int x = 0, y = 0;
        if (sscanf(riadok, "%d %d", &x, &y) == 2) {
            if (a) *a = x;
            if (b) *b = y;
            return 0;
        }

        printf("Zadaj prosim dve cele cisla (napr. 2 -3)\n");
    }
}

 void vytvor_novu_simulaciu(void) {
    int typ_svetu = 0;

    printf("Typ sveta (0=svet bez prekazok, 1=svet s prekazkami): ");
    scanf("%d", &typ_svetu);
    vycisti_stdin();

    int sirka = 0;
    int vyska = 0;

    char *policka = NULL;

    if (typ_svetu == SVET_BEZPREKAZOK) {
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

        (void)nacitaj_dve_inty("Interaktivna trasa - start relativne ku stredu (dx dy): ", &dx, &dy);

        start_x_trasa = (sirka / 2) + dx;
        start_y_trasa = (vyska / 2) + dy;

        if (start_x_trasa < 0) start_x_trasa = 0;
        if (start_x_trasa >= sirka) start_x_trasa = sirka - 1;
        if (start_y_trasa < 0) start_y_trasa = 0;
        if (start_y_trasa >= vyska) start_y_trasa = vyska - 1;

        /* ak je start na prekazke, posun na volne policko */
        (void)najdi_najblizsie_volne(sirka, vyska, policka, &start_x_trasa, &start_y_trasa);
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
    stav.mod_simulacie = mod;

    snprintf(stav.vystupny_subor, sizeof(stav.vystupny_subor), "%s", vystup);

    /* priprav data pre interaktivne vykreslovanie */
    if (mod == MOD_INTERAKTIVNY) {
        stav.typ_svetu = typ_svetu;
        stav.sirka = sirka;
        stav.vyska = vyska;

        stav.ox = sirka / 2;
        stav.oy = vyska / 2;

        stav.policka = (char *)malloc((size_t)(sirka * vyska));
        stav.navstivene = (unsigned char *)calloc((size_t)(sirka * vyska), 1);

        if (stav.policka != NULL) {
            memcpy(stav.policka, policka, (size_t)(sirka * vyska));
        }

        stav.curx = start_x_trasa;
        stav.cury = start_y_trasa;
        stav.trasa_aktivna = 0;
        stav.trasa_rep = 0;
        stav.krok = 0;
    }

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
    free(stav.policka);
    free(stav.navstivene);

    close(socket);

    (void)waitpid((pid_t)pid, NULL, 0);
}


 void opatovne_spustenie_simulacie(void) {
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
    stav.mod_simulacie = MOD_SUMARNY;

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
        printf("2) opatovne spustenie simulacie zo suboru\n");
        printf("3) koniec\n");
        printf("Volba: ");

        int volba = 0;

        scanf("%d", &volba);
        vycisti_stdin();

        if (volba == 1) {
            vytvor_novu_simulaciu();
        } else if (volba == 2) {
            opatovne_spustenie_simulacie();
        } else if (volba == 3) {
            break;
        }
    }

    return 0;
}

