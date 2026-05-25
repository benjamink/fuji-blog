#include <fujinet-fuji.h>
#include <fujinet-network.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "api.h"
#include "network.h"

#define MAX_TITLE_LEN 80
#define MAX_CATEGORY_LEN 64
#define MAX_CONTENT_LEN 2000
#define MAX_RESPONSE_LEN 4096
#define MAX_POSTS 50

#ifdef __CC65__
#include <conio.h>
#include <apple2.h>
#define HOME() clrscr()
#define CLEAR_LINE()
#define NORMAL_TEXT()
#define BOLD_TEXT()
#elif defined(_CMOC_VERSION_)
#define HOME() clrscr()
#define CLEAR_LINE()
#define NORMAL_TEXT()
#define BOLD_TEXT()
#else
#define HOME() printf("\033[H\033[J")
#define CLEAR_LINE() printf("\033[K")
#define NORMAL_TEXT() printf("\033[0m")
#define BOLD_TEXT() printf("\033[1m")
#endif

/* Forward declarations */
void main_menu(void);
void show_splash(void);
void list_posts(void);
void view_post(const char *id);
void new_post(void);
void edit_post(void);
void toggle_publish(void);
void delete_post(void);
void show_network_status(void);
void get_screen_width(void);
void show_config(void);
void test_server(void);
int  read_line(char *buf, int maxlen);
void read_line_with_default(char *buf, int maxlen);
void body_editor(void);
int  editor_count_lines(const char *body);
int  editor_get_line(const char *body, int n, char *buf, int buf_len);
int  editor_replace_line(char *body, int body_max, int n, const char *new_text);
int  editor_delete_line(char *body, int n);
int  editor_insert_line(char *body, int body_max, int n, const char *text);

/* Global variables */
int screen_width = 40;  /* Detect 40 or 80 column mode */
char server_url[256] = "http://192.168.15.35:8001";

/* Shared BSS buffers — menu functions never run concurrently so one copy each
   covers all callers. Keeps the apple2 BSS segment within its size limit. */
static char s_spec[300];
static char s_ids[MAX_API_POSTS][MAX_API_ID_LEN + 1];
static char s_titles[MAX_API_POSTS][MAX_API_TITLE_LEN + 1];
static int  s_pub[MAX_API_POSTS];
static char s_path[16];
static char s_val[MAX_API_TITLE_LEN + 1];
static char s_body[MAX_API_MARKDOWN_BODY_LEN + 1];
static char s_json_buf[2048];
static char s_id_result[MAX_API_ID_LEN + 1];

int main(void)
{
    get_screen_width();
    show_splash();
    
    while (1) {
        main_menu();
    }
    
    return 0;
}

void get_screen_width(void)
{
#ifdef __CC65__
    screen_width = (videomode(VIDEOMODE_80COL) >= 0) ? 80 : 40;
#elif defined(_CMOC_VERSION_)
    screen_width = 40;
#else
    screen_width = 80;
#endif
}

void show_splash(void)
{
    AdapterConfigExtended ace;

    HOME();
    BOLD_TEXT();
    printf("========================================\n");
    printf("    FujiNet Apple IIc Blog Client\n");
    printf("========================================\n");
    NORMAL_TEXT();
    printf("\nDetected screen width: %d columns\n", screen_width);
    printf("\nInitializing FujiNet...\n");
    
    if (!fuji_get_adapter_config_extended(&ace)) {
        printf("ERROR: FujiNet not detected!\n");
        printf("Press any key to exit.\n");
        getchar();
        exit(1);
    }
    
    printf("FujiNet Version: %s\n", ace.fn_version);
    printf("Adapter SSID: %s\n", ace.ssid);
    printf("\nPress any key to continue...\n");
    getchar();
}

void main_menu(void)
{
    int choice;
    
    HOME();
    BOLD_TEXT();
    if (screen_width >= 40) {
        printf("=== MAIN MENU ===\n");
    }
    NORMAL_TEXT();
    
    printf("\n1. List Posts\n");
    printf("2. New Post\n");
    printf("3. Edit Post\n");
    printf("4. Toggle Publish\n");
    printf("5. Delete Post\n");
    printf("6. Network Status\n");
    printf("7. Configuration\n");
    printf("Q. Quit\n");
    
    printf("\nSelect option: ");
    choice = getchar();
    choice = toupper(choice);
    
    switch (choice) {
        case '1':
            list_posts();
            break;
        case '2':
            new_post();
            break;
        case '3':
            edit_post();
            break;
        case '4':
            toggle_publish();
            break;
        case '5':
            delete_post();
            break;
        case '6':
            show_network_status();
            break;
        case '7':
            show_config();
            break;
        case 'Q':
            HOME();
            printf("Goodbye!\n");
            exit(0);
        default:
            break;
    }
}

