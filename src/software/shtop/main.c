#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_PROCS 64
#define CLEAR     "\033[H\033[J"

typedef struct {
    int  pid;
    char name[64];
    char state;
    int  ppid;
} ProcInfo;

int read_file(char *path, char *buf, int maxlen) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, maxlen - 1);
    close(fd);
    if (n < 0) return -1;
    buf[n] = '\0';
    return n;
}

int parse_stat(int pid, ProcInfo *info) {
    char path[64], buf[512];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (read_file(path, buf, sizeof(buf)) < 0)
        return -1;
    char name[64] = {0};
    char state = '?';
    int  ppid = 0;
    sscanf(buf, "%d (%63[^)]) %c %d", &info->pid, name, &state, &ppid);
    strncpy(info->name, name, 63);
    info->state = state;
    info->ppid = ppid;
    return 0;
}

void read_uptime(long *secs) {
    char buf[64];
    read_file("/proc/uptime", buf, sizeof(buf));
    sscanf(buf, "%ld", secs);
}

void read_meminfo(long *total, long *free_mem, long *available) {
    char buf[512];
    read_file("/proc/meminfo", buf, sizeof(buf));
    sscanf(buf, "MemTotal: %ld kB MemFree: %ld kB MemAvailable: %ld kB",
           total, free_mem, available);
}

void draw_top(ProcInfo *procs, int count) {
    long uptime = 0, mem_total = 0, mem_free = 0, mem_avail = 0;
    read_uptime(&uptime);
    read_meminfo(&mem_total, &mem_free, &mem_avail);
    long mem_used = mem_total - mem_avail;

    long hours   = uptime / 3600;
    long minutes = (uptime % 3600) / 60;
    long seconds = uptime % 60;

    printf(CLEAR);

    printf("  ======================================================\n");
    printf("              SANJHA OS - PROCESS MONITOR               \n");
    printf("  ======================================================\n");

    printf("  Uptime : %ldh %ldm %lds", hours, minutes, seconds);
    printf("    Tasks : %d\n", count);
    printf("  Memory : %ld MB used / %ld MB total\n",
           mem_used / 1024, mem_total / 1024);

    // Memory bar
    int bar_width = 40;
    int filled = mem_total > 0 ? (mem_used * bar_width) / mem_total : 0;
    printf("  MEM    : [");
    for (int i = 0; i < filled; i++)          printf("|");
    for (int i = filled; i < bar_width; i++)  printf("-");
    printf("]\n");

    printf("  ------------------------------------------------------\n");
    printf("  %-6s  %-5s  %-5s  %-20s\n", "PID", "PPID", "STATE", "NAME");
    printf("  ------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        char state_ch[2] = { procs[i].state, '\0' };
        printf("  %-6d  %-5d  %-5s  %-20s\n",
               procs[i].pid,
               procs[i].ppid,
               state_ch,
               procs[i].name);
    }

    printf("  ------------------------------------------------------\n");
    printf("  Press Q to quit | Refreshes every 1 second\n");
}

struct termios orig_term;

void term_raw() {
    struct termios t;
    tcgetattr(0, &orig_term);
    t = orig_term;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
}

void term_restore() {
    tcsetattr(0, TCSANOW, &orig_term);
}

void busy_wait_ms(int ms) {
    for (volatile long i = 0; i < ms * 50000L; i++);
}

int main() {
    term_raw();

    while (1) {
        ProcInfo procs[MAX_PROCS];
        int count = 0;

        DIR *dir = opendir("/proc");
        if (!dir) {
            printf("Cannot open /proc\n");
            break;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && count < MAX_PROCS) {
            int pid = atoi(entry->d_name);
            if (pid <= 0) continue;
            if (parse_stat(pid, &procs[count]) == 0)
                count++;
        }
        closedir(dir);

        draw_top(procs, count);

        char c = 0;
        read(0, &c, 1);
        if (c == 'q' || c == 'Q') break;

        busy_wait_ms(1000);
    }

    term_restore();
    printf(CLEAR);
    return 0;
}
