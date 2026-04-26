#include "sched.h"
#include "heap.h"
#include "serial.h"
#include "vga.h"
#include "pit.h"

#define SCHED_QUANTUM  10

static pcb_t   *current          = (void *)0;
static pcb_t   *queue            = (void *)0;
static uint64_t next_pid         = 0;
static uint64_t ticks_remaining  = SCHED_QUANTUM;
static int      sched_ready      = 0;
static volatile int needs_reschedule = 0;
static uint32_t switchlog_seq = 0;

static switch_entry_t switchlog_buf[SWITCHLOG_SIZE];
static int            switchlog_head   = 0;
static int            switchlog_count  = 0;
static volatile int   switchlog_active = 0;
static uint64_t       switchlog_start_tick = 0;

static uint8_t sched_color(void) {
    return VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLUE);
}

static void sched_puts(const char *s) {
    vga_puts(s, sched_color());
    serial_puts(s);
}

static void sched_putuint(uint64_t v) {
    char b[24]; int i = 0;
    if (!v) { vga_putc('0', sched_color()); serial_putc('0'); return; }
    while (v) { b[i++] = '0' + (v % 10); v /= 10; }
    for (int j = i - 1; j >= 0; j--) {
        vga_putc(b[j], sched_color());
        serial_putc(b[j]);
    }
}

static void sched_puthex(uint64_t v) {
    const char *d = "0123456789abcdef";
    sched_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        char c = d[(v >> i) & 0xF];
        vga_putc(c, sched_color());
        serial_putc(c);
    }
}

static void kstrncpy(char *dst, const char *src, int n) {
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int kstreq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void proc_entry(void) {
    void (*fn)(void) = (void (*)(void))current->stack_top;
    __asm__ volatile("sti");
    fn();

    sched_puts("[sched] process '");
    sched_puts(current->name);
    sched_puts("' (pid ");
    sched_putuint(current->pid);
    sched_puts(") exited\n");

    current->state = PROC_DEAD;
    sched_yield();

    while (1) __asm__ volatile("hlt");
}

void sched_init(void) {
    pcb_t *idle = (pcb_t *)kzalloc(sizeof(pcb_t));
    if (!idle) {
        serial_puts("[sched] FATAL: cannot allocate idle PCB\n");
        __asm__ volatile("cli; hlt");
    }

    idle->pid        = next_pid++;
    idle->state      = PROC_RUNNING;
    idle->rsp        = 0;
    idle->stack_top  = 0;
    idle->stack_base = 0;
    idle->stack_size = 0;
    idle->next       = idle;
    kstrncpy(idle->name, "idle", 32);

    current = idle;
    queue   = idle;

    ticks_remaining  = SCHED_QUANTUM;
    needs_reschedule = 0;
    sched_ready      = 1;

    serial_printf("[sched] Initialised. Idle PID=%u, quantum=%u ticks\n",
                  (unsigned int)idle->pid, (unsigned int)SCHED_QUANTUM);
}

void sched_spawn(void (*fn)(void), const char *name) {
    pcb_t *p = (pcb_t *)kzalloc(sizeof(pcb_t));
    if (!p) {
        sched_puts("[sched] ERROR: cannot allocate PCB\n");
        return;
    }

    p->pid   = next_pid++;
    p->state = PROC_READY;
    kstrncpy(p->name, name, 32);

    uint8_t *stack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!stack) {
        sched_puts("[sched] ERROR: cannot allocate stack\n");
        kfree(p);
        return;
    }

    p->stack_top  = (uint64_t)fn;
    p->stack_base = (uint64_t)stack;
    p->stack_size = PROC_STACK_SIZE;

    uint64_t *sp = (uint64_t *)(stack + PROC_STACK_SIZE);
    *--sp = (uint64_t)fn;
    *--sp = (uint64_t)proc_entry;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    p->rsp = (uint64_t)sp;

    p->next       = current->next;
    current->next = p;

    sched_puts("[sched] Spawned '");
    sched_puts(name);
    sched_puts("' PID=");
    sched_putuint(p->pid);
    sched_puts(" rsp=");
    sched_puthex(p->rsp);
    sched_puts("\n");
}

void sched_tick(void) {
    if (!sched_ready) return;
    ticks_remaining--;
    if (ticks_remaining > 0) return;
    ticks_remaining  = SCHED_QUANTUM;
    needs_reschedule = 1;
}