void show_network_status(void)
{
    AdapterConfigExtended ace;
    int i;
    int ch;

    do {
        HOME();
        BOLD_TEXT();
        printf("NETWORK STATUS\n");
        NORMAL_TEXT();
        printf("================\n\n");

        if (fuji_get_adapter_config_extended(&ace)) {
            printf("FujiNet: Connected\n");
            printf("Version: %s\n", ace.fn_version);
            printf("SSID: %s\n", ace.ssid);
            printf("MAC: ");
            for (i = 0; i < 6; i++) {
                printf("%02X", ace.macAddress[i]);
                if (i < 5) printf(":");
            }
            printf("\n");
        } else {
            printf("FujiNet: NOT DETECTED\n");
        }

        printf("\nServer: %s\n", server_url);
        printf("Screen: %d columns\n", screen_width);
        printf("\nT. Test Server  Q. Back\n");
        printf("Select: ");

        ch = toupper(getchar());
        if (ch == 'T') test_server();
    } while (ch != 'Q');
}

void test_server(void)
{
    int bytes;
    int ch;

    do {
        HOME();
        BOLD_TEXT();
        printf("SERVER TEST\n");
        NORMAL_TEXT();
        printf("===========\n");
        printf("%s\n\n", server_url);
        printf("G. GET  /api/ping\n");
        printf("P. PUT  /api/posts (create test)\n");
        printf("Q. Back\n\n");
        printf("Select: ");

        ch = toupper(getchar());

        if (ch == 'G') {
            HOME();
            printf("GET /api/ping...\n\n");
            bytes = network_get(server_url, "/api/ping",
                                (uint8_t *)s_val, (int)sizeof(s_val) - 1);
            if (bytes > 0) {
                s_val[bytes] = '\0';
                printf("OK! %d bytes:\n%s\n", bytes, s_val);
            } else {
                printf("FAILED (status %d)\n",
                       (int)network_get_last_error());
            }
            printf("\nPress any key...\n");
            getchar();

        } else if (ch == 'P') {
            static const char test_json[] =
                "{\"title\":\"Test\",\"markdown_body\":\"# Test\","
                "\"category\":\"test\",\"published\":false}";
            uint8_t perr;
            int16_t pn;
            HOME();
            printf("PUT /api/posts (test create)...\n\n");
            snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts", server_url);
            perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
            if (perr) {
                printf("Open error: %d\n", (int)perr);
            } else {
                network_write(s_spec, (const uint8_t *)test_json,
                              (uint16_t)(sizeof(test_json) - 1));
                printf("Waiting for response...\n");
                perr = network_json_parse(s_spec);
                s_id_result[0] = '\0';
                pn = network_json_query(s_spec, "/id", s_id_result);
                network_close(s_spec);
                if (pn > 0 && s_id_result[0]) {
                    printf("OK! Post ID:\n%s\n", s_id_result);
                } else {
                    printf("FAILED (parse err %d)\n", (int)perr);
                }
            }
            printf("\nPress any key...\n");
            getchar();
        }
    } while (ch != 'Q');
}

void list_posts(void)
{
    int count;
    int i;
    int ch;
    int tlen;
    int maxw;
    uint8_t perr;
    int16_t pn;

    count = 0;

    HOME();
    BOLD_TEXT();
    printf("PUBLISHED POSTS\n");
    NORMAL_TEXT();
    printf("Loading...\n");

    snprintf(s_spec, sizeof(s_spec),
             "N1:%s/api/posts/summaries?published_only=true", server_url);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        printf("Connection error: %d\n", (int)perr);
        printf("Press any key...\n");
        getchar();
        return;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        printf("Parse error: %d\n", (int)perr);
        network_close(s_spec);
        printf("Press any key...\n");
        getchar();
        return;
    }

    for (i = 0; i < MAX_API_POSTS; i++) {
        snprintf(s_path, sizeof(s_path), "/%d/id", i);
        s_val[0] = '\0';
        pn = network_json_query(s_spec, s_path, s_val);
        if (pn <= 0 || !s_val[0]) break;
        strncpy(s_ids[i], s_val, MAX_API_ID_LEN);
        s_ids[i][MAX_API_ID_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/title", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        strncpy(s_titles[i], s_val, MAX_API_TITLE_LEN);
        s_titles[i][MAX_API_TITLE_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/published", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        s_pub[i] = (strcmp(s_val, "true") == 0);

        count++;
    }
    network_close(s_spec);

    HOME();
    BOLD_TEXT();
    printf("PUBLISHED POSTS\n");
    NORMAL_TEXT();
    printf("===============\n\n");

    if (count == 0) {
        printf("No published posts found.\n");
        printf("\nPress any key...\n");
        getchar();
        return;
    }

    maxw = screen_width - 5;
    for (i = 0; i < count; i++) {
        tlen = (int)strlen(s_titles[i]);
        if (tlen > maxw) s_titles[i][maxw] = '\0';
        printf("%d. %s%s\n", i + 1, s_titles[i], s_pub[i] ? "" : " [D]");
    }
    printf("\nSelect (1-%d) or Q: ", count);

#ifdef __CC65__
    ch = cgetc();
#else
    ch = getchar();
#endif
    putchar('\n');
    if (ch >= '1' && ch < '1' + count) {
        view_post(s_ids[ch - '1']);
    }
}

void view_post(const char *id)
{
    const char *p;
    int page_size;
    int chars;
    int ch;
    uint8_t perr;

    HOME();
    printf("Loading post...\n");

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/markdown",
             server_url, id);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        printf("Connection error: %d\n", (int)perr);
        printf("Press any key...\n");
        getchar();
        return;
    }
    perr = network_json_parse(s_spec);
    s_val[0] = '\0';   /* s_val used for title — view_post doesn't use it otherwise */
    s_body[0] = '\0';
    network_json_query(s_spec, "/title", s_val);
    network_json_query(s_spec, "/markdown_body", s_body);
    network_close(s_spec);

    /* Characters of body text per page: 3 rows for header/footer
       leaves (screen rows - 3) * screen_width characters visible */
    page_size = (screen_width >= 80) ? 80 * 18 : 40 * 16;

    p = s_body;
    do {
        HOME();
        BOLD_TEXT();
        printf("%s\n", s_val);   /* s_val holds the title */
        NORMAL_TEXT();
        printf("----------------------------------------\n");

        chars = 0;
        while (*p && chars < page_size) {
            putchar(*p++);
            chars++;
        }

        if (*p) {
            printf("\n[Space]=more  [Q]=back\n");
#ifdef __CC65__
            ch = toupper(cgetc());
#else
            ch = toupper(getchar());
#endif
        } else {
            printf("\n\n[END] Press any key...\n");
#ifdef __CC65__
            cgetc();
#else
            getchar();
#endif
            ch = 'Q';
        }
    } while (*p && ch != 'Q');
}

