#ifndef BOOT_H
#define BOOT_H

#include <boot/limine.h>
#include <lib/flanterm/flanterm.h>

extern uint64_t limine_base_revision[3];

/* Requests */
extern struct limine_framebuffer_request framebuffer_request;

/* Public */
extern struct flanterm_context *ft_ctx;

#endif // BOOT_H