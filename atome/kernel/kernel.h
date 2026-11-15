#pragma once
// Atome kernel — what the pieces of the kernel share. No libc: everything here is ours.
#ifdef ATOME_HOSTED
// Built on a normal computer to test the TON interpreter quickly (tests/atome-ton-host.c).
#include <stddef.h>
#include <stdint.h>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint64_t u64;
#else
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed int         i32;
typedef unsigned long long u64;
typedef u32                size_t;
#define NULL ((void*)0)

// ---- io ports ----
static inline void outb(u16 port, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
static inline u8   inb(u16 port) { u8 v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v; }
#endif

// ---- libk.c: the few library functions we need ----
#ifndef ATOME_HOSTED
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
#endif
void  kheap_init(void *start, size_t size);
void  *kalloc(size_t n);              // bump allocator, never freed (the interpreter frees by resetting)
size_t kheap_used(void);
size_t kheap_size(void);
void   kheap_reset(size_t mark);
size_t kheap_mark(void);
int    kitoa(i32 v, char *out);       // returns the length

// ---- console.c: screen (VGA text mode 80x25) and serial port (COM1) ----
#define VGA_W 80
#define VGA_H 25
void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_puts(const char *s);
void console_print_int(i32 v);
void console_color(u8 fg, u8 bg);
void console_put_at(int x, int y, const char *s, u8 attr);   // draws text at a cell, no cursor move
void console_fill(int x, int y, int w, int h, char ch, u8 attr);
void console_cursor(int x, int y);
void serial_putc(char c);

// ---- input.c: keyboard (PS/2, polled) and serial input ----
int  input_poll(void);                // a character, or -1 if none is waiting
int  input_wait(void);                // blocks
int  input_readline(char *buf, int max);   // echoes, handles backspace; returns the length
#define KEY_UP 0x100
#define KEY_DOWN 0x101
#define KEY_LEFT 0x102
#define KEY_RIGHT 0x103
#define KEY_ESC 27

// ---- time.c: a clock without interrupts (PIT) ----
void  time_init(void);
void  sleep_ms(u32 ms);
u32   uptime_ms(void);

// ---- kmain.c ----
void  panic(const char *msg);
void  power_off(void);
void  reboot(void);

// ---- ton.c (the TON interpreter) ----
int   ton_run(const char *source, size_t len);     // returns 0 on success
