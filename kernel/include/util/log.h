#ifndef LOG_H
#define LOG_H

#include <lib/kprintf.h>

#define COLOR_INFO "\033[32m"
#define COLOR_WARN "\033[33m"
#define COLOR_ERR "\033[1;31m"
#define COLOR_DEBUG "\033[37m"
#define COLOR_TRACE "\033[90m"
#define COLOR_MEM "\033[36m"
#define COLOR_CRIT "\033[31m"
#define COLOR_RESET "\033[0m"

#ifndef LOG_MODULE
#define LOG_MODULE "kernel"
#endif

#define _LOG(kind, color, module, fmt, ...) \
    kprintf("%s[%s] " fmt "%s\n", color, kind "::" module, ##__VA_ARGS__, COLOR_RESET)

#define info(fmt, ...) _LOG("INFO", COLOR_INFO, LOG_MODULE, fmt, ##__VA_ARGS__)
#define warn(fmt, ...) _LOG("WARN", COLOR_WARN, LOG_MODULE, fmt, ##__VA_ARGS__)
#define err(fmt, ...) _LOG("ERR", COLOR_ERR, LOG_MODULE, fmt, ##__VA_ARGS__)
#define debug(fmt, ...) _LOG("DEBUG", COLOR_DEBUG, LOG_MODULE, fmt, ##__VA_ARGS__)
#define trace(fmt, ...) _LOG("TRACE", COLOR_TRACE, LOG_MODULE, fmt, ##__VA_ARGS__)
#define mem(fmt, ...) _LOG("MEM", COLOR_MEM, LOG_MODULE, fmt, ##__VA_ARGS__)
#define crit(fmt, ...) _LOG("CRIT", COLOR_CRIT, LOG_MODULE, fmt, ##__VA_ARGS__)

#define info_module(module, fmt, ...) _LOG("INFO", COLOR_INFO, module, fmt, ##__VA_ARGS__)
#define warn_module(module, fmt, ...) _LOG("WARN", COLOR_WARN, module, fmt, ##__VA_ARGS__)
#define err_module(module, fmt, ...) _LOG("ERR", COLOR_ERR, module, fmt, ##__VA_ARGS__)
#define debug_module(module, fmt, ...) _LOG("DEBUG", COLOR_DEBUG, module, fmt, ##__VA_ARGS__)
#define trace_module(module, fmt, ...) _LOG("TRACE", COLOR_TRACE, module, fmt, ##__VA_ARGS__)
#define mem_module(module, fmt, ...) _LOG("MEM", COLOR_MEM, module, fmt, ##__VA_ARGS__)
#define crit_module(module, fmt, ...) _LOG("CRIT", COLOR_CRIT, module, fmt, ##__VA_ARGS__)

#endif /* LOG_H */