#ifndef INC_KERNEL_H
#define INC_KERNEL_H

#include "common-defines.h"
#include <stdint.h>
#include <stdbool.h>

/* Maximum number of tasks the RTOS can manage */
#define MAX_TASKS       8

/* Stack size for each task (in 32-bit words, so 256 = 1KB stack) */
#define TASK_STACK_SIZE 256

/* Task states */
typedef enum {
    TASK_READY    = 0,
    TASK_RUNNING  = 1,
    TASK_BLOCKED  = 2,
    TASK_DEAD     = 3
} task_state_t;

/* Task Control Block */
typedef struct tcb {
    uint32_t *sp;               /* Saved stack pointer (points to saved r4-r11) */
    task_state_t state;         /* Current task state */
    void (*entry)(void *);      /* Task function entry point */
    void *arg;                  /* Argument passed to the task */
    uint8_t *stack_base;        /* Bottom of the stack (for overflow detection) */
    uint32_t stack_size;        /* Stack size in bytes */
    uint64_t wakeup_tick;       /* For os_delay(): tick to wake up */
    struct tcb *next;           /* Linked list pointer for ready queue */
    struct tcb *prev;           /* Previous pointer for circular list */
    char name[12];              /* Task name for debugging */
} tcb_t;

/* ── Simple spinlock mutex (for now) ── */
typedef struct {
    volatile bool locked;
} mutex_t;

#define MUTEX_INIT { .locked = false }

void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

/* ── Kernel API ── */
void        os_kernel_init(void);
void        os_kernel_start(void);
tcb_t*      os_task_create(const char *name, void (*entry)(void *), void *arg);
void        os_delay(uint64_t ms);

/* ── Functions called from PendSV assembly ── */
void        save_context(uint32_t *psp);
tcb_t*      get_next_task(void);
uint32_t*   load_context(tcb_t *next_task);

#endif /* INC_KERNEL_H */
