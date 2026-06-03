#include <fujinet-fuji.h>
#include <fujinet-network.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "api.h"
#include "network.h"
#include "ui.h"

/* FujiNet App Key — creator @BEE5, app 0x01 (FujiBlogger).
   Key slot 0x00 stores the server URL (up to MAX_APPKEY_LEN = 64 bytes).
   $DD Read returns a 66-byte response (2-byte length prefix + 64 data bytes)
   so the read buffer must be MAX_APPKEY_LEN + 2; fuji_read_appkey() sets
   *count to the actual stored byte count via that prefix. */
#define APPKEY_CREATOR  0xBEE5
#define APPKEY_APP      0x01
#define APPKEY_KEY_URL  0x00

#define MAX_TITLE_LEN 80
#define MAX_CATEGORY_LEN 64
#define MAX_CONTENT_LEN 2000
#define MAX_RESPONSE_LEN 4096
#define MAX_POSTS 50

/* Screen layout constants */
#define VP_HDR 4   /* rows above view-post viewport (hdr+hline+title+hline) */

#ifdef __CC65__
#include <conio.h>
#include <apple2.h>
#define HOME() clrscr()
#elif defined(_CMOC_VERSION_)
#define HOME() clrscr()
#else
#define HOME() printf("\033[H\033[J")
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
void show_stats(void);
void show_network_status(void);
void get_screen_width(void);
void show_config(void);
void test_server(void);
static void appkey_load(void);
static void appkey_save(void);
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
char server_url[256] = "http://fujiblogger.example.com";

/* Shared BSS buffers — menu functions never run concurrently so one copy each
   covers all callers. Sizes chosen to keep the apple2 BSS segment within its
   ~7 KB limit.
   s_json_buf: 2400 bytes handles real-world Markdown with up to ~65% newline
   density in a 1280-char body (each '\n' → 2-char escape).  The encoder in
   api.c truncates gracefully if the limit is ever reached. */
static char s_spec[300];
static char s_ids[MAX_API_POSTS][MAX_API_ID_LEN + 1];
static char s_titles[MAX_API_POSTS][MAX_API_TITLE_LEN + 1];
static int  s_pub[MAX_API_POSTS];
static char s_path[16];
static char s_val[MAX_API_TITLE_LEN + 1];
static char s_body[MAX_API_MARKDOWN_BODY_LEN + 1];
static char s_json_buf[2048];
static char s_id_result[MAX_API_ID_LEN + 1];

/* Month abbreviations packed as a flat string — RODATA, not BSS.
   Access month i with: %.3s applied to (s_months_str + i*3)            */
static const char s_months_str[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

/* Static forward declarations — helpers defined near body_editor but
   also called from view_post which appears earlier in this file.      */
static int  row_next(int off, int len, int cols, int *disp_end);
static void wp_get_pos(int target, int len, int cols, int *row, int *col);
static int  wp_offset_at(int tr, int tc, int len, int cols);
static void vp_draw(int top_line, int er, int len, int cols);
static void we_draw(int top, int cur, int er, int len, int cols,
                    int first_row, int last_row, int *cr, int *cc);

/* ── Internal helpers ──────────────────────────────────────── */

/* Show a loading screen for a given section title. */
static void screen_loading(const char *title)
{
    HOME();
    ui_header(title, "");
    ui_hline();
    printf("\n  Loading...\n");
}

/* Show a connection/parse error and wait for a keypress. */
static void screen_error(const char *msg, int code)
{
    printf("\n  %s: %d\n\n", msg, code);
    ui_hline();
    printf("  Press any key...\n");
    getchar();
}

/* Draw a footer separator and wait for any keypress. */
static void wait_key(void)
{
    ui_hline();
    printf("  Press any key...\n");
    getchar();
}

/* Fetch the raw Markdown body for post `id` into s_body via the plain-text
   /api/posts/{id}/body endpoint.

   network_json_query() is limited to 512 bytes per field by the IWM firmware.
   network_read() has no such per-field limit but is bounded by one SmartPort
   block (~512 bytes) per call.  We therefore poll with network_status() and
   loop until the firmware reports no more bytes available.

   Returns the number of bytes stored in s_body (0 on error). */
static int fetch_body(const char *id)
{
    int16_t n;
    uint8_t perr;
    uint16_t bw;
    uint8_t conn, nerr;
    int retries, offset, to_read;

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/body",
             server_url, id);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        screen_error("Body fetch error", (int)perr);
        s_body[0] = '\0';
        return 0;
    }

    /* Wait for the HTTP response to start arriving. */
    bw = 0; conn = 1; nerr = 0;
    retries = 100;
    while (bw == 0 && conn != 0 && retries > 0) {
        network_status(s_spec, &bw, &conn, &nerr);
        retries--;
    }

    /* Read in 512-byte SmartPort chunks until the buffer is full or
       the firmware reports no more bytes available. */
    offset = 0;
    while (bw > 0 && offset < MAX_API_MARKDOWN_BODY_LEN) {
        to_read = (int)bw;
        if (to_read > MAX_API_MARKDOWN_BODY_LEN - offset)
            to_read = MAX_API_MARKDOWN_BODY_LEN - offset;
        n = network_read(s_spec, (uint8_t *)s_body + offset,
                         (uint16_t)to_read);
        if (n <= 0) break;
        offset += (int)n;
        /* Check whether more bytes arrived while we were processing. */
        bw = 0; conn = 1; nerr = 0;
        network_status(s_spec, &bw, &conn, &nerr);
    }

    network_close(s_spec);
    s_body[offset] = '\0';
    return offset;
}

/* Fetch /api/posts/summaries (with optional suffix, e.g. "?published_only=true")
   into s_ids / s_titles / s_pub.  Returns count (>=0) or -1 on error
   (error already displayed to the user). */
static int fetch_post_list(const char *suffix)
{
    int count;
    int i;
    int16_t pn;
    uint8_t perr;

    count = 0;
    snprintf(s_spec, sizeof(s_spec),
             "N1:%s/api/posts/summaries%s", server_url, suffix);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        screen_error("Connection error", (int)perr);
        return -1;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        printf("\n  Parse error: %d\n", (int)perr);
        network_close(s_spec);
        wait_key();
        return -1;
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
        /* server returns published as integer 1/0 so network_json_query can read it */
        s_pub[i] = (s_val[0] == '1');

        count++;
    }
    network_close(s_spec);
    return count;
}

/* Display a numbered post list and prompt for a selection.
   Returns the 0-based index chosen, or -1 if the user cancelled or
   there are no posts (error/empty already displayed). */
