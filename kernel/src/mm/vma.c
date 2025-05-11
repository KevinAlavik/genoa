#include <mm/vma.h>
#include <lib/string.h>
#include <util/log.h>
#include <util/memory.h>
#include <sys/idt.h>

#define VMA_MIN_ADDRESS 0x100000

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
    (void)ctx;
    kpanic(NULL, "vma_destroy_context is unimplemented");
}

void *vma_alloc(vma_context_t *ctx, uint64_t pages, uint64_t flags)
{
    (void)ctx;
    (void)pages;
    (void)flags;
    kpanic(NULL, "vma_alloc is unimplemented");
    return NULL;
}

void vma_free(vma_context_t *ctx, void *ptr)
{
    (void)ctx;
    (void)ptr;
    kpanic(NULL, "vma_free is unimplemented");
}