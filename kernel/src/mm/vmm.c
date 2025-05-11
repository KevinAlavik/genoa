#include <mm/vmm.h>
#include <boot/boot.h>
#include <lib/string.h>
#include <mm/pmm.h>
#include <util/log.h>
#include <sys/cpu.h>
#include <util/memory.h>

uint64_t *kernel_pagemap;

/* External symbols, defined in linker script (hopefully) */
extern char __limine_requests_start[];
extern char __limine_requests_end[];
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];

/* debug */
#define PRINT_SECTION(name, start, end) info("section=%s, start=0x%.16llx, end=0x%.16llx, size=%d", name, start, end, end - start)

/* Helpers */
static inline uint64_t page_index(uint64_t virt, uint64_t shift)
{
    return (virt & (uint64_t)0x1ff << shift) >> shift;
}

static inline uint64_t *get_table(uint64_t *table, uint64_t index)
{
    return (uint64_t *)HIGHER_HALF(table[index] & PAGE_MASK);
}

static inline uint64_t *get_or_alloc_table(uint64_t *table, uint64_t index, uint64_t flags)
{
    if (!(table[index] & VMM_PRESENT))
    {
        uint64_t *pml = HIGHER_HALF(pmm_request_page());
        memset(pml, 0, PAGE_SIZE);
        table[index] = (uint64_t)PHYSICAL(pml) | 0b111;
    }
    table[index] |= flags & 0xFF;
    return (uint64_t *)HIGHER_HALF(table[index] & PAGE_MASK);
}

/* Translation */
uint64_t virt_to_phys(uint64_t *pagemap, uint64_t virt)
{
    uint64_t pml4_idx = page_index(virt, PML4_SHIFT);
    if (!(pagemap[pml4_idx] & VMM_PRESENT))
        return 0;

    uint64_t *pml3 = get_table(pagemap, pml4_idx);
    uint64_t pml3_idx = page_index(virt, PML3_SHIFT);
    if (!(pml3[pml3_idx] & VMM_PRESENT))
        return 0;

    uint64_t *pml2 = get_table(pml3, pml3_idx);
    uint64_t pml2_idx = page_index(virt, PML2_SHIFT);
    if (!(pml2[pml2_idx] & VMM_PRESENT))
        return 0;

    uint64_t *pml1 = get_table(pml2, pml2_idx);
    uint64_t pml1_idx = page_index(virt, PML1_SHIFT);
    if (!(pml1[pml1_idx] & VMM_PRESENT))
        return 0;

    return pml1[pml1_idx] & PAGE_MASK;
}

void vmm_map(uint64_t *pagemap, uint64_t virt, uint64_t phys, uint64_t flags)
{
    uint64_t pml4_idx = page_index(virt, PML4_SHIFT);
    uint64_t pml3_idx = page_index(virt, PML3_SHIFT);
    uint64_t pml2_idx = page_index(virt, PML2_SHIFT);
    uint64_t pml1_idx = page_index(virt, PML1_SHIFT);

    uint64_t *pml3 = get_or_alloc_table(pagemap, pml4_idx, flags);
    uint64_t *pml2 = get_or_alloc_table(pml3, pml3_idx, flags);
    uint64_t *pml1 = get_or_alloc_table(pml2, pml2_idx, flags);

    pml1[pml1_idx] = phys | flags;
}

