#include "kernel.h"

// ---- serial port COM1: what the tests read, and a second way to type ----
#define COM1 0x3F8
static void serial_init(void) {
    outb(COM1 + 1, 0x00);   // no interrupts
    outb(COM1 + 3, 0x80);   // DLAB on
    outb(COM1 + 0, 0x01);   // 115200 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);   // FIFO on
    outb(COM1 + 4, 0x0B);
}
void serial_putc(char c) {
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++) { }
    outb(COM1, (u8)c);
}

// ---- VGA text mode ----
static volatile u16 *const vga = (volatile u16 *)0xB8000;
static int cx, cy;
static u8 attr = 0x07;   // light grey on black

static void hw_cursor(void) {
    u16 pos = (u16)(cy * VGA_W + cx);
    outb(0x3D4, 14); outb(0x3D5, (u8)(pos >> 8));
    outb(0x3D4, 15); outb(0x3D5, (u8)(pos & 0xFF));
}
void console_cursor(int x, int y) { cx = x; cy = y; hw_cursor(); }
void console_color(u8 fg, u8 bg) { attr = (u8)((bg << 4) | (fg & 0x0F)); }

void console_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = (u16)(attr << 8) | ' ';
    cx = cy = 0;
    hw_cursor();
}
static void scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++) vga[i] = vga[i + VGA_W];
    for (int i = 0; i < VGA_W; i++) vga[VGA_W * (VGA_H - 1) + i] = (u16)(attr << 8) | ' ';
    cy = VGA_H - 1;
}
void console_putc(char c) {
    if (c == '\n') { serial_putc('\r'); serial_putc('\n'); cx = 0; cy++; }
    else if (c == '\r') { cx = 0; }
    else if (c == '\b') {
        serial_putc('\b'); serial_putc(' '); serial_putc('\b');
        if (cx > 0) cx--; else if (cy > 0) { cy--; cx = VGA_W - 1; }
        vga[cy * VGA_W + cx] = (u16)(attr << 8) | ' ';
    } else {
        serial_putc(c);
        vga[cy * VGA_W + cx] = (u16)(attr << 8) | (u8)c;
        if (++cx >= VGA_W) { cx = 0; cy++; }
    }
    if (cy >= VGA_H) scroll();
    hw_cursor();
}
void console_puts(const char *s) { while (*s) console_putc(*s++); }
void console_print_int(i32 v) { char b[16]; kitoa(v, b); console_puts(b); }

void console_put_at(int x, int y, const char *s, u8 a) {
    for (; *s && x < VGA_W; s++, x++) if (x >= 0 && y >= 0 && y < VGA_H) vga[y * VGA_W + x] = (u16)(a << 8) | (u8)*s;
}
void console_fill(int x, int y, int w, int h, char ch, u8 a) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < VGA_W && j >= 0 && j < VGA_H) vga[j * VGA_W + i] = (u16)(a << 8) | (u8)ch;
}
void console_init(void) { serial_init(); console_clear(); }
