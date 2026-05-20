#include <common.h>

static uint16_t* video = (uint16_t*)0xB8000;

static void set_cursor(int row, int col) {
    int pos = row * 80 + col;
    __asm__ volatile (
        "mov $0x3D4, %%dx\n"
        "mov $0x0F, %%al\n"
        "out %%al, %%dx\n"
        "mov $0x3D5, %%dx\n"
        "mov %b0, %%al\n"
        "out %%al, %%dx\n"
        "mov $0x3D4, %%dx\n"
        "mov $0x0E, %%al\n"
        "out %%al, %%dx\n"
        "mov $0x3D5, %%dx\n"
        "shr $8, %0\n"
        "mov %b0, %%al\n"
        "out %%al, %%dx\n"
        : : "r"(pos) : "eax", "edx"
    );
}

void putchar(char c) {
    static int cursor_x = 0, cursor_y = 0;
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= 25) {
            for (int row = 1; row < 25; row++)
                for (int col = 0; col < 80; col++)
                    video[(row-1)*80+col] = video[row*80+col];
            for (int col = 0; col < 80; col++)
                video[24*80+col] = (uint16_t)' ' | (0x0F << 8);
            cursor_y = 24;
        }
        set_cursor(cursor_y, cursor_x);
        return;
    }
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = 79;
        } else {
            return;
        }
        video[cursor_y*80 + cursor_x] = (uint16_t)' ' | (0x0F << 8);
        set_cursor(cursor_y, cursor_x);
        return;
    }
    if (c == '\t') {
        for (int i = 0; i < 4; i++) putchar(' ');
        return;
    }
    video[cursor_y*80 + cursor_x] = (uint16_t)c | (0x0F << 8);
    cursor_x++;
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= 25) {
            for (int row = 1; row < 25; row++)
                for (int col = 0; col < 80; col++)
                    video[(row-1)*80+col] = video[row*80+col];
            for (int col = 0; col < 80; col++)
                video[24*80+col] = (uint16_t)' ' | (0x0F << 8);
            cursor_y = 24;
        }
    }
    set_cursor(cursor_y, cursor_x);
}

void print(const char* str) {
    while (*str) putchar(*str++);
}

void print_int(int n) {
    char buf[12];
    int i = 0;
    if (n == 0) putchar('0');
    else {
        if (n < 0) { putchar('-'); n = -n; }
        while (n) { buf[i++] = '0' + (n % 10); n /= 10; }
        while (i--) putchar(buf[i]);
    }
}

void print_hex(uint32_t n) {
    const char* hex = "0123456789ABCDEF";
    putchar('0');
    putchar('x');
    for (int i = 7; i >= 0; i--) putchar(hex[(n >> (i*4)) & 0xF]);
}