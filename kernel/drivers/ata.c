#include "ata.h"
#include <common.h>

#define ATA_PRIMARY_IO_BASE 0x1F0
#define ATA_PRIMARY_CTRL_BASE 0x3F6
#define ATA_CMD_READ_PIO     0x20
#define ATA_CMD_WRITE_PIO    0x30
#define ATA_CMD_IDENTIFY     0xEC

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "d"(port));
}
static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}
static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "d"(port));
}

static int ata_wait_ready(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_IO_BASE + 7);
        if (!(status & 0x80)) return (status & 0x01) ? -1 : 0;
    }
    return -1;
}

int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    outb(ATA_PRIMARY_IO_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO_BASE + 2, 1);
    outb(ATA_PRIMARY_IO_BASE + 3, (uint8_t)(lba));
    outb(ATA_PRIMARY_IO_BASE + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO_BASE + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO_BASE + 7, ATA_CMD_READ_PIO);
    if (ata_wait_ready() != 0) return -1;
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(ATA_PRIMARY_IO_BASE);
        buffer[i*2] = word & 0xFF;
        buffer[i*2+1] = (word >> 8) & 0xFF;
    }
    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    outb(ATA_PRIMARY_IO_BASE + 6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO_BASE + 2, 1);
    outb(ATA_PRIMARY_IO_BASE + 3, (uint8_t)(lba));
    outb(ATA_PRIMARY_IO_BASE + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO_BASE + 5, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO_BASE + 7, ATA_CMD_WRITE_PIO);
    // 等待 DRQ
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PRIMARY_IO_BASE + 7);
        if ((status & 0x08) && !(status & 0x80)) break;
        if (status & 0x01) return -1;
    }
    for (int i = 0; i < 256; i++) {
        uint16_t word = buffer[i*2] | (buffer[i*2+1] << 8);
        outw(ATA_PRIMARY_IO_BASE, word);
    }
    return ata_wait_ready();
}

static void identify_disk(void) {
    outb(ATA_PRIMARY_IO_BASE + 6, 0xE0);
    outb(ATA_PRIMARY_IO_BASE + 2, 0);
    outb(ATA_PRIMARY_IO_BASE + 3, 0);
    outb(ATA_PRIMARY_IO_BASE + 4, 0);
    outb(ATA_PRIMARY_IO_BASE + 5, 0);
    outb(ATA_PRIMARY_IO_BASE + 7, ATA_CMD_IDENTIFY);
    if (ata_wait_ready() != 0) {
        print("No ATA disk.\n");
        return;
    }
    uint8_t buf[512];
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(ATA_PRIMARY_IO_BASE);
        buf[i*2] = word & 0xFF;
        buf[i*2+1] = (word >> 8) & 0xFF;
    }
    print("Disk model: ");
    for (int i = 27; i < 27+40; i+=2) {
        uint8_t c1 = buf[i+1];
        uint8_t c2 = buf[i];
        if (c1 >= 0x20) putchar(c1);
        if (c2 >= 0x20) putchar(c2);
    }
    print("\n");
}

void ata_init(void) {
    print("Initializing ATA...\n");
    identify_disk();
}