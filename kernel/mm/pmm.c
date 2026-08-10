#include "pmm.h"
#include <common.h>

#define PAGE_SIZE 4096

// 内核结束地址（由链接脚本定义）
extern uint32_t _kernel_end;

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

    // 位图放在内核结束之后的第一个页（动态分配，不再硬编码 16MB）
    uint32_t bitmap_start = (uint32_t)&_kernel_end;
    bitmap_start = (bitmap_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); // 页对齐
    bitmap = (uint32_t*)bitmap_start;

    uint32_t bitmap_size = (total_pages + 31) / 32;
    for (uint32_t i = 0; i < bitmap_size; i++) bitmap[i] = 0;

    // 标记内核占用的所有页为已使用（从 0 到 _kernel_end）
    uint32_t kernel_end = (uint32_t)&_kernel_end;
    uint32_t kernel_pages = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < kernel_pages; i++) {
        bitmap_set(i);
        used_pages++;
    }

    // 标记位图自身占用的页为已使用
    uint32_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < bitmap_pages; i++) {
        uint32_t page = (bitmap_start / PAGE_SIZE) + i;
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
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

// 分配连续的物理页
void* pmm_alloc_pages(uint32_t count) {
    uint32_t start_page = 0;
    uint32_t found = 0;
    
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (found == 0) start_page = i;
            found++;
            if (found == count) {
                for (uint32_t j = 0; j < count; j++) {
                    bitmap_set(start_page + j);
                    used_pages++;
                }
                return (void*)(start_page * PAGE_SIZE);
            }
        } else {
            found = 0;
        }
    }
    return NULL;
}

// 释放连续的物理页
void pmm_free_pages(void* addr, uint32_t count) {
    uint32_t page_num = (uint32_t)addr / PAGE_SIZE;
    for (uint32_t i = 0; i < count; i++) {
        if (bitmap_test(page_num + i)) {
            bitmap_clear(page_num + i);
            used_pages--;
        }
    }
}