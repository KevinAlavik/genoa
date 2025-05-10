#ifndef ASSERT_H
#define ASSERT_H

#include <util/log.h>
#include <sys/cpu.h>

#define assert(expr)                                                                      \
    do                                                                                    \
    {                                                                                     \
        if (!(expr))                                                                      \
        {                                                                                 \
            err("Assertion failed: (%s), file: %s, line: %d", #expr, __FILE__, __LINE__); \
            hlt();                                                                        \
        }                                                                                 \
    } while (0)

#endif // ASSERT_H