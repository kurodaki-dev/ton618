// Runs the kernel's TON interpreter on a normal computer, with a fake 80x25 screen and scripted keys,
// to test programs quickly (tests/atome-host.sh). Usage: atome-host program.ton ["keys"] ; keys use
// <enter> <up> <down> <left> <right> <esc> <tab> <bs>, other characters are typed as they are.
#define ATOME_HOSTED
#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char screen[VGA_H][VGA_W + 1];
static unsigned char attrs[VGA_H][VGA_W];
static const char *keys; static size_t key_pos;
static int cx_, cy_;

void console_init(void) {}
void console_clear(void) { for (int y = 0; y < VGA_H; y++) { memset(screen[y], ' ', VGA_W); screen[y][VGA_W] = 0; } memset(attrs, 7, sizeof attrs); cx_ = cy_ = 0; }
void console_putc(char c) { putchar(c); }
void console_puts(const char *s) { fputs(s, stdout); }
void console_print_int(i32 v) { printf("%d", v); }
void console_color(u8 fg, u8 bg) { (void)fg; (void)bg; }
void console_put_at(int x, int y, const char *s, u8 a) { for (; *s && x < VGA_W; s++, x++) if (x >= 0 && y >= 0 && y < VGA_H) { screen[y][x] = *s; attrs[y][x] = a; } }
void console_fill(int x, int y, int w, int h, char ch, u8 a) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < VGA_W && j >= 0 && j < VGA_H) { screen[j][i] = ch; attrs[j][i] = a; } }
void console_cursor(int x, int y) { cx_ = x; cy_ = y; }
void serial_putc(char c) { (void)c; }
static int ticks_left = 4;   // when the keys run out, a few idle polls are answered (so ticks can be tested) before the program is stopped
int input_poll(void) {
    if (keys && keys[key_pos]) return input_wait();
    if (ticks_left-- > 0) return -1;
    exit(0);
}
int input_wait(void) {
    if (!keys || !keys[key_pos]) { fprintf(stderr, "[host] no more keys\n"); exit(0); }
    if (keys[key_pos] == '<') {
        struct { const char *n; int c; } t[] = { {"<enter>", '\n'}, {"<up>", KEY_UP}, {"<down>", KEY_DOWN}, {"<left>", KEY_LEFT}, {"<right>", KEY_RIGHT}, {"<esc>", KEY_ESC}, {"<tab>", '\t'}, {"<bs>", 8} };
        for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) { size_t n = strlen(t[i].n); if (strncmp(keys + key_pos, t[i].n, n) == 0) { key_pos += n; return t[i].c; } }
    }
    return (unsigned char)keys[key_pos++];
}
int input_readline(char *buf, int max) { int n = 0, c; while ((c = input_wait()) != '\n' && n < max - 1) buf[n++] = (char)c; buf[n] = 0; return n; }
void sleep_ms(u32 ms) { (void)ms; }
u32 uptime_ms(void) { static u32 t; return t += 1000; }
void reboot(void) { printf("[reboot]\n"); exit(0); }
void power_off(void) { printf("[poweroff]\n"); exit(0); }
static FILE *disk_f; static u32 disk_n;
int disk_init(void) { const char *p = getenv("ATOME_DISK"); if (!p || !(disk_f = fopen(p, "r+b"))) return 0; fseek(disk_f, 0, SEEK_END); disk_n = (u32)(ftell(disk_f) / 512); return disk_n > 0; }
u32 disk_sectors(void) { if (!disk_f) disk_init(); return disk_n; }
int disk_read(u32 lba, void *buf) { if (!disk_f || lba >= disk_n) return 0; fseek(disk_f, (long)lba * 512, SEEK_SET); return fread(buf, 512, 1, disk_f) == 1; }
int disk_write(u32 lba, const void *buf) { if (!disk_f || lba >= disk_n) return 0; fseek(disk_f, (long)lba * 512, SEEK_SET); int ok = fwrite(buf, 512, 1, disk_f) == 1; fflush(disk_f); return ok; }
void panic(const char *m) { fprintf(stderr, "panic: %s\n", m); exit(1); }

static void dump(void) {
    printf("\n-- screen --\n");
    int last = VGA_H - 1;
    while (last > 0) { int blank = 1; for (int x = 0; x < VGA_W; x++) if (screen[last][x] != ' ') blank = 0; if (!blank) break; last--; }
    for (int y = 0; y <= last; y++) { int e = VGA_W; while (e > 0 && screen[y][e - 1] == ' ') e--; printf("%.*s\n", e, screen[y]); }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: atome-host program.ton [keys]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc((size_t)n + 1); if (fread(src, 1, (size_t)n, f) != (size_t)n) return 2; src[n] = 0; fclose(f);
    keys = argc > 2 ? argv[2] : "";
    size_t cap = 64u << 20; kheap_init(malloc(cap), cap);
    console_clear();
    atexit(dump);
    return ton_run(src, (size_t)n);
}
