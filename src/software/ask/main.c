#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_QUESTION 1024
// Using current directory instead of /tmp to avoid permission/mount issues
#define TMP_OUT      "ai_res.txt"
#define TMP_CMD      "ai_cmd.sh"
#define TMP_JSON     "ai_req.json"

// !!! PASTE YOUR KEY FROM https://aistudio.google.com/apikey HERE !!!
#define API_KEY      "AIzaSyBdJsO9qV5JhFensPl-YlJGsPRYwD31aBc" 

void ask_ai(char *question) {
    char escaped[MAX_QUESTION * 2] = {0};
    int j = 0;
    for (int i = 0; question[i] && j < (int)sizeof(escaped) - 2; i++) {
        if (question[i] == '"')       { escaped[j++] = '\\'; escaped[j++] = '"';  }
        else if (question[i] == '\\') { escaped[j++] = '\\'; escaped[j++] = '\\'; }
        else escaped[j++] = question[i];
    }

    FILE *jf = fopen(TMP_JSON, "w");
    if (!jf) { printf("  Error: Cannot write JSON. Disk might be Read-Only.\n"); return; }
    fprintf(jf, "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}", escaped);
    fclose(jf);

    FILE *f = fopen(TMP_CMD, "w");
    if (!f) { printf("  Error: Cannot write script file.\n"); return; }
    fprintf(f, "#!/bin/sh\n"
               "curl -s -k -X POST \"https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=AIzaSyBdJsO9qV5JhFensPl-YlJGsPRYwD31aBc" "
               "-H \"Content-Type: application/json\" -d @%s > %s 2>&1\n", 
               API_KEY, TMP_JSON, TMP_OUT);
    fclose(f);
    
    chmod(TMP_CMD, 0755);
    printf("  Thinking... (Connecting to Gemini)\n");
    fflush(stdout);

    // Running via 'sh' explicitly
    system("sh ai_cmd.sh");

    FILE *r = fopen(TMP_OUT, "r");
    if (!r) { 
        printf("  Error: No network response received.\n"); 
        return; 
    }
    
    char response[8192] = {0};
    fread(response, 1, sizeof(response)-1, r);
    fclose(r);

    if (strlen(response) == 0) {
        printf("  Error: Response file is empty. Check your internet.\n");
        return;
    }

    // Improved Parsing
    char *text_pos = strstr(response, "\"text\": \"");
    if (text_pos) {
        text_pos += 9;
        printf("\n  Sanjha AI: ");
        while (*text_pos && *text_pos != '"') {
            if (*text_pos == '\\' && *(text_pos+1) == 'n') { 
                printf("\n  "); 
                text_pos += 2; 
            } else { 
                putchar(*text_pos); 
                text_pos++; 
            }
        }
        printf("\n\n");
    } else {
        printf("  Error: API Error or Parse Failure. Raw data:\n%s\n", response);
    }

    // Clean up
    remove(TMP_CMD); remove(TMP_JSON); remove(TMP_OUT);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("  Usage: ask <your question>\n");
        return 1;
    }
    char q[MAX_QUESTION] = {0};
    for(int i=1; i<argc; i++) {
        strncat(q, argv[i], MAX_QUESTION - strlen(q) - 1);
        strncat(q, " ", MAX_QUESTION - strlen(q) - 1);
    }
    ask_ai(q);
    return 0;
}