void new_post(void)
{
    /* s_val = title, s_id_result = category — free at this call site */
    int i = 0;
    int ch;
    int json_len;
    uint8_t perr;
    int16_t pn;

    s_val[0] = '\0';
    s_id_result[0] = '\0';
    s_body[0] = '\0';

    HOME();
    BOLD_TEXT();
    printf("NEW POST\n");
    NORMAL_TEXT();
    printf("========\n\n");

    printf("Title: ");
    read_line(s_val, MAX_TITLE_LEN);

    printf("\nCategory: ");
    read_line(s_id_result, MAX_CATEGORY_LEN);

    printf("\nContent (press ESC when done):\n");
    printf("----------------------------------------\n");

#ifdef __CC65__
    while (i < MAX_API_MARKDOWN_BODY_LEN) {
        ch = cgetc();
        if (ch == 27) break;
        if ((ch == 8 || ch == 127) && i > 0) {
            i--;
            putchar('\b');
            putchar(' ');
            putchar('\b');
            continue;
        }
        if (ch == '\r') ch = '\n';
        if (ch >= 32 || ch == '\n') {
            s_body[i++] = (char)ch;
            putchar(ch);
        }
    }
#else
    while ((ch = getchar()) != 4 && ch != 26 && ch != EOF
           && i < MAX_API_MARKDOWN_BODY_LEN) {
        s_body[i++] = (char)ch;
    }
#endif
    s_body[i] = '\0';

    printf("\n----------------------------------------\n");
    printf("Title: %s\n", s_val);
    printf("Category: %s\n", s_id_result);
    printf("Length: %d chars\n", i);
    printf("\nSave as draft? (Y/N): ");

    if (toupper(getchar()) == 'Y') {
        json_len = build_update_json(s_val, s_id_result, s_body,
                                     s_json_buf, sizeof(s_json_buf));
        if (json_len <= 0) {
            printf("\nError building request.\n");
        } else {
            snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts", server_url);
            /* Apple IIc IWM firmware bug: network_http_set_channel_mode() always
               delivers mode 0, so POST body writes are silently discarded.
               PUT mode in DATA mode (mode 0) correctly stores writes to postData. */
            perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
            if (perr) {
                printf("\nOpen error: %d\n", (int)perr);
            } else {
                network_write(s_spec, (uint8_t *)s_json_buf, (uint16_t)json_len);
                printf("\nSending to server...\n");
                perr = network_json_parse(s_spec);
                s_id_result[0] = '\0';
                pn = network_json_query(s_spec, "/id", s_id_result);
                network_close(s_spec);
                if (pn > 0 && s_id_result[0]) {
                    printf("Draft saved!\nID: %s\n", s_id_result);
                } else {
                    printf("Server error (parse %d).\n", (int)perr);
                    printf("Check server URL and connection.\n");
                }
            }
        }
    }

    printf("\nPress any key to return...\n");
    getchar();
}

