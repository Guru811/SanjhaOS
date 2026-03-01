// Sanjha OS - Glowing Image Boot Screen
// Built by Gurshant Singh

#include <console.h>
#include <fb.h>
#include <printf.h>
#include <logo_data.h>

static void boot_delay(volatile int count) {
    while (count-- > 0)
        __asm__ volatile("nop");
}

static void draw_logo_image(int brightness) {
    // Fill screen black
    drawRect(0, 0, fb.width, fb.height, 0, 0, 0);

    // Center the logo
    int start_x = (fb.width  - LOGO_WIDTH)  / 2;
    int start_y = (fb.height - LOGO_HEIGHT) / 2;

    for (int y = 0; y < LOGO_HEIGHT; y++) {
        for (int x = 0; x < LOGO_WIDTH; x++) {
            unsigned int pixel = sanjha_logo[y * LOGO_WIDTH + x];

            unsigned int a = (pixel >> 24) & 0xFF;
            unsigned int r = (pixel >> 16) & 0xFF;
            unsigned int g = (pixel >>  8) & 0xFF;
            unsigned int b = (pixel >>  0) & 0xFF;

            // Skip fully transparent pixels
            if (a < 10) continue;

            // Apply brightness
            r = (r * brightness) / 255;
            g = (g * brightness) / 255;
            b = (b * brightness) / 255;

            // Draw single pixel using drawRect 1x1
            drawRect(start_x + x, start_y + y, 1, 1, r, g, b);
        }
    }

    // Glow text below logo
    int text_y = start_y + LOGO_HEIGHT - 50;
    setConsoleY(text_y);
    setConsoleX(0);

    int tr = (255 * brightness) / 255;
    int tg = (140 * brightness) / 255;
    changeTextColor(tr, tg, 0);

    // Center text
    int pad = (fb.width / 8 - 24) / 2;
    for (int i = 0; i < pad; i++) printf(" ");
    printf("Sanjha OS - Shared. Together.\n");

}

void showBootScreen() {
	cursorHidden = true;    // hide cursor during boot screen
    int delay = 60000000;

    // Fade IN
    for (int b = 0; b <= 255; b += 3) {
        draw_logo_image(b);
        boot_delay(delay);
    }

    // Hold full brightness
    draw_logo_image(255);
    boot_delay(delay * 1000);

    // Pulse down
    for (int b = 255; b >= 80; b -= 2) {
        draw_logo_image(b);
        boot_delay(delay / 3);
    }

    // Pulse up
    for (int b = 80; b <= 255; b += 2) {
        draw_logo_image(b);
        boot_delay(delay / 3);
    }

    // Hold again
    boot_delay(delay * 1000);

    // Fade OUT
    for (int b = 255; b >= 0; b -= 3) {
        draw_logo_image(b);
        boot_delay(delay);
    }

    // Clear and reset
    changeBg(0, 0, 0);
    changeTextColor(255, 255, 255);
    clearScreen();
	 cursorHidden = false;   // restore cursor after boot screen
}
