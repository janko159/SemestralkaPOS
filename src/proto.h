#ifndef PROTO_H
#define PROTO_H

#include <stddef.h>



int proto_posli(int socket, int typ_spravy, const void *data, int dlzka);

int proto_prijmi(
    int socket,
    int *typ_spravy,
    void *buffer,
    int kapacita_buffera,
    int *dlzka_dat
);

#endif

