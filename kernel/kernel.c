#include "idt.h"
#include "vga.h"

void kernel_main(void) {
    uint8_t base = VGA_COLOR(VGA_WHITE,       VGA_BLUE);
    uint8_t hi   = VGA_COLOR(VGA_YELLOW,      VGA_BLUE);
    uint8_t info = VGA_COLOR(VGA_LIGHT_CYAN,  VGA_BLUE);
    uint8_t good = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLUE);

    vga_clear(base);

    vga_puts_at(0, 0, hi,   "  MyOS  |  x86_64  |  64-bit Long Mode  |  Kernel v0.1");
    vga_puts_at(1, 0, info, "--------------------------------------------------------");
    vga_puts_at(2, 2, info, "Kernel loaded at : 0x100000");
    vga_puts_at(3, 2, info, "Mode             : 64-bit Long Mode");
    vga_puts_at(4, 2, info, "Boot chain       : S0 > S1 > S2 > S3 > Kernel");

    idt_init();

    vga_puts_at(5, 2, good, "[ OK ] IDT installed. Interrupts enabled.");
    vga_puts_at(6, 0, info, "--------------------------------------------------------");
    vga_puts_at(7, 0, info, "");   /* keyboard output starts here, scrolls down */

    while (1)
        __asm__ volatile("hlt");
}