/* UI utility functions */

#include <stdio.h>
#include <string.h>

void wrap_text(const char *text, int width)
{
    /* Display text wrapped to fit within width columns */
    const char *pos = text;
    int line_pos = 0;
    
    while (*pos) {
        if (*pos == '\n') {
            printf("\n");
            line_pos = 0;
            pos++;
        } else if (line_pos >= width) {
            printf("\n");
            line_pos = 0;
        } else {
            printf("%c", *pos);
            line_pos++;
            pos++;
        }
    }
    printf("\n");
}

void center_text(const char *text, int width)
{
    /* Print text centered within width columns */
    int text_len = strlen(text);
    int left_pad = (width - text_len) / 2;
    
    for (int i = 0; i < left_pad; i++) {
        printf(" ");
    }
    printf("%s\n", text);
}

void draw_line(int width, char ch)
{
    /* Draw a horizontal line */
    for (int i = 0; i < width; i++) {
        printf("%c", ch);
    }
    printf("\n");
}

int get_input(char *buffer, int max_len, int *width)
{
    /* Get user input with optional width specification */
    return fgets(buffer, max_len, stdin) != NULL ? 1 : 0;
}
