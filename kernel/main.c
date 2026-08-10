#include <common.h>
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/paging.h"
#include "mm/pmm.h"
#include "task/process.h"
#include "ipc/ipc.h"
#include "drivers/keyboard.h"
#include "drivers/ata.h"
#include "fs/fat32.h"

#define MAX_CMD_LEN 64

static void readline(char* buffer, int max_len) {
    int idx = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == 0) continue;
        if (c == '\n') { buffer[idx] = '\0'; putchar('\n'); return; }
        else if (c == '\b') { if (idx > 0) { idx--; putchar('\b'); } }
        else if (idx < max_len - 1) { buffer[idx++] = c; putchar(c); }
    }
}

static void reboot(void) {
    print("Rebooting...\n");
    for (volatile int i = 0; i < 100000; i++);
    __asm__ volatile (
        "mov $0x64, %%dx\n"
        "mov $0xFE, %%al\n"
        "out %%al, %%dx\n"
        : : : "eax", "edx"
    );
    while (1);
}

static int is_echo(const char* cmd) {
    const char* prefix = "echo ";
    for (int i = 0; i < 5; i++) if (cmd[i] != prefix[i]) return 0;
    return 1;
}

static int is_cat(const char* cmd) {
    const char* prefix = "cat ";
    for (int i = 0; i < 4; i++) if (cmd[i] != prefix[i]) return 0;
    return 1;
}

static int is_write(const char* cmd) {
    const char* prefix = "write ";
    for (int i = 0; i < 6; i++) if (cmd[i] != prefix[i]) return 0;
    return 1;
}

static int is_mkdir(const char* cmd) {
    const char* prefix = "mkdir ";
    for (int i = 0; i < 6; i++) if (cmd[i] != prefix[i]) return 0;
    return 1;
}

static int is_del(const char* cmd) {
    const char* prefix = "del ";
    for (int i = 0; i < 4; i++) if (cmd[i] != prefix[i]) return 0;
    return 1;
}

static int is_rmdir(const char* cmd) {
    const char* prefix = "rmdir ";
    for (int i = 0; i < 6; i++) if (cmd[i] != prefix[i]) return 0;
    return 1;
}

static void process_command(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (strcmp(cmd, "help") == 0) {
        print("Available commands:\n");
        print("  help       - Show this help\n");
        print("  echo <text> - Print <text>\n");
        print("  clear      - Clear screen\n");
        print("  reboot     - Restart system\n");
        print("  threads    - List running threads\n");
        print("  ls         - List root directory\n");
        print("  cat <file> - Show file content\n");
        print("  write <file> <text> - Create file with content\n");
        print("  mkdir <dir> - Create directory\n");
        print("  del <file>  - Delete file\n");
        print("  rmdir <dir> - Delete empty directory\n");
        print("  fatinfo    - Show FAT32 info\n");
        return;
    }
    if (strcmp(cmd, "clear") == 0) {
        uint16_t* video = (uint16_t*)0xB8000;
        for (int i = 0; i < 80*25; i++) video[i] = (uint16_t)' ' | (0x0F << 8);
        __asm__ volatile (
            "mov $0x3D4, %%dx\n mov $0x0F, %%al\n out %%al, %%dx\n"
            "mov $0x3D5, %%dx\n mov $0, %%al\n out %%al, %%dx\n"
            "mov $0x3D4, %%dx\n mov $0x0E, %%al\n out %%al, %%dx\n"
            "mov $0x3D5, %%dx\n mov $0, %%al\n out %%al, %%dx\n"
            : : : "eax", "edx"
        );
        print("Screen cleared.\n");
        return;
    }
    if (is_echo(cmd)) { print(cmd + 5); putchar('\n'); return; }
    if (is_cat(cmd)) {
        const char* file = cmd + 4;
        while (*file == ' ') file++;
        char buffer[2048];
        size_t sz;
        if (fat32_read_file(file, buffer, sizeof(buffer)-1, &sz) == 0) { buffer[sz] = '\0'; print(buffer); }
        else print("File not found.\n");
        return;
    }
    if (is_write(cmd)) {
        const char* rest = cmd + 6;
        while (*rest == ' ') rest++;
        const char* file = rest;
        while (*rest && *rest != ' ') rest++;
        if (*rest == 0) { print("Usage: write <file> <text>\n"); return; }
        char file_path[64];
        int i = 0;
        while (file < rest && i < 63) file_path[i++] = *file++;
        file_path[i] = '\0';
        while (*rest == ' ') rest++;
        const char* content = rest;
        size_t len = 0;
        while (content[len]) len++;
        if (fat32_write_file(file_path, content, len) == 0) print("File written.\n");
        else print("Write failed.\n");
        return;
    }
    if (is_mkdir(cmd)) {
        const char* dir = cmd + 6;
        while (*dir == ' ') dir++;
        if (fat32_mkdir(dir) == 0) print("Directory created.\n");
        else print("Failed to create directory.\n");
        return;
    }
    if (is_del(cmd)) {
        const char* file = cmd + 4;
        while (*file == ' ') file++;
        if (fat32_delete_file(file) == 0) print("File deleted.\n");
        else print("Delete failed.\n");
        return;
    }
    if (is_rmdir(cmd)) {
        const char* dir = cmd + 6;
        while (*dir == ' ') dir++;
        if (fat32_rmdir(dir) == 0) print("Directory removed.\n");
        else print("Remove failed (not empty or not a directory).\n");
        return;
    }
    if (strcmp(cmd, "reboot") == 0) { reboot(); return; }
    if (strcmp(cmd, "threads") == 0) {
        extern int proc_count, current_pid;
        print("Threads count: "); print_int(proc_count); print(", current PID: "); print_int(current_pid); print("\n");
        return;
    }
    if (strcmp(cmd, "fatinfo") == 0) { fat32_init(); return; }
    if (strcmp(cmd, "ls") == 0) { fat32_ls(); return; }
    print("Unknown command: "); print(cmd); print("\nType 'help'.\n");
}

void kernel_main(void) {
    uint16_t* video = (uint16_t*)0xB8000;
    for (int i = 0; i < 80*25; i++) video[i] = (uint16_t)' ' | (0x0F << 8);
    __asm__ volatile (
        "mov $0x3D4, %%dx\n mov $0x0F, %%al\n out %%al, %%dx\n"
        "mov $0x3D5, %%dx\n mov $0, %%al\n out %%al, %%dx\n"
        "mov $0x3D4, %%dx\n mov $0x0E, %%al\n out %%al, %%dx\n"
        "mov $0x3D5, %%dx\n mov $0, %%al\n out %%al, %%dx\n"
        : : : "eax", "edx"
    );
    pmm_init(640, 1024 * 16);
    paging_init();
    paging_enable();
    print("MICF Kernel v0.6.0 (with delete and rmdir)\n");
    print("Type 'help' for commands.\n");
    keyboard_init();
    ata_init();
    fat32_init();
    char cmdline[MAX_CMD_LEN];
    while (1) { print("> "); readline(cmdline, MAX_CMD_LEN); process_command(cmdline); }
}