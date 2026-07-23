#ifndef API_H
#define API_H

/* Post IDs are UUIDs (36 chars).  40 leaves a null + small margin while
   reclaiming BSS vs the old 64 (10 list slots + s_id_result share this). */
#define MAX_API_ID_LEN           40
#define MAX_API_TITLE_LEN        64
#define MAX_API_SLUG_LEN         128
/* Body is streamed to the server in small chunks (see send_body_chunks in
   main.c), so this limit is now bounded by client RAM, not the FujiNet write
   buffer.  2560 keeps the BSS segment safely under __HIMEM__ = $B000 (the
   proven-safe ceiling — raising __HIMEM__ further freezes the IIc). */
#define MAX_API_MARKDOWN_BODY_LEN 2000
#define MAX_API_CATEGORY_LEN     64
#define MAX_API_TIMESTAMP_LEN    32
#define MAX_API_POSTS            10

int build_update_json(const char *title, const char *category,
                      const char *body, char *json, int json_len);

#endif
