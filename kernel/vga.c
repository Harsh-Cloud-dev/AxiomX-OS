#include "vga.h"

#define VGA_BUF  ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

#define CELL(c, col) ((uint16_t)(((col) << 8) | (uint8_t)(c)))

static int     cur_row       = 0;
static int     cur_col       = 0;
static uint8_t default_color = 0;

static void scroll(void) {
    for (int r = 0; r < VGA_ROWS - 1; r++)
        for (int c = 0; c < VGA_COLS; c++)
            VGA_BUF[r * VGA_COLS + c] = VGA_BUF[(r + 1) * VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; c++)
        VGA_BUF[(VGA_ROWS - 1) * VGA_COLS + c] = CELL(' ', default_color);
    cur_row = VGA_ROWS - 1;
    cur_col = 0;
}

void vga_init(void) {
    default_color = VGA_COLOR(VGA_WHITE, VGA_BLUE);
    cur_row = 0;
    cur_col = 0;
}

void vga_clear(uint8_t color) {
    default_color = color;
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_BUF[i] = CELL(' ', color);
    cur_row = 0;
    cur_col = 0;
}

void vga_putc(char c, uint8_t color) {
    if (c == '\n') {
        cur_col = 0;
        cur_row++;
        if (cur_row >= VGA_ROWS) scroll();
        return;
    }
    if (c == '\r') { cur_col = 0; return; }
    if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            VGA_BUF[cur_row * VGA_COLS + cur_col] = CELL(' ', color);
        }
        return;
    }
    if (c == '\t') {
        cur_col = (cur_col + 4) & ~3;
        if (cur_col >= VGA_COLS) {
            cur_col = 0;
            cur_row++;
            if (cur_row >= VGA_ROWS) scroll();
        }
        return;
    }
    VGA_BUF[cur_row * VGA_COLS + cur_col] = CELL(c, color);
    cur_col++;
    if (cur_col >= VGA_COLS) {
        cur_col = 0;
        cur_row++;
        if (cur_row >= VGA_ROWS) scroll();
    }
}

void vga_puts(const char *s, uint8_t color) {
    while (*s) vga_putc(*s++, color);
}

void vga_puts_at(int row, int col, uint8_t color, const char *s) {
    cur_row = row;
    cur_col = col;
    vga_puts(s, color);
}

void vga_puthex(uint64_t val, uint8_t color) {
    const char *d = "0123456789ABCDEF";
    vga_puts("0x", color);
    for (int i = 60; i >= 0; i -= 4)
        vga_putc(d[(val >> i) & 0xF], color);
}

void vga_set_cursor(int row, int col) {
    cur_row = row;
    cur_col = col;
}

void vga_get_cursor(int *row, int *col) {
    *row = cur_row;
    *col = cur_col;
}