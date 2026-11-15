#include "kernel.h"

// No interrupts yet, so time comes from polling the PIT (channel 2, one-shot) and counting
// processor cycles (rdtsc) over a known interval.
static u32 tsc_per_ms = 1;
static u64 tsc_start;

static inline u64 rdtsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return ((u64)hi << 32) | lo; }

static void pit_wait_ticks(u16 ticks) {         // the PIT runs at 1,193,182 Hz
    outb(0x61, (u8)((inb(0x61) & 0xFC) | 0x01)); // gate on, speaker off
    outb(0x43, 0xB0);                            // channel 2, lobyte/hibyte, mode 0
    outb(0x42, (u8)(ticks & 0xFF));
    outb(0x42, (u8)(ticks >> 8));
    outb(0x61, (u8)(inb(0x61) & 0xFE));          // pulse the gate to start the count
    outb(0x61, (u8)(inb(0x61) | 0x01));
    while (!(inb(0x61) & 0x20)) { }
}

// 64-bit / 32-bit division without libgcc
static u64 div64(u64 n, u32 d) {
    u64 q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (u64)1 << i; }
    }
    return q;
}

void time_init(void) {
    u64 a = rdtsc();
    pit_wait_ticks(59659);                       // 50 ms
    u64 b = rdtsc();
    tsc_per_ms = (u32)div64(b - a, 50);
    if (tsc_per_ms == 0) tsc_per_ms = 1;
    tsc_start = rdtsc();
}
u32 uptime_ms(void) { return (u32)div64(rdtsc() - tsc_start, tsc_per_ms); }
void sleep_ms(u32 ms) {
    u32 end = uptime_ms() + ms;
    while ((i32)(uptime_ms() - end) < 0) __asm__ volatile("pause");
}
