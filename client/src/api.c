/* JSON building for API requests */

#include <string.h>
#include <stdio.h>
#include "api.h"

/* Build JSON for updating a post: title, category, and escaped markdown body */
int build_update_json(const char *title, const char *category,
                      const char *body, char *json, int json_len)
{
    int pos = 0;
    int i;

    if (!json) return -1;

    pos += snprintf(json + pos, json_len - pos,
                    "{\"title\":\"%s\",\"category\":\"%s\",\"markdown_body\":\"",
                    title ? title : "", category ? category : "");

    if (body) {
        for (i = 0; body[i] && pos < json_len - 4; i++) {
            if (body[i] == '\n')
                pos += snprintf(json + pos, json_len - pos, "\\n");
            else if (body[i] == '"')
                pos += snprintf(json + pos, json_len - pos, "\\\"");
            else if (body[i] == '\\')
                pos += snprintf(json + pos, json_len - pos, "\\\\");
            else
                json[pos++] = body[i];
        }
    }
    pos += snprintf(json + pos, json_len - pos, "\"}");

    return pos;
}
