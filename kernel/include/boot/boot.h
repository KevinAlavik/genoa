#ifndef BOOT_H
#define BOOT_H

#include <boot/limine.h>

extern uint64_t limine_base_revision[3];

/* Requests */
extern struct limine_framebuffer_request framebuffer_request;

#endif // BOOT_H