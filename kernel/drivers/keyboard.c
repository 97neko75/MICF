#include "keyboard.h"
#include <common.h>

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

static int shift_pressed = 0;
static int caps_lock = 0;

// 扫描码到字符的映射表（索引 0~0x3A）
static const char normal_map[0x3A] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,   0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,   0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' '
};
static const char shift_map[0x3A] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' '
};

// 判断是否为字母键（扫描码）
static int is_letter_scancode(unsigned char sc) {
    static const unsigned char letters[] = {
        0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,
        0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1F,0x14,
        0x16,0x2F,0x11,0x2D,0x15,0x2C
    };
    for (int i = 0; i < 26; i++) {
        if (sc == letters[i]) return 1;
    }
    return 0;
}

char keyboard_getchar(void) {
    if ((inb(0x64) & 1) == 0) return 0;
    unsigned char sc = inb(0x60);
    int pressed = (sc & 0x80) == 0;
    unsigned char key = sc & 0x7F;

    if (key == 0x2A || key == 0x36) { shift_pressed = pressed; return 0; }
    if (key == 0x3A && pressed) { caps_lock = !caps_lock; return 0; }
    if (!pressed) return 0;
    if (key == 0xE0) {
        while ((inb(0x64) & 1) == 0);
        inb(0x60);
        return 0;
    }
    if (key >= 0x3A) return 0;

    char ascii;
    if (is_letter_scancode(key)) {
        // 字母：Shift 和 Caps 异或
        if (shift_pressed ^ caps_lock) {
            ascii = shift_map[key];
        } else {
            ascii = normal_map[key];
        }
    } else {
        // 符号：仅 Shift 有效
        if (shift_pressed) {
            ascii = shift_map[key];
        } else {
            ascii = normal_map[key];
        }
    }

    // 特殊处理回车和退格
    if (key == 0x1C) ascii = '\n';
    else if (key == 0x0E) ascii = '\b';

    return ascii;
}

void keyboard_init(void) {
    shift_pressed = 0;
    caps_lock = 0;
}
//我不好