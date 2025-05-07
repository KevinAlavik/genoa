#include <arch/cpu.h>

[[noreturn]] void hlt()
{
    for (;;)
        __asm__ volatile("hlt");
    __builtin_unreachable();
}
[[noreturn]] void hcf()
{
    __asm__ volatile("cli");
    hlt();
    __builtin_unreachable();
}