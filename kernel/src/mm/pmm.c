#include <mm/pmm.h>
#include <boot/boot.h>
#include <util/log.h>
#include <sys/cpu.h>
#include <util/memory.h>
#include <lib/string.h>
#include <lib/bitmap.h>
#include <sys/spinlock.h>

#define PAGE_CACHE_SIZE 1024
#define MIN_ALIGN PAGE_SIZE
#define BITMAP_WORD_SIZE (sizeof(uint64_t) * 8)

struct limine_memmap_response *memmap;
uint64_t bitmap_pages;
uint64_t bitmap_size;
uint8_t *bitmap;
static uint64_t free_pages;
static spinlock_t pmm_lock;
static uint64_t page_cache[PAGE_CACHE_SIZE];
static size_t cache_size;
static size_t cache_index;

static inline bool is_aligned(void *addr, size_t align)
{
    return ((uintptr_t)addr % align) == 0;
}

void pmm_init(void)
{
    if (!memmap_request.response)
    {
        err("Failed to get memory map, halting");
        hcf();
    }
    memmap = memmap_request.response;
    spinlock_init(&pmm_lock);

    uint64_t high = 0;
    free_pages = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t top = e->base + e->length;
            if (top > high)
                high = top;
            free_pages += e->length / PAGE_SIZE;
            info("Usable memory region: 0x%.16llx -> 0x%.16llx", e->base, e->base + e->length);
        }
    }

    bitmap_pages = high / PAGE_SIZE;
    bitmap_size = ALIGN_UP(bitmap_pages / 8, PAGE_SIZE);

    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size)
        {
            bitmap = (uint8_t *)(e->base + hhdm_offset);
            memset(bitmap, 0xFF, bitmap_size);
            e->base += bitmap_size;
            e->length -= bitmap_size;
            free_pages -= bitmap_size / PAGE_SIZE;
            break;
        }
    }

    cache_size = PAGE_CACHE_SIZE;
    cache_index = 0;
    memset(page_cache, 0, sizeof(page_cache));

    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            for (uint64_t j = e->base; j < e->base + e->length; j += PAGE_SIZE)
            {
                if ((j / PAGE_SIZE) < bitmap_pages)
                {
                    bitmap_clear(bitmap, j / PAGE_SIZE);
                }
            }
        }
    }
}

void *pmm_request_pages(size_t pages, bool higher_half)
{
    if (pages == 0 || pages > free_pages)
        return NULL;

    spinlock_acquire(&pmm_lock);

    if (pages == 1 && cache_index > 0)
    {
        void *addr = (void *)(page_cache[--cache_index] * PAGE_SIZE);
        bitmap_set(bitmap, (uint64_t)addr / PAGE_SIZE);
        free_pages--;
        spinlock_release(&pmm_lock);
        return higher_half ? (void *)((uint64_t)addr + hhdm_offset) : addr;
    }

    uint64_t word_count = (bitmap_pages + BITMAP_WORD_SIZE - 1) / BITMAP_WORD_SIZE;
    uint64_t *bitmap_words = (uint64_t *)bitmap;

    for (uint64_t i = 0; i < word_count; i++)
    {
        if (bitmap_words[i] != UINT64_MAX)
        {
            uint64_t start_bit = i * BITMAP_WORD_SIZE;
            uint64_t consecutive = 0;

            for (uint64_t j = 0; j < BITMAP_WORD_SIZE && start_bit + j < bitmap_pages; j++)
            {
                if (!bitmap_get(bitmap, start_bit + j))
                {
                    if (++consecutive == pages)
                    {
                        for (uint64_t k = 0; k < pages; k++)
                        {
                            bitmap_set(bitmap, start_bit + j - pages + 1 + k);
                        }
                        free_pages -= pages;

                        void *addr = (void *)((start_bit + j - pages + 1) * PAGE_SIZE);
                        spinlock_release(&pmm_lock);
                        return higher_half ? (void *)((uint64_t)addr + hhdm_offset) : addr;
                    }
                }
                else
                {
                    consecutive = 0;
                }
            }
        }
    }

    spinlock_release(&pmm_lock);
    return NULL;
}

void pmm_release_pages(void *ptr, size_t pages)
{
    if (!ptr || !is_aligned(ptr, MIN_ALIGN))
        return;

    spinlock_acquire(&pmm_lock);

    uint64_t start = ((uint64_t)ptr - (hhdm_offset * ((uint64_t)ptr >= hhdm_offset))) / PAGE_SIZE;

    if (start + pages > bitmap_pages)
    {
        spinlock_release(&pmm_lock);
        return;
    }

    for (size_t i = 0; i < pages; i++)
    {
        if (bitmap_get(bitmap, start + i))
        {
            bitmap_clear(bitmap, start + i);
            free_pages++;

            if (pages == 1 && cache_index < cache_size)
            {
                page_cache[cache_index++] = start;
            }
        }
    }

    spinlock_release(&pmm_lock);
}

uint64_t pmm_get_free_pages(void)
{
    spinlock_acquire(&pmm_lock);
    uint64_t count = free_pages;
    spinlock_release(&pmm_lock);
    return count;
}