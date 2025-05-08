#ifndef BOOT_H
#define BOOT_H

#include <boot/limine.h>
#include <lib/flanterm/flanterm.h>

extern uint64_t limine_base_revision[3];

/* Requests */
extern struct limine_framebuffer_request framebuffer_request;
extern struct limine_hhdm_request hhdm_request;
extern struct limine_memmap_request memmap_request;
extern struct limine_executable_address_request kernel_address_request;

/* Public */
extern struct flanterm_context *ft_ctx;
extern uint64_t kernel_stack_top;
extern uint64_t hhdm_offset;

#endif // BOOT_H