#include "kernel.h"

// Scancode set 1, US layout. 0 = no character.
static const char plain[0x59] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', 8, '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
    'b','n','m',',','.','/', 0,'*', 0,' ', 0, 0,0,0,0,0, 0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1','2','3','0','.'
};
static const char shifted[0x59] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', 8, '\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X','C','V',
    'B','N','M','<','>','?', 0,'*', 0,' ', 0, 0,0,0,0,0, 0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1','2','3','0','.'
};
static int shift, ext;

static int keyboard_poll(void) {
    if (!(inb(0x64) & 1)) return -1;
    u8 sc = inb(0x60);
    if (sc == 0xE0) { ext = 1; return -1; }
    int release = sc & 0x80;
    sc &= 0x7F;
    if (sc == 0x2A || sc == 0x36) { shift = !release; ext = 0; return -1; }
    if (release) { ext = 0; return -1; }
    if (ext) {
        ext = 0;
        switch (sc) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            default: return -1;
        }
    }
    if (sc >= 0x59) return -1;
    char c = shift ? shifted[sc] : plain[sc];
    return c ? (u8)c : -1;
}

// Serial input is accepted too: that is how a script (or `qemu -serial stdio`) types into the system.
static int serial_poll(void) {
    if (!(inb(0x3F8 + 5) & 1)) return -1;
    int c = inb(0x3F8);
    if (c == '\r') c = '\n';
    if (c == 127) c = 8;
    return c;
}

int input_poll(void) {
    int c = serial_poll();
    return c >= 0 ? c : keyboard_poll();
}
int input_wait(void) {
    for (;;) { int c = input_poll(); if (c >= 0) return c; __asm__ volatile("pause"); }
}
int input_readline(char *buf, int max) {
    int n = 0;
    for (;;) {
        int c = input_wait();
        if (c == '\n') { console_putc('\n'); buf[n] = 0; return n; }
        if (c == 8) { if (n > 0) { n--; console_putc('\b'); } continue; }
        if (c >= 32 && c < 127 && n < max - 1) { buf[n++] = (char)c; console_putc((char)c); }
    }
}