static int pick_from_list(int count)
{
    int i, ch, tlen, maxw;

    if (count == 0) {
        printf("  No posts found.\n\n");
        wait_key();
        return -1;
    }

    maxw = screen_width - (screen_width >= 80 ? 28 : 12);
    if (maxw < 4) maxw = 4;
    for (i = 0; i < count; i++) {
        tlen = (int)strlen(s_titles[i]);
        if (tlen > maxw) s_titles[i][maxw] = '\0';
        ui_post_row(i + 1, s_pub[i], s_titles[i]);
    }
    printf("\n");
    ui_hline();
    printf("  Select (1-%d) or Q: ", count);

#ifdef __CC65__
    ch = cgetc();
#else
    ch = getchar();
#endif
    putchar('\n');

    if (ch >= '1' && ch < '1' + count)
        return ch - '1';
    return -1;
}

/* ── fetch_category_list ───────────────────────────────────── */
/* Fetches /api/categories and stores names in s_titles[].  s_ids[] and
   s_val[] are deliberately NOT touched: s_ids[] may hold a live post-ID
   for edit_post's Phase 4, and s_val[] holds the post title entered before
   pick_category() was called.
   Returns count (0..MAX_API_POSTS) or -1 on connection error. */
static int fetch_category_list(void)
{
    int count, i;
    int16_t pn;
    uint8_t perr;

    count = 0;
    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/categories", server_url);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        screen_error("Connection error", (int)perr);
        return -1;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        network_close(s_spec);
        return 0;   /* no categories yet — not a hard error */
    }

    for (i = 0; i < MAX_API_POSTS; i++) {
        snprintf(s_path, sizeof(s_path), "/%d/name", i);
        s_titles[i][0] = '\0';
        pn = network_json_query(s_spec, s_path, s_titles[i]);
        if (pn <= 0 || !s_titles[i][0]) break;
        s_titles[i][MAX_API_TITLE_LEN] = '\0';
        count++;
    }
    network_close(s_spec);
    return count;
}

/* ── pick_category ─────────────────────────────────────────── */
/* Show a numbered category picker.  s_id_result must hold the
   current category on entry (empty string for new posts).
   On return s_id_result holds the chosen or typed category.
   Returns 0 normally, -1 if ESC pressed with no value set. */
static int pick_category(void)
{
    int count, cap, i, ch, tlen, maxw;

    screen_loading("CATEGORY");
    count = fetch_category_list();
    if (count < 0) return -1;

    cap = (count > 9) ? 9 : count;

    HOME();
    ui_header("CATEGORY", "");
    ui_hline();
    printf("\n");

    maxw = screen_width - (screen_width >= 80 ? 12 : 8);
    if (maxw < 4) maxw = 4;

    for (i = 0; i < cap; i++) {
        tlen = (int)strlen(s_titles[i]);
        if (tlen > maxw) { s_titles[i][maxw] = '\0'; tlen = maxw; }
        printf("  %d. %s\n", i + 1, s_titles[i]);
    }
    printf("  N. New...\n");

    if (s_id_result[0]) {
        tlen = (int)strlen(s_id_result);
        if (tlen > maxw) s_id_result[maxw] = '\0';
        printf("\n  Current: %s\n", s_id_result);
    }

    printf("\n");
    ui_hline();
    if (s_id_result[0])
        printf("  1-%d/N/Enter to keep: ", cap > 0 ? cap : 0);
    else
        printf("  Select (1-%d) or N: ", cap > 0 ? cap : 0);

#ifdef __CC65__
    ch = cgetc();
#else
    ch = getchar();
#endif
    putchar('\n');

    if (ch == 27) {                         /* ESC */
        if (s_id_result[0]) return 0;       /* keep existing */
        return -1;
    }

    if ((ch == '\r' || ch == '\n') && s_id_result[0])
        return 0;                           /* keep existing */

    ch = toupper((unsigned char)ch);

    if (ch >= '1' && ch < '1' + cap) {
        strncpy(s_id_result, s_titles[ch - '1'], MAX_API_CATEGORY_LEN);
        s_id_result[MAX_API_CATEGORY_LEN] = '\0';
        return 0;
    }

    if (ch == 'N') {
        printf("  New category: ");
        s_id_result[0] = '\0';
        read_line(s_id_result, MAX_CATEGORY_LEN);
        return 0;
    }

    return 0;   /* unrecognised key — keep whatever was in s_id_result */
}

/* ── main ──────────────────────────────────────────────────── */

