#ifndef PERSIST_H
#define PERSIST_H

int persist_nacitaj_svet(
    const char *cesta,
    int *sirka,
    int *vyska,
    char **policka
);

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
);

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
);

int persist_uloz_text_do_suboru(const char *cesta, const char *text);

#endif

