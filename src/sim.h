#ifndef SIM_H
#define SIM_H

typedef struct {
    int sirka;
    int vyska;
    char *policka;
    double *priemerne_kroky;
    double *pravdepodobnosti_k;
} vysledok_simulacie_t;

void simulacia_uvolni_vysledok(vysledok_simulacie_t *vysledok);

int simulacia_vygeneruj_prekazky(
    int sirka,
    int vyska,
    int percento_prekazok,
    char **policka
);

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
    int *stop_flag,
    void (*callback_priebeh)(int aktualna, int celkovo, void *user),
    void (*callback_trasa_text)(const char *text, void *user),
    void *user,
    vysledok_simulacie_t *vysledok
);

#endif

