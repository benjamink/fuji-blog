/* Network layer for FujiNet HTTP communication */

#include <fujinet-network.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "network.h"

#define MAX_URL_LEN 256  /* devicespec: "N1:" + server_url + short path (ping) */
#define MAX_RESPONSE_LEN 8192
#define NETWORK_UNIT 1  /* Use network unit N1: */

typedef struct {
    char devicespec[MAX_URL_LEN];
    int16_t last_read_size;
    uint8_t last_error;
} NetworkState;

static NetworkState net_state = {0};

int client_network_init(void)
{
    /* Initialize network device - returns 0 on success */
    uint8_t err = network_init();
    if (err != 0) {
        net_state.last_error = err;
        return -1;
    }
    return 0;
}

static void build_devicespec(char *spec, const char *server_url, const char *path)
{
    /* Build device spec: N1:HTTP://host:port/path */
    snprintf(spec, MAX_URL_LEN, "N%d:%s%s", NETWORK_UNIT, server_url, path);
}

int network_get(const char *server_url, const char *path, unsigned char *response, int response_len)
{
    /* Perform HTTP GET request
       Returns number of bytes read, or -1 on error */
    uint8_t err;
    int16_t bytes_read;
    
    build_devicespec(net_state.devicespec, server_url, path);
    
    err = network_open(net_state.devicespec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (err != 0) {
        net_state.last_error = err;
        return -1;
    }
    
    bytes_read = network_read(net_state.devicespec, response, response_len);
    network_close(net_state.devicespec);
    
    if (bytes_read < 0) {
        net_state.last_error = fn_device_error;
        net_state.last_read_size = 0;
        return -1;
    }
    
    net_state.last_read_size = bytes_read;
    net_state.last_error = 0;
    return (int)bytes_read;
}

uint8_t network_get_last_error(void)
{
    /* Get the last network error code */
    return net_state.last_error;
}

int network_get_last_read_size(void)
{
    /* Get size of last successful read */
    return net_state.last_read_size;
}
