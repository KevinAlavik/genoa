#ifndef PMM_H
#define PMM_H

#include <lib/types.h>

void pmm_init();
void *pmm_request_pages(size_t pages, bool higher_half);
void pmm_release_pages(void *ptr, size_t pages);

#endif // PMM_H