void vmm_unmap(uint64_t *pagemap, uint64_t virt)
{
    uint64_t pml4_idx = page_index(virt, PML4_SHIFT);
    if (!(pagemap[pml4_idx] & VMM_PRESENT))
        return;

    uint64_t *pml3 = get_table(pagemap, pml4_idx);
    uint64_t pml3_idx = page_index(virt, PML3_SHIFT);
    if (!(pml3[pml3_idx] & VMM_PRESENT))
        return;

    uint64_t *pml2 = get_table(pml3, pml3_idx);
    uint64_t pml2_idx = page_index(virt, PML2_SHIFT);
    if (!(pml2[pml2_idx] & VMM_PRESENT))
        return;

    uint64_t *pml1 = get_table(pml2, pml2_idx);
    uint64_t pml1_idx = page_index(virt, PML1_SHIFT);

    pml1[pml1_idx] = 0;
    __asm__ volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

uint64_t *vmm_new_pagemap()
{
    uint64_t *pagemap = (uint64_t *)HIGHER_HALF(pmm_request_page());
    if (pagemap == NULL)
    {
        err("Failed to allocate page for new pagemap.");
        return NULL;
    }

    memset(pagemap, 0, PAGE_SIZE);

    if (kernel_pagemap)
    {
        memcpy(pagemap + 256, kernel_pagemap + 256, 256 * sizeof(uint64_t));
    }
    else
    {
        warn("Kernel pagemap is NULL. New pagemap will not inherit kernel mappings.");
    }

    info("Created new pagemap at 0x%.16llx, kernel space inherited: %s",
         (uint64_t)pagemap, kernel_pagemap ? "yes" : "no");

    return pagemap;
}

void vmm_destroy_pagemap(uint64_t *pagemap)
{
    if (pagemap == 0)
    {
        warn("Tried to destroy pagemap at address 0, skipping");
        return;
    }
    pmm_release_pages((void *)PHYSICAL(pagemap), 1);
    warn("Destroyed pagemap at 0x%.16llx", (uint64_t)pagemap);
}

void vmm_switch_pagemap(uint64_t *new_pagemap)
{
    __asm__ volatile("movq %0, %%cr3" ::"r"(PHYSICAL((uint64_t)new_pagemap)));
}

/* Initialization */
void vmm_init()
{
    kernel_pagemap = (uint64_t *)pmm_request_pages(1, true);
    if (kernel_pagemap == NULL)
    {
        info("error: Failed to allocate page for kernel pagemap, halting");
        hcf();
    }
    memset(kernel_pagemap, 0, PAGE_SIZE);

    uint64_t kvirt = kernel_address_request.response->virtual_base;
    uint64_t kphys = kernel_address_request.response->physical_base;

    PRINT_SECTION("text", __text_start, __text_end);
    PRINT_SECTION("rodata", __rodata_start, __rodata_end);
    PRINT_SECTION("data", __data_start, __data_end);

    kernel_stack_top = ALIGN_UP(kernel_stack_top, PAGE_SIZE);
    for (uint64_t stack = kernel_stack_top - (16 * 1024); stack < kernel_stack_top; stack += PAGE_SIZE)
    {
        vmm_map(kernel_pagemap, stack, (uint64_t)PHYSICAL(stack), VMM_PRESENT | VMM_WRITE | VMM_NX);
    }
    info("Mapped kernel stack");

    for (uint64_t reqs = ALIGN_DOWN(__limine_requests_start, PAGE_SIZE); reqs < ALIGN_UP(__limine_requests_end, PAGE_SIZE); reqs += PAGE_SIZE)
    {
        vmm_map(kernel_pagemap, reqs, reqs - kvirt + kphys, VMM_PRESENT | VMM_WRITE);
    }
    info("Mapped Limine Requests region.");

    for (uint64_t text = ALIGN_DOWN(__text_start, PAGE_SIZE); text < ALIGN_UP(__text_end, PAGE_SIZE); text += PAGE_SIZE)
    {
        vmm_map(kernel_pagemap, text, text - kvirt + kphys, VMM_PRESENT);
    }
    info("Mapped .text");

    for (uint64_t rodata = ALIGN_DOWN(__rodata_start, PAGE_SIZE); rodata < ALIGN_UP(__rodata_end, PAGE_SIZE); rodata += PAGE_SIZE)
    {
        vmm_map(kernel_pagemap, rodata, rodata - kvirt + kphys, VMM_PRESENT | VMM_NX);
    }
    info("Mapped .rodata");

    for (uint64_t data = ALIGN_DOWN(__data_start, PAGE_SIZE); data < ALIGN_UP(__data_end, PAGE_SIZE); data += PAGE_SIZE)
    {
        vmm_map(kernel_pagemap, data, data - kvirt + kphys, VMM_PRESENT | VMM_WRITE | VMM_NX);
    }
    info("Mapped .data");

    if (memmap_request.response)
    {
        struct limine_memmap_response *memmap = memmap_request.response;
        for (uint64_t i = 0; i < memmap->entry_count; i++)
        {
            struct limine_memmap_entry *entry = memmap->entries[i];
            uint64_t base = ALIGN_DOWN(entry->base, PAGE_SIZE);
            uint64_t end = ALIGN_UP(entry->base + entry->length, PAGE_SIZE);

            for (uint64_t addr = base; addr < end; addr += PAGE_SIZE)
            {
                vmm_map(kernel_pagemap, (uint64_t)HIGHER_HALF(addr), addr, VMM_PRESENT | VMM_WRITE | VMM_NX);
            }
            info("Mapped memory map entry %d: base=0x%.16llx, length=0x%.16llx, type=%d", i, entry->base, entry->length, entry->type);
        }
    }

    for (uint64_t gb4 = 0; gb4 < 0x100000000; gb4 += PAGE_SIZE)
    {
        vmm_map(kernel_pagemap, (uint64_t)HIGHER_HALF(gb4), gb4, VMM_PRESENT | VMM_WRITE);
    }
    info("Mapped HHDM");

    vmm_switch_pagemap(kernel_pagemap);
}