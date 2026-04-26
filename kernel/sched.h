#ifndef SCHED_H
#define SCHED_H
#include "types.h"

#define PROC_READY    0
#define PROC_RUNNING  1
#define PROC_BLOCKED  2
#define PROC_DEAD     3

#define PROC_STACK_SIZE  (4096 * 4)

typedef struct pcb {
    uint64_t    pid;
    char        name[32];
    int         state;
    uint64_t    rsp;
    uint64_t    stack_top;
    uint64_t    stack_base;
    uint64_t    stack_size;
    struct pcb *next;
} pcb_t;

#define SWITCHLOG_SIZE 128

typedef struct {
    char     from[32];
    char     to[32];
    uint64_t tick;
    uint32_t seq;       /* sequence number */
} switch_entry_t;

void    sched_init(void);
void    sched_spawn(void (*fn)(void), const char *name);
void    sched_tick(void);
void    sched_yield(void);
void    sched_reap(void);
pcb_t  *sched_current(void);
void    sched_print(void);

void    sched_switchlog_enable(void);
void    sched_switchlog_disable(void);
int     sched_switchlog_active(void);
void    sched_switchlog_clear(void);
int     sched_switchlog_count(void);
switch_entry_t *sched_switchlog_get(int idx);
uint64_t sched_ticks_remaining(void);

extern void context_switch(uint64_t *old_rsp, uint64_t new_rsp);
#endif