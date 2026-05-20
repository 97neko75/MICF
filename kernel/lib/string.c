#include <common.h>

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') break;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t i = 0;
    while (s[i]) i++;
    return i;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* a = s1;
    const unsigned char* b = s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}