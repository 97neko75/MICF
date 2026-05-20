#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

#define PANIC(msg) do { print("PANIC: " msg "\n"); for(;;); } while(0)

void print(const char* str);
void putchar(char c);
void print_int(int n);
void print_hex(uint32_t n);

int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, int n);
size_t strlen(const char* s);
void strcpy(char* dest, const char* src);

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

#endif