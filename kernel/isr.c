#include "idt.h"
#include "vga.h"
#include "serial.h"
#include "pit.h"
#include "sched.h"
#include "shell.h"

static const char *exc_names[] = {
    "Division By Zero","Debug","Non-Maskable Interrupt","Breakpoint",
    "Overflow","Bound Range Exceeded","Invalid Opcode",
    "Device Not Available","Double Fault","Coprocessor Segment",
    "Invalid TSS","Segment Not Present","Stack Segment Fault",
    "General Protection Fault","Page Fault","Reserved",
    "x87 FPU Error","Alignment Check","Machine Check",
    "SIMD FPU Exception","Virtualization Exception","Control Protection",
};
#define NUM_EXC 22

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

#define PIC1_CMD 0x20
#define PIC2_CMD 0xA0
#define PIC_EOI  0x20

static void pic_eoi(uint64_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

static volatile uint64_t last_statusbar_sec = 0;

static void update_statusbar_if_needed(void) {
    uint32_t hz = pit_hz();
    if (!hz) return;

    uint64_t secs = pit_ticks() / hz;
    if (secs == last_statusbar_sec) return;
    last_statusbar_sec = secs;

    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    uint8_t bar = VGA_COLOR(VGA_WHITE,  VGA_DARK_GREY);
    uint8_t clk = VGA_COLOR(VGA_YELLOW, VGA_DARK_GREY);

    for (int c = 0; c < 80; c++)
        vga[24 * 80 + c] = (uint16_t)(bar << 8 | ' ');

    const char *title = "AxiomX v0.1";
    int col = 1;
    for (int i = 0; title[i]; i++)
        vga[24 * 80 + col++] = (uint16_t)(bar << 8 | (uint8_t)title[i]);

    const char *up = "Uptime:";
    col = 30;
    for (int i = 0; up[i]; i++)
        vga[24 * 80 + col++] = (uint16_t)(bar << 8 | (uint8_t)up[i]);

    col = 38;
    for (int i = 0; i < 12; i++)
        vga[24 * 80 + col + i] = (uint16_t)(clk << 8 | ' ');

    char buf[20]; int i = 0;
    uint64_t t = secs;
    if (!t) buf[i++] = '0';
    else while (t) { buf[i++] = '0' + (t % 10); t /= 10; }

    for (int j = i - 1; j >= 0; j--)
        vga[24 * 80 + col++] = (uint16_t)(clk << 8 | (uint8_t)buf[j]);
    vga[24 * 80 + col] = (uint16_t)(clk << 8 | 's');

    const char *hz_str = "| 100 Hz";
    col = 52;
    for (int k = 0; hz_str[k]; k++)
        vga[24 * 80 + col++] = (uint16_t)(bar << 8 | (uint8_t)hz_str[k]);
}

void isr_handler(cpu_state_t *state) {
    if (state->vector == 1 || state->vector == 3) {
        serial_printf("[debug] vector %u at RIP=%p\n",
            (uint64_t)state->vector, (void *)state->rip);
        return;
    }
    
    const char *name;
    if (state->vector < NUM_EXC) name = exc_names[state->vector];
    else if (state->vector == 0xFF) name = "Unhandled Interrupt";
    else name = "Unknown";

    serial_puts("\n!!! EXCEPTION !!!\n");
    serial_printf("EXCEPTION %u : %s\n", (uint64_t)state->vector, name);
    serial_printf("RIP=%p RSP=%p ERR=0x%x\n",
        (void *)state->rip, (void *)state->rsp, state->error_code);

    vga_clear(VGA_COLOR(VGA_WHITE, VGA_RED));
    vga_puts("Kernel Panic\n", VGA_COLOR(VGA_YELLOW, VGA_RED));

    __asm__ volatile("cli; hlt");
}

/* keyboard maps */
static const char sc_normal[128] = {
0,0x1B,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
'b','n','m',',','.','/',0,'*',0,' '
};

static const char sc_shifted[128] = {
0,0x1B,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
'B','N','M','<','>','?',0,'*',0,' '
};

static volatile int shift_state = 0;
static volatile int extended_state = 0;

void irq_handler(cpu_state_t *state) {
    uint64_t irq = state->vector - 32;

    switch (irq) {
        case 0:
    pit_tick();
    update_statusbar_if_needed();
    pic_eoi(irq);
    if (shell_trace_active()) {
        static uint64_t last_trace_tick = 0;
        uint64_t now = pit_ticks();
        if (now - last_trace_tick >= 100) {
            last_trace_tick = now;
            uint8_t tc  = VGA_COLOR(VGA_YELLOW,      VGA_BLUE);
            uint8_t tc2 = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLUE);
            uint64_t hz = pit_hz();
            uint64_t s  = hz ? now / hz : 0;
            vga_puts("[timer] tick=", tc);
            char b[20]; int i = 0;
            uint64_t t = now;
            if (!t) { b[i++] = '0'; }
            else { while (t) { b[i++] = '0' + (t % 10); t /= 10; } }
            for (int j = i - 1; j >= 0; j--) vga_putc(b[j], tc);
            vga_puts("  uptime=", tc);
            i = 0; t = s;
            if (!t) { b[i++] = '0'; }
            else { while (t) { b[i++] = '0' + (t % 10); t /= 10; } }
            for (int j = i - 1; j >= 0; j--) vga_putc(b[j], tc2);
            vga_puts("s\n", tc);
        }
    }
    sched_tick();
    return; 
    /* trace output every 100 ticks (1 second) to avoid flood */
                if (shell_trace_active()) {
                static uint64_t last_trace_tick = 0;
                uint64_t now = pit_ticks();
                if (now - last_trace_tick >= 100) {
                    last_trace_tick = now;
                    uint8_t tc = VGA_COLOR(VGA_YELLOW, VGA_BLUE);
                    vga_puts("[int] timer tick ", tc);
            /* print tick count */
                    char b[20]; int i = 0;
                    uint64_t t = now;
                    if (!t) { b[i++] = '0'; }
                else { while (t) { b[i++] = '0' + (t % 10); t /= 10; } }
                for (int j = i - 1; j >= 0; j--) vga_putc(b[j], tc);
                vga_puts("\n", tc);
            }
        }
            sched_tick();
            return; 

        case 1: {
            uint8_t sc = inb(0x60);

            if (sc == 0xE0) {
                extended_state = 1;
                break;
            }

            if (extended_state) {
                extended_state = 0;

                if (!(sc & 0x80)) {
                    if (sc == 0x48) {
                        if (shift_state) vga_scroll_view_up(3);
                        else             shell_special(sc);
                    } 
                    else if (sc == 0x50) {
                        if (shift_state) vga_scroll_view_down(3);
                        else             shell_special(sc);
                    } 
                    else if (sc == 0x4B || sc == 0x4D) {
                        shell_special(sc);
                    }
                }
                break;
            }

            if (sc == 0x2A || sc == 0x36) {
                shift_state = 1;
                shell_set_shift(1);
                break;
            }

            if (sc == 0xAA || sc == 0xB6) {
                shift_state = 0;
                shell_set_shift(0);
                break;
            }

            if (sc & 0x80) break;

            if (sc < 128) {
                if (vga_is_scrolled())
                    vga_scroll_view_down(9999);

                char c = shift_state ? sc_shifted[sc] : sc_normal[sc];
                if (c) shell_putchar(c);
            }
            break;
        }

        default:
            break;
    }

    pic_eoi(irq);
}