void edit_post(void)
{
    /* s_val = edit_title, s_id_result = edit_cat — free at this call site */
    int count;
    int i;
    int ch;
    int tlen;
    int maxw;
    int sel;
    int json_len;
    uint8_t perr;
    int16_t pn;

    count = 0;

    /* --- Phase 1: load post list --- */
    HOME();
    BOLD_TEXT();
    printf("EDIT POST\n");
    NORMAL_TEXT();
    printf("Loading...\n");

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/summaries", server_url);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        printf("Connection error: %d\n", (int)perr);
        printf("Press any key...\n");
        getchar();
        return;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        printf("Parse error: %d\n", (int)perr);
        network_close(s_spec);
        printf("Press any key...\n");
        getchar();
        return;
    }

    for (i = 0; i < MAX_API_POSTS; i++) {
        snprintf(s_path, sizeof(s_path), "/%d/id", i);
        s_val[0] = '\0';
        pn = network_json_query(s_spec, s_path, s_val);
        if (pn <= 0 || !s_val[0]) break;
        strncpy(s_ids[i], s_val, MAX_API_ID_LEN);
        s_ids[i][MAX_API_ID_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/title", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        strncpy(s_titles[i], s_val, MAX_API_TITLE_LEN);
        s_titles[i][MAX_API_TITLE_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/published", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        s_pub[i] = (strcmp(s_val, "true") == 0);

        count++;
    }
    network_close(s_spec);

    /* --- Phase 2: select post --- */
    HOME();
    BOLD_TEXT();
    printf("EDIT POST\n");
    NORMAL_TEXT();
    printf("=========\n\n");

    if (count == 0) {
        printf("No posts found.\n\nPress any key...\n");
        getchar();
        return;
    }

    maxw = screen_width - 8;
    for (i = 0; i < count; i++) {
        tlen = (int)strlen(s_titles[i]);
        if (tlen > maxw) s_titles[i][maxw] = '\0';
        printf("%d. [%s] %s\n", i + 1, s_pub[i] ? "P" : "D", s_titles[i]);
    }
    printf("\nSelect (1-%d) or Q: ", count);

#ifdef __CC65__
    ch = cgetc();
#else
    ch = getchar();
#endif
    putchar('\n');
    if (ch < '1' || ch >= '1' + count) return;
    sel = ch - '1';

    /* --- Phase 3: load selected post --- */
    HOME();
    printf("Loading post...\n");

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/markdown",
             server_url, s_ids[sel]);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        printf("Connection error: %d\n", (int)perr);
        printf("Press any key...\n");
        getchar();
        return;
    }
    perr = network_json_parse(s_spec);
    s_val[0]      = '\0';
    s_id_result[0] = '\0';
    s_body[0]     = '\0';
    network_json_query(s_spec, "/title", s_val);
    network_json_query(s_spec, "/category", s_id_result);
    network_json_query(s_spec, "/markdown_body", s_body);
    network_close(s_spec);

    /* --- Phase 4: edit fields --- */
    HOME();
    BOLD_TEXT();
    printf("EDIT POST\n");
    NORMAL_TEXT();
    printf("=========\n\n");

    printf("Title: ");
    read_line_with_default(s_val, (int)sizeof(s_val));

    printf("\nCategory: ");
    read_line_with_default(s_id_result, MAX_API_CATEGORY_LEN + 1);

    printf("\nBody: %d chars\n", (int)strlen(s_body));
    body_editor();

    /* --- Phase 5: confirm and PUT --- */
    printf("Save changes? (Y/N): ");
#ifdef __CC65__
    ch = toupper(cgetc());
#else
    ch = toupper(getchar());
#endif
    putchar('\n');

    if (ch != 'Y') {
        printf("\nCancelled.\n");
        printf("Press any key...\n");
        getchar();
        return;
    }

    json_len = build_update_json(s_val, s_id_result, s_body,
                                 s_json_buf, sizeof(s_json_buf));
    if (json_len <= 0) {
        printf("\nError building request.\n");
    } else {
        snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s",
                 server_url, s_ids[sel]);
        /* Same PUT + network_write workaround as new_post */
        perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
        if (perr) {
            printf("\nOpen error: %d\n", (int)perr);
        } else {
            network_write(s_spec, (uint8_t *)s_json_buf, (uint16_t)json_len);
            printf("Saving...\n");
            perr = network_json_parse(s_spec);
            s_id_result[0] = '\0';
            pn = network_json_query(s_spec, "/id", s_id_result);
            network_close(s_spec);
            if (pn > 0 && s_id_result[0]) {
                printf("\nSaved!\n");
            } else {
                printf("Server error (parse %d).\n", (int)perr);
            }
        }
    }

    printf("\nPress any key...\n");
    getchar();
}