int main(void)
{
    get_screen_width();
    appkey_load();   /* override default server URL if one is stored */
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

/* ── show_splash ───────────────────────────────────────────── */

void show_splash(void)
{
    AdapterConfigExtended ace;
    int i;

    HOME();
    ui_header("FUJIBLOGGER", "");
    ui_hline();
    printf("\n  Initializing FujiNet...\n\n");

    if (!fuji_get_adapter_config_extended(&ace)) {
        printf("  ERROR: FujiNet not detected!\n");
        printf("  Check hardware and restart.\n\n");
        wait_key();
        exit(1);
    }

    printf("  FujiNet Version : %s\n", ace.fn_version);
    printf("  Adapter SSID    : %s\n", ace.ssid);
    printf("  MAC             : ");
    for (i = 0; i < 6; i++) {
        printf("%02X", ace.macAddress[i]);
        if (i < 5) printf(":");
    }
    printf("\n");
    printf("  Screen          : %d columns\n\n", screen_width);

    wait_key();
}

/* ── main_menu ─────────────────────────────────────────────── */

void main_menu(void)
{
    int choice;

    HOME();
    ui_header("MAIN MENU", "Q: Quit");
    ui_hline();
    printf("\n");
    ui_indent(); printf("1.  List Posts\n");
    ui_indent(); printf("2.  New Post\n");
    ui_indent(); printf("3.  Edit Post\n");
    ui_indent(); printf("4.  Toggle Publish\n");
    ui_indent(); printf("5.  Delete Post\n");
    ui_indent(); printf("6.  Stats\n");
    ui_indent(); printf("7.  Network Status\n");
    ui_indent(); printf("8.  Configuration\n");
    ui_indent(); printf("Q.  Quit\n");
    printf("\n");
    ui_hline();
    printf("  Select option: ");

    choice = toupper(getchar());

    switch (choice) {
        case '1': list_posts();          break;
        case '2': new_post();            break;
        case '3': edit_post();           break;
        case '4': toggle_publish();      break;
        case '5': delete_post();         break;
        case '6': show_stats();          break;
        case '7': show_network_status(); break;
        case '8': show_config();         break;
        case 'Q': HOME(); exit(0);
        default:  break;
    }
}

/* ── show_network_status ───────────────────────────────────── */

void show_network_status(void)
{
    AdapterConfigExtended ace;
    int i;
    int ch;

    do {
        HOME();
        ui_header("NETWORK STATUS", "T: Test  Q: Back");
        ui_hline();
        printf("\n");

        if (fuji_get_adapter_config_extended(&ace)) {
            printf("  Status  : Connected\n");
            printf("  Version : %s\n", ace.fn_version);
            printf("  SSID    : %s\n", ace.ssid);
            printf("  MAC     : ");
            for (i = 0; i < 6; i++) {
                printf("%02X", ace.macAddress[i]);
                if (i < 5) printf(":");
            }
            printf("\n");
        } else {
            printf("  Status  : NOT DETECTED\n");
        }

        printf("\n");
        printf("  Server  : %s\n", server_url);
        printf("  Screen  : %d columns\n\n", screen_width);
        ui_hline();
        printf("  T: Test Server   Q: Back   Select: ");

        ch = toupper(getchar());
        if (ch == 'T') test_server();
    } while (ch != 'Q');
}

/* ── test_server ───────────────────────────────────────────── */

void test_server(void)
{
    int bytes;
    int ch;

    do {
        HOME();
        ui_header("SERVER TEST", "Q: Back");
        ui_hline();
        printf("\n  %s\n\n", server_url);
        ui_indent(); printf("G.  GET  /api/ping\n");
        ui_indent(); printf("P.  PUT  /api/posts (test create)\n");
        ui_indent(); printf("Q.  Back\n\n");
        ui_hline();
        printf("  Select: ");

        ch = toupper(getchar());

        if (ch == 'G') {
            HOME();
            ui_header("SERVER TEST", "GET /api/ping");
            ui_hline();
            printf("\n  Sending GET /api/ping...\n\n");
            bytes = network_get(server_url, "/api/ping",
                                (uint8_t *)s_val, (int)sizeof(s_val) - 1);
            if (bytes > 0) {
                s_val[bytes] = '\0';
                printf("  OK!  %d bytes received:\n  %s\n", bytes, s_val);
            } else {
                printf("  FAILED  (error %d)\n", (int)network_get_last_error());
            }
            printf("\n");
            wait_key();

        } else if (ch == 'P') {
            static const char test_json[] =
                "{\"title\":\"Test\",\"markdown_body\":\"# Test\","
                "\"category\":\"test\",\"published\":false}";
            uint8_t perr;
            int16_t pn;

            HOME();
            ui_header("SERVER TEST", "PUT /api/posts");
            ui_hline();
            printf("\n  Sending PUT /api/posts...\n\n");
            snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts", server_url);
            perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
            if (perr) {
                printf("  Open error: %d\n", (int)perr);
            } else {
                network_write(s_spec, (const uint8_t *)test_json,
                              (uint16_t)(sizeof(test_json) - 1));
                printf("  Waiting for response...\n");
                perr = network_json_parse(s_spec);
                s_id_result[0] = '\0';
                pn = network_json_query(s_spec, "/id", s_id_result);
                network_close(s_spec);
                if (pn > 0 && s_id_result[0]) {
                    printf("  OK!  Post created:\n  %s\n", s_id_result);
                } else {
                    printf("  FAILED  (parse error %d)\n", (int)perr);
                }
            }
            printf("\n");
            wait_key();
        }
    } while (ch != 'Q');
}

/* ── list_posts ────────────────────────────────────────────── */

void list_posts(void)
{
    int count;
    int sel;

    screen_loading("PUBLISHED POSTS");
    count = fetch_post_list("?published_only=true");
    if (count < 0) return;

    HOME();
    ui_header("PUBLISHED POSTS", "Q: Back");
    ui_hline();
    printf("\n");

    sel = pick_from_list(count);
    if (sel >= 0)
        view_post(s_ids[sel]);
}

/* ── view_post ─────────────────────────────────────────────── */
/* Fixed-frame layout (24-row screen):
     Row  0 : header bar       (ui_header)
     Row  1 : hline            (ui_hline)
     Row  2 : post title
     Row  3 : hline
     Rows 4-22 : scrollable content viewport  (VP_HDR=4, er=19)
     Row 23 : status bar (inverse-video)
   On non-CC65 platforms (dev/test) a simpler character-count pager
   is used since gotoxy() is not available.                         */

void view_post(const char *id)
{
    int len, total_lines, er, cols;
    int top_line, ch, tl, tc, i;
    uint8_t perr;

    er   = 19;   /* content rows: 24 total - 4 header - 1 status */
    cols = screen_width;

    /* Loading screen */
    HOME();
    ui_header("VIEW POST", "");
    ui_hline();
    printf("\n  Loading...\n");

    /* Fetch title via JSON (short field — json_query is safe here). */
    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/markdown",
             server_url, id);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        screen_error("Connection error", (int)perr);
        return;
    }
    perr = network_json_parse(s_spec);
    s_val[0] = '\0';
    network_json_query(s_spec, "/title", s_val);
    network_close(s_spec);

    /* Fetch body via plain text to bypass the IWM firmware's 512-byte
       network_json_query result limit. */
    len = fetch_body(id);

    /* Count total visual lines (with word-wrap) for scroll limiting. */
    wp_get_pos(len, len, cols, &tl, &tc);
    total_lines = tl + 1;

    top_line = 0;

#ifdef __CC65__
    /* Draw fixed frame — never redrawn inside the loop to avoid flicker. */
    HOME();
    ui_header("VIEW POST", "Q:back");
    ui_hline();
    /* Row 2: post title, padded to full width */
    gotoxy(0, 2);
    {
        int tlen = (int)strlen(s_val);
        if (tlen > cols - 2) { s_val[cols - 2] = '\0'; tlen = cols - 2; }
        printf("  %s", s_val);
        for (i = tlen + 2; i < cols; i++) putchar(' ');
    }
    /* Row 3: separator */
    gotoxy(0, 3);
    for (i = 0; i < cols; i++) putchar('=');

    for (;;) {
        /* Repaint the scrollable content area */
        vp_draw(top_line, er, len, cols);

        /* Status bar: inverse-video on the last screen row.
           String is exactly 39 chars; fill to cols-1 to avoid wrapping. */
        gotoxy(0, VP_HDR + er);
        revers(1);
        printf("Arrows:scroll  Spc:pgdn  B:pgup  Q:back");
        for (i = 39; i < cols - 1; i++) putchar(' ');
        revers(0);

        ch = cgetc();

        if (ch == 'q' || ch == 'Q') break;
        if (ch == 0x0B) {                    /* up arrow */
            if (top_line > 0) top_line--;
        } else if (ch == 0x0A) {             /* down arrow */
            if (top_line + er < total_lines) top_line++;
        } else if (ch == ' ') {              /* Space = page down */
            top_line += er;
            if (top_line + er > total_lines)
                top_line = (total_lines > er) ? total_lines - er : 0;
        } else if (ch == 'b' || ch == 'B') { /* B = page up */
            top_line -= er;
            if (top_line < 0) top_line = 0;
        }
    }

