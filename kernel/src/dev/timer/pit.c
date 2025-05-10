#include <dev/timer/pit.h>
#include <dev/portio.h>
#include <sys/pic.h>
#include <sys/idt.h>

void (*pit_callback)(struct register_ctx *ctx) = NULL;

void pit_handler(struct register_ctx *frame)
{
    if (pit_callback)
        pit_callback(frame);
    pic_eoi(0);
}

void pit_init(void (*callback)(struct register_ctx *ctx))
{
    if (callback)
        pit_callback = callback;
    // Setup channel 0 at mode 3 (lohi)
    outb(0x43, 0x36);

    // Register our IRQ0 handler (aka the pit handler)
    idt_register_handler(IDT_IRQ_BASE + 0, pit_handler);

    // Setup the divisor
    uint16_t divisor = 5966; // ~200Hz
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    // unmask the IRQ0
    pic_unmask(0);
}
