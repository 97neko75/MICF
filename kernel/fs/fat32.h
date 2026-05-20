#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

void fat32_init(void);
void fat32_ls(void);
int fat32_read_file(const char* path, char* buffer, size_t buf_size, size_t* out_size);
int fat32_write_file(const char* path, const char* data, size_t size);
int fat32_mkdir(const char* path);
int fat32_delete_file(const char* path);
int fat32_rmdir(const char* path);

#endif