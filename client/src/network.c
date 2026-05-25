/* Network layer for FujiNet HTTP communication */

#include <fujinet-network.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "network.h"

#define MAX_URL_LEN 512
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

int network_post(const char *server_url, const char *path, const char *body, unsigned char *response, int response_len)
{
    uint8_t err;
    int16_t bytes_read;
    uint16_t bw;
    uint8_t conn;
    uint8_t nerr;
    uint8_t retries;

    build_devicespec(net_state.devicespec, server_url, path);

    /* Open in read mode — network_http_post() registers the body and method,
       network_read() then triggers the actual HTTP transaction, same as GET. */
    err = network_open(net_state.devicespec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (err != 0) {
        net_state.last_error = err;
        return -1;
    }

    err = network_http_post(net_state.devicespec, body);
    if (err != 0) {
        net_state.last_error = err;
        network_close(net_state.devicespec);
        return -1;
    }

    /* Poll until bytes are available from the server. */
    bw = 0; conn = 1; nerr = 0; retries = 50;
    do {
        network_status(net_state.devicespec, &bw, &conn, &nerr);
        retries--;
    } while (bw == 0 && conn != 0 && retries > 0);
    net_state.last_error = nerr;

    if (bw == 0) {
        network_close(net_state.devicespec);
        return -1;
    }

    if (bw > (uint16_t)response_len)
        bw = (uint16_t)response_len;

    bytes_read = network_read(net_state.devicespec, response, bw);
    network_close(net_state.devicespec);

    if (bytes_read < 0) {
        net_state.last_error = fn_device_error;
        net_state.last_read_size = 0;
        return -1;
    }

    net_state.last_read_size = bytes_read;
    return (int)bytes_read;
}

int network_put(const char *server_url, const char *path, const char *body, unsigned char *response, int response_len)
{
    /* Perform HTTP PUT request with JSON body
       Returns number of bytes read, or -1 on error */
    uint8_t err;
    int16_t bytes_read;
    
    build_devicespec(net_state.devicespec, server_url, path);
    
    /* Open with PUT mode */
    err = network_open(net_state.devicespec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    if (err != 0) {
        net_state.last_error = err;
        return -1;
    }
    
    /* Add Content-Type header */
    err = network_http_start_add_headers(net_state.devicespec);
    if (err == 0) {
        network_http_add_header(net_state.devicespec, "Content-Type: application/json");
        network_http_end_add_headers(net_state.devicespec);
    }
    
    /* Send PUT body */
    err = network_http_put(net_state.devicespec, body);
    if (err != 0) {
        net_state.last_error = err;
        network_close(net_state.devicespec);
        return -1;
    }
    
    /* Read response */
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

int network_delete(const char *server_url, const char *path, unsigned char *response, int response_len)
{
    /* Perform HTTP DELETE request
       Returns number of bytes read, or -1 on error */
    uint8_t err;
    int16_t bytes_read;
    
    build_devicespec(net_state.devicespec, server_url, path);
    
    /* Open with DELETE mode */
    err = network_open(net_state.devicespec, OPEN_MODE_HTTP_DELETE, OPEN_TRANS_NONE);
    if (err != 0) {
        net_state.last_error = err;
        return -1;
    }
    
    /* Read response (DELETE typically returns confirmation) */
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

int network_patch(const char *server_url, const char *path, const char *body, unsigned char *response, int response_len)
{
    /* PATCH is not directly supported, use PUT mode as workaround 
       Returns number of bytes read, or -1 on error */
    return network_put(server_url, path, body, response, response_len);
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
