#ifndef PIC_H
#define PIC_H

// Kevin's advanced legacy PIC "driver"

#include <stdint.h>
#include <stdbool.h>

void pic_init();
void pic_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);
void pic_maskall();

#endif // PIC_H