void toggle_publish(void)
{
    /* s_val reused for publish JSON — free after post-list queries */
    int count;
    int i;
    int ch;
    int tlen;
    int maxw;
    int sel;
    int new_pub;
    uint8_t perr;
    int16_t pn;

    count = 0;

    HOME();
    BOLD_TEXT();
    printf("TOGGLE PUBLISH\n");
    NORMAL_TEXT();
    printf("Loading...\n");

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/summaries", server_url);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        printf("Connection error: %d\n", (int)perr);
        printf("Press any key...\n");
        getchar();
        return;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        printf("Parse error: %d\n", (int)perr);
        network_close(s_spec);
        printf("Press any key...\n");
        getchar();
        return;
    }

    for (i = 0; i < MAX_API_POSTS; i++) {
        snprintf(s_path, sizeof(s_path), "/%d/id", i);
        s_val[0] = '\0';
        pn = network_json_query(s_spec, s_path, s_val);
        if (pn <= 0 || !s_val[0]) break;
        strncpy(s_ids[i], s_val, MAX_API_ID_LEN);
        s_ids[i][MAX_API_ID_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/title", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        strncpy(s_titles[i], s_val, MAX_API_TITLE_LEN);
        s_titles[i][MAX_API_TITLE_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/published", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        s_pub[i] = (strcmp(s_val, "true") == 0);

        count++;
    }
    network_close(s_spec);

    HOME();
    BOLD_TEXT();
    printf("TOGGLE PUBLISH\n");
    NORMAL_TEXT();
    printf("==============\n\n");

    if (count == 0) {
        printf("No posts found.\n");
        printf("\nPress any key...\n");
        getchar();
        return;
    }

    maxw = screen_width - 8;
    for (i = 0; i < count; i++) {
        tlen = (int)strlen(s_titles[i]);
        if (tlen > maxw) s_titles[i][maxw] = '\0';
        printf("%d. [%s] %s\n", i + 1, s_pub[i] ? "P" : "D", s_titles[i]);
    }
    printf("\nSelect (1-%d) or Q: ", count);

#ifdef __CC65__
    ch = cgetc();
#else
    ch = getchar();
#endif
    putchar('\n');

    if (ch < '1' || ch >= '1' + count) return;

    sel = ch - '1';
    new_pub = !s_pub[sel];

    HOME();
    BOLD_TEXT();
    printf("TOGGLE PUBLISH\n");
    NORMAL_TEXT();
    printf("==============\n\n");
    printf("Post: %s\n\n", s_titles[sel]);
    printf("Now: %s\n", s_pub[sel] ? "Published" : "Draft");
    printf("Set: %s\n\n", new_pub ? "Published" : "Draft");
    printf("Confirm? (Y/N): ");

#ifdef __CC65__
    ch = toupper(cgetc());
#else
    ch = toupper(getchar());
#endif
    putchar('\n');

    if (ch != 'Y') return;

    snprintf(s_val, sizeof(s_val), "{\"published\":%s}",
             new_pub ? "true" : "false");
    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/publish",
             server_url, s_ids[sel]);

    perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    if (perr) {
        printf("\nOpen error: %d\n", (int)perr);
    } else {
        network_write(s_spec, (uint8_t *)s_val, (uint16_t)strlen(s_val));
        printf("Sending...\n");
        perr = network_json_parse(s_spec);
        s_id_result[0] = '\0';
        pn = network_json_query(s_spec, "/id", s_id_result);
        network_close(s_spec);
        if (pn > 0 && s_id_result[0]) {
            printf("\nDone! %s -> %s\n", s_titles[sel],
                   new_pub ? "Published" : "Draft");
        } else {
            printf("Server error (parse %d).\n", (int)perr);
        }
    }

    printf("\nPress any key...\n");
    getchar();
}

void delete_post(void)
{
    int count;
    int i;
    int ch;
    int tlen;
    int maxw;
    int sel;
    uint8_t perr;
    int16_t pn;

    count = 0;

    /* --- Phase 1: load post list --- */
    HOME();
    BOLD_TEXT();
    printf("DELETE POST\n");
    NORMAL_TEXT();
    printf("Loading...\n");

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/summaries", server_url);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        printf("Connection error: %d\n", (int)perr);
        printf("Press any key...\n");
        getchar();
        return;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        printf("Parse error: %d\n", (int)perr);
        network_close(s_spec);
        printf("Press any key...\n");
        getchar();
        return;
    }

    for (i = 0; i < MAX_API_POSTS; i++) {
        snprintf(s_path, sizeof(s_path), "/%d/id", i);
        s_val[0] = '\0';
        pn = network_json_query(s_spec, s_path, s_val);
        if (pn <= 0 || !s_val[0]) break;
        strncpy(s_ids[i], s_val, MAX_API_ID_LEN);
        s_ids[i][MAX_API_ID_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/title", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        strncpy(s_titles[i], s_val, MAX_API_TITLE_LEN);
        s_titles[i][MAX_API_TITLE_LEN] = '\0';

        snprintf(s_path, sizeof(s_path), "/%d/published", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_path, s_val);
        s_pub[i] = (strcmp(s_val, "true") == 0);

        count++;
    }
    network_close(s_spec);

    /* --- Phase 2: select post --- */
    HOME();
    BOLD_TEXT();
    printf("DELETE POST\n");
    NORMAL_TEXT();
    printf("===========\n\n");

    if (count == 0) {
        printf("No posts found.\n");
        printf("\nPress any key...\n");
        getchar();
        return;
    }

    maxw = screen_width - 8;
    for (i = 0; i < count; i++) {
        tlen = (int)strlen(s_titles[i]);
        if (tlen > maxw) s_titles[i][maxw] = '\0';
        printf("%d. [%s] %s\n", i + 1, s_pub[i] ? "P" : "D", s_titles[i]);
    }
    printf("\nSelect (1-%d) or Q: ", count);

#ifdef __CC65__
    ch = cgetc();
#else
    ch = getchar();
#endif
    putchar('\n');

    if (ch < '1' || ch >= '1' + count) return;
    sel = ch - '1';

    /* --- Phase 3: confirm deletion --- */
    HOME();
    BOLD_TEXT();
    printf("DELETE POST\n");
    NORMAL_TEXT();
    printf("===========\n\n");
    printf("Post: %s\n\n", s_titles[sel]);
    printf("PERMANENTLY DELETE?\n");
    printf("Cannot be undone!\n\n");
    printf("Confirm? (Y/N): ");

#ifdef __CC65__
    ch = toupper(cgetc());
#else
    ch = toupper(getchar());
#endif
    putchar('\n');

    if (ch != 'Y') return;

    /* --- Phase 4: send DELETE via PUT workaround ---
       Apple IIc IWM firmware: OPEN_MODE_HTTP_DELETE is untested; the
       established workaround is OPEN_MODE_HTTP_PUT + network_write().
       The server PUT /api/posts/{id}/delete performs the deletion and
       returns {"id":"<uuid>"} so we can confirm success. */
    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/delete",
             server_url, s_ids[sel]);
    perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    if (perr) {
        printf("\nOpen error: %d\n", (int)perr);
    } else {
        network_write(s_spec, (const uint8_t *)"{}", 2);
        printf("Deleting...\n");
        perr = network_json_parse(s_spec);
        s_id_result[0] = '\0';
        pn = network_json_query(s_spec, "/id", s_id_result);
        network_close(s_spec);
        if (pn > 0 && s_id_result[0]) {
            printf("\nDeleted: %s\n", s_titles[sel]);
        } else {
            printf("Server error (parse %d).\n", (int)perr);
        }
    }

    printf("\nPress any key...\n");
    getchar();
}

