#include "fs.h"
#include <common.h>

// 模拟文件节点
struct memfile {
    const char* name;
    enum file_type type;
    const char* content;
    size_t size;
};

// 预置文件列表
static struct memfile files[] = {
    { "/hello.txt", FT_FILE, "Hello from MICF virtual file system!\n", 34 },
    { "/README",    FT_FILE, "This is a demo file system.\nYou can add more files by editing memfs.c.\n", 60 },
    { "/",          FT_DIR,  NULL, 0 }
};

int fs_ls(void (*callback)(const char* name, enum file_type type, size_t size)) {
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        if (files[i].type == FT_DIR) continue; // skip root
        callback(files[i].name, files[i].type, files[i].size);
    }
    return 0;
}

int fs_read_file(const char* path, char* buffer, size_t buf_size, size_t* out_size) {
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        if (files[i].type == FT_FILE && strcmp(files[i].name, path) == 0) {
            size_t len = files[i].size;
            if (len > buf_size) len = buf_size;
            for (size_t j = 0; j < len; j++) buffer[j] = files[i].content[j];
            *out_size = len;
            return 0;
        }
    }
    return -1; // 文件未找到
}