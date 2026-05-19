#include <fujinet-fuji.h>
#include <fujinet-network.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_TITLE_LEN 80
#define MAX_CATEGORIES_LEN 100
#define MAX_CONTENT_LEN 2000
#define MAX_RESPONSE_LEN 4096
#define MAX_POSTS 50

#ifndef _CMOC_VERSION_
#define HOME() printf("\033[H\033[J")
#define CLEAR_LINE() printf("\033[K")
#define NORMAL_TEXT() printf("\033[0m")
#define BOLD_TEXT() printf("\033[1m")
#else
#define HOME() clrscr()
#define CLEAR_LINE()
#define NORMAL_TEXT()
#define BOLD_TEXT()
#endif

/* Forward declarations */
void main_menu(void);
void show_splash(void);
void list_posts(void);
void new_post(void);
void edit_post(void);
void toggle_publish(void);
void delete_post(void);
void show_network_status(void);
void get_screen_width(void);

/* Global variables */
int screen_width = 40;  /* Detect 40 or 80 column mode */
char server_url[256] = "http://192.168.1.100:8000";  /* Server URL - user configurable */

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
#ifndef _CMOC_VERSION_
    /* For CC65, check if 80-column card is present */
    /* This is a simplified check - actual implementation depends on compiler */
    screen_width = 80;  /* Default to 80 for now */
#else
    screen_width = 40;  /* CMOC defaults to 40 columns */
#endif
}

void show_splash(void)
{
    HOME();
    BOLD_TEXT();
    printf("========================================\n");
    printf("    FujiNet Apple IIc Blog Client\n");
    printf("========================================\n");
    NORMAL_TEXT();
    printf("\nDetected screen width: %d columns\n", screen_width);
    printf("\nInitializing FujiNet...\n");
    
    AdapterConfigExtended ace;
    if (!fuji_get_adapter_config_extended(&ace)) {
        printf("ERROR: FujiNet not detected!\n");
        printf("Press any key to exit.\n");
        getchar();
        exit(1);
    }
    
    printf("FujiNet Version: %s\n", ace.fn_version);
    printf("Signal Strength: %d%%\n", ace.signal_strength);
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
            /* TODO: Configuration menu */
            HOME();
            printf("Configuration menu not yet implemented.\n");
            printf("Press any key to continue...\n");
            getchar();
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
    HOME();
    BOLD_TEXT();
    printf("NETWORK STATUS\n");
    NORMAL_TEXT();
    printf("================\n\n");
    
    AdapterConfigExtended ace;
    if (fuji_get_adapter_config_extended(&ace)) {
        printf("FujiNet: Connected\n");
        printf("Version: %s\n", ace.fn_version);
        printf("Signal: %d%%\n", ace.signal_strength);
        printf("MAC: ");
        for (int i = 0; i < 6; i++) {
            printf("%02X", ace.mac[i]);
            if (i < 5) printf(":");
        }
        printf("\n");
    } else {
        printf("FujiNet: NOT DETECTED\n");
    }
    
    printf("\nServer: %s\n", server_url);
    printf("\nScreen Width: %d columns\n", screen_width);
    printf("\nPress any key to return...\n");
    getchar();
}

void list_posts(void)
{
    HOME();
    BOLD_TEXT();
    printf("POSTS\n");
    NORMAL_TEXT();
    printf("=====\n\n");
    
    /* TODO: Implement network call to GET /api/posts */
    printf("Loading posts from server...\n");
    printf("(Not yet implemented)\n");
    printf("\nPress any key to return...\n");
    getchar();
}

void new_post(void)
{
    HOME();
    BOLD_TEXT();
    printf("NEW POST\n");
    NORMAL_TEXT();
    printf("========\n\n");
    
    char title[MAX_TITLE_LEN];
    char categories[MAX_CATEGORIES_LEN];
    char content[MAX_CONTENT_LEN];
    
    printf("Title: ");
    fgets(title, sizeof(title), stdin);
    
    printf("\nCategories (comma-separated): ");
    fgets(categories, sizeof(categories), stdin);
    
    printf("\nContent (Press Ctrl+D or Ctrl+Z when done):\n");
    printf("----------------------------------------\n");
    
    int i = 0;
    int ch;
    while ((ch = getchar()) != EOF && i < MAX_CONTENT_LEN - 1) {
        content[i++] = ch;
    }
    content[i] = '\0';
    
    printf("----------------------------------------\n");
    printf("\nPost preview:\n");
    printf("Title: %s", title);
    printf("Categories: %s", categories);
    printf("\nSave? (Y/N): ");
    
    if (toupper(getchar()) == 'Y') {
        printf("Saving...\n");
        /* TODO: Implement network call to POST /api/posts */
        printf("(Not yet implemented)\n");
    }
    
    printf("\nPress any key to return...\n");
    getchar();
}

void edit_post(void)
{
    HOME();
    BOLD_TEXT();
    printf("EDIT POST\n");
    NORMAL_TEXT();
    printf("=========\n\n");
    
    /* TODO: Implement post selection and editing */
    printf("This feature is not yet implemented.\n");
    printf("Press any key to return...\n");
    getchar();
}

void toggle_publish(void)
{
    HOME();
    BOLD_TEXT();
    printf("TOGGLE PUBLISH\n");
    NORMAL_TEXT();
    printf("==============\n\n");
    
    /* TODO: Implement publish toggle */
    printf("This feature is not yet implemented.\n");
    printf("Press any key to return...\n");
    getchar();
}

void delete_post(void)
{
    HOME();
    BOLD_TEXT();
    printf("DELETE POST\n");
    NORMAL_TEXT();
    printf("===========\n\n");
    
    /* TODO: Implement post deletion */
    printf("This feature is not yet implemented.\n");
    printf("Press any key to return...\n");
    getchar();
}
