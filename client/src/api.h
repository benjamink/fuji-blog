#ifndef API_H
#define API_H

#define MAX_API_ID_LEN           64
#define MAX_API_TITLE_LEN        64
#define MAX_API_SLUG_LEN         128
#define MAX_API_MARKDOWN_BODY_LEN 1280
#define MAX_API_CATEGORY_LEN     64
#define MAX_API_TIMESTAMP_LEN    32
#define MAX_API_POSTS            10

int build_update_json(const char *title, const char *category,
                      const char *body, char *json, int json_len);

#endif
