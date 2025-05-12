#define LOG_MODULE "vma"
#include <mm/vma.h>
#include <lib/string.h>
#include <util/log.h>
#include <util/memory.h>
#include <sys/idt.h>

#define VMA_MIN_ADDRESS 0x1000

vma_context_t *vma_create_context(uint64_t *pagemap)
{
    vma_context_t *ctx = (vma_context_t *)HIGHER_HALF(pmm_request_page());
    if (ctx == NULL)
    {
        err("Failed to allocate VMA context");
        return NULL;
    }
    memset(ctx, 0, sizeof(vma_context_t));

    ctx->root = (vma_region_t *)HIGHER_HALF(pmm_request_page());
    if (ctx->root == NULL)
    {
        err("Failed to allocate root region");
        pmm_release_pages((void *)PHYSICAL(ctx), 1);
        return NULL;
    }

    ctx->pagemap = pagemap;
    ctx->root->start = VMA_MIN_ADDRESS;
    ctx->root->pages = 0;
    return ctx;
}

void vma_destroy_context(vma_context_t *ctx)
{
    info("Destroying VMA context at 0x%.16llx", (uint64_t)ctx);
    if (ctx->root == NULL || ctx->pagemap == NULL)
    {
        err("Invalid context or root passed to vma_destroy_context");
        return;
    }

    vma_region_t *region = ctx->root;
    while (region != NULL)
    {
        info("Freeing region at 0x%.16llx", (uint64_t)region);
        vma_region_t *next = region->next;
        pmm_release_pages((void *)PHYSICAL(region), 1);
        region = next;
    }

    pmm_release_pages((void *)PHYSICAL(ctx), 1);
    info("Destroyed VMA context at 0x%.16llx", (uint64_t)ctx);
}

void *vma_alloc(vma_context_t *ctx, uint64_t pages, uint64_t flags)
{
    if (ctx == NULL || ctx->root == NULL || ctx->pagemap == NULL)
    {
        err("Invalid context or root passed to vma_alloc");
        return NULL;
    }

    vma_region_t *region = ctx->root;
    vma_region_t *new_region;
    vma_region_t *last_region = ctx->root;

    while (region != NULL)
    {
        if (region->next == NULL || region->start + region->pages < region->next->start)
        {
            new_region = (vma_region_t *)HIGHER_HALF(pmm_request_page());
            if (new_region == NULL)
            {
                err("Failed to allocate new VMA region");
                return NULL;
            }

            memset(new_region, 0, sizeof(vma_region_t));
            new_region->pages = pages;
            new_region->flags = flags;
            new_region->start = region->start + region->pages;
            new_region->next = region->next;
            new_region->prev = region;
            region->next = new_region;

            for (uint64_t i = 0; i < pages; i++)
            {
                uint64_t page = (uint64_t)pmm_request_page();
                if (page == 0)
                {
                    err("Failed to allocate physical memory for VMA region");
                    return NULL;
                }

                vmm_map(ctx->pagemap, new_region->start + i * PAGE_SIZE, page, new_region->flags);
            }

            return (void *)new_region->start;
        }
        region = region->next;
    }

    new_region = (vma_region_t *)HIGHER_HALF(pmm_request_page());
    if (new_region == NULL)
    {
        err("Failed to allocate new VMA region");
        return NULL;
    }

    memset(new_region, 0, sizeof(vma_region_t));

    last_region->next = new_region;
    new_region->prev = last_region;
    new_region->start = last_region->start + last_region->pages;
    new_region->pages = pages;
    new_region->flags = flags;
    new_region->next = NULL;

    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t page = (uint64_t)pmm_request_page();
        if (page == 0)
        {
            err("Failed to allocate physical memory for VMA region");
            return NULL;
        }

        vmm_map(ctx->pagemap, new_region->start + i * PAGE_SIZE, page, new_region->flags);
    }

    return (void *)new_region->start;
}

void vma_free(vma_context_t *ctx, void *ptr)
{
    if (ctx == NULL)
    {
        err("Invalid context passed to vma_free");
        return;
    }

    vma_region_t *region = ctx->root;
    while (region != NULL)
    {
        if (region->start == (uint64_t)ptr)
        {
            break;
        }
        region = region->next;
    }

    if (region == NULL)
    {
        err("Unable to find region to free at address 0x%.16llx", (uint64_t)ptr);
        return;
    }

    vma_region_t *prev = region->prev;
    vma_region_t *next = region->next;

    for (uint64_t i = 0; i < region->pages; i++)
    {
        uint64_t virt = region->start + i * PAGE_SIZE;
        uint64_t phys = virt_to_phys(kernel_pagemap, virt);

        if (phys != 0)
        {
            pmm_release_pages((void *)phys, 1);
            vmm_unmap(ctx->pagemap, virt);
        }
    }

    if (prev != NULL)
    {
        prev->next = next;
    }

    if (next != NULL)
    {
        next->prev = prev;
    }

    if (region == ctx->root)
    {
        ctx->root = next;
    }

    pmm_release_pages((void *)PHYSICAL(region), 1);
}