#ifndef PIT_H
#define PIT_H

#include "types.h"

/* ── 8254 PIT ports ──────────────────────────────────────────────────────── */
#define PIT_CHANNEL0   0x40     /* channel 0 data port (IRQ0)        */
#define PIT_CHANNEL1   0x41     /* channel 1 (unused)                */
#define PIT_CHANNEL2   0x42     /* channel 2 (PC speaker)            */
#define PIT_CMD        0x43     /* mode/command register             */

/* ── Command byte ────────────────────────────────────────────────────────── */
/* Channel 0 | lobyte/hibyte access | mode 3 (square wave) | binary */
#define PIT_CMD_CHANNEL0  0x00
#define PIT_CMD_LOBYTE    0x10
#define PIT_CMD_HIBYTE    0x20
#define PIT_CMD_LOHIBYTE  0x30  /* send low byte then high byte      */
#define PIT_CMD_MODE2     0x04  /* rate generator                    */
#define PIT_CMD_MODE3     0x06  /* square wave                       */
#define PIT_CMD_BINARY    0x00  /* binary counting                   */

/* Standard command: ch0, lobyte/hibyte, mode 3, binary */
#define PIT_CMD_INIT  (PIT_CMD_CHANNEL0 | PIT_CMD_LOHIBYTE | PIT_CMD_MODE3 | PIT_CMD_BINARY)

/* ── PIT base frequency ──────────────────────────────────────────────────── */
#define PIT_BASE_HZ   1193182UL

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Initialise PIT channel 0 to fire IRQ0 at `hz` times per second.
   Typical values: 100 (10ms tick), 250 (4ms), 1000 (1ms).          */
void pit_init(uint32_t hz);

/* Called by IRQ0 handler — increments tick counter. */
void pit_tick(void);

/* Return current tick count (incremented hz times per second). */
uint64_t pit_ticks(void);

/* Return tick frequency (hz value passed to pit_init). */
uint32_t pit_hz(void);

/* Busy-wait for approximately `ms` milliseconds.
   Accuracy depends on tick frequency — at 100Hz, resolution is 10ms. */
void pit_sleep(uint64_t ms);

#endif