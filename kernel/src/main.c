#include <sys/cpu.h>
#include <boot/limine.h>
#include <boot/boot.h>
#include <lib/kprintf.h>
#include <lib/types.h>
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
#include <util/log.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/vma.h>
#include <mm/kmalloc.h>
#include <sys/pic.h>
#include <dev/timer/pit.h>
#include <sys/sched.h>

/* Public */
struct flanterm_context *ft_ctx = NULL;
uint64_t kernel_stack_top = 0;
uint64_t hhdm_offset = 0;
uint64_t __kernel_phys_base = 0;
uint64_t __kernel_virt_base = 0;
vma_context_t *kernel_vma_context = NULL;

/* Scheduler test */
void tick(struct register_ctx *c)
{
    scheduler_tick(c);
}

void test_proc()
{
    info("Hello from process: %d", scheduler_get_current()->pid);
}

/* Kernel Entry */
void genoa_entry(void)
{
    __asm__ volatile("movq %%rsp, %0" : "=r"(kernel_stack_top));
    if (!LIMINE_BASE_REVISION_SUPPORTED)
    {
        err("Unsupported limine base revision");
        hlt();
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
    {
        err("Failed to get framebuffer");
        hlt();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch,
        framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift,
        framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0);

    if (ft_ctx == NULL)
    {
        err("Failed to initialize flanterm context!");
        hcf();
    }

    info("Genoa Kernel v1.0.0");
    hhdm_offset = hhdm_request.response->offset;
    info("HHDM Offset: 0x%.16llx", hhdm_offset);
    info("Kernel Stack: 0x%.16llx", kernel_stack_top);

    /* Interrupts */
    gdt_init();
    idt_init();
    load_idt();

    __asm__ volatile("cli");
    pic_init();
    __asm__ volatile("sti");

    /* Memory initialization */
    pmm_init();
    char *a = pmm_request_pages(1, true);
    if (a == NULL)
    {
        err("Failed to allocate single physical page");
        hcf();
    }
    *a = 32;
    info("Allocated physical page @ 0x%.16llx", (uint64_t)a);
    pmm_release_pages(a, 1);
    info("Initialized PMM");

    __kernel_phys_base = kernel_address_request.response->physical_base;
    __kernel_virt_base = kernel_address_request.response->virtual_base;
    vmm_init();
    info("Initialized VMM");

    kernel_vma_context = vma_create_context(kernel_pagemap);
    if (kernel_vma_context == NULL)
    {
        err("Failed to create kernel VMA context");
        hcf();
    }

    char *b = vma_alloc(kernel_vma_context, 1, VMM_PRESENT | VMM_WRITE);
    if (b == NULL)
    {
        err("Failed to allocate single virtual page");
        hcf();
    }
    *b = 32;
    info("Allocated virtual page @ 0x%.16llx", (uint64_t)b);
    vma_free(kernel_vma_context, b);
    info("Initialized VMA");

    /* Test heap */
    char *c = kmalloc(1);
    if (c == NULL)
    {
        err("Failed to allocate single byte using heap");
        hcf();
    }
    *c = 32;
    info("Allocated single byte using heap @ 0x%.16llx", (uint64_t)c);
    kfree(c);
    info("Initialized heap");

    /* Start the timer */
    scheduler_init();
    scheduler_spawn(false, test_proc, kernel_pagemap);
    pit_init(tick);

    hlt();
}