/* Reads a line of input with backspace/delete support.
   Returns 1 if confirmed (Enter), 0 if cancelled (ESC). */
int read_line(char *buf, int maxlen)
{
    int i = 0;
    int ch;
#ifdef __CC65__
    while (i < maxlen - 1) {
        ch = cgetc();
        if (ch == '\r') {
            putchar('\n');
            break;
        }
        if (ch == 27) {      /* ESC = cancel */
            putchar('\n');
            buf[0] = '\0';
            return 0;
        }
        if ((ch == 8 || ch == 127) && i > 0) {
            i--;
            putchar('\b');
            putchar(' ');
            putchar('\b');
            continue;
        }
        if (ch >= 32) {
            buf[i++] = (char)ch;
            putchar(ch);
        }
    }
#else
    if (!fgets(buf, maxlen, stdin)) {
        buf[0] = '\0';
        return 0;
    }
    i = strlen(buf);
    if (i > 0 && buf[i - 1] == '\n') i--;
#endif
    buf[i] = '\0';
    return 1;
}

/* Show buf's current content and let the user edit it in-place.
   On cc65: prints existing text then accepts input from that position.
   On host: prints existing text; empty Enter keeps the old value. */
void read_line_with_default(char *buf, int maxlen)
{
    int i;
    int ch;
#ifdef __CC65__
    i = (int)strlen(buf);
    printf("%s", buf);
    while (i < maxlen - 1) {
        ch = cgetc();
        if (ch == '\r') { putchar('\n'); break; }
        if (ch == 27)   { putchar('\n'); break; }   /* ESC = keep as-is */
        if ((ch == 8 || ch == 127) && i > 0) {
            i--;
            putchar('\b'); putchar(' '); putchar('\b');
            continue;
        }
        if (ch >= 32) {
            buf[i++] = (char)ch;
            putchar(ch);
        }
    }
    buf[i] = '\0';
#else
    {
        char old[256];
        strncpy(old, buf, sizeof(old) - 1);
        old[sizeof(old) - 1] = '\0';
        printf("%s", buf);
        if (fgets(buf, maxlen, stdin)) {
            i = (int)strlen(buf);
            if (i > 0 && buf[i - 1] == '\n') { i--; buf[i] = '\0'; }
            if (i == 0) { strncpy(buf, old, maxlen - 1); buf[maxlen - 1] = '\0'; }
        }
    }
#endif
}

/* ---- full-screen word-processor body editor ---- */

#define WP_HDR 2   /* header rows above edit area (title row + separator row) */

/* Render s_body in the edit area starting at byte offset `top`.
   `cur` is the cursor offset; drawn in inverse video.
   `first_row` is the first edit-area row to render (rows above are untouched).
   `len` = strlen(s_body), `er` = edit rows, `cols` = screen width.
   Sets cr and cc to the cursor's row/col within the edit area. */
