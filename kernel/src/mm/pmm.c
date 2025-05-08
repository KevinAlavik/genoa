#include <mm/pmm.h>
#include <boot/boot.h>
#include <util/log.h>
#include <sys/cpu.h>
#include <util/memory.h>
#include <lib/string.h>
#include <lib/bitmap.h>

struct limine_memmap_response *memmap;
uint64_t bitmap_pages;
uint64_t bitmap_size;
uint8_t *bitmap;

void pmm_init()
{
    if (!memmap_request.response)
    {
        err("Failed to get memory map, halting");
        hcf();
    }
    memmap = memmap_request.response;

    uint64_t top = 0;
    uint64_t high = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t top = e->base + e->length;
            if (top > high)
                high = top;
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
            uint8_t *ptr = (uint8_t *)(e->base + hhdm_offset);
            bitmap = ptr;
            memset((void *)bitmap, 0xFF, bitmap_size);
            e->base += bitmap_size;
            e->length -= bitmap_size;
            break;
        }
    }

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
    uint64_t last_idx = 0;

    while (last_idx < bitmap_pages)
    {
        size_t consecutive_free_pages = 0;

        for (size_t i = 0; i < pages; i++)
        {
            if (last_idx + i >= bitmap_pages)
            {
                return NULL;
            }

            if (!bitmap_get(bitmap, last_idx + i))
            {
                consecutive_free_pages++;
            }
            else
            {
                consecutive_free_pages = 0;
                break;
            }
        }

        if (consecutive_free_pages == pages)
        {
            for (size_t i = 0; i < pages; i++)
            {
                bitmap_set(bitmap, last_idx + i);
            }

            if (higher_half)
            {
                return (void *)(hhdm_offset + (last_idx * PAGE_SIZE));
            }
            else
            {
                return (void *)(last_idx * PAGE_SIZE);
            }
        }

        last_idx++;
    }

    return NULL;
}

void pmm_release_pages(void *ptr, size_t pages)
{
    uint64_t start = ((uint64_t)ptr) / PAGE_SIZE;
    for (size_t i = 0; i < pages; ++i)
    {
        if ((start + i) < bitmap_pages)
        {
            bitmap_clear(bitmap, start + i);
        }
    }
}