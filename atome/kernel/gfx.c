#include "kernel.h"
#include "font8x16.h"

// A graphical screen. GRUB (asked through the Multiboot header of atome-gfx.elf) sets a 32-bit linear framebuffer
// and tells us where it is. We draw into a back buffer in memory, then copy it to the screen in one go: no flicker,
// and the mouse pointer is drawn on the screen only, so moving it never redraws the scene.
static u32 *screen, *back;
static u32 pitch_px, width, height;
static int cur_x = -100, cur_y = -100, cur_on;

#ifdef ATOME_HOSTED
#include <string.h>
#define fast_copy memcpy
#else
static void fast_copy(void *d, const void *s, size_t n) {
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(n) : : "memory");   // n is a number of 32-bit words here
}
#endif

void gfx_init(void *fb, u32 pitch, u32 w, u32 h, u32 bpp) {
    if (bpp != 32 || !fb || w < 320 || h < 200 || w > 4096 || h > 4096) return;
    back = kalloc((size_t)w * h * 4);
    if (!back) return;
    screen = fb; pitch_px = pitch / 4; width = w; height = h;
    memset(back, 0, (size_t)w * h * 4);
}
int gfx_ok(void) { return screen != NULL; }
u32 gfx_w(void) { return width; }
u32 gfx_h(void) { return height; }

void gfx_clear(u32 rgb) {
    if (!back) return;
    for (u32 i = 0; i < width * height; i++) back[i] = rgb;
}
void gfx_rect(int x, int y, int w, int h, u32 rgb) {
    if (!back) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)width) w = (int)width - x;
    if (y + h > (int)height) h = (int)height - y;
    for (int j = 0; j < h; j++) {
        u32 *row = back + (size_t)(y + j) * width + x;
        for (int i = 0; i < w; i++) row[i] = rgb;
    }
}
void gfx_text(int x, int y, const char *s, u32 rgb, int scale) {
    if (!back) return;
    if (scale < 1) scale = 1;
    for (; *s; s++, x += 8 * scale) {
        unsigned char c = (unsigned char)*s;
        if (c < 32 || c > 126) c = '?';
        const unsigned char *g = font8x16[c - 32];
        for (int r = 0; r < 16; r++)
            for (int b = 0; b < 8; b++)
                if (g[r] & (0x80 >> b)) gfx_rect(x + b * scale, y + r * scale, scale, scale, rgb);
    }
}

// the pointer: an arrow, 1 = white, 2 = black outline
static const char arrow[19][12] = {
    "2           ", "22          ", "212         ", "2112        ", "21112       ", "211112      ", "2111112     ", "21111112    ",
    "211111112   ", "2111111112  ", "21111122222 ", "2112112     ", "212 2112    ", "22  2112    ", "2    2112   ", "     2112   ", "      2112  ", "      2112  ", "       22   "
};
void gfx_cursor(int x, int y, int visible) { cur_x = x; cur_y = y; cur_on = visible; }

void gfx_flip(void) {
    if (!back) return;
    for (u32 y = 0; y < height; y++) fast_copy(screen + (size_t)y * pitch_px, back + (size_t)y * width, width);
    if (!cur_on) return;
    for (int j = 0; j < 19; j++)
        for (int i = 0; i < 12; i++) {
            char c = arrow[j][i];
            int px = cur_x + i, py = cur_y + j;
            if (c == ' ' || px < 0 || py < 0 || px >= (int)width || py >= (int)height) continue;
            screen[(size_t)py * pitch_px + px] = c == '1' ? 0xFFFFFF : 0x000000;
        }
}
