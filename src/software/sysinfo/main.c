#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <time.h>

int main() {
  // Get sysinfo
  struct sysinfo info;
  sysinfo(&info);

  // Get framebuffer resolution
  int fb_w = 0, fb_h = 0;
  // try reading from /sys or just show N/A

  printf("\n");
  printf("\033[38;2;255;140;0m"); // orange
  printf("         SANJHA OS — SYSTEM INFO        \n");
  printf("\033[0m");

  // OS
  printf("\033[38;2;0;220;255m  [OS]\033[0m\n");
  printf("    Name      : Sanjha OS v0.1\n");
  printf("    Author    : Gurshant Singh\n");

  // Memory
  unsigned long total_mb = info.totalram / 1024 / 1024;
  unsigned long free_mb  = info.freeram  / 1024 / 1024;
  unsigned long used_mb  = total_mb - free_mb;

  printf("\033[38;2;0;220;255m  [Memory]\033[0m\n");
  printf("    Total     : %lu MB\n", total_mb);
  printf("    Used      : %lu MB\n", used_mb);
  printf("    Free      : %lu MB\n", free_mb);

  // Uptime
  long hours   = info.uptime / 3600;
  long minutes = (info.uptime % 3600) / 60;
  long seconds = info.uptime % 60;

  printf("\033[38;2;0;220;255m  [System]\033[0m\n");
  printf("    Uptime    : %ldh %ldm %lds\n", hours, minutes, seconds);
  printf("    Processes : %d\n", info.procs);

  // CPU (basic)
  printf("\033[38;2;0;220;255m  [CPU]\033[0m\n");
  FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
  if (cpuinfo) {
    char line[256];
    while (fgets(line, sizeof(line), cpuinfo)) {
      if (strncmp(line, "model name", 10) == 0) {
        char *colon = strchr(line, ':');
        if (colon)
          printf("    Model     :%s", colon + 1);
        break;
      }
    }
    fclose(cpuinfo);
  } else {
    printf("    Model     : N/A\n");
  }

  printf("\033[38;2;255;140;0m");
  printf("\033[0m\n");

  return 0;
}
