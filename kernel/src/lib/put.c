#include <lib/types.h>
#include <dev/portio.h>
#include <boot/boot.h>

void put(const char *data, size_t length)
{
    if (ft_ctx)
        flanterm_write(ft_ctx, data, length);

    for (size_t i = 0; i < length; i++)
    {
        outb(0xE9, data[i]);
    }
}