#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <termios.h>
#include "linux.h"

#define CELL     16
#define COLS     40
#define ROWS     30
#define SCREEN_W 640
#define SCREEN_H 480
#define MAX_LEN  (COLS * ROWS)

uint32_t fb_w, fb_h;
uint8_t *fbRegion = 0;

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

void draw_rect(int x, int y, int w, int h, int r, int g, int b) {
    int off_x = (fb_w - SCREEN_W) / 2;
    int off_y = (fb_h - SCREEN_H) / 2;
    x += off_x;
    y += off_y;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col;
            int py = y + row;
            if (px < 0 || py < 0 || px >= (int)fb_w || py >= (int)fb_h) continue;
            uint32_t offset = (px + py * fb_w) * 4;
            fbRegion[offset + 0] = b;
            fbRegion[offset + 1] = g;
            fbRegion[offset + 2] = r;
            fbRegion[offset + 3] = 0;
        }
    }
}

void draw_cell(int cx, int cy, int r, int g, int b) {
    draw_rect(cx * CELL + 1, cy * CELL + 1, CELL - 2, CELL - 2, r, g, b);
}

void draw_background() {
    draw_rect(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0);
    for (int x = 0; x <= COLS; x++)
        draw_rect(x * CELL, 0, 1, SCREEN_H, 0x11, 0x11, 0x11);
    for (int y = 0; y <= ROWS; y++)
        draw_rect(0, y * CELL, SCREEN_W, 1, 0x11, 0x11, 0x11);
}

int snake_x[MAX_LEN], snake_y[MAX_LEN];
int snake_len;
int dir_x, dir_y;
int food_x, food_y;
int score;
int game_over;

void place_food() {
    int ok = 0;
    while (!ok) {
        food_x = rand() % COLS;
        food_y = rand() % ROWS;
        ok = 1;
        for (int i = 0; i < snake_len; i++) {
            if (snake_x[i] == food_x && snake_y[i] == food_y) {
                ok = 0;
                break;
            }
        }
    }
}

void game_init() {
    snake_len = 4;
    dir_x = 1;
    dir_y = 0;
    score = 0;
    game_over = 0;
    for (int i = 0; i < snake_len; i++) {
        snake_x[i] = COLS/2 - i;
        snake_y[i] = ROWS/2;
    }
    place_food();
}

void game_draw() {
    draw_background();
    draw_cell(food_x, food_y, 0xFF, 0x45, 0x00);
    for (int i = snake_len - 1; i > 0; i--)
        draw_cell(snake_x[i], snake_y[i], 0x00, 0xAA, 0x30);
    draw_cell(snake_x[0], snake_y[0], 0x00, 0xFF, 0x50);
}

void game_update() {
    int nx = snake_x[0] + dir_x;
    int ny = snake_y[0] + dir_y;

    if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
        game_over = 1;
        return;
    }

    for (int i = 0; i < snake_len; i++) {
        if (snake_x[i] == nx && snake_y[i] == ny) {
            game_over = 1;
            return;
        }
    }

    for (int i = snake_len - 1; i > 0; i--) {
        snake_x[i] = snake_x[i-1];
        snake_y[i] = snake_y[i-1];
    }
    snake_x[0] = nx;
    snake_y[0] = ny;

    if (nx == food_x && ny == food_y) {
        score++;
        snake_len++;
        place_food();
    }
}

void busy_wait(int ms) {
    for (volatile long i = 0; i < ms * 50000L; i++);
}

int main() {
    struct fb_var_screeninfo fbInfo = {0};
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf("Cannot open /dev/fb0\n");
        return 1;
    }

    ioctl(fd, FBIOGET_VSCREENINFO, &fbInfo);
    fb_w = fbInfo.xres;
    fb_h = fbInfo.yres;
    size_t fbLen = fb_w * fb_h * 4;
    fbRegion = mmap(NULL, fbLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ((size_t)fbRegion == (size_t)(-1)) {
        printf("Cannot mmap fb\n");
        return 1;
    }

    term_raw();
    printf("\033[H\033[J");
    srand(12345);
    game_init();

    while (!game_over) {
        char buf[3] = {0};
        int n = read(0, buf, 3);
        if (n == 1) {
            switch (buf[0]) {
                case 'w': case 'W': if (dir_y != 1)  { dir_x=0; dir_y=-1; } break;
                case 's': case 'S': if (dir_y != -1) { dir_x=0; dir_y=1;  } break;
                case 'a': case 'A': if (dir_x != 1)  { dir_x=-1; dir_y=0; } break;
                case 'd': case 'D': if (dir_x != -1) { dir_x=1;  dir_y=0; } break;
                case 'q': case 'Q': game_over = 1; break;
            }
        } else if (n == 3 && buf[0] == '\033' && buf[1] == '[') {
            switch (buf[2]) {
                case 'A': if (dir_y != 1)  { dir_x=0; dir_y=-1; } break;
                case 'B': if (dir_y != -1) { dir_x=0; dir_y=1;  } break;
                case 'C': if (dir_x != -1) { dir_x=1;  dir_y=0; } break;
                case 'D': if (dir_x != 1)  { dir_x=-1; dir_y=0; } break;
            }
        }

        game_update();
        game_draw();
        printf("\033[H  SANJHA OS - SNAKE   Score: %d   (WASD/Arrows to move, Q to quit)", score);
        busy_wait(120);
    }

    draw_rect(SCREEN_W/2 - 80, SCREEN_H/2 - 20, 160, 40, 0xFF, 0x45, 0x00);
    printf("\033[H  GAME OVER! Final Score: %d   Press any key...", score);

    term_restore();
    char c = 0;
    while (!c) {
        read(0, &c, 1);
    }

    draw_rect(0, 0, fb_w, fb_h, 0, 0, 0);
    printf("\033[H\033[J");
    return 0;
}
