#include <sys/cpu.h>
#include <boot/limine.h>
#include <boot/boot.h>
#include <lib/kprintf.h>
#include <lib/types.h>
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
#include <util/log.h>

/* Public */
struct flanterm_context *ft_ctx = NULL;

/* Kernel Entry */
void genoa_entry(void)
{
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
    hlt();
}