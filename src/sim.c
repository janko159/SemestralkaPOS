#define _DEFAULT_SOURCE
#include "sim.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "common.h"

int index_policka(int x, int y, int sirka) {
    return y * sirka + x;
}

int dosiahnutelne_vsetko(int sirka, int vyska, const char *policka) {
    int ox = sirka / 2;
    int oy = vyska / 2;

    if (!(policka[index_policka(ox, oy, sirka)] != '#')) { 
        return 0;
    }

    int pocet = sirka * vyska;

    char *navstivene = (char *)calloc((size_t)pocet, 1);
    int *fronta_x = (int *)malloc((size_t)pocet * sizeof(int));
    int *fronta_y = (int *)malloc((size_t)pocet * sizeof(int));

    if (navstivene == NULL || fronta_x == NULL || fronta_y == NULL) {
        free(navstivene);
        free(fronta_x);
        free(fronta_y);
        return 0;
    }

    int hlava = 0;
    int chvost = 0;

    navstivene[index_policka(ox, oy, sirka)] = 1;
    fronta_x[chvost] = ox;
    fronta_y[chvost] = oy;
    chvost++;

    while (hlava < chvost) {
        int x = fronta_x[hlava];
        int y = fronta_y[hlava];
        hlava++;

        int dx[4] = { 0, 0, -1, 1 };
        int dy[4] = { -1, 1, 0, 0 };

        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx < 0 || nx >= sirka || ny < 0 || ny >= vyska) {
                continue;
            }

            if (!(policka[index_policka(nx, ny, sirka)] != '#') ) {
                continue;
            }

            int ind = index_policka(nx, ny, sirka);

            if (navstivene[ind]) {
                continue;
            }

            navstivene[ind] = 1;
            fronta_x[chvost] = nx;
            fronta_y[chvost] = ny;
            chvost++;
        }
    }

    int ok = 1;

    for (int y = 0; y < vyska && ok; y++) {
        for (int x = 0; x < sirka; x++) {
            int ind = index_policka(x, y, sirka);

            if (policka[ind] != '#' && navstivene[ind] == 0) {
                ok = 0;
                break;
            }
        }
    }

    free(navstivene);
    free(fronta_x);
    free(fronta_y);

    return ok;
}

int simulacia_vygeneruj_prekazky(
    int sirka,
    int vyska,
    int percento_prekazok,
    char **policka
) {
    if (policka == NULL) {
        return -1;
    }

    if (percento_prekazok < 0) {
        percento_prekazok = 0;
    }

    if (percento_prekazok > 90) {
        percento_prekazok = 90;
    }

    int ox = sirka / 2;
    int oy = vyska / 2;

    for (int pokus = 0; pokus < 200; pokus++) {
        char *p = (char *)malloc((size_t)(sirka * vyska));

        if (p == NULL) {
            return -1;
        }

        for (int y = 0; y < vyska; y++) {
            for (int x = 0; x < sirka; x++) {
                if (x == ox && y == oy) {
                    p[index_policka(x, y, sirka)] = '.';
                    continue;
                }

                int r = rand() % 100;

                if (r < percento_prekazok) {
                    p[index_policka(x, y, sirka)] = '#';
                } else {
                    p[index_policka(x, y, sirka)] = '.';
                }
            }
        }

        if (dosiahnutelne_vsetko(sirka, vyska, p)) {
            *policka = p;
            return 0;
        }

        free(p);
    }

    return -1;
}

void simulacia_uvolni_vysledok(vysledok_simulacie_t *vysledok) {
    if (vysledok == NULL) {
        return;
    }

    free(vysledok->policka);
    free(vysledok->priemerne_kroky);
    free(vysledok->pravdepodobnosti_k);

    vysledok->policka = NULL;
    vysledok->priemerne_kroky = NULL;
    vysledok->pravdepodobnosti_k = NULL;

    vysledok->sirka = 0;
    vysledok->vyska = 0;
}

int vyber_smer(double pravd_hore, double pravd_dole, double pravd_vlavo, double pravd_vpravo) {
    double u = (double)rand() / ((double)RAND_MAX + 1.0);

    double a = pravd_hore;
    double b = a + pravd_dole;
    double c = b + pravd_vlavo;

    if (u < a) {
        return 0;
    }

    if (u < b) {
        return 1;
    }

    if (u < c) {
        return 2;
    }

    (void)pravd_vpravo;
    return 3;
}

void posun(
    int typ_svetu,
    int sirka,
    int vyska,
    const char *policka,
    int *x,
    int *y,
    int smer
) {
    int nx = *x;
    int ny = *y;

    if (smer == 0) {
        ny--;
    } else if (smer == 1) {
        ny++;
    } else if (smer == 2) {
        nx--;
    } else {
        nx++;
    }

    if (typ_svetu == SVET_BEZPREKAZOK) {
        if (nx < 0) {
            nx = sirka - 1;
        }

        if (nx >= sirka) {
            nx = 0;
        }

        if (ny < 0) {
            ny = vyska - 1;
        }

        if (ny >= vyska) {
            ny = 0;
        }

        *x = nx;
        *y = ny;
        return;
    }

    if (nx < 0 || nx >= sirka || ny < 0 || ny >= vyska) {
        return;
    }

    if (policka[index_policka(nx, ny, sirka)] == '#') {
        return;
    }

    *x = nx;
    *y = ny;
}

