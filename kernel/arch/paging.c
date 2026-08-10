// arch/paging.c
#include "paging.h"
#include "../mm/pmm.h"
#include <common.h>

page_dir_entry_t* kernel_page_dir = NULL;

// 分配一个页表（返回物理地址）
static uint32_t alloc_page_table() {
    void* page = pmm_alloc_page();
    if (page) {
        memset(page, 0, PAGE_SIZE);
        return (uint32_t)page;
    }
    return 0;
}

// 初始化分页
void paging_init() {
    print("Initializing paging...\n");
    
    // 1. 分配页目录（4KB对齐）
    kernel_page_dir = (page_dir_entry_t*)pmm_alloc_page();
    if (!kernel_page_dir) {
        print("PANIC: Failed to allocate page directory\n");
        for(;;);
    }
    memset(kernel_page_dir, 0, PAGE_SIZE);
    
    // 2. 映射内核代码/数据（前16MB，identity mapping + 高地址映射）
    //    identity mapping: 虚拟地址 = 物理地址 (0x00000000 - 0x01000000)
    //    高地址映射: 虚拟地址 0xC0000000 开始映射到物理地址 0x00000000
    for (uint32_t paddr = 0; paddr < 0x01000000; paddr += PAGE_SIZE) {
        // identity mapping (低地址)
        page_map(kernel_page_dir, paddr, paddr, PAGE_PRESENT | PAGE_WRITE);
        // 内核高地址映射 (0xC0000000 + paddr)
        page_map(kernel_page_dir, 0xC0000000 + paddr, paddr, PAGE_PRESENT | PAGE_WRITE);
    }
    
    print("Paging initialized\n");
}

// 启用分页（切换到 kernel_page_dir）
void paging_enable() {
    // 加载页目录
    __asm__ volatile ("mov %0, %%cr3" : : "r"(kernel_page_dir));
    
    // 启用分页
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // 设置 PG 位
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    
    print("Paging enabled\n");
}

// 在指定页目录中建立映射
void page_map(page_dir_entry_t* page_dir, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;
    
    page_dir_entry_t* pde = &page_dir[pd_index];
    
    // 如果页目录项不存在，分配一个页表
    if (!(*pde & PAGE_PRESENT)) {
        uint32_t pt_phys = alloc_page_table();
        if (!pt_phys) {
            print("PANIC: Failed to allocate page table\n");
            for(;;);
        }
        *pde = (pt_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_WRITE;
    }
    
    // 获取页表的虚拟地址
    uint32_t pt_vaddr = *pde & 0xFFFFF000;
    page_table_entry_t* page_table = (page_table_entry_t*)pt_vaddr;
    
    // 设置页表项
    page_table[pt_index] = (paddr & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
    
    // 更新 TLB（如果当前页目录是活动的）
    // 注意：这里假设我们还没启用分页，或者已经在使用该页目录
}

// 解除映射
void page_unmap(page_dir_entry_t* page_dir, uint32_t vaddr) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;
    
    page_dir_entry_t* pde = &page_dir[pd_index];
    if (!(*pde & PAGE_PRESENT)) return;
    
    uint32_t pt_vaddr = *pde & 0xFFFFF000;
    page_table_entry_t* page_table = (page_table_entry_t*)pt_vaddr;
    
    page_table[pt_index] = 0;
    
    // 如果整个页表为空，可以选择释放它（这里不实现，保持简单）
}

// 获取物理地址
uint32_t page_get_physical(page_dir_entry_t* page_dir, uint32_t vaddr) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;
    
    page_dir_entry_t* pde = &page_dir[pd_index];
    if (!(*pde & PAGE_PRESENT)) return 0;
    
    uint32_t pt_vaddr = *pde & 0xFFFFF000;
    page_table_entry_t* page_table = (page_table_entry_t*)pt_vaddr;
    
    return (page_table[pt_index] & 0xFFFFF000) | (vaddr & 0xFFF);
}

// 映射一段连续范围
void page_map_range(page_dir_entry_t* page_dir, uint32_t vaddr_start, uint32_t paddr_start, uint32_t size, uint32_t flags) {
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        page_map(page_dir, vaddr_start + offset, paddr_start + offset, flags);
    }
}