#include <sys/cpu.h>
#include <boot/limine.h>
#include <boot/boot.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void genoa_entry(void)
{
    if (LIMINE_BASE_REVISION_SUPPORTED == false)
    {
        hlt();
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
    {
        hlt();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    for (size_t i = 0; i < 100; i++)
    {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    hlt();
}