#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "interrupts.h"
#include "io.h" // чтобы inb/outb были известны
#define PIT_FREQUENCY 1193182

static volatile uint32_t tick_count = 0;

void pit_handler() {
    tick_count++;
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Устанавливаем обработчик IRQ0 (PIT)
    add_interrupt_handler(0x20, pit_handler);

}

uint32_t timer_ticks() {
    return tick_count;
}
void timer_sleep(uint32_t ms) {
    uint32_t start = tick_count;
    uint32_t ticks_to_wait = ms; // если PIT на 1000 Гц, то 1 тик = 1 мс

    while (tick_count < start + ticks_to_wait) {
        pit_handler();
    }
}

#endif
