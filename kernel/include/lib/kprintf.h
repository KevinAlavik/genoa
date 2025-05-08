#ifndef KPRINTF_H
#define KPRINTF_H

#include <lib/types.h>
#include <stdarg.h>

int kprintf(const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

#endif // KPRINTF_H