int simulacia_spusti(
    int typ_svetu,
    int sirka,
    int vyska,
    const char *policka,
    int pocet_replikacii,
    int hodnota_k,
    double pravd_hore,
    double pravd_dole,
    double pravd_vlavo,
    double pravd_vpravo,
    int mod_simulacie,
    int start_x_trasa,
    int start_y_trasa,
    int *stop,
    void (*callback_priebeh)(int aktualna, int celkovo, void *user),
    void (*callback_trasa_text)(const char *text, void *user),
    void *user,
    vysledok_simulacie_t *vysledok
) {
    if (policka == NULL || vysledok == NULL) {
        return -1;
    }

    int pocet_policka = sirka * vyska;

    double *suma_krokov = (double *)calloc((size_t)pocet_policka, sizeof(double));
    double *suma_uspech_k = (double *)calloc((size_t)pocet_policka, sizeof(double));

    if (suma_krokov == NULL || suma_uspech_k == NULL) {
        free(suma_krokov);
        free(suma_uspech_k);
        return -1;
    }

    int ox = sirka / 2;
    int oy = vyska / 2;

    int ind_trasa = index_policka(start_x_trasa, start_y_trasa, sirka);

    int limit_krokov = sirka * vyska * 1000;

    for (int rep = 1; rep <= pocet_replikacii; rep++) {
        if (stop != NULL && *stop) {
            break;
        }

        if (callback_priebeh != NULL) {
            callback_priebeh(rep, pocet_replikacii, user);
        }

        for (int sy = 0; sy < vyska; sy++) {
            for (int sx = 0; sx < sirka; sx++) {
                if (stop != NULL && *stop) {
                    break;
                }

                int ind = index_policka(sx, sy, sirka);

                if (typ_svetu == SVET_PREKAZKY && policka[ind] == '#') {
                    continue;
                }

                int x = sx;
                int y = sy;

                int kroky = 0;
                int uspech_do_k = 0;

                int je_to_trasa = (mod_simulacie == MOD_INTERAKTIVNY && ind == ind_trasa);

                if (je_to_trasa && callback_trasa_text != NULL) {
                    char uvod[256];
                    snprintf(uvod, sizeof(uvod), "TRASA rep=%d start=(%d,%d)\n", rep, sx - ox, sy - oy);
                    callback_trasa_text(uvod, user);
                }

                while (!(x == ox && y == oy) && kroky < limit_krokov) {
                    if (stop != NULL && *stop) {
                        break;
                    }
                    int smer = vyber_smer(pravd_hore, pravd_dole, pravd_vlavo, pravd_vpravo);

                    posun(typ_svetu, sirka, vyska, policka, &x, &y, smer);

                    kroky++;

                    if (!uspech_do_k && kroky <= hodnota_k && x == ox && y == oy) {
                        uspech_do_k = 1;
                    }

                    if (je_to_trasa && callback_trasa_text != NULL) {
                        char riadok[128];
                        snprintf(riadok, sizeof(riadok), "  krok=%d pozicia=(%d,%d)\n", kroky, x - ox, y - oy);
                        callback_trasa_text(riadok, user);
                    }
                }

                suma_krokov[ind] = suma_krokov[ind] + (double)kroky;

                if (uspech_do_k) {
                    suma_uspech_k[ind] = suma_uspech_k[ind] + 1.0;
                }
            }
        }
    }

    double *priemerne = (double *)calloc((size_t)pocet_policka, sizeof(double));
    double *pravdepodobnosti = (double *)calloc((size_t)pocet_policka, sizeof(double));

    if (priemerne == NULL || pravdepodobnosti == NULL) {
        free(suma_krokov);
        free(suma_uspech_k);
        free(priemerne);
        free(pravdepodobnosti);
        return -1;
    }

    for (int i = 0; i < pocet_policka; i++) {
        if (typ_svetu == SVET_PREKAZKY && policka[i] == '#') {
            priemerne[i] = 0.0;
            pravdepodobnosti[i] = 0.0;
        } else {
            priemerne[i] = suma_krokov[i] / (double)pocet_replikacii;
            pravdepodobnosti[i] = suma_uspech_k[i] / (double)pocet_replikacii;
        }
    }

    free(suma_krokov);
    free(suma_uspech_k);

    vysledok->sirka = sirka;
    vysledok->vyska = vyska;

    vysledok->policka = (char *)malloc((size_t)pocet_policka);

    if (vysledok->policka == NULL) {
        free(priemerne);
        free(pravdepodobnosti);
        return -1;
    }

    memcpy(vysledok->policka, policka, (size_t)pocet_policka);

    vysledok->priemerne_kroky = priemerne;
    vysledok->pravdepodobnosti_k = pravdepodobnosti;

    return 0;
}
