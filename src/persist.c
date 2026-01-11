#define _DEFAULT_SOURCE
#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int index_policka(int x, int y, int sirka) {
    return y * sirka + x;
}

int persist_nacitaj_svet(
    const char *cesta,
    int *sirka,
    int *vyska,
    char **policka
) {
    if (cesta == NULL || sirka == NULL || vyska == NULL || policka == NULL) {
        return -1;
    }

    FILE *f = fopen(cesta, "r");

    if (f == NULL) {
        return -1;
    }

    int s = 0;
    int v = 0;

    if (fscanf(f, "%d %d\n", &s, &v) != 2 || s <= 0 || v <= 0) {
        fclose(f);
        return -1;
    }

    char *p = (char *)malloc((size_t)(s * v));

    if (p == NULL) {
        fclose(f);
        return -1;
    }

    char *riadok = (char *)malloc((size_t)s + 8);

    if (riadok == NULL) {
        free(p);
        fclose(f);
        return -1;
    }

    for (int y = 0; y < v; y++) {
        if (fgets(riadok, s + 8, f) == NULL) {
            free(riadok);
            free(p);
            fclose(f);
            return -1;
        }

        for (int x = 0; x < s; x++) {
            char c = riadok[x];

            if (c == '#') {
                p[index_policka(x, y, s)] = '#';
            } else {
                p[index_policka(x, y, s)] = '.';
            }
        }
    }

    free(riadok);
    fclose(f);

    *sirka = s;
    *vyska = v;
    *policka = p;

    return 0;
}

 int zacina_s(const char *riadok, const char *prefix) {
    size_t a = strlen(prefix);
    return strncmp(riadok, prefix, a) == 0;
}

int persist_nacitaj_konfiguraciu_zo_suboru_vysledku(
    const char *cesta,
    int *typ_svetu,
    int *sirka,
    int *vyska,
    int *hodnota_k,
    double *pravd_hore,
    double *pravd_dole,
    double *pravd_vlavo,
    double *pravd_vpravo,
    char **policka
) {
    if (cesta == NULL || typ_svetu == NULL || sirka == NULL || vyska == NULL || hodnota_k == NULL) {
        return -1;
    }

    FILE *f = fopen(cesta, "r");

    if (f == NULL) {
        return -1;
    }

    char riadok[4096];

    int s = -1;
    int v = -1;
    int t = -1;
    int k = -1;

    double ph = 0.25;
    double pd = 0.25;
    double pv = 0.25;
    double pp = 0.25;

    char *p = NULL;

    while (fgets(riadok, (int)sizeof(riadok), f) != NULL) {
        if (zacina_s(riadok, "W ")) {
            sscanf(riadok, "W %d", &s);
        } else if (zacina_s(riadok, "H ")) {
            sscanf(riadok, "H %d", &v);
        } else if (zacina_s(riadok, "WORLD_TYPE ")) {
            sscanf(riadok, "WORLD_TYPE %d", &t);
        } else if (zacina_s(riadok, "K ")) {
            sscanf(riadok, "K %d", &k);
        } else if (zacina_s(riadok, "PROBS ")) {
            sscanf(riadok, "PROBS %lf %lf %lf %lf", &ph, &pd, &pv, &pp);
        } else if (zacina_s(riadok, "GRID")) {
            if (s <= 0 || v <= 0) {
                fclose(f);
                return -1;
            }

            p = (char *)malloc((size_t)(s * v));

            if (p == NULL) {
                fclose(f);
                return -1;
            }

            for (int y = 0; y < v; y++) {
                if (fgets(riadok, (int)sizeof(riadok), f) == NULL) {
                    free(p);
                    fclose(f);
                    return -1;
                }

                for (int x = 0; x < s; x++) {
                    char c = riadok[x];

                    if (c == '#') {
                        p[index_policka(x, y, s)] = '#';
                    } else {
                        p[index_policka(x, y, s)] = '.';
                    }
                }
            }

            break;
        }
    }

    fclose(f);

    if (s <= 0 || v <= 0 || t < 0 || k < 0 || p == NULL) {
        free(p);
        return -1;
    }

    *typ_svetu = t;
    *sirka = s;
    *vyska = v;
    *hodnota_k = k;

    if (pravd_hore) {
        *pravd_hore = ph;
    }

    if (pravd_dole) {
        *pravd_dole = pd;
    }

    if (pravd_vlavo) {
        *pravd_vlavo = pv;
    }

    if (pravd_vpravo) {
        *pravd_vpravo = pp;
    }

    if (policka) {
        *policka = p;
    } else {
        free(p);
    }

    return 0;
}

char *persist_vytvor_text_vysledku(
    int typ_svetu,
    int sirka,
    int vyska,
    int hodnota_k,
    int pocet_replikacii,
    double pravd_hore,
    double pravd_dole,
    double pravd_vlavo,
    double pravd_vpravo,
    const char *policka,
    const double *priemerne_kroky,
    const double *pravdepodobnosti_k
) {
    size_t kapacita = (size_t)(sirka * vyska) * 40u + 8192u;

    char *text = (char *)malloc(kapacita);

    if (text == NULL) {
        return NULL;
    }

    size_t pozicia = 0;

    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "# Vysledok\n");
    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "W %d\n", sirka);
    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "H %d\n", vyska);
    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "WORLD_TYPE %d\n", typ_svetu);
    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "K %d\n", hodnota_k);
    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "REPS %d\n", pocet_replikacii);
    pozicia += (size_t)snprintf(
        text + pozicia,
        kapacita - pozicia,
        "PROBS %.10f %.10f %.10f %.10f\n",
        pravd_hore,
        pravd_dole,
        pravd_vlavo,
        pravd_vpravo
    );

    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "GRID\n");

    for (int y = 0; y < vyska; y++) {
        for (int x = 0; x < sirka; x++) {
            text[pozicia] = policka[index_policka(x, y, sirka)];
            pozicia++;
        }

        text[pozicia] = '\n';
        pozicia++;
    }

    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "AVG_STEPS\n");

    for (int y = 0; y < vyska; y++) {
        for (int x = 0; x < sirka; x++) {
            int i = index_policka(x, y, sirka);

            if (policka[i] == '#') {
                pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "  ###### ");
            } else {
                pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "%8.2f ", priemerne_kroky[i]);
            }
        }

        pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "\n");
    }

    pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "PROB_K\n");

    for (int y = 0; y < vyska; y++) {
        for (int x = 0; x < sirka; x++) {
            int i = index_policka(x, y, sirka);

            if (policka[i] == '#') {
                pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "  ###### ");
            } else {
                pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "%8.4f ", pravdepodobnosti_k[i]);
            }
        }

        pozicia += (size_t)snprintf(text + pozicia, kapacita - pozicia, "\n");
    }

    return text;
}

int persist_uloz_text_do_suboru(const char *cesta, const char *text) {
    if (cesta == NULL || text == NULL) {
        return -1;
    }

    FILE *f = fopen(cesta, "w");

    if (f == NULL) {
        return -1;
    }

    fputs(text, f);

    fclose(f);

    return 0;
}

