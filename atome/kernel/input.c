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

// ---- the PS/2 mouse: three-byte packets from the controller's second port (status bit 5 tells its bytes apart)
static int mx, my, mbuttons, mlatch, mw = 1024, mh = 768, mstep, mchanged;
static u8 mpacket[3];
static void ps2_write_wait(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) { } }
static int ps2_read_wait(void) { for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return 1; return 0; }
static void mouse_cmd(u8 v) {
    ps2_write_wait(); outb(0x64, 0xD4);
    ps2_write_wait(); outb(0x60, v);
    if (ps2_read_wait()) inb(0x60);             // the acknowledge byte
}
void mouse_init(int w, int h) {
    mw = w; mh = h; mx = w / 2; my = h / 2;
    ps2_write_wait(); outb(0x64, 0xA8);         // enable the second port
    ps2_write_wait(); outb(0x64, 0x20);
    if (ps2_read_wait()) {
        u8 cfg = inb(0x60);
        cfg &= (u8)~0x22;                       // no interrupts (we poll), second port clock on
        ps2_write_wait(); outb(0x64, 0x60);
        ps2_write_wait(); outb(0x60, cfg);
    }
    mouse_cmd(0xF6);                            // defaults
    mouse_cmd(0xF4);                            // start sending packets
}
static void mouse_feed(u8 b) {
    if (mstep == 0 && !(b & 8)) return;         // out of step: wait for a first byte
    mpacket[mstep++] = b;
    if (mstep < 3) return;
    mstep = 0;
    int dx = mpacket[1] - ((mpacket[0] & 0x10) ? 256 : 0);
    int dy = mpacket[2] - ((mpacket[0] & 0x20) ? 256 : 0);
    mx += dx; my -= dy;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > mw - 1) mx = mw - 1;
    if (my > mh - 1) my = mh - 1;
    mbuttons = mpacket[0] & 7;
    mlatch |= mbuttons;                         // a click shorter than one frame of the program is still seen
    mchanged = 1;
}
int mouse_get(int *x, int *y, int *buttons) {
    u8 st;
    while (((st = inb(0x64)) & 1) && (st & 0x20)) mouse_feed(inb(0x60));   // only the mouse's bytes: the keyboard's stay for key()
    *x = mx; *y = my; *buttons = mbuttons | mlatch; mlatch = 0;
    int c = mchanged; mchanged = 0;
    return c;
}

static int keyboard_poll(void) {
    u8 st = inb(0x64);
    if (!(st & 1)) return -1;
    u8 sc = inb(0x60);
    if (st & 0x20) { mouse_feed(sc); return -1; }
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