#else
    /* Non-CC65: character-count pager (no gotoxy available) */
    {
        const char *p = s_body;
        int page_size = (screen_width >= 80) ? 80 * 17 : 40 * 16;
        int chars;
        do {
            HOME();
            ui_header("VIEW POST", *p ? "Spc: More  Q: Back" : "Any: Back");
            ui_hline();
            printf("\n  %s\n", s_val);
            ui_hline();
            chars = 0;
            while (*p && chars < page_size) {
                putchar(*p++);
                chars++;
            }
            printf("\n");
            ui_hline();
            if (*p) {
                printf("  [Space]: more    [Q]: back\n");
                ch = toupper(getchar());
            } else {
                printf("  [END]  Press any key...\n");
                getchar();
                ch = 'Q';
            }
        } while (*p && ch != 'Q');
    }
#endif
}

/* ── new_post ──────────────────────────────────────────────── */

void new_post(void)
{
    /* s_val = title, s_id_result = category */
    int json_len;
    uint8_t perr;
    int16_t pn;

    s_val[0]       = '\0';
    s_id_result[0] = '\0';
    s_body[0]      = '\0';

    HOME();
    ui_header("NEW POST", "Esc: Cancel");
    ui_hline();
    printf("\n");
    ui_indent(); printf("Title    : ");
    read_line(s_val, MAX_TITLE_LEN);
    printf("\n");
    pick_category();

    body_editor();

    HOME();
    ui_header("NEW POST", "Save Draft?");
    ui_hline();
    printf("\n");
    printf("  Title    : %s\n", s_val);
    printf("  Category : %s\n", s_id_result);
    printf("  Length   : %d chars\n\n", (int)strlen(s_body));
    ui_hline();
    printf("  Save as draft? (Y/N): ");

    if (toupper(getchar()) == 'Y') {
        json_len = build_update_json(s_val, s_id_result, s_body,
                                     s_json_buf, sizeof(s_json_buf));
        if (json_len <= 0) {
            printf("\n  Error building request.\n");
        } else {
            snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts", server_url);
            /* Apple IIc IWM firmware bug: network_http_set_channel_mode() always
               delivers mode 0, so POST body writes are silently discarded.
               PUT mode in DATA mode (mode 0) correctly stores writes to postData. */
            perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
            if (perr) {
                printf("\n  Open error: %d\n", (int)perr);
            } else {
                network_write(s_spec, (uint8_t *)s_json_buf, (uint16_t)json_len);
                printf("\n  Sending to server...\n");
                perr = network_json_parse(s_spec);
                s_id_result[0] = '\0';
                pn = network_json_query(s_spec, "/id", s_id_result);
                network_close(s_spec);
                if (pn > 0 && s_id_result[0]) {
                    printf("  Draft saved!\n");
                } else {
                    printf("  Server error (parse %d).\n", (int)perr);
                }
            }
        }
    } else {
        printf("\n  Cancelled.\n");
    }

    printf("\n");
    wait_key();
}

/* ── edit_post ─────────────────────────────────────────────── */

void edit_post(void)
{
    /* s_val = title, s_id_result = category */
    int count;
    int sel;
    int ch;
    int json_len;
    uint8_t perr;
    int16_t pn;

    /* --- Phase 1: load and display post list --- */
    screen_loading("EDIT POST");
    count = fetch_post_list("");
    if (count < 0) return;

    HOME();
    ui_header("EDIT POST", "Q: Back");
    ui_hline();
    printf("\n");

    sel = pick_from_list(count);
    if (sel < 0) return;

    /* --- Phase 2: load selected post --- */
    HOME();
    ui_header("EDIT POST", "");
    ui_hline();
    printf("\n  Loading post...\n");

    /* Fetch title + category via JSON (short fields, json_query safe). */
    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/markdown",
             server_url, s_ids[sel]);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        screen_error("Connection error", (int)perr);
        return;
    }
    perr = network_json_parse(s_spec);
    s_val[0]       = '\0';
    s_id_result[0] = '\0';
    network_json_query(s_spec, "/title", s_val);
    network_json_query(s_spec, "/category", s_id_result);
    network_close(s_spec);

    /* Fetch body via plain text to bypass the IWM firmware's 512-byte
       network_json_query result limit. */
    fetch_body(s_ids[sel]);

    /* --- Phase 3: edit fields --- */
    HOME();
    ui_header("EDIT POST", "Esc: Keep  Enter: Change");
    ui_hline();
    printf("\n");
    ui_indent(); printf("Title    : ");
    read_line_with_default(s_val, (int)sizeof(s_val));
    printf("\n");
    pick_category();
    printf("\n  Body: %d chars\n", (int)strlen(s_body));
    body_editor();

    /* --- Phase 4: confirm and PUT --- */
    HOME();
    ui_header("EDIT POST", "Y: Save  N: Cancel");
    ui_hline();
    printf("\n");
    printf("  Title    : %s\n", s_val);
    printf("  Category : %s\n\n", s_id_result);
    ui_hline();
    printf("  Save changes? (Y/N): ");

#ifdef __CC65__
    ch = toupper(cgetc());
#else
    ch = toupper(getchar());
#endif
    putchar('\n');

    if (ch != 'Y') {
        printf("\n  Cancelled.\n\n");
        wait_key();
        return;
    }

    json_len = build_update_json(s_val, s_id_result, s_body,
                                 s_json_buf, sizeof(s_json_buf));
    if (json_len <= 0) {
        printf("\n  Error building request.\n");
    } else {
        snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s",
                 server_url, s_ids[sel]);
        /* Same PUT + network_write workaround as new_post */
        perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
        if (perr) {
            printf("\n  Open error: %d\n", (int)perr);
        } else {
            network_write(s_spec, (uint8_t *)s_json_buf, (uint16_t)json_len);
            printf("\n  Saving...\n");
            perr = network_json_parse(s_spec);
            s_id_result[0] = '\0';
            pn = network_json_query(s_spec, "/id", s_id_result);
            network_close(s_spec);
            if (pn > 0 && s_id_result[0]) {
                printf("  Saved!\n");
            } else {
                printf("  Server error (parse %d).\n", (int)perr);
            }
        }
    }

    printf("\n");
    wait_key();
}

