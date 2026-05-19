/* JSON parsing and API communication layer */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Simple JSON object to hold blog post data */
typedef struct {
    char id[64];
    char title[256];
    char slug[256];
    char markdown_body[4096];
    char categories[256];
    int published;
    char created_at[32];
    char updated_at[32];
} BlogPost;

typedef struct {
    BlogPost posts[50];
    int count;
} PostList;

/* Helper to extract quoted string value from JSON */
static char* json_extract_string(const char *json, const char *key, char *output, int max_len)
{
    const char *start = strstr(json, key);
    if (!start) return NULL;
    
    start = strchr(start, ':');
    if (!start) return NULL;
    
    start = strchr(start, '"');
    if (!start) return NULL;
    start++;
    
    const char *end = strchr(start, '"');
    if (!end) return NULL;
    
    int len = end - start;
    if (len >= max_len) len = max_len - 1;
    
    strncpy(output, start, len);
    output[len] = '\0';
    
    return output;
}

/* Helper to extract boolean value from JSON */
static int json_extract_bool(const char *json, const char *key)
{
    const char *start = strstr(json, key);
    if (!start) return 0;
    
    start = strchr(start, ':');
    if (!start) return 0;
    
    while (*start && (*start == ' ' || *start == ':')) start++;
    
    return (strncmp(start, "true", 4) == 0);
}

/* Parse categories array from JSON */
static void json_extract_categories(const char *json, char *output, int max_len)
{
    output[0] = '\0';
    
    const char *start = strstr(json, "\"categories\"");
    if (!start) return;
    
    start = strchr(start, '[');
    if (!start) return;
    start++;
    
    const char *end = strchr(start, ']');
    if (!end) return;
    
    /* Simple extraction: find quoted strings between [ and ] */
    int pos = 0;
    int in_quote = 0;
    int cat_count = 0;
    
    while (start < end && pos < max_len - 1) {
        if (*start == '"') {
            if (!in_quote) {
                in_quote = 1;
                start++;
                if (cat_count > 0) {
                    output[pos++] = ',';
                    output[pos++] = ' ';
                }
                cat_count++;
            } else {
                in_quote = 0;
            }
        } else if (in_quote) {
            output[pos++] = *start;
        }
        start++;
    }
    
    output[pos] = '\0';
}

/* Parse a single post JSON object */
int parse_post_response(const char *json, BlogPost *post)
{
    /* TODO: Implement JSON parser for single post */
    if (!json || !post) return -1;
    
    memset(post, 0, sizeof(BlogPost));
    
    json_extract_string(json, "\"id\"", post->id, sizeof(post->id));
    json_extract_string(json, "\"title\"", post->title, sizeof(post->title));
    json_extract_string(json, "\"slug\"", post->slug, sizeof(post->slug));
    json_extract_string(json, "\"markdown_body\"", post->markdown_body, sizeof(post->markdown_body));
    json_extract_categories(json, post->categories, sizeof(post->categories));
    post->published = json_extract_bool(json, "\"published\"");
    json_extract_string(json, "\"created_at\"", post->created_at, sizeof(post->created_at));
    json_extract_string(json, "\"updated_at\"", post->updated_at, sizeof(post->updated_at));
    
    return (post->id[0] != '\0') ? 0 : -1;
}

/* Parse a JSON array of posts */
int parse_posts_response(const char *json, PostList *posts)
{
    /* TODO: Implement simple JSON parser for posts array */
    if (!json || !posts) return -1;
    
    posts->count = 0;
    
    const char *start = json;
    int in_object = 0;
    int brace_count = 0;
    char obj_buffer[5000];
    int obj_pos = 0;
    
    while (*start && posts->count < 50) {
        if (*start == '{') {
            if (!in_object) {
                in_object = 1;
                obj_pos = 0;
                brace_count = 1;
            } else {
                brace_count++;
            }
            obj_buffer[obj_pos++] = *start;
        } else if (*start == '}') {
            obj_buffer[obj_pos++] = *start;
            brace_count--;
            if (brace_count == 0 && in_object) {
                obj_buffer[obj_pos] = '\0';
                if (parse_post_response(obj_buffer, &posts->posts[posts->count]) == 0) {
                    posts->count++;
                }
                in_object = 0;
            }
        } else if (in_object && obj_pos < sizeof(obj_buffer) - 1) {
            obj_buffer[obj_pos++] = *start;
        }
        start++;
    }
    
    return posts->count;
}

/* Build JSON for creating/updating a post */
int build_post_json(const BlogPost *post, char *json, int json_len)
{
    /* TODO: Implement JSON builder for POST/PUT requests */
    if (!post || !json) return -1;
    
    /* Simple JSON builder - escape special chars in strings */
    int pos = 0;
    
    pos += snprintf(json + pos, json_len - pos, "{");
    
    if (post->title[0]) {
        pos += snprintf(json + pos, json_len - pos, "\"title\":\"%s\",", post->title);
    }
    
    if (post->markdown_body[0]) {
        /* Escape newlines and quotes in markdown body */
        pos += snprintf(json + pos, json_len - pos, "\"markdown_body\":\"");
        for (int i = 0; post->markdown_body[i] && pos < json_len - 10; i++) {
            if (post->markdown_body[i] == '\n') {
                pos += snprintf(json + pos, json_len - pos, "\\n");
            } else if (post->markdown_body[i] == '"') {
                pos += snprintf(json + pos, json_len - pos, "\\\"");
            } else if (post->markdown_body[i] == '\\') {
                pos += snprintf(json + pos, json_len - pos, "\\\\");
            } else {
                json[pos++] = post->markdown_body[i];
            }
        }
        pos += snprintf(json + pos, json_len - pos, "\",");
    }
    
    if (post->categories[0]) {
        pos += snprintf(json + pos, json_len - pos, "\"categories\":[");
        char *cat = strdup(post->categories);
        char *tok = strtok(cat, ",");
        int first = 1;
        while (tok) {
            /* Trim whitespace */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && *end == ' ') end--;
            *(end + 1) = '\0';
            
            if (!first) pos += snprintf(json + pos, json_len - pos, ",");
            pos += snprintf(json + pos, json_len - pos, "\"%s\"", tok);
            first = 0;
            tok = strtok(NULL, ",");
        }
        free(cat);
        pos += snprintf(json + pos, json_len - pos, "],");
    }
    
    pos += snprintf(json + pos, json_len - pos, "\"published\":%s", post->published ? "true" : "false");
    pos += snprintf(json + pos, json_len - pos, "}");
    
    return pos;
}
