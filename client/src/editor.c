/* Text editor for composing blog posts */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define EDITOR_MAX_LINES 100
#define EDITOR_MAX_LINE_LEN 80

typedef struct {
    char lines[EDITOR_MAX_LINES][EDITOR_MAX_LINE_LEN];
    int line_count;
    int cursor_line;
    int cursor_col;
} TextEditor;

void editor_init(TextEditor *editor)
{
    editor->line_count = 0;
    editor->cursor_line = 0;
    editor->cursor_col = 0;
    memset(editor->lines, 0, sizeof(editor->lines));
}

void editor_display(TextEditor *editor, int screen_width)
{
    /* Display editor content with wrapping for screen_width */
    for (int i = 0; i < editor->line_count; i++) {
        printf("%d: %s\n", i + 1, editor->lines[i]);
    }
}

void editor_add_line(TextEditor *editor, const char *line)
{
    /* Add a line to the editor */
    if (editor->line_count < EDITOR_MAX_LINES) {
        strncpy(editor->lines[editor->line_count], line, EDITOR_MAX_LINE_LEN - 1);
        editor->line_count++;
    }
}

void editor_delete_line(TextEditor *editor, int line_num)
{
    /* Delete a line from the editor */
    if (line_num >= 0 && line_num < editor->line_count) {
        for (int i = line_num; i < editor->line_count - 1; i++) {
            strcpy(editor->lines[i], editor->lines[i + 1]);
        }
        editor->line_count--;
    }
}

int editor_get_content(TextEditor *editor, char *output, int output_len)
{
    /* Concatenate all lines into a single string */
    int offset = 0;
    for (int i = 0; i < editor->line_count && offset < output_len; i++) {
        int line_len = strlen(editor->lines[i]);
        if (offset + line_len + 1 < output_len) {
            strcpy(output + offset, editor->lines[i]);
            offset += line_len;
            output[offset++] = '\n';
        }
    }
    output[offset] = '\0';
    return offset;
}
