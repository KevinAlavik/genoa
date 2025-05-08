#ifndef VMM_H
#define VMM_H

#include <lib/types.h>
#include <util/memory.h>

#define VMM_PRESENT BIT(0)
#define VMM_WRITE BIT(1)
#define VMM_USER BIT(2)
#define VMM_NX BIT(63)

#define PAGE_MASK 0x000FFFFFFFFFF000ULL
#define PAGE_INDEX_MASK 0x1FF

#define PML1_SHIFT 12
#define PML2_SHIFT 21
#define PML3_SHIFT 30
#define PML4_SHIFT 39

extern uint64_t *kernel_pagemap;

void vmm_init();
void vmm_switch_pagemap(uint64_t *pagemap);
uint64_t *vmm_new_pagemap();
void vmm_map(uint64_t *pagemap, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_destroy_pagemap(uint64_t *pagemap);

#endif // VMM_H