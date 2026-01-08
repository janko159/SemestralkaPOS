#ifndef SPOLOCNE_H
#define SPOLOCNE_H

#define MSG_PRIEBEH   1
#define MSG_TEXT      2
#define MSG_CHYBA     3
#define MSG_HOTOVO    4

#define SVET_TORUS        0
#define SVET_PREKAZKY     1

#define MOD_SUMARNY       0
#define MOD_INTERAKTIVNY  1

#define ZOBRAZ_AVG        0
#define ZOBRAZ_PROB       1
#define ZOBRAZ_OBE        2

typedef struct {
    int aktualna_replikacia;
    int celkovy_pocet_replikacii;
} sprava_priebeh_t;

#endif

