#ifndef NETWORK_H
#define NETWORK_H

int client_network_init(void);
int network_get(const char *server_url, const char *path,
                unsigned char *response, int response_len);
unsigned char network_get_last_error(void);
int network_get_last_read_size(void);

#endif
