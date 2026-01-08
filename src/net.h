#ifndef NET_H
#define NET_H

int net_pocuvaj(int port);

int net_prijmi_klienta(int pasivny_socket);

int net_pripoj_sa(const char *host, int port);

#endif