/* ── toggle_publish ────────────────────────────────────────── */

void toggle_publish(void)
{
    /* s_val reused for publish JSON after post-list queries */
    int count;
    int sel;
    int new_pub;
    int ch;
    uint8_t perr;
    int16_t pn;

    screen_loading("TOGGLE PUBLISH");
    count = fetch_post_list("");
    if (count < 0) return;

    HOME();
    ui_header("TOGGLE PUBLISH", "Q: Back");
    ui_hline();
    printf("\n");

    sel = pick_from_list(count);
    if (sel < 0) return;

    new_pub = !s_pub[sel];

    HOME();
    ui_header("TOGGLE PUBLISH", "Y: Confirm  N: Cancel");
    ui_hline();
    printf("\n");
    printf("  Post    : %s\n\n", s_titles[sel]);
    printf("  Current : %s\n", s_pub[sel] ? "Published" : "Draft");
    printf("  New     : %s\n\n", new_pub   ? "Published" : "Draft");
    ui_hline();
    printf("  Confirm? (Y/N): ");

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
        printf("\n  Open error: %d\n", (int)perr);
    } else {
        network_write(s_spec, (uint8_t *)s_val, (uint16_t)strlen(s_val));
        printf("\n  Sending...\n");
        perr = network_json_parse(s_spec);
        s_id_result[0] = '\0';
        pn = network_json_query(s_spec, "/id", s_id_result);
        network_close(s_spec);
        if (pn > 0 && s_id_result[0]) {
            printf("  Done!  '%s'  ->  %s\n",
                   s_titles[sel], new_pub ? "Published" : "Draft");
        } else {
            printf("  Server error (parse %d).\n", (int)perr);
        }
    }

    printf("\n");
    wait_key();
}

/* ── delete_post ───────────────────────────────────────────── */

void delete_post(void)
{
    int count;
    int sel;
    int ch;
    uint8_t perr;
    int16_t pn;

    screen_loading("DELETE POST");
    count = fetch_post_list("");
    if (count < 0) return;

    HOME();
    ui_header("DELETE POST", "Q: Back");
    ui_hline();
    printf("\n");

    sel = pick_from_list(count);
    if (sel < 0) return;

    HOME();
    ui_header("DELETE POST", "Y: Delete  N: Cancel");
    ui_hline();
    printf("\n");
    printf("  Post    : %s\n\n", s_titles[sel]);
    printf("  ! PERMANENTLY DELETE !\n");
    printf("    Cannot be undone!\n\n");
    ui_hline();
    printf("  Confirm? (Y/N): ");

#ifdef __CC65__
    ch = toupper(cgetc());
#else
    ch = toupper(getchar());
#endif
    putchar('\n');

    if (ch != 'Y') return;

    /* Apple IIc IWM firmware: OPEN_MODE_HTTP_DELETE is untested; the
       established workaround is OPEN_MODE_HTTP_PUT + network_write().
       The server PUT /api/posts/{id}/delete performs the deletion and
       returns {"id":"<uuid>"} so we can confirm success. */
    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/posts/%s/delete",
             server_url, s_ids[sel]);
    perr = network_open(s_spec, OPEN_MODE_HTTP_PUT, OPEN_TRANS_NONE);
    if (perr) {
        printf("\n  Open error: %d\n", (int)perr);
    } else {
        network_write(s_spec, (const uint8_t *)"{}", 2);
        printf("\n  Deleting...\n");
        perr = network_json_parse(s_spec);
        s_id_result[0] = '\0';
        pn = network_json_query(s_spec, "/id", s_id_result);
        network_close(s_spec);
        if (pn > 0 && s_id_result[0]) {
            printf("  Deleted: %s\n", s_titles[sel]);
        } else {
            printf("  Server error (parse %d).\n", (int)perr);
        }
    }

    printf("\n");
    wait_key();
}

/* ── read_line ─────────────────────────────────────────────── */

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

/* ── read_line_with_default ────────────────────────────────── */

/* Show buf's current content and let the user edit it in-place. */
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

/* ── body_editor ───────────────────────────────────────────── */

#define WP_HDR 2   /* header rows above body-editor area (header + dash-line) */

/* Render s_body in the edit area starting at byte offset `top`.
   `cur` is the cursor offset; drawn in inverse video.
   `first_row` is the first edit-area row to render (rows above are untouched).
   `last_row` limits how far down the repaint goes; pass `er` for a full repaint.
     Rows [last_row, er) are only cleared when last_row == er (full repaint).
     For text-only changes pass a small last_row to avoid clearing blank rows.
   `len` = strlen(s_body), `er` = edit rows, `cols` = screen width.
   Sets cr and cc to the cursor's row/col within the edit area. */
static void we_draw(int top, int cur, int er, int len, int cols,
                    int first_row, int last_row, int *cr, int *cc)
{
#ifdef __CC65__
    int off = top, row = 0, col, de, ns, p;
    int found = 0;
    *cr = 0; *cc = 0;

    /* Advance past first_row rows without rendering, tracking cursor */
    while (row < first_row) {
        ns = row_next(off, len, cols, &de);
        if (!found && cur >= off && cur < ns) {
            *cr = row;
            *cc = (cur < de) ? (cur - off) : (de - off);
            found = 1;
        }
        if (off >= len) { row++; break; }
        off = ns; row++;
    }

    /* Render rows [first_row, last_row) */
    while (row < last_row) {
        gotoxy(0, WP_HDR + row);
        de = off; ns = off;
        if (off < len) ns = row_next(off, len, cols, &de);
        col = 0; p = off;

        /* Print chars up to disp_end, with inverse-video cursor */
        while (p < de) {
            if (p == cur && !found) {
                *cr = row; *cc = col; found = 1;
                revers(1); putchar(s_body[p]); revers(0);
            } else {
                putchar(s_body[p]);
            }
            p++; col++;
        }

        /* Cursor at end-of-row (newline pos, wrap-space, or end-of-text) */
        if (p == cur && !found) {
            *cr = row; *cc = col; found = 1;
            revers(1); putchar(' '); revers(0);
            col++;
        }

        /* Pad to full screen width */
        for (; col < cols; col++) putchar(' ');

        if (off >= len) { row++; break; }
        off = ns; row++;
    }

    if (!found) { *cr = (row < er ? row : er - 1); *cc = 0; }
    /* Only clear trailing blank rows on a full repaint.  Partial repaints
       (text-only changes) leave rows beyond last_row untouched — they cannot
       have changed since a single-char edit affects at most ~3 visual rows. */
    if (last_row >= er) {
        for (; row < er; row++) {
            gotoxy(0, WP_HDR + row);
            for (col = 0; col < cols; col++) putchar(' ');
        }
    }
#else
    (void)top; (void)cur; (void)er; (void)len; (void)cols; (void)first_row;
    (void)last_row;
    *cr = 0; *cc = 0;
#endif
}