static void we_draw(int top, int cur, int er, int len, int cols,
                    int first_row, int *cr, int *cc)
{
#ifdef __CC65__
    int off = top, row = 0, col = 0, r, found = 0;
    *cr = 0; *cc = 0;

    /* Advance offset to first_row without touching the screen */
    while (row < first_row) {
        col = 0;
        while (col < cols) {
            if (off == cur && !found) { *cr = row; *cc = col; found = 1; }
            if (off >= len || s_body[off] == '\n') break;
            off++; col++;
        }
        if (off < len && s_body[off] == '\n') off++;
        else if (off >= len) { row++; break; }
        row++;
    }

    /* Render from first_row onward */
    while (row < er) {
        gotoxy(0, WP_HDR + row);
        col = 0;
        while (col < cols) {
            if (off == cur && !found) {
                *cr = row; *cc = col; found = 1;
                revers(1);
                if (off < len && s_body[off] != '\n') putchar(s_body[off++]);
                else putchar(' ');
                revers(0);
                col++; continue;
            }
            if (off >= len || s_body[off] == '\n') break;
            putchar(s_body[off++]);
            col++;
        }
        for (r = col; r < cols; r++) putchar(' ');
        if (off < len && s_body[off] == '\n') off++;
        else if (off >= len) { row++; break; }
        row++;
    }
    if (!found) { *cr = (row < er ? row : er - 1); *cc = col; }
    for (; row < er; row++) {
        gotoxy(0, WP_HDR + row);
        for (col = 0; col < cols; col++) putchar(' ');
    }
#else
    (void)top; (void)cur; (void)er; (void)len; (void)cols; (void)first_row;
    *cr = 0; *cc = 0;
#endif
}

/* Find the absolute (screen row, col) of byte offset `target` in s_body. */
static void wp_get_pos(int target, int len, int cols, int *row, int *col)
{
    int off = 0, r = 0, c = 0;
    while (off < target && off < len) {
        if (s_body[off++] == '\n') { r++; c = 0; }
        else if (++c >= cols) { c = 0; r++; }
    }
    *row = r; *col = c;
}

/* Find the byte offset of the first char on the screen-row containing `target`. */
static int wp_row_start(int target, int len, int cols)
{
    int off = 0, col = 0, rs = 0;
    while (off < target && off < len) {
        char c = s_body[off++];
        if (c == '\n') { col = 0; rs = off; }
        else if (++col >= cols) { col = 0; rs = off; }
    }
    return rs;
}

/* Find the byte offset of the char at absolute screen position (tr, tc). */
static int wp_offset_at(int tr, int tc, int len, int cols)
{
    int off = 0, row = 0, col = 0;
    while (off <= len) {
        if (row == tr && (col >= tc || off >= len || s_body[off] == '\n'))
            return off;
        if (off >= len) break;
        if (s_body[off] == '\n') { row++; col = 0; }
        else if (++col >= cols) { col = 0; row++; }
        off++;
    }
    return len;
}

/* Full-screen word-processor style editor for s_body.
   Arrow keys navigate; printable keys insert at cursor; DEL=backspace.
   Frame is drawn once. Text/scroll changes call we_draw (full repaint of
   edit area). Cursor-only movement erases old inverse char and draws new
   one without a full repaint. */
