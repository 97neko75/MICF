#include "fat32.h"
#include "../drivers/ata.h"
#include <common.h>

// 自定义 strchr（因为开发者没有标准库，绝对不是懒！）
static char* strchr(const char* s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return NULL;
}

#define SECTOR_SIZE 512
#define ATTR_DIRECTORY 0x10
#define ATTR_VOLUME_ID 0x08
#define ATTR_LONG_NAME 0x0F

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

static fat32_bpb_t bpb;
static uint32_t fat_start_lba;
static uint32_t data_start_lba;
static uint32_t root_cluster;

static int read_sector(uint32_t lba, void* buffer) {
    return ata_read_sector(lba, (uint8_t*)buffer);
}
static int write_sector(uint32_t lba, const void* buffer) {
    return ata_write_sector(lba, (const uint8_t*)buffer);
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
}

static uint32_t read_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + fat_offset / SECTOR_SIZE;
    uint32_t ent_offset = fat_offset % SECTOR_SIZE;
    uint8_t sector[SECTOR_SIZE];
    if (read_sector(fat_sector, sector) != 0) return 0x0FFFFFF8;
    return *(uint32_t*)(sector + ent_offset) & 0x0FFFFFFF;
}

static void write_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + fat_offset / SECTOR_SIZE;
    uint32_t ent_offset = fat_offset % SECTOR_SIZE;
    uint8_t sector[SECTOR_SIZE];
    read_sector(fat_sector, sector);
    *(uint32_t*)(sector + ent_offset) = value & 0x0FFFFFFF;
    write_sector(fat_sector, sector);
    // 更新第二个 FAT
    read_sector(fat_sector + bpb.sectors_per_fat_32, sector);
    *(uint32_t*)(sector + ent_offset) = value & 0x0FFFFFFF;
    write_sector(fat_sector + bpb.sectors_per_fat_32, sector);
}

static uint32_t find_free_cluster(void) {
    uint32_t fat_entries = bpb.sectors_per_fat_32 * SECTOR_SIZE / 4;
    for (uint32_t i = 2; i < fat_entries; i++) {
        if (read_fat_entry(i) == 0) return i;
    }
    return 0;
}

static uint32_t allocate_clusters(uint32_t count) {
    uint32_t first = 0, prev = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t cur = find_free_cluster();
        if (cur == 0) {
            uint32_t c = first;
            while (c) { uint32_t next = read_fat_entry(c); write_fat_entry(c, 0); c = next; }
            return 0;
        }
        write_fat_entry(cur, 0x0FFFFFF8);
        if (first == 0) first = cur;
        else write_fat_entry(prev, cur);
        prev = cur;
    }
    return first;
}

// 释放一个簇链（标记为空闲）
static void free_cluster_chain(uint32_t first_cluster) {
    uint32_t cur = first_cluster;
    while (cur < 0x0FFFFFF8) {
        uint32_t next = read_fat_entry(cur);
        write_fat_entry(cur, 0);
        cur = next;
    }
}

static int read_cluster(uint32_t cluster, uint8_t* buffer) {
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t i = 0; i < bpb.sectors_per_cluster; i++) {
        if (read_sector(lba + i, buffer + i * SECTOR_SIZE) != 0) return -1;
    }
    return 0;
}

static int write_cluster(uint32_t cluster, const uint8_t* buffer) {
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t i = 0; i < bpb.sectors_per_cluster; i++) {
        if (write_sector(lba + i, buffer + i * SECTOR_SIZE) != 0) return -1;
    }
    return 0;
}

static void make_short_name(const char* long_name, uint8_t* short_name) {
    int i, dot = -1;
    for (i = 0; long_name[i]; i++) if (long_name[i] == '.') { dot = i; break; }
    for (i = 0; i < 8; i++) {
        if (dot == -1 && long_name[i] == 0) break;
        if (dot != -1 && i >= dot) break;
        char c = long_name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        short_name[i] = c ? c : ' ';
    }
    while (i < 8) short_name[i++] = ' ';
    if (dot != -1) {
        int j = dot + 1;
        for (i = 0; i < 3; i++) {
            char c = long_name[j + i];
            if (c == 0) break;
            if (c >= 'a' && c <= 'z') c -= 32;
            short_name[8 + i] = c;
        }
        while (i < 3) short_name[8 + i++] = ' ';
    } else {
        for (i = 0; i < 3; i++) short_name[8 + i] = ' ';
    }
}

static int find_free_dir_entry(uint32_t dir_cluster, uint8_t* entry, uint32_t* out_cluster, uint32_t* out_offset) {
    uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    uint32_t cur = dir_cluster;
    while (cur < 0x0FFFFFF8) {
        if (read_cluster(cur, buf) != 0) return -1;
        for (uint32_t off = 0; off < bpb.sectors_per_cluster * SECTOR_SIZE; off += 32) {
            if (buf[off] == 0x00 || buf[off] == 0xE5) {
                memcpy(entry, buf + off, 32);
                *out_cluster = cur;
                *out_offset = off;
                return 0;
            }
        }
        cur = read_fat_entry(cur);
    }
    return -1;
}

static void write_dir_entry(uint32_t dir_cluster, uint32_t offset, const uint8_t* entry) {
    uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    read_cluster(dir_cluster, buf);
    memcpy(buf + offset, entry, 32);
    write_cluster(dir_cluster, buf);
}

// 在指定目录中查找文件或目录项，返回其所在簇、偏移和属性
static int find_entry(uint32_t parent_cluster, const uint8_t* short_name, uint32_t* out_cluster, uint32_t* out_offset, uint8_t* out_attr) {
    uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    uint32_t cur = parent_cluster;
    while (cur < 0x0FFFFFF8) {
        if (read_cluster(cur, buf) != 0) return -1;
        for (uint32_t off = 0; off < bpb.sectors_per_cluster * SECTOR_SIZE; off += 32) {
            uint8_t* entry = buf + off;
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;
            if (entry[11] & ATTR_VOLUME_ID) continue;
            if (entry[11] & ATTR_LONG_NAME) continue;
            if (memcmp(entry, short_name, 11) == 0) {
                *out_cluster = cur;
                *out_offset = off;
                if (out_attr) *out_attr = entry[11];
                return 0;
            }
        }
        cur = read_fat_entry(cur);
    }
    return -1;
}

int fat32_write_file(const char* path, const char* data, size_t size) {
    uint8_t short_name[11];
    make_short_name(path, short_name);
    uint8_t entry[32];
    uint32_t dir_cluster = root_cluster, offset;
    if (find_free_dir_entry(dir_cluster, entry, &dir_cluster, &offset) != 0) {
        print("No free directory entry\n");
        return -1;
    }
    uint32_t clusters = (size + bpb.sectors_per_cluster * SECTOR_SIZE - 1) / (bpb.sectors_per_cluster * SECTOR_SIZE);
    uint32_t first = allocate_clusters(clusters);
    if (first == 0) {
        print("No free clusters\n");
        return -1;
    }
    uint32_t cur = first;
    size_t remaining = size;
    size_t pos = 0;
    while (remaining > 0) {
        uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
        uint32_t copy = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        memcpy(buf, data + pos, copy);
        if (copy < sizeof(buf)) memset(buf + copy, 0, sizeof(buf) - copy);
        if (write_cluster(cur, buf) != 0) {
            free_cluster_chain(first);
            return -1;
        }
        remaining -= copy;
        pos += copy;
        cur = read_fat_entry(cur);
        if (remaining > 0 && cur == 0x0FFFFFF8) { print("Cluster chain exhausted\n"); return -1; }
    }
    memset(entry, 0, 32);
    memcpy(entry, short_name, 11);
    entry[11] = 0x20;
    *(uint16_t*)(entry + 0x1A) = first & 0xFFFF;
    *(uint16_t*)(entry + 0x14) = (first >> 16) & 0xFFFF;
    *(uint32_t*)(entry + 0x1C) = size;
    write_dir_entry(dir_cluster, offset, entry);
    return 0;
}

int fat32_read_file(const char* path, char* buffer, size_t buf_size, size_t* out_size) {
    uint8_t short_name[11];
    make_short_name(path, short_name);
    uint32_t parent_cluster = root_cluster;
    uint32_t entry_cluster, offset;
    uint8_t attr;
    if (find_entry(parent_cluster, short_name, &entry_cluster, &offset, &attr) != 0) return -1;
    if (attr & ATTR_DIRECTORY) return -1;
    uint8_t cluster_data[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    // 重新读取目录项以获取第一簇和大小
    uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    if (read_cluster(entry_cluster, buf) != 0) return -1;
    uint8_t* entry = buf + offset;
    uint32_t first_cluster = *(uint16_t*)(entry + 0x1A) | (*(uint16_t*)(entry + 0x14) << 16);
    uint32_t file_size = *(uint32_t*)(entry + 0x1C);
    if (file_size > buf_size) file_size = buf_size;
    *out_size = 0;
    uint32_t cl = first_cluster;
    while (cl < 0x0FFFFFF8 && *out_size < file_size) {
        if (read_cluster(cl, cluster_data) != 0) return -1;
        uint32_t copy = file_size - *out_size;
        if (copy > bpb.sectors_per_cluster * SECTOR_SIZE) copy = bpb.sectors_per_cluster * SECTOR_SIZE;
        memcpy(buffer + *out_size, cluster_data, copy);
        *out_size += copy;
        cl = read_fat_entry(cl);
    }
    return 0;
}

int fat32_delete_file(const char* path) {
    uint8_t short_name[11];
    make_short_name(path, short_name);
    uint32_t parent_cluster = root_cluster;
    uint32_t entry_cluster, offset;
    uint8_t attr;
    if (find_entry(parent_cluster, short_name, &entry_cluster, &offset, &attr) != 0) return -1;
    if (attr & ATTR_DIRECTORY) return -1; // 是目录，用 rmdir
    // 读取目录项获取第一簇
    uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    if (read_cluster(entry_cluster, buf) != 0) return -1;
    uint8_t* entry = buf + offset;
    uint32_t first_cluster = *(uint16_t*)(entry + 0x1A) | (*(uint16_t*)(entry + 0x14) << 16);
    // 释放数据簇
    if (first_cluster >= 2) free_cluster_chain(first_cluster);
    // 标记目录项为已删除（第一个字节设为 0xE5）
    entry[0] = 0xE5;
    write_dir_entry(entry_cluster, offset, entry);
    return 0;
}

int fat32_mkdir(const char* path) {
    if (strchr(path, '/') != NULL || strchr(path, '.') != NULL) {
        print("mkdir: only simple name without dot and slash\n");
        return -1;
    }
    uint8_t short_name[11];
    for (int i = 0; i < 8; i++) {
        if (path[i] == 0) break;
        char c = path[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        short_name[i] = c;
    }
    for (int i = 0; i < 8; i++) if (short_name[i] == 0) short_name[i] = ' ';
    for (int i = 0; i < 3; i++) short_name[8+i] = ' ';

    uint8_t entry[32];
    uint32_t dir_cluster, offset;
    if (find_free_dir_entry(root_cluster, entry, &dir_cluster, &offset) != 0) {
        print("mkdir: no free directory entry\n");
        return -1;
    }

    uint32_t cluster = allocate_clusters(1);
    if (cluster == 0) {
        print("mkdir: no free cluster\n");
        return -1;
    }

    uint8_t cluster_buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    memset(cluster_buf, 0, sizeof(cluster_buf));

    // "." entry
    uint8_t dot_entry[32];
    memset(dot_entry, 0, 32);
    dot_entry[0] = '.';
    for (int i = 1; i < 8; i++) dot_entry[i] = ' ';
    dot_entry[11] = ATTR_DIRECTORY;
    *(uint16_t*)(dot_entry + 0x1A) = cluster & 0xFFFF;
    *(uint16_t*)(dot_entry + 0x14) = (cluster >> 16) & 0xFFFF;
    memcpy(cluster_buf, dot_entry, 32);

    // ".." entry
    uint8_t dotdot_entry[32];
    memset(dotdot_entry, 0, 32);
    dotdot_entry[0] = '.';
    dotdot_entry[1] = '.';
    for (int i = 2; i < 8; i++) dotdot_entry[i] = ' ';
    dotdot_entry[11] = ATTR_DIRECTORY;
    *(uint16_t*)(dotdot_entry + 0x1A) = root_cluster & 0xFFFF;
    *(uint16_t*)(dotdot_entry + 0x14) = (root_cluster >> 16) & 0xFFFF;
    memcpy(cluster_buf + 32, dotdot_entry, 32);

    if (write_cluster(cluster, cluster_buf) != 0) {
        free_cluster_chain(cluster);
        print("mkdir: failed to write directory data\n");
        return -1;
    }

    memset(entry, 0, 32);
    memcpy(entry, short_name, 11);
    entry[11] = ATTR_DIRECTORY;
    *(uint16_t*)(entry + 0x1A) = cluster & 0xFFFF;
    *(uint16_t*)(entry + 0x14) = (cluster >> 16) & 0xFFFF;
    *(uint32_t*)(entry + 0x1C) = 0;
    write_dir_entry(dir_cluster, offset, entry);
    return 0;
}

int fat32_rmdir(const char* path) {
    uint8_t short_name[11];
    make_short_name(path, short_name);
    uint32_t parent_cluster = root_cluster;
    uint32_t entry_cluster, offset;
    uint8_t attr;
    if (find_entry(parent_cluster, short_name, &entry_cluster, &offset, &attr) != 0) return -1;
    if (!(attr & ATTR_DIRECTORY)) return -1; // 不是目录
    // 读取目录项获取第一簇
    uint8_t buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    if (read_cluster(entry_cluster, buf) != 0) return -1;
    uint8_t* entry = buf + offset;
    uint32_t dir_cluster = *(uint16_t*)(entry + 0x1A) | (*(uint16_t*)(entry + 0x14) << 16);
    // 检查目录是否为空（除了 "." 和 ".." 之外没有其他条目）
    uint8_t dir_buf[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    if (read_cluster(dir_cluster, dir_buf) != 0) return -1;
    for (uint32_t i = 64; i < bpb.sectors_per_cluster * SECTOR_SIZE; i += 32) { // 跳过前两个条目
        uint8_t* e = dir_buf + i;
        if (e[0] == 0x00) break;
        if (e[0] != 0xE5 && !(e[11] & ATTR_VOLUME_ID) && !(e[11] & ATTR_LONG_NAME)) {
            print("Directory not empty\n");
            return -1;
        }
    }
    // 释放目录的簇
    if (dir_cluster >= 2) free_cluster_chain(dir_cluster);
    // 标记目录项为已删除
    entry[0] = 0xE5;
    write_dir_entry(entry_cluster, offset, entry);
    return 0;
}

void fat32_init(void) {
    print("Reading FAT32 boot sector...\n");
    uint8_t boot[SECTOR_SIZE];
    if (read_sector(0, boot) != 0) { print("Failed to read boot sector.\n"); return; }
    bpb = *(fat32_bpb_t*)boot;
    if (bpb.bytes_per_sector != SECTOR_SIZE) { print("Unsupported sector size.\n"); return; }
    fat_start_lba = bpb.reserved_sectors;
    data_start_lba = fat_start_lba + bpb.num_fats * bpb.sectors_per_fat_32;
    root_cluster = bpb.root_cluster;
    print("FAT32 initialized: sectors/cluster="); print_int(bpb.sectors_per_cluster);
    print(", root cluster="); print_hex(root_cluster); print("\n");
}

void fat32_ls(void) {
    print("Directory listing:\n");
    uint8_t cluster_data[ bpb.sectors_per_cluster * SECTOR_SIZE ];
    uint32_t cur_cluster = root_cluster;
    while (cur_cluster < 0x0FFFFFF8) {
        if (read_cluster(cur_cluster, cluster_data) != 0) break;
        for (uint32_t i = 0; i < bpb.sectors_per_cluster * SECTOR_SIZE; i += 32) {
            uint8_t* entry = cluster_data + i;
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;
            if (entry[11] & ATTR_VOLUME_ID) continue;
            if (entry[11] & ATTR_LONG_NAME) continue;
            for (int j = 0; j < 8; j++) if (entry[j] != ' ') putchar(entry[j]);
            if (entry[8] != ' ') { putchar('.'); for (int j=0;j<3;j++) putchar(entry[8+j]); }
            if (entry[11] & ATTR_DIRECTORY) print("/\n");
            else { print(" "); print_int(*(uint32_t*)(entry+28)); print(" bytes\n"); }
        }
        cur_cluster = read_fat_entry(cur_cluster);
    }
}
//你好