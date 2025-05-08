#include <sys/gdt.h>

gdt_entry_t gdt[7];
gdt_ptr_t gdt_ptr;
tss_entry_t tss; // Not used yet

void gdt_init()
{
    gdt[0] = (gdt_entry_t){0, 0, 0, 0x00, 0x00, 0};                               // Null descriptor
    gdt[1] = (gdt_entry_t){0, 0, 0, GDT_KERNEL_CODE, GDT_GRANULARITY_FLAT, 0};    // Kernel code segment
    gdt[2] = (gdt_entry_t){0, 0, 0, GDT_KERNEL_DATA, GDT_GRANULARITY_FLAT, 0};    // Kernel data segment
    gdt[3] = (gdt_entry_t){0, 0, 0, GDT_USER_CODE, GDT_GRANULARITY_LONG_MODE, 0}; // User code segment
    gdt[4] = (gdt_entry_t){0, 0, 0, GDT_USER_DATA, 0x00, 0};                      // User data segment

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base = (uint64_t)&gdt;

    gdt_flush(gdt_ptr);
}

void gdt_flush(gdt_ptr_t gdt_ptr)
{
    __asm__ volatile(
        "mov %0, %%rdi\n"
        "lgdt (%%rdi)\n"
        "push $0x8\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%fs\n"
        :
        : "r"(&gdt_ptr)
        : "memory");
}