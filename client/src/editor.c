/* Line-oriented body editor — operates on a flat NUL-terminated string
   where lines are separated by '\n'. No static storage; all state is in
   the caller's buffer (s_body in main.c). */

#include <string.h>

/* Return pointer to the start of line n (0-based). If n >= line count,
   returns pointer to the NUL terminator. */
static char *line_start(char *body, int n)
{
    int cur = 0;
    while (*body && cur < n) {
        if (*body++ == '\n') cur++;
    }
    return body;
}

/* Count displayable lines (trailing newline is NOT an extra blank line). */
int editor_count_lines(const char *body)
{
    const char *last;
    int n;
    if (!body || !*body) return 0;
    n = 1;
    last = body;
    while (*body) {
        if (*body++ == '\n') { n++; last = body; }
    }
    if (!*last) n--;   /* trailing newline: "foo\n" = 1 line, not 2 */
    return n;
}

/* Copy line n into buf (without the newline). Returns chars copied or -1. */
int editor_get_line(const char *body, int n, char *buf, int buf_len)
{
    int cur = 0;
    int len = 0;
    if (!body || !buf || buf_len <= 0) return -1;
    while (*body && cur < n) {
        if (*body++ == '\n') cur++;
    }
    if (cur < n && !*body) { buf[0] = '\0'; return -1; }
    while (*body && *body != '\n' && len < buf_len - 1)
        buf[len++] = *body++;
    buf[len] = '\0';
    return len;
}

/* Replace line n with new_text. Returns new body length or -1 on overflow. */
int editor_replace_line(char *body, int body_max, int n, const char *new_text)
{
    char *start, *eol;
    int new_len, tail_len, total;

    start = line_start(body, n);
    eol = start;
    while (*eol && *eol != '\n') eol++;

    new_len  = (int)strlen(new_text);
    tail_len = (int)strlen(eol);          /* includes '\n' if present */
    total    = (int)(start - body) + new_len + tail_len + 1;

    if (total > body_max) return -1;

    memmove(start + new_len, eol, tail_len + 1);
    memcpy(start, new_text, new_len);
    return (int)strlen(body);
}

/* Delete line n (including its newline). Returns new body length or -1. */
int editor_delete_line(char *body, int n)
{
    char *start, *eol;

    start = line_start(body, n);
    if (!*start) return -1;

    eol = start;
    while (*eol && *eol != '\n') eol++;
    if (*eol == '\n') eol++;

    memmove(start, eol, strlen(eol) + 1);
    return (int)strlen(body);
}

/* Insert text as a new line BEFORE line n (pass n == count to append).
   Returns new body length or -1 on overflow. */
int editor_insert_line(char *body, int body_max, int n, const char *text)
{
    char *pos;
    int new_len, rest_len, total;

    pos = line_start(body, n);
    new_len  = (int)strlen(text);
    rest_len = (int)strlen(pos);
    total    = (int)(pos - body) + new_len + 1 + rest_len + 1;

    if (total > body_max) return -1;

    memmove(pos + new_len + 1, pos, rest_len + 1);
    memcpy(pos, text, new_len);
    pos[new_len] = '\n';
    return (int)strlen(body);
}
