#include <mm/vma.h>
#include <lib/string.h>
#include <util/log.h>
#include <util/memory.h>

#define VMA_MIN_ADDRESS 0x1000

vma_context_t *vma_create_context(uint64_t *pagemap)
{
    if (pagemap == NULL)
    {
        err("Null pagemap provided to vma_create_context");
        return NULL;
    }

    info("Creating VMA context with pagemap: 0x%.16llx", (uint64_t)pagemap);
    vma_context_t *ctx = (vma_context_t *)HIGHER_HALF(pmm_request_page());
    if (ctx == NULL)
    {
        err("Failed to allocate VMA context");
        return NULL;
    }
    memset(ctx, 0, sizeof(vma_context_t));

    ctx->pagemap = pagemap;
    ctx->root = NULL;
    return ctx;
}

void vma_destroy_context(vma_context_t *ctx)
{
    if (ctx == NULL)
    {
        err("Invalid context passed to vma_destroy_context");
        return;
    }

    info("Destroying VMA context at 0x%.16llx", (uint64_t)ctx);

    vma_region_t *region = ctx->root;
    while (region != NULL)
    {
        vma_region_t *next = region->next;
        uint64_t page_count = ALIGN_UP(region->size, PAGE_SIZE) / PAGE_SIZE;
        for (uint64_t i = 0; i < page_count; i++)
        {
            uint64_t virt = region->start + (i * PAGE_SIZE);
            uint64_t phys = virt_to_phys(ctx->pagemap, virt);
            if (phys != 0)
            {
                vmm_unmap(ctx->pagemap, virt);
                pmm_release_pages((void *)phys, 1);
            }
        }
        pmm_release_pages((void *)PHYSICAL(region), 1);
        region = next;
    }

    pmm_release_pages((void *)PHYSICAL(ctx), 1);
    info("Destroyed VMA context at 0x%.16llx", (uint64_t)ctx);
}

void *vma_alloc(vma_context_t *ctx, uint64_t size, uint64_t flags)
{
    if (ctx == NULL || ctx->pagemap == NULL || size == 0)
    {
        err("Invalid parameters passed to vma_alloc");
        return NULL;
    }

    size = ALIGN_UP(size, PAGE_SIZE);

    uint64_t start_addr = VMA_MIN_ADDRESS;
    vma_region_t *region = ctx->root;
    vma_region_t *prev = NULL;

    while (region != NULL)
    {
        uint64_t gap_start = prev ? prev->start + prev->size : start_addr;
        uint64_t gap_end = region->start;

        if (gap_end > gap_start && gap_end - gap_start >= size)
        {
            start_addr = gap_start;
            break;
        }
        prev = region;
        region = region->next;
    }

    if (region == NULL && prev)
    {
        start_addr = prev->start + prev->size;
    }
    else if (region == NULL)
    {
        start_addr = VMA_MIN_ADDRESS;
    }

    if (start_addr < VMA_MIN_ADDRESS)
    {
        err("Computed start address 0x%.16llx is below minimum 0x%.16llx", start_addr, VMA_MIN_ADDRESS);
        return NULL;
    }

    vma_region_t *new_region = (vma_region_t *)HIGHER_HALF(pmm_request_page());
    if (new_region == NULL)
    {
        err("Failed to allocate new VMA region");
        return NULL;
    }
    memset(new_region, 0, sizeof(vma_region_t));

    uint64_t page_count = size / PAGE_SIZE;
    for (uint64_t i = 0; i < page_count; i++)
    {
        uint64_t phys = (uint64_t)pmm_request_page();
        if (phys == 0)
        {
            err("Failed to allocate physical memory for VMA region at page %llu", i);
            for (uint64_t j = 0; j < i; j++)
            {
                uint64_t virt = start_addr + (j * PAGE_SIZE);
                phys = virt_to_phys(ctx->pagemap, virt);
                vmm_unmap(ctx->pagemap, virt);
                pmm_release_pages((void *)phys, 1);
            }
            pmm_release_pages((void *)PHYSICAL(new_region), 1);
            return NULL;
        }
        vmm_map(ctx->pagemap, start_addr + (i * PAGE_SIZE), phys, flags);
    }

    new_region->start = start_addr;
    new_region->size = size;
    new_region->flags = flags;
    new_region->prev = prev;
    new_region->next = region;

    if (prev)
    {
        prev->next = new_region;
    }
    if (region)
    {
        region->prev = new_region;
    }
    if (!ctx->root || start_addr < ctx->root->start)
    {
        ctx->root = new_region;
    }

    info("Allocated VMA region: start=0x%.16llx, size=0x%.16llx, flags=0x%.16llx",
         start_addr, size, flags);
    return (void *)start_addr;
}

void vma_free(vma_context_t *ctx, void *ptr)
{
    if (ctx == NULL || ptr == NULL)
    {
        err("Invalid parameters passed to vma_free");
        return;
    }

    uint64_t addr = (uint64_t)ptr;
    if (addr < VMA_MIN_ADDRESS)
    {
        err("Attempt to free invalid address 0x%.16llx below minimum 0x%.16llx", addr, VMA_MIN_ADDRESS);
        return;
    }

    vma_region_t *region = ctx->root;
    while (region != NULL)
    {
        if (region->start == addr)
        {
            break;
        }
        region = region->next;
    }

    if (region == NULL)
    {
        err("Unable to find region to free at address 0x%.16llx", addr);
        return;
    }

    uint64_t page_count = ALIGN_UP(region->size, PAGE_SIZE) / PAGE_SIZE;
    for (uint64_t i = 0; i < page_count; i++)
    {
        uint64_t virt = region->start + (i * PAGE_SIZE);
        uint64_t phys = virt_to_phys(ctx->pagemap, virt);
        if (phys != 0)
        {
            vmm_unmap(ctx->pagemap, virt);
            pmm_release_pages((void *)phys, 1);
        }
    }

    if (region->prev)
    {
        region->prev->next = region->next;
    }
    if (region->next)
    {
        region->next->prev = region->prev;
    }
    if (ctx->root == region)
    {
        ctx->root = region->next;
    }

    pmm_release_pages((void *)PHYSICAL(region), 1);
    info("Freed VMA region at 0x%.16llx", addr);
}

void vma_dump_context(vma_context_t *ctx)
{
    if (ctx == NULL)
    {
        err("Invalid context passed to vma_dump_context");
        return;
    }

    info("VMA Context Dump (pagemap: 0x%.16llx)", (uint64_t)ctx->pagemap);
    if (ctx->root == NULL)
    {
        info("  No regions allocated");
        return;
    }

    vma_region_t *region = ctx->root;
    int index = 0;
    while (region != NULL)
    {
        info("  Region %d: start=0x%.16llx, size=0x%.16llx, flags=0x%.16llx",
             index, region->start, region->size, region->flags);
        region = region->next;
        index++;
    }
}