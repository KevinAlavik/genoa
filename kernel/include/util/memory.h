#ifndef MEMORY_H
#define MEMORY_H

#include <lib/types.h>
#include <boot/boot.h>

#define PAGE_SIZE 0x1000
#define BIT(x) (1ULL << (x))

#define DIV_ROUND_UP(x, y) (((uint64_t)(x) + ((uint64_t)(y) - 1)) / (uint64_t)(y))
#define ALIGN_UP(x, y) (DIV_ROUND_UP(x, y) * (uint64_t)(y))
#define ALIGN_DOWN(x, y) (((uint64_t)(x) / (uint64_t)(y)) * (uint64_t)(y))

#define IS_PAGE_ALIGNED(x) (((uintptr_t)(x) & (PAGE_SIZE - 1)) == 0)

#define HIGHER_HALF(ptr) ((void *)((uint64_t)ptr) + hhdm_offset)
#define PHYSICAL(ptr) ((void *)((uint64_t)ptr) - hhdm_offset)

#endif // MEMORY_H