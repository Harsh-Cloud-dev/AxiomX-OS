#include "pit.h"
#include "serial.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static volatile uint64_t tick_count = 0;
static volatile uint32_t tick_hz    = 0;

void pit_init(uint32_t hz) {
    tick_hz    = hz;
    tick_count = 0;

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 1)      divisor = 1;

    outb(PIT_CMD,      PIT_CMD_INIT);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    serial_printf("[pit] Initialised at %u Hz (divisor=%u)\n",
                  (unsigned int)hz, (unsigned int)divisor);
}

void pit_tick(void) {
    tick_count++;
}

uint64_t pit_ticks(void) {
    return tick_count;
}

uint32_t pit_hz(void) {
    return tick_hz;
}

void pit_sleep(uint64_t ms) {
    if (tick_hz == 0) return;
    uint64_t ticks_needed = (ms * tick_hz) / 1000;
    if (ticks_needed == 0) ticks_needed = 1;
    uint64_t start = tick_count;
    while ((tick_count - start) < ticks_needed) {
        __asm__ volatile("sti");
        __asm__ volatile("pause");
    }
}