void body_editor(void)
{
    int cursor, top_char, cur_row, cur_col;
    int er, cols, ch, len;
    int ar, ac, tr, new_pos, i;
    int text_changed, scroll_changed;
    int old_cursor, old_cr, old_cc;
    int first_row;
    int sl_col, sl_off, sl_col2, sl_found;  /* single-line fast path */

    cursor = 0; top_char = 0; cur_row = 0; cur_col = 0;
    er   = (screen_width >= 80) ? 20 : 18;
    cols = screen_width;

    HOME();
    BOLD_TEXT();
    printf("BODY EDITOR");
    NORMAL_TEXT();
    printf("  ESC=done\n");
    for (i = 0; i < cols; i++) putchar('-');
    putchar('\n');

    len = (int)strlen(s_body);
    we_draw(top_char, cursor, er, len, cols, 0, &cur_row, &cur_col);

#ifdef __CC65__
    gotoxy(0, WP_HDR + er);
    printf("Len:%-4d", len);
    gotoxy(cur_col, WP_HDR + cur_row);
#endif

    for (;;) {
        old_cursor = cursor;
        old_cr = cur_row;
        old_cc = cur_col;

#ifdef __CC65__
        ch = cgetc();
#else
        ch = getchar();
#endif
        text_changed = 0; scroll_changed = 0;

        if (ch == 27) break;                        /* ESC = done */

        if (ch == 0x08) {                           /* left arrow */
            if (cursor > 0) cursor--;
        } else if (ch == 0x15) {                    /* right arrow */
            if (cursor < len) cursor++;
        } else if (ch == 0x0B) {                    /* up arrow */
            wp_get_pos(cursor, len, cols, &ar, &ac);
            if (ar > 0) cursor = wp_offset_at(ar - 1, ac, len, cols);
        } else if (ch == 0x0A) {                    /* down arrow */
            wp_get_pos(cursor, len, cols, &ar, &ac);
            new_pos = wp_offset_at(ar + 1, ac, len, cols);
            if (new_pos != cursor) cursor = new_pos;
        } else if (ch == 0x7F) {                    /* DEL = backspace */
            if (cursor > 0) {
                cursor--;
                memmove(s_body + cursor, s_body + cursor + 1, len - cursor);
                len--;
                text_changed = 1;
            }
        } else if (ch == 0x0D) {                    /* RETURN = newline */
            if (len < MAX_API_MARKDOWN_BODY_LEN) {
                memmove(s_body + cursor + 1, s_body + cursor, len - cursor + 1);
                s_body[cursor++] = '\n';
                len++;
                text_changed = 1;
            }
        } else if (ch >= 0x20 && ch < 0x7F) {      /* printable char */
            if (len < MAX_API_MARKDOWN_BODY_LEN) {
                memmove(s_body + cursor + 1, s_body + cursor, len - cursor + 1);
                s_body[cursor++] = (char)ch;
                len++;
                text_changed = 1;
            }
        }

        wp_get_pos(cursor, len, cols, &ar, &ac);

        if (cursor < top_char) {
            top_char = wp_row_start(cursor, len, cols);
            scroll_changed = 1;
            wp_get_pos(top_char, len, cols, &tr, &new_pos);
        } else {
            wp_get_pos(top_char, len, cols, &tr, &new_pos);
            if (ar - tr >= er) {
                top_char = wp_offset_at(ar - er + 1, 0, len, cols);
                wp_get_pos(top_char, len, cols, &tr, &new_pos);
                scroll_changed = 1;
            }
        }

        if (text_changed || scroll_changed) {
#ifdef __CC65__
            if (!scroll_changed && ch != 0x0D && ar - tr == old_cr) {
                /* Single-line fast path: only repaint from edit point to EOL.
                   For insertion: start at old_cc (new char + shifted tail).
                   For backspace: start at ac (shifted tail + erased last char). */
                if (ch == 0x7F) { sl_col = ac;     sl_off = cursor; }
                else             { sl_col = old_cc; sl_off = cursor - 1; }
                gotoxy(sl_col, WP_HDR + (ar - tr));
                sl_col2 = sl_col; sl_found = 0;
                while (sl_col2 < cols) {
                    if (sl_off == cursor && !sl_found) {
                        sl_found = 1;
                        revers(1);
                        if (sl_off < len && s_body[sl_off] != '\n') putchar(s_body[sl_off++]);
                        else putchar(' ');
                        revers(0);
                        sl_col2++; continue;
                    }
                    if (sl_off >= len || s_body[sl_off] == '\n') break;
                    putchar(s_body[sl_off++]);
                    sl_col2++;
                }
                for (; sl_col2 < cols; sl_col2++) putchar(' ');
                cur_row = ar - tr; cur_col = ac;
                gotoxy(0, WP_HDR + er);
                printf("Len:%-4d", len);
            } else {
                first_row = scroll_changed ? 0 : (old_cr > 0 ? old_cr - 1 : 0);
                we_draw(top_char, cursor, er, len, cols, first_row, &cur_row, &cur_col);
                gotoxy(0, WP_HDR + er);
                printf("Len:%-4d", len);
                gotoxy(cur_col, WP_HDR + cur_row);
            }
#else
            first_row = scroll_changed ? 0 : (old_cr > 0 ? old_cr - 1 : 0);
            we_draw(top_char, cursor, er, len, cols, first_row, &cur_row, &cur_col);
#endif
        } else {
            cur_row = ar - tr;
            cur_col = ac;
#ifdef __CC65__
            /* Fast path: erase old inverse cursor, draw new one */
            gotoxy(old_cc, WP_HDR + old_cr);
            if (old_cursor < len && s_body[old_cursor] != '\n')
                putchar(s_body[old_cursor]);
            else
                putchar(' ');
            gotoxy(cur_col, WP_HDR + cur_row);
            revers(1);
            if (cursor < len && s_body[cursor] != '\n')
                putchar(s_body[cursor]);
            else
                putchar(' ');
            revers(0);
#endif
        }
    }
}

void show_config(void)
{
    int choice;

    while (1) {
        HOME();
        BOLD_TEXT();
        printf("=== CONFIGURATION ===\n");
        NORMAL_TEXT();
        printf("\n");
        printf("1. Server URL\n");
        printf("   %s\n", server_url);
        printf("\nQ. Back\n");
        printf("\nSelect option: ");

        choice = toupper(getchar());
        if (choice == 'Q') break;

        if (choice == '1') {
            HOME();
            BOLD_TEXT();
            printf("=== SERVER URL ===\n");
            NORMAL_TEXT();
            printf("\nCurrent: %s\n", server_url);
            printf("\nNew URL (ESC to cancel):\n");
            s_val[0] = '\0';
            if (read_line(s_val, (int)sizeof(s_val)) && s_val[0] != '\0') {
                strncpy(server_url, s_val, sizeof(server_url) - 1);
                server_url[sizeof(server_url) - 1] = '\0';
                printf("\nURL updated.\n");
                printf("Press any key to continue...\n");
                getchar();
            }
        }
    }
}
