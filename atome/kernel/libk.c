#include "kernel.h"

#ifndef ATOME_HOSTED
void *memset(void *d, int c, size_t n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }
int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (u8)*a - (u8)*b; }
int strncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) { if (*a != *b) return (u8)*a - (u8)*b; if (!*a) return 0; }
    return 0;
}

#endif

// The heap: all the memory after the kernel, given out in order (the machine's memory map decides
// how much that is). The TON interpreter takes a mark before a run and resets to it afterwards,
// which is all the "freeing" the kernel needs for now (there is no garbage collector yet).
static u8 *heap_base;
static size_t heap_cap, heap_top;
void kheap_init(void *start, size_t size) { heap_base = start; heap_cap = size; heap_top = 0; }
void *kalloc(size_t n) {
    n = (n + 7) & ~7u;
    if (heap_top + n > heap_cap) return NULL;
    void *p = heap_base + heap_top;
    heap_top += n;
    return p;
}
size_t kheap_used(void) { return heap_top; }
size_t kheap_size(void) { return heap_cap; }
size_t kheap_mark(void) { return heap_top; }
void kheap_reset(size_t mark) { heap_top = mark; }

int kitoa(i32 v, char *out) {
    char tmp[12];
    int n = 0, len = 0;
    u32 u = v < 0 ? (u32)(-(v + 1)) + 1u : (u32)v;
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) out[len++] = '-';
    while (n) out[len++] = tmp[--n];
    out[len] = 0;
    return len;
}
