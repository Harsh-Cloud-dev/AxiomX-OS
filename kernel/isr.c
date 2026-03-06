#include "idt.h"
#include "vga.h"
#include "serial.h"

static const char *exc_names[] = {
    "Division By Zero",         /*  0 */
    "Debug",                    /*  1 */
    "Non-Maskable Interrupt",   /*  2 */
    "Breakpoint",               /*  3 */
    "Overflow",                 /*  4 */
    "Bound Range Exceeded",     /*  5 */
    "Invalid Opcode",           /*  6 */
    "Device Not Available",     /*  7 */
    "Double Fault",             /*  8 */
    "Coprocessor Segment",      /*  9 */
    "Invalid TSS",              /* 10 */
    "Segment Not Present",      /* 11 */
    "Stack Segment Fault",      /* 12 */
    "General Protection Fault", /* 13 */
    "Page Fault",               /* 14 */
    "Reserved",                 /* 15 */
    "x87 FPU Error",            /* 16 */
    "Alignment Check",          /* 17 */
    "Machine Check",            /* 18 */
    "SIMD FPU Exception",       /* 19 */
    "Virtualization Exception", /* 20 */
    "Control Protection",       /* 21 */
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

void isr_handler(cpu_state_t *state) {
    const char *name = (state->vector < NUM_EXC)
                       ? exc_names[state->vector]
                       : "Unknown";

    /* Serial dump */
    serial_puts("\n");
    serial_puts("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    serial_printf("  EXCEPTION %u : %s\n", (uint64_t)state->vector, name);
    serial_puts("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    serial_printf("  RIP    = %p\n", (void *)state->rip);
    serial_printf("  RSP    = %p\n", (void *)state->rsp);
    serial_printf("  ERR    = 0x%x\n", state->error_code);
    serial_printf("  RAX=%p  RBX=%p\n", (void *)state->rax, (void *)state->rbx);
    serial_printf("  RCX=%p  RDX=%p\n", (void *)state->rcx, (void *)state->rdx);
    serial_printf("  RSI=%p  RDI=%p\n", (void *)state->rsi, (void *)state->rdi);
    serial_printf("  RBP=%p\n",         (void *)state->rbp);
    serial_puts("  System halted.\n");

    /* VGA dump */
    uint8_t panic = VGA_COLOR(VGA_YELLOW, VGA_RED);
    uint8_t reg   = VGA_COLOR(VGA_WHITE,  VGA_RED);

    vga_puts("\n", panic);
    vga_puts("!!! EXCEPTION ", panic);
    vga_puthex(state->vector, panic);
    vga_puts(" : ", panic);
    vga_puts(name, panic);
    vga_puts(" !!!\n", panic);
    vga_puts("  RIP=", reg); vga_puthex(state->rip,        reg);
    vga_puts("  ERR=", reg); vga_puthex(state->error_code, reg);
    vga_puts("\n", reg);
    vga_puts("  RAX=", reg); vga_puthex(state->rax, reg);
    vga_puts("  RBX=", reg); vga_puthex(state->rbx, reg);
    vga_puts("\n", reg);
    vga_puts("  RSP=", reg); vga_puthex(state->rsp, reg);
    vga_puts("  RBP=", reg); vga_puthex(state->rbp, reg);
    vga_puts("\n  System halted.\n", panic);

    __asm__ volatile("cli; hlt");
}

void irq_handler(cpu_state_t *state) {
    uint64_t irq = state->vector - 32;

    switch (irq) {
        case 0:
            /* PIT timer — nothing yet */
            break;

        case 1: {
            static const char scancode_map[128] = {
                0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',
                '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
                'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
                'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
                'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
                '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
                'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',
                0,    ' ',
            };

            uint8_t sc = inb(0x60);
            if (sc & 0x80) break;

            if (sc < 128) {
                char c = scancode_map[sc];
                if (c) {
                    uint8_t col = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLUE);
                    vga_putc(c, col);
                    serial_putc(c);
                }
            }
            break;
        }

        default:
            break;
    }

    pic_eoi(irq);
}