/* Scan one visual row of s_body starting at byte offset `off`.
   Word-wrap: break at the last space before the column limit; fall back to a
   hard break at `cols` if no space exists (long URLs, etc.).
   *disp_end: first offset NOT displayed on this row (the wrap-space or '\n'
              or end-of-text position; chars [off .. *disp_end) are printed).
   Returns the offset where the NEXT visual row starts
   (*disp_end+1 for '\n' or word-wrap; *disp_end for hard-wrap/end-of-text). */
static int row_next(int off, int len, int cols, int *disp_end)
{
    int col = 0, last_sp = -1, p = off;
    /* cols - 1: keep one column free so content never occupies the last
       screen column.  On 80-col screens this wraps at 79, on 40-col at 39,
       on any arbitrary width at width-1. */
    while (p < len && s_body[p] != '\n' && col < cols - 1) {
        if (s_body[p] == ' ') last_sp = p;
        p++; col++;
    }
    if (p >= len)          { *disp_end = p; return p; }      /* end of text  */
    if (s_body[p] == '\n') { *disp_end = p; return p + 1; }  /* explicit '\n' */
    /* col == cols - 1: prefer word-wrap, else hard-wrap */
    if (last_sp >= 0)      { *disp_end = last_sp; return last_sp + 1; }
    *disp_end = p; return p;                                  /* hard wrap    */
}

/* Find the absolute (screen row, col) of byte offset `target` in s_body. */
static void wp_get_pos(int target, int len, int cols, int *row, int *col)
{
    int off = 0, r = 0, de, ns;
    while (off < len) {
        ns = row_next(off, len, cols, &de);
        if (target < ns) {
            *row = r;
            *col = (target < de) ? (target - off) : (de - off);
            return;
        }
        off = ns; r++;
    }
    *row = r; *col = 0;
}

/* Find the byte offset of the first char on the screen-row containing `target`. */
static int wp_row_start(int target, int len, int cols)
{
    int off = 0, de, ns;
    while (off < len) {
        ns = row_next(off, len, cols, &de);
        if (target < ns) return off;
        off = ns;
    }
    return off;
}

/* Find the byte offset of the char at absolute screen position (tr, tc). */
static int wp_offset_at(int tr, int tc, int len, int cols)
{
    int off = 0, r = 0, de, ns;
    while (off < len) {
        ns = row_next(off, len, cols, &de);
        if (r == tr) {
            int row_w = de - off;
            return (tc < row_w) ? off + tc : de;
        }
        off = ns; r++;
    }
    return len;
}

/* Render s_body in the view-post content area starting at visual line
   `top_line`.  Each of the `er` rows is positioned with gotoxy() and
   padded to `cols` with spaces so stale content from a prior paint is
   fully erased.  Visual line numbering matches wp_offset_at / wp_get_pos
   — newline-terminated lines AND word-wrapped lines both advance the
   line counter by one.                                                 */
static void vp_draw(int top_line, int er, int len, int cols)
{
#ifdef __CC65__
    int row, col, off, de, ns;
    off = wp_offset_at(top_line, 0, len, cols);
    for (row = 0; row < er; row++) {
        gotoxy(0, VP_HDR + row);
        de = off; ns = off;
        if (off < len) ns = row_next(off, len, cols, &de);
        col = 0;
        while (off < de) { putchar(s_body[off++]); col++; }
        while (col < cols) { putchar(' '); col++; }
        off = ns;
    }
#else
    (void)top_line; (void)er; (void)len; (void)cols;
#endif
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
    int old_cursor, old_cr, old_cc, old_ar;
    int first_row;

    cursor = 0; top_char = 0; cur_row = 0; cur_col = 0; tr = 0;
    ar = 0; ac = 0;   /* cursor visual position — maintained incrementally */
    er   = (screen_width >= 80) ? 20 : 18;
    cols = screen_width;

    HOME();
    ui_header("BODY EDITOR", "Esc: Done");
    for (i = 0; i < cols; i++) putchar('-');
    putchar('\n');

    len = (int)strlen(s_body);
    we_draw(top_char, cursor, er, len, cols, 0, er, &cur_row, &cur_col);

#ifdef __CC65__
    /* Redraw the fixed frame after we_draw: the 80-col firmware's inverse-
       video state from the cursor draw can leave row 0 corrupted.
       Repositioning at row 0 and redrawing is the reliable fix.          */
    revers(0);
    gotoxy(0, 0);
    ui_header("BODY EDITOR", "Esc: Done");
    gotoxy(0, 1);
    for (i = 0; i < cols; i++) putchar('-');

    gotoxy(0, WP_HDR + er);
    printf("Len:%-4d", len);
    gotoxy(cur_col, WP_HDR + cur_row);
#endif

    for (;;) {
        old_cursor = cursor;
        old_cr = cur_row;
        old_cc = cur_col;
        old_ar = ar;

#ifdef __CC65__
        ch = cgetc();
#else
        ch = getchar();
#endif
        text_changed = 0; scroll_changed = 0;

        if (ch == 27) break;                        /* ESC = done */

        /* Key handlers maintain ar/ac incrementally to avoid a full
           wp_get_pos(cursor,...) traversal on every keypress.
           Rules:
             printable/RETURN: ac++ or ar++,ac=0  (O(1), no scan)
             right:            check one byte      (O(1))
             left/DEL ac>0:    ac--                (O(1))
             left/DEL ac==0:   wp_get_pos needed   (moving to prev row end)
             up/down:          wp_get_pos needed   (clamped col on target row)
           tr (row of top_char) is cached; only recomputed on scroll.      */

        if (ch == 0x08) {                           /* left arrow */
            if (cursor > 0) cursor--;
        } else if (ch == 0x15) {                    /* right arrow */
            if (cursor < len) cursor++;
        } else if (ch == 0x0B) {                    /* up arrow */
            if (ar > 0) cursor = wp_offset_at(ar - 1, ac, len, cols);
        } else if (ch == 0x0A) {                    /* down arrow */
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

#ifdef __CC65__
        /* Ultra-fast path: printable char appended at end of text AND the
           cursor is safely away from the row edge (old_cc < cols-2 means
           adding one char cannot trigger word-wrap).  We skip wp_get_pos
           (an O(n) scan from position 0), scroll detection, we_draw, and
           printf — all of which dominate per-keypress cost on a 1 MHz 6502.
           The Len counter is updated on the next non-fast-path event. */
        if (text_changed
                && old_cursor == len - 1
                && old_cc < cols - 2
                && s_body[cursor - 1] != '\n') {
            ar  = old_ar;
            ac  = old_cc + 1;
            cur_row = old_cr;
            cur_col = ac;
            gotoxy(old_cc, WP_HDR + old_cr);
            putchar(s_body[old_cursor]);        /* newly typed char */
            revers(1); putchar(' '); revers(0); /* inverse cursor   */
            gotoxy(cur_col, WP_HDR + cur_row);
            continue;  /* skip wp_get_pos, scroll, repaint entirely */
        }
#endif

        /* General path: recompute position and repaint as needed. */
        wp_get_pos(cursor, len, cols, &ar, &ac);

        /* tr = visual row of top_char — only recomputed on scroll. */
        if (cursor < top_char) {
            top_char = wp_row_start(cursor, len, cols);
            scroll_changed = 1;
            wp_get_pos(top_char, len, cols, &tr, &new_pos);
        } else if (ar - tr >= er) {
            top_char = wp_offset_at(ar - er + 1, 0, len, cols);
            wp_get_pos(top_char, len, cols, &tr, &new_pos);
            scroll_changed = 1;
        }

        if (text_changed || scroll_changed) {
#ifdef __CC65__
            /* Mid-row fast path: single-char insert or backspace that did
               not cause a word-wrap.  Only the tail of the current row
               changed — skip we_draw and printf entirely.
               off_row = start of current row in s_body = old_cursor - old_cc.
               For insert start_col = old_cc; for backspace start_col = ac. */
            if (text_changed && !scroll_changed
                    && ar == old_ar
                    && ch != 0x0D) {
                int off_row  = old_cursor - old_cc;
                int start_col = (cursor < old_cursor) ? ac : old_cc;
                int de_row, p, col;
                row_next(off_row, len, cols, &de_row);
                cur_row = old_cr;
                cur_col = ac;
                gotoxy(start_col, WP_HDR + old_cr);
                p = off_row + start_col; col = start_col;
                while (p < de_row) {
                    if (p == cursor) {
                        revers(1);
                        putchar(s_body[p]);
                        revers(0);
                    } else {
                        putchar(s_body[p]);
                    }
                    p++; col++;
                }
                if (p == cursor) {
                    revers(1); putchar(' '); revers(0);
                    col++;
                }
                for (; col < cols; col++) putchar(' ');
                gotoxy(cur_col, WP_HDR + cur_row);
            } else {
                /* Full/partial repaint for word-wrap, scroll, RETURN. */
                if (scroll_changed) {
                    first_row = 0;
                } else {
                    first_row = old_cr > 0 ? old_cr - 1 : 0;
                }
                {
                    int last_row = scroll_changed ? er
                                 : (old_cr + 3 < er ? old_cr + 3 : er);
                    we_draw(top_char, cursor, er, len, cols, first_row,
                            last_row, &cur_row, &cur_col);
                }
                ar = cur_row + tr;  ac = cur_col;
                revers(0);
                /* On scroll, we_draw rewrites all content rows which can
                   leave the 80-col firmware's inverse state dirty.
                   Redraw the header + dashes to guarantee a clean frame. */
                if (scroll_changed) {
                    gotoxy(0, 0);
                    ui_header("BODY EDITOR", "Esc: Done");
                    gotoxy(0, 1);
                    for (i = 0; i < cols; i++) putchar('-');
                }
                gotoxy(0, WP_HDR + er);
                printf("Len:%-4d", len);
                gotoxy(cur_col, WP_HDR + cur_row);
            }
#else
            first_row = scroll_changed ? 0 : (old_cr > 0 ? old_cr - 1 : 0);
            we_draw(top_char, cursor, er, len, cols, first_row, er,
                    &cur_row, &cur_col);
            ar = cur_row + tr;  ac = cur_col;
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

/* ── show_stats ────────────────────────────────────────────── */

void show_stats(void)
{
    /* BSS strategy — no new static arrays are declared here:
       - s_ids[][]    reused for category names
       - s_pub[]      reused for category counts
       - s_id_result  reused as path scratch (65 bytes; longest path = 20 chars)
       - s_body       first 12 shorts (24 bytes) reused for posts-per-month;
                      safe because show_stats() never runs with body_editor()
       - s_months_str file-scope const string (RODATA, not BSS)             */
    int total_posts, total_cats, avg_bytes, year_val;
    int cat_count, cat_max, ppm_max;
    int i, j, bar_len, name_max, bar_area, nlen;
    int *ppm;     /* points into s_body — no extra BSS */
    uint8_t perr;
    int16_t pn;

    total_posts = total_cats = avg_bytes = year_val = 0;
    cat_count = cat_max = ppm_max = 0;
    ppm = (int *)s_body;   /* borrow first 12 ints (24 bytes) of s_body */

    /* ---- Phase 1: fetch ---- */
    screen_loading("BLOG STATS");

    snprintf(s_spec, sizeof(s_spec), "N1:%s/api/stats", server_url);
    perr = network_open(s_spec, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE);
    if (perr) {
        screen_error("Connection error", (int)perr);
        return;
    }
    perr = network_json_parse(s_spec);
    if (perr) {
        printf("\n  Parse error: %d\n", (int)perr);
        network_close(s_spec);
        wait_key();
        return;
    }

    /* Scalar fields */
    s_val[0] = '\0';
    network_json_query(s_spec, "/total_posts", s_val);
    total_posts = atoi(s_val);

    s_val[0] = '\0';
    network_json_query(s_spec, "/total_categories", s_val);
    total_cats = atoi(s_val);

    s_val[0] = '\0';
    network_json_query(s_spec, "/avg_bytes", s_val);
    avg_bytes = atoi(s_val);

    s_val[0] = '\0';
    network_json_query(s_spec, "/year", s_val);
    year_val = atoi(s_val);

    /* Categories — s_id_result used as path buffer */
    for (i = 0; i < MAX_API_POSTS; i++) {
        snprintf(s_id_result, sizeof(s_id_result), "/categories/%d/name", i);
        s_val[0] = '\0';
        pn = network_json_query(s_spec, s_id_result, s_val);
        if (pn <= 0 || !s_val[0]) break;
        strncpy(s_ids[i], s_val, MAX_API_ID_LEN);
        s_ids[i][MAX_API_ID_LEN] = '\0';

        snprintf(s_id_result, sizeof(s_id_result), "/categories/%d/count", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_id_result, s_val);
        s_pub[i] = atoi(s_val);
        if (s_pub[i] > cat_max) cat_max = s_pub[i];
        cat_count++;
    }

    /* Posts per month */
    for (i = 0; i < 12; i++) {
        snprintf(s_id_result, sizeof(s_id_result), "/posts_per_month/%d", i);
        s_val[0] = '\0';
        network_json_query(s_spec, s_id_result, s_val);
        ppm[i] = atoi(s_val);
        if (ppm[i] > ppm_max) ppm_max = ppm[i];
    }

    network_close(s_spec);

    /* ---- Screen 1: Summary + Categories ---- */
    HOME();
    ui_header("BLOG STATS", "Spc: Chart");
    ui_hline();
    printf("\n");
    printf("  Posts      : %d\n", total_posts);
    printf("  Categories : %d\n", total_cats);
    if (avg_bytes >= 1024)
        printf("  Avg size   : %d KB\n", avg_bytes / 1024);
    else
        printf("  Avg size   : %d bytes\n", avg_bytes);

    if (cat_count > 0) {
        printf("\n");
        ui_hline();
        printf("\n");

        name_max = (screen_width >= 80) ? 20 : 14;
        /* bar area: screen width minus indent(2), name col, '| ', and " NNN\n" (5 chars) */
        bar_area = screen_width - 2 - name_max - 7;
        if (bar_area < 4) bar_area = 4;

        for (i = 0; i < cat_count; i++) {
            nlen = (int)strlen(s_ids[i]);
            if (nlen > name_max) { s_ids[i][name_max] = '\0'; nlen = name_max; }
            printf("  %s", s_ids[i]);
            for (j = nlen; j < name_max; j++) putchar(' ');
            printf("| ");
            bar_len = (cat_max > 0)
                      ? (s_pub[i] * bar_area) / cat_max
                      : 0;
            for (j = 0; j < bar_len; j++) putchar('#');
            printf(" %d\n", s_pub[i]);
        }
    }

    printf("\n");
    ui_hline();
    printf("  Press any key for monthly chart...\n");
    getchar();

    /* ---- Screen 2: Monthly Histogram ---- */
    /* Reuse s_id_result (done with JSON queries) for header title string */
    if (year_val > 0)
        snprintf(s_id_result, sizeof(s_id_result), "POSTS/MONTH %d", year_val);
    else
        snprintf(s_id_result, sizeof(s_id_result), "POSTS/MONTH");

    HOME();
    ui_header(s_id_result, "Any: Back");
    ui_hline();
    printf("\n");

    /* "Mon| " = 5 chars; leave 5 chars for " NNN\n" on right */
    bar_area = screen_width - 10;
    if (bar_area < 4) bar_area = 4;

    for (i = 0; i < 12; i++) {
        /* 3-char month abbreviation from packed string */
        printf("  ");
        putchar(s_months_str[i * 3]);
        putchar(s_months_str[i * 3 + 1]);
        putchar(s_months_str[i * 3 + 2]);
        printf("| ");
        bar_len = (ppm_max > 0)
                  ? (ppm[i] * bar_area) / ppm_max
                  : 0;
        for (j = 0; j < bar_len; j++) putchar('#');
        if (ppm[i] > 0)
            printf(" %d", ppm[i]);
        putchar('\n');
    }

    printf("\n");
    wait_key();
}

/* ── appkey_load / appkey_save ─────────────────────────────── */
/* Uses the FujiNet $DD/$DE SIO App Key commands via the fujinet-lib
   high-level wrappers.  fuji_set_appkey_details() sets the creator and
   app context; fuji_read_appkey/$fuji_write_appkey() issue Open + Read/
   Write, auto-closing the key file after each operation.
   Read buffer must be MAX_APPKEY_LEN + 2 (66 bytes): the $DD response
   prepends a 2-byte actual-length field before the 64-byte data payload. */

static void appkey_load(void)
{
    static uint8_t buf[MAX_APPKEY_LEN + 2]; /* 66: 2-byte length + 64 data */
    uint16_t count = 0;

    fuji_set_appkey_details(APPKEY_CREATOR, APPKEY_APP, DEFAULT);
    buf[0] = '\0';
    if (!fuji_read_appkey(APPKEY_KEY_URL, &count, buf)) return;
    if (count == 0 || buf[0] == '\0') return;
    if (count >= MAX_APPKEY_LEN) count = MAX_APPKEY_LEN - 1;
    buf[count] = '\0';
    strncpy(server_url, (char *)buf, sizeof(server_url) - 1);
    server_url[sizeof(server_url) - 1] = '\0';
}

static void appkey_save(void)
{
    uint16_t len = (uint16_t)strlen(server_url);
    if (len >= MAX_APPKEY_LEN) len = MAX_APPKEY_LEN - 1;
    fuji_set_appkey_details(APPKEY_CREATOR, APPKEY_APP, DEFAULT);
    fuji_write_appkey(APPKEY_KEY_URL, len, (uint8_t *)server_url);
}

/* ── show_config ───────────────────────────────────────────── */

void show_config(void)
{
    int choice;

    while (1) {
        HOME();
        ui_header("CONFIGURATION", "Q: Back");
        ui_hline();
        printf("\n");
        ui_indent(); printf("1.  Server URL\n");
        printf("\n      %s\n\n", server_url);
        ui_indent(); printf("2.  Save URL to App Key\n");
        printf("\n");
        ui_hline();
        printf("  Q: Back   Select: ");

        choice = toupper(getchar());
        if (choice == 'Q') break;

        if (choice == '1') {
            HOME();
            ui_header("SERVER URL", "Esc: Cancel");
            ui_hline();
            printf("\n  Current:\n    %s\n\n", server_url);
            ui_hline();
            printf("  New URL: ");
            s_val[0] = '\0';
            if (read_line(s_val, (int)sizeof(s_val)) && s_val[0] != '\0') {
                strncpy(server_url, s_val, sizeof(server_url) - 1);
                server_url[sizeof(server_url) - 1] = '\0';
                printf("\n  URL updated.\n\n");
                wait_key();
            }
        } else if (choice == '2') {
            appkey_save();
            printf("\n  URL saved to App Key.\n\n");
            wait_key();
        }
    }
}
