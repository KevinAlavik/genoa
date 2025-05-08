#ifndef LOG_H
#define LOG_H

#include <lib/kprintf.h>

#define LOG_TIMESTAMP "0.000"

#define _LOG(kind, fmt, ...) \
    kprintf(LOG_TIMESTAMP " | %s: " fmt "\n", kind, ##__VA_ARGS__)

#define info(fmt, ...) _LOG("info", fmt, ##__VA_ARGS__)
#define warn(fmt, ...) _LOG("warn", fmt, ##__VA_ARGS__)
#define err(fmt, ...) _LOG("err", fmt, ##__VA_ARGS__)

#endif /* LOG_H */
