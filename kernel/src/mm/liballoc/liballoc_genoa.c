#include <mm/vma.h>
#include <sys/spinlock.h>
#include <stddef.h>
#include <boot/boot.h>

extern vma_context_t *kernel_vma_context;

static spinlock_t liballoc_lock_var = {0};

int liballoc_lock()
{
    spinlock_acquire(&liballoc_lock_var);
    return 0;
}

int liballoc_unlock()
{
    spinlock_release(&liballoc_lock_var);
    return 0;
}

void *liballoc_alloc(int pages)
{
    return vma_alloc(kernel_vma_context, pages, VMM_PRESENT | VMM_WRITE);
}

int liballoc_free(void *ptr, int pages)
{
    (void)pages;
    vma_free(kernel_vma_context, ptr);
    return 0;
}