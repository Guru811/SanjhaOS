#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_Q    1024
#define MAX_URL  2048
#define TMP_OUT  "/tmp/ai_out.txt"
#define TMP_CMD  "/tmp/ai_cmd.sh"
#define API_KEY  "AIzaSyA_Xatl8iymhkmaI0Ut1_IHj6FifxSS0Go"
#define API_URL  "https://generativelanguage.googleapis.com/v1beta/models/gemini-flash-latest:generateContent"

void header() {
    printf("\n  =========================================\n");
    printf("       SANJHA OS - Smart Ask Command      \n");
    printf("       AI + Google Search + Chrome        \n");
    printf("  =========================================\n\n");
}

// URL encode for Google search
void url_encode(char *src, char *dst, int max_len) {
    int j = 0;
    for (int i = 0; src[i] && j < max_len - 1; i++) {
        if ((src[i] >= 'a' && src[i] <= 'z') ||
            (src[i] >= 'A' && src[i] <= 'Z') ||
            (src[i] >= '0' && src[i] <= '9') ||
            src[i] == '-' || src[i] == '_' ||
            src[i] == '.' || src[i] == '~') {
            dst[j++] = src[i];
        } else if (src[i] == ' ') {
            dst[j++] = '+';
        } else {
            if (j + 3 < max_len - 1) {
                j += sprintf(dst + j, "%%%02X", (unsigned char)src[i]);
            }
        }
    }
    dst[j] = '\0';
}

// Detect query type
int detect_query_type(char *query) {
    // AI queries: "what is", "how to", "explain", "define", "who is"
    // Search queries: everything else with spaces
    
    char lower[MAX_Q] = {0};
    for (int i = 0; query[i]; i++) {
        lower[i] = tolower(query[i]);
    }
    
    // AI indicator keywords
    if (strstr(lower, "what is") || 
        strstr(lower, "how to") || 
        strstr(lower, "explain ") ||
        strstr(lower, "define ") ||
        strstr(lower, "who is ") ||
        strstr(lower, "describe ") ||
        strstr(lower, "tell me ") ||
        strstr(lower, "write ") ||
        strstr(lower, "create ") ||
        strstr(lower, "help with")) {
        return 1; // AI mode
    }
    
    // Search indicator keywords
    if (strstr(lower, "search ") ||
        strstr(lower, "find ") ||
        strstr(lower, "look for ") ||
        strstr(lower, "browse ")) {
        return 2; // Chrome search mode
    }
    
    // Default: if has spaces, ask user
    if (strchr(query, ' ')) {
        return 0; // Ask user which mode
    }
    
    return 0; // Ask user
}

// ===== AI MODE (Gemini) =====
void ask_gemini_ai(char *question) {
    char esc[MAX_Q * 2] = {0};
    int j = 0;
    for (int i = 0; question[i] && j < (int)sizeof(esc)-2; i++) {
        if      (question[i] == '"')  { esc[j++] = '\\'; esc[j++] = '"'; }
        else if (question[i] == '\\') { esc[j++] = '\\'; esc[j++] = '\\'; }
        else if (question[i] == '\n') { esc[j++] = '\\'; esc[j++] = 'n'; }
        else esc[j++] = question[i];
    }

    FILE *f = fopen(TMP_CMD, "w");
    if (!f) { printf("  Error: cannot create temp file\n"); return; }

    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "curl -s -X POST \"%s\" \\\n", API_URL);
    fprintf(f, "  -H \"Content-Type: application/json\" \\\n");
    fprintf(f, "  -H \"X-goog-api-key: %s\" \\\n", API_KEY);
    fprintf(f, "  -d \"{\\\"contents\\\":[{\\\"parts\\\":[{\\\"text\\\":\\\"%s\\\"}]}],"
               "\\\"generationConfig\\\":{\\\"maxOutputTokens\\\":300}}\" > %s 2>/dev/null\n",
               esc, TMP_OUT);
    fclose(f);
    chmod(TMP_CMD, 0755);

    printf("  [AI MODE] Querying Gemini...\n\n");
    fflush(stdout);
    system(TMP_CMD);

    FILE *r = fopen(TMP_OUT, "r");
    if (!r) { printf("  Error: no response\n\n"); return; }
    char resp[8192] = {0};
    fread(resp, 1, sizeof(resp)-1, r);
    fclose(r);

    // Check for error
    if (strstr(resp, "\"error\"")) {
        char *err = strstr(resp, "\"message\":\"");
        if (err) {
            err += 11;
            char *end = strchr(err, '"');
            if (end) *end = 0;
            if (strlen(err) > 100) err[100] = 0;
            printf("  API Error: %s\n\n", err);
        } else {
            printf("  Error: API request failed\n\n");
        }
        remove(TMP_OUT); remove(TMP_CMD);
        return;
    }

    // Find parts section
    char *parts = strstr(resp, "\"parts\"");
    if (!parts) {
        printf("  Error: no parts in response\n\n");
        remove(TMP_OUT); remove(TMP_CMD);
        return;
    }

    // Find actual answer text
    char *ts = parts;
    char *found = NULL;
    while (1) {
        ts = strstr(ts, "\"text\":\"");
        if (!ts) break;
        ts += 8;

        int is_real = 0;
        for (int x = 0; x < 60 && ts[x] && ts[x] != '"'; x++) {
            if (ts[x] == ' ' || ts[x] == '\n' ||
                ts[x] == '\\' || ts[x] == ',' ||
                ts[x] == '.' || ts[x] == '-') {
                is_real = 1;
                break;
            }
        }
        if (is_real) {
            found = ts;
            break;
        }
    }

    if (!found) {
        printf("  Error: no text in response\n\n");
        remove(TMP_OUT); remove(TMP_CMD);
        return;
    }

    // Unescape
    char out[4096] = {0};
    int k = 0;
    for (int i = 0; found[i] && k < (int)sizeof(out)-1; i++) {
        if (found[i] == '\\' && found[i+1]) {
            i++;
            if      (found[i] == 'n')  out[k++] = '\n';
            else if (found[i] == 't')  out[k++] = '\t';
            else if (found[i] == '"')  out[k++] = '"';
            else if (found[i] == '\\') out[k++] = '\\';
            else                       out[k++] = found[i];
        } else if (found[i] == '"') {
            break;
        } else if (found[i] == '*' || found[i] == '#') {
            continue; // skip markdown
        } else {
            out[k++] = found[i];
        }
    }

    char *start = out;
    while (*start == '\n' || *start == ' ') start++;

    printf("  Gemini AI says:\n\n  ");
    for (int i = 0; start[i]; i++) {
        putchar(start[i]);
        if (start[i] == '\n' && start[i+1]) printf("  ");
    }
    printf("\n\n");

    remove(TMP_OUT);
    remove(TMP_CMD);
}

// ===== CHROME SEARCH MODE =====
void open_chrome_search(char *query) {
    char encoded[MAX_URL] = {0};
    char url[MAX_URL] = {0};
    
    url_encode(query, encoded, sizeof(encoded));
    snprintf(url, sizeof(url),
             "https://www.google.com/search?q=%s",
             encoded);
    
    printf("  [CHROME MODE] Opening Google search in Chrome...\n");
    printf("  Query: %s\n\n", query);
    fflush(stdout);
    
    // Kill existing
    system("pkill -f Xvfb 2>/dev/null");
    system("pkill -f chrome 2>/dev/null");
    sleep(1);
    
    // Write launch script
    FILE *f = fopen(TMP_CMD, "w");
    if (!f) {
        printf("  Error: cannot create temp file\n");
        return;
    }
    
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "export DISPLAY=:0\n");
    fprintf(f, "Xvfb :0 -screen 0 1024x768x24 &\n");
    fprintf(f, "XPID=$!\n");
    fprintf(f, "sleep 2\n");
    fprintf(f, "fluxbox &\n");
    fprintf(f, "sleep 1\n");
    fprintf(f, "chromium-browser \"%s\" &\n", url);
    fprintf(f, "CHROME_PID=$!\n");
    fprintf(f, "wait $CHROME_PID\n");
    fprintf(f, "kill $XPID 2>/dev/null\n");
    fprintf(f, "pkill fluxbox 2>/dev/null\n");
    
    fclose(f);
    chmod(TMP_CMD, 0755);
    
    system(TMP_CMD);
    
    printf("\n  Chrome session closed.\n\n");
    remove(TMP_CMD);
}

// ===== INTERACTIVE MODE =====
void interactive_mode() {
    printf("  Available modes:\n");
    printf("  1. 'ai: <question>'  → Use Gemini AI\n");
    printf("  2. 'search: <query>' → Use Chrome + Google\n");
    printf("  3. Auto-detect (enter query, we'll choose)\n\n");
    printf("  Type 'exit' to quit:\n\n");
    
    char input[MAX_Q];
    while (1) {
        printf("  >> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;
        
        if (!strlen(input)) continue;
        if (!strcmp(input, "exit") || !strcmp(input, "quit")) break;
        
        // Check for explicit mode prefix
        if (strncmp(input, "ai:", 3) == 0) {
            ask_gemini_ai(input + 3);
        } else if (strncmp(input, "search:", 7) == 0) {
            open_chrome_search(input + 7);
        } else {
            // Auto-detect
            int mode = detect_query_type(input);
            if (mode == 1) {
                ask_gemini_ai(input);
            } else if (mode == 2) {
                open_chrome_search(input);
            } else {
                // Ask user
                printf("\n  Which mode?\n");
                printf("  1) AI Mode (Gemini)\n");
                printf("  2) Chrome Search\n");
                printf("  Enter choice (1-2): ");
                fflush(stdout);
                
                char choice[10];
                fgets(choice, sizeof(choice), stdin);
                
                if (choice[0] == '1') {
                    ask_gemini_ai(input);
                } else if (choice[0] == '2') {
                    open_chrome_search(input);
                } else {
                    printf("  Invalid choice.\n\n");
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    header();
    
    // Command line mode
    if (argc > 1) {
        char query[MAX_Q] = {0};
        
        // Check for explicit mode
        if (strcmp(argv[1], "ai") == 0 && argc > 2) {
            // ask ai what is linux
            for (int i = 2; i < argc; i++) {
                if (i > 2) strcat(query, " ");
                strncat(query, argv[i], MAX_Q - strlen(query) - 1);
            }
            ask_gemini_ai(query);
            return 0;
        } else if (strcmp(argv[1], "search") == 0 && argc > 2) {
            // ask search best linux distros
            for (int i = 2; i < argc; i++) {
                if (i > 2) strcat(query, " ");
                strncat(query, argv[i], MAX_Q - strlen(query) - 1);
            }
            open_chrome_search(query);
            return 0;
        } else {
            // ask what is linux (auto-detect)
            for (int i = 1; i < argc; i++) {
                if (i > 1) strcat(query, " ");
                strncat(query, argv[i], MAX_Q - strlen(query) - 1);
            }
            
            int mode = detect_query_type(query);
            if (mode == 1) {
                ask_gemini_ai(query);
            } else if (mode == 2) {
                open_chrome_search(query);
            } else {
                // Default to AI for full answers
                ask_gemini_ai(query);
            }
            return 0;
        }
    }
    
    // Interactive mode
    interactive_mode();
    
    printf("\n  Goodbye!\n\n");
    return 0;
}
