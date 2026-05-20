#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

// 文件类型
enum file_type { FT_DIR, FT_FILE };

// 目录项结构
struct dirent {
    char name[32];
    enum file_type type;
    size_t size;
};

// 文件系统操作接口
int fs_ls(void (*callback)(const char* name, enum file_type type, size_t size));
int fs_read_file(const char* path, char* buffer, size_t buf_size, size_t* out_size);

#endif