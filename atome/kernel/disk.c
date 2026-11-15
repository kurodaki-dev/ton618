#include "kernel.h"

// A hard disk through ATA PIO, LBA28, on the primary bus (ports 0x1F0-0x1F7): what QEMU's -hda and most virtual
// machines' IDE disks look like, and what a real PC offers when its disk controller is in legacy / IDE mode.
// (A modern NVMe or AHCI-only disk is not reachable: that needs other drivers.)
#define ATA_IO   0x1F0
#define ATA_CTL  0x3F6
#define ST_ERR   0x01
#define ST_DRQ   0x08
#define ST_BSY   0x80

static u32 total;

static void delay400(void) { for (int i = 0; i < 4; i++) inb(ATA_CTL); }
static int wait_not_busy(void) {
    for (u32 i = 0; i < 4000000; i++) { u8 st = inb(ATA_IO + 7); if (!(st & ST_BSY)) return st; }
    return -1;
}
static int wait_drq(void) {
    for (u32 i = 0; i < 4000000; i++) {
        u8 st = inb(ATA_IO + 7);
        if (st & ST_ERR) return 0;
        if (!(st & ST_BSY) && (st & ST_DRQ)) return 1;
    }
    return 0;
}

int disk_init(void) {
    total = 0;
    outb(ATA_IO + 6, 0xA0);                  // master drive
    delay400();
    outb(ATA_IO + 2, 0); outb(ATA_IO + 3, 0); outb(ATA_IO + 4, 0); outb(ATA_IO + 5, 0);
    outb(ATA_IO + 7, 0xEC);                  // IDENTIFY
    u8 st = inb(ATA_IO + 7);
    if (st == 0 || st == 0xFF) return 0;     // nothing there
    if (wait_not_busy() < 0) return 0;
    if (inb(ATA_IO + 4) || inb(ATA_IO + 5)) return 0;   // not a plain ATA disk (a CD-ROM answers differently)
    if (!wait_drq()) return 0;
    u16 id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(ATA_IO);
    total = (u32)id[60] | ((u32)id[61] << 16);
    return total > 0;
}
u32 disk_sectors(void) { return total; }

static void setup(u32 lba, u8 cmd) {
    outb(ATA_IO + 6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_IO + 2, 1);
    outb(ATA_IO + 3, (u8)lba); outb(ATA_IO + 4, (u8)(lba >> 8)); outb(ATA_IO + 5, (u8)(lba >> 16));
    outb(ATA_IO + 7, cmd);
}
int disk_read(u32 lba, void *buf) {
    if (lba >= total || wait_not_busy() < 0) return 0;
    setup(lba, 0x20);
    if (!wait_drq()) return 0;
    u16 *w = buf;
    for (int i = 0; i < 256; i++) w[i] = inw(ATA_IO);
    return 1;
}
int disk_write(u32 lba, const void *buf) {
    if (lba >= total || wait_not_busy() < 0) return 0;
    setup(lba, 0x30);
    if (!wait_drq()) return 0;
    const u16 *w = buf;
    for (int i = 0; i < 256; i++) outw(ATA_IO, w[i]);
    outb(ATA_IO + 7, 0xE7);                  // flush the disk's cache
    return wait_not_busy() >= 0;
}
