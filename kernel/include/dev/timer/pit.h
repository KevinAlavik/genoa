#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include <sys/idt.h>

void pit_init(void (*callback)(struct register_ctx *ctx));

#endif // PIT_H