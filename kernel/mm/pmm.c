#include "pmm.h"
#include <common.h>

#define PAGE_SIZE 4096
static uint32_t* bitmap = NULL;
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;

static void bitmap_set(uint32_t page) {
    bitmap[page / 32] |= (1 << (page % 32));
}
static void bitmap_clear(uint32_t page) {
    bitmap[page / 32] &= ~(1 << (page % 32));
}
static int bitmap_test(uint32_t page) {
    return (bitmap[page / 32] >> (page % 32)) & 1;
}
static uint32_t bitmap_find_free(void) {
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) return i;
    }
    return (uint32_t)-1;
}

void pmm_init(uint32_t mem_lower, uint32_t mem_upper) {
    uint32_t total_mem_kb = mem_lower + mem_upper;
    total_pages = total_mem_kb * 1024 / PAGE_SIZE;
    uint32_t bitmap_size = (total_pages + 7) / 8;
    // 位图放在 16MB 处
    bitmap = (uint32_t*)0x01000000;
    for (uint32_t i = 0; i < bitmap_size / 4; i++) bitmap[i] = 0;
    // 标记前 16MB 已使用
    uint32_t kernel_pages = 16 * 1024 * 1024 / PAGE_SIZE;
    for (uint32_t i = 0; i < kernel_pages; i++) {
        bitmap_set(i);
        used_pages++;
    }
}

void* pmm_alloc_page(void) {
    uint32_t page = bitmap_find_free();
    if (page == (uint32_t)-1) return NULL;
    bitmap_set(page);
    used_pages++;
    return (void*)(page * PAGE_SIZE);
}

void pmm_free_page(void* page) {
    uint32_t page_num = (uint32_t)page / PAGE_SIZE;
    if (!bitmap_test(page_num)) return;
    bitmap_clear(page_num);
    used_pages--;
}

void pmm_dump_info(void) {
    print("Total pages: ");
    print_int(total_pages);
    print(" Used pages: ");
    print_int(used_pages);
    print("\n");
}