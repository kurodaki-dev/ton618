#include "kernel.h"

#define ATOME_VERSION "0.1.0"

struct mb_module { u32 start, end, string, reserved; };
struct mb_info {
    u32 flags, mem_lower, mem_upper, boot_device, cmdline, mods_count, mods_addr;
};

static const char *program;        // the TON program that came with the image (a Multiboot module)
static size_t program_len;

static inline void outw(u16 port, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port)); }

void power_off(void) {
    console_puts("Powering off...\n");
    outw(0x604, 0x2000);     // QEMU (ACPI)
    outw(0xB004, 0x2000);    // Bochs / older QEMU
    outw(0x4004, 0x3400);    // VirtualBox
    for (;;) __asm__ volatile("cli; hlt");
}
void reboot(void) {
    console_puts("Rebooting...\n");
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) { }
    outb(0x64, 0xFE);        // pulse the CPU reset line through the keyboard controller
    for (;;) __asm__ volatile("cli; hlt");
}
void panic(const char *msg) {
    console_color(15, 4);
    console_puts("\n*** Atome kernel panic: ");
    console_puts(msg);
    console_puts(" ***\n");
    for (;;) __asm__ volatile("cli; hlt");
}

static void banner(void) {
    console_color(11, 0);
    console_puts("  ___  _                          \n / _ \\| |_ ___  _ __ ___   ___ \n| |_| | __/ _ \\| '_ ` _ \\ / _ \\\n|  _  | || (_) | | | | | |  __/\n|_| |_|\\__\\___/|_| |_| |_|\\___|\n");
    console_color(7, 0);
    console_puts("Atome kernel " ATOME_VERSION "  -  an operating system written from scratch\n\n");
}

static int is_cmd(const char *line, const char *cmd) {
    size_t n = strlen(cmd);
    return strncmp(line, cmd, n) == 0 && (line[n] == 0 || line[n] == ' ');
}

static void shell(void) {
    char line[160];
    for (;;) {
        console_color(10, 0); console_puts("atome");
        console_color(7, 0);  console_puts("> ");
        input_readline(line, sizeof line);
        const char *a = line;
        while (*a == ' ') a++;
        if (!*a) continue;
        if (is_cmd(a, "help")) {
            console_puts("help  clear  echo <text>  mem  uptime  ver  run  colors  reboot  poweroff\n");
        } else if (is_cmd(a, "clear")) console_clear();
        else if (is_cmd(a, "echo")) { console_puts(a[4] ? a + 5 : ""); console_putc('\n'); }
        else if (is_cmd(a, "ver")) console_puts("Atome kernel " ATOME_VERSION "\n");
        else if (is_cmd(a, "mem")) {
            console_puts("heap: "); console_print_int((i32)(kheap_used() / 1024));
            console_puts(" KB used of "); console_print_int((i32)(kheap_size() / 1024)); console_puts(" KB\n");
        } else if (is_cmd(a, "uptime")) {
            console_print_int((i32)(uptime_ms() / 1000)); console_puts(" s\n");
        } else if (is_cmd(a, "colors")) {
            for (int c = 0; c < 16; c++) { console_color((u8)c, 0); console_puts("Atome "); }
            console_color(7, 0); console_putc('\n');
        } else if (is_cmd(a, "run")) {
            if (program) ton_run(program, program_len);
            else console_puts("no TON program in this image\n");
        } else if (is_cmd(a, "reboot")) reboot();
        else if (is_cmd(a, "poweroff") || is_cmd(a, "halt")) power_off();
        else { console_puts("unknown command: "); console_puts(a); console_putc('\n'); }
    }
}

void kmain(u32 magic, struct mb_info *mbi) {
    console_init();
    time_init();
    banner();
    if (magic != 0x2BADB002) panic("not started by a Multiboot loader");

    // The heap is everything between the end of the kernel (and of the TON program) and the top of memory.
    extern u8 kernel_end;
    u32 heap_start = (u32)&kernel_end, mem_top = 0x100000 + ((mbi->flags & 1) ? mbi->mem_upper * 1024 : 15u * 1024 * 1024);
    if ((mbi->flags & (1u << 3)) && mbi->mods_count > 0) {
        struct mb_module *m0 = (struct mb_module *)mbi->mods_addr;
        for (u32 i = 0; i < mbi->mods_count; i++) if (m0[i].end > heap_start) heap_start = m0[i].end;
    }
    heap_start = (heap_start + 0xFFF) & ~0xFFFu;
    kheap_init((void *)heap_start, mem_top - 0x80000 - heap_start);   // keep a little room at the top

    console_puts("[boot] memory: ");
    if (mbi->flags & 1) { console_print_int((i32)((mbi->mem_lower + mbi->mem_upper) / 1024 + 1)); console_puts(" MB\n"); }
    else console_puts("unknown\n");
    console_puts("[boot] clock calibrated, keyboard and serial ready\n");

    if ((mbi->flags & (1u << 3)) && mbi->mods_count > 0) {
        struct mb_module *m = (struct mb_module *)mbi->mods_addr;
        program = (const char *)m[0].start;
        program_len = m[0].end - m[0].start;
        console_puts("[boot] TON program: "); console_print_int((i32)program_len); console_puts(" bytes\n");
    }
    if (program) {
        console_puts("[boot] starting the program\n\n");
        int r = ton_run(program, program_len);
        console_puts(r == 0 ? "\n[atome] the program ended\n" : "\n[atome] the program stopped with an error\n");
    }
    console_puts("Type 'help'.\n");
    shell();
}
