// arch/paging.h
#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024

// 页表条目标志位
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITE      (1 << 1)
#define PAGE_USER       (1 << 2)
#define PAGE_WRITETHROUGH (1 << 3)
#define PAGE_CACHE_DISABLE (1 << 4)
#define PAGE_ACCESSED   (1 << 5)
#define PAGE_DIRTY      (1 << 6)
#define PAGE_PAT        (1 << 7)
#define PAGE_GLOBAL     (1 << 8)

typedef uint32_t page_dir_entry_t;
typedef uint32_t page_table_entry_t;

// 页目录结构（在物理内存中）
extern page_dir_entry_t* kernel_page_dir;

void paging_init(void);
void paging_enable(void);
void page_map(page_dir_entry_t* page_dir, uint32_t vaddr, uint32_t paddr, uint32_t flags);
void page_unmap(page_dir_entry_t* page_dir, uint32_t vaddr);
uint32_t page_get_physical(page_dir_entry_t* page_dir, uint32_t vaddr);
void page_map_range(page_dir_entry_t* page_dir, uint32_t vaddr_start, uint32_t paddr_start, uint32_t size, uint32_t flags);

#endif