void sched_yield(void) {
    if (!sched_ready) return;
    needs_reschedule = 0;

    pcb_t *next = current->next;
    int attempts = 0;
    while (next->state != PROC_READY && next->state != PROC_RUNNING) {
        next = next->next;
        attempts++;
        if (attempts > 64) return;
    }

    if (next == current) return;

    if (switchlog_active) {
        int boring = (kstreq(current->name, "idle") && kstreq(next->name, "shell")) ||
                     (kstreq(current->name, "shell") && kstreq(next->name, "idle"));
        if (!boring) {
            switch_entry_t *e = &switchlog_buf[switchlog_head];
            kstrncpy(e->from, current->name, 32);
            kstrncpy(e->to,   next->name,    32);
            e->tick = pit_ticks() - switchlog_start_tick;
            e->seq  = switchlog_seq++;
            switchlog_head = (switchlog_head + 1) % SWITCHLOG_SIZE;
            if (switchlog_count < SWITCHLOG_SIZE) switchlog_count++;
        } 
    }

    pcb_t *prev = current;
    if (prev->state != PROC_DEAD)
        prev->state = PROC_READY;
    current     = next;
    next->state = PROC_RUNNING;

    context_switch(&prev->rsp, next->rsp);
}

void sched_reap(void) {
    if (!sched_ready) return;

    __asm__ volatile("cli");

    pcb_t *to_free[128];
    int    free_count = 0;

    int found;
    int safety = 0;
    do {
        found = 0;
        pcb_t *p     = queue;
        int    count = 0;

        do {
            pcb_t *next = p->next;
            if (next != queue &&
                next != current &&
                next->state == PROC_DEAD) {
                p->next = next->next;
                if (free_count < 128)
                    to_free[free_count++] = next;
                found = 1;
            } else {
                p = next;
            }
            count++;
            if (count > 256) break;
        } while (p != queue);

        safety++;
    } while (found && safety < 16);

    __asm__ volatile("sti");

    for (int i = 0; i < free_count; i++) {
        if (to_free[i]->stack_base)
            kfree((void *)to_free[i]->stack_base);
        kfree(to_free[i]);
    }
}

void     sched_switchlog_enable(void)  {
    switchlog_start_tick = pit_ticks();
    switchlog_active = 1;
}
void     sched_switchlog_disable(void) { switchlog_active = 0; }
int      sched_switchlog_active(void)  { return switchlog_active; }
void     sched_switchlog_clear(void)   { switchlog_head = 0; switchlog_count = 0; switchlog_seq=0; }
int      sched_switchlog_count(void)   { return switchlog_count; }

switch_entry_t *sched_switchlog_get(int idx) {
    if (idx < 0 || idx >= switchlog_count) return (void *)0;
    int oldest = (switchlog_head - switchlog_count + SWITCHLOG_SIZE) % SWITCHLOG_SIZE;
    return &switchlog_buf[(oldest + idx) % SWITCHLOG_SIZE];
}

uint64_t sched_ticks_remaining(void) { return ticks_remaining; }

pcb_t *sched_current(void) { return current; }

void sched_print(void) {
    static const char *state_names[] = {
        "READY  ", "RUNNING", "BLOCKED", "DEAD   "
    };
    uint8_t col   = VGA_COLOR(VGA_WHITE,       VGA_BLUE);
    uint8_t col_h = VGA_COLOR(VGA_LIGHT_CYAN,  VGA_BLUE);
    uint8_t col_r = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLUE);

    pcb_t *snap_current = current;

    vga_puts("\n  PID  State    Name\n", col_h);
    vga_puts("  ---  -------  --------------------\n", col);
    serial_puts("\n[sched] Process List:\n");
    serial_puts("  PID  State    Name\n");

    pcb_t *p = queue;
    int count = 0;
    do {
        char buf[24]; int i = 0;
        uint64_t pid = p->pid;
        if (!pid) { buf[i++] = '0'; }
        else { while (pid) { buf[i++] = '0' + (pid % 10); pid /= 10; } }

        int is_cur = (p == snap_current);

        vga_puts("  ", col);
        for (int j = i - 1; j >= 0; j--) vga_putc(buf[j], col);
        vga_puts("    ", col);
        vga_puts(state_names[p->state], is_cur ? col_r : col);
        vga_puts("  ", col);
        vga_puts(p->name, is_cur ? col_r : col);
        if (is_cur) vga_puts(" <--", col_r);
        vga_putc('\n', col);

        serial_printf("  %u    %s  %s%s\n",
                      (unsigned int)p->pid,
                      state_names[p->state],
                      p->name,
                      is_cur ? " <-- running" : "");

        p = p->next;
        count++;
    } while (p != queue && count < 64);
}