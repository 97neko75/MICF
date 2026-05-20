#ifndef PMM_H
#define PMM_H

#include <stdint.h>

void pmm_init(uint32_t mem_lower, uint32_t mem_upper);
void* pmm_alloc_page(void);
void pmm_free_page(void* page);
void pmm_dump_info(void);

#endif