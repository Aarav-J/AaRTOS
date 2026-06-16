#include "core/kernel.h"
#include "core/print.h"
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include "core/system.h"
#include <stddef.h>

/* ==========================================================================
 * Static data
 * ========================================================================== */

static tcb_t tcb_pool[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE * 4] __attribute__((aligned(8)));

/* Non-static so assembly (context_switch.S) can access it */
tcb_t *current_task = NULL;

static tcb_t *ready_head = NULL;
extern volatile uint64_t ticks;

/* ==========================================================================
 * TCB pool management
 * ========================================================================== */

static tcb_t *alloc_tcb(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tcb_pool[i].state == TASK_DEAD) {
            return &tcb_pool[i];
        }
    }
    return NULL;
}

/* ==========================================================================
 * Ready list management
 * ========================================================================== */

static void ready_list_add(tcb_t *tcb) {
    tcb->state = TASK_READY;
    if (ready_head == NULL) {
        ready_head = tcb;
        tcb->next = tcb;
        tcb->prev = tcb;
    } else {
        tcb->next = ready_head;
        tcb->prev = ready_head->prev;
        ready_head->prev->next = tcb;
        ready_head->prev = tcb;
    }
}

/* ==========================================================================
 * Scheduler — round-robin
 * ========================================================================== */

static tcb_t *scheduler(void) {
    if (ready_head == NULL) return NULL;
    if (current_task != NULL) {
        tcb_t *next = current_task->next;
        if (next != NULL) return next;
    }
    return ready_head;
}

/* ==========================================================================
 * Functions called from PendSV assembly
 * ========================================================================== */

void save_context(uint32_t *psp) {
    if (current_task != NULL) {
        current_task->sp = psp;
    }
}

tcb_t *get_next_task(void) {
    return scheduler();
}

uint32_t *load_context(tcb_t *next_task) {
    current_task = next_task;
    if (next_task) next_task->state = TASK_RUNNING;
    return next_task ? next_task->sp : NULL;
}

/* ==========================================================================
 * Task creation
 * ========================================================================== */

tcb_t *os_task_create(const char *name, void (*entry)(void *), void *arg) {
    tcb_t *tcb = alloc_tcb();
    if (tcb == NULL) {
        print("ERROR: No free TCB!\r\n");
        return NULL;
    }

    int stack_idx = tcb - tcb_pool;
    uint8_t *stack = task_stacks[stack_idx];

    tcb->state = TASK_READY;
    tcb->entry = entry;
    tcb->arg = arg;
    tcb->stack_base = stack;
    tcb->stack_size = TASK_STACK_SIZE * 4;
    tcb->wakeup_tick = 0;

    int i;
    for (i = 0; i < 11 && name[i] != '\0'; i++) tcb->name[i] = name[i];
    tcb->name[i] = '\0';

    /* Build initial stack frame
     *
     * The PendSV handler uses stmdb/ldmia to save/restore r4-r11+LR
     * onto/from the PROCESS stack (PSP). The stack layout is:
     *
     *   High addr
     *     xPSR = 0x01000000    \\  exception frame,
     *     PC   = entry         //  popped by CPU on BX LR
     *     LR   = 0
     *     r12  = 0
     *     r3   = 0
     *     r2   = 0
     *     r1   = 0
     *     r0   = arg           (popped first by CPU)
     *     LR   = 0xFFFFFFFD    \\  callee-saved area,
     *     r11  = 0             //  popped by ldmia {r4-r11, lr}
     *     r10  = 0
     *     r9   = 0
     *     r8   = 0
     *     r7   = 0
     *     r6   = 0
     *     r5   = 0
     *   Low addr
     *     r4   = 0              (popped first by ldmia)
     *     ^^^ tcb->sp points here
     */
    uint32_t *sp = (uint32_t *)(stack + tcb->stack_size);

    /* Exception frame (higher addresses, popped by CPU on BX LR) */
    *(--sp) = 0x01000000;            /* xPSR: Thumb bit */
    *(--sp) = (uint32_t)entry;       /* PC: entry point */
    *(--sp) = 0x00000000;            /* LR: return addr (crash if returns) */
    *(--sp) = 0x00000000;            /* r12 */
    *(--sp) = 0x00000000;            /* r3 */
    *(--sp) = 0x00000000;            /* r2 */
    *(--sp) = 0x00000000;            /* r1 */
    *(--sp) = (uint32_t)arg;         /* r0: task argument */

    /* Callee-saved area (lower addresses, popped by PendSV ldmia) */
    *(--sp) = 0xFFFFFFFD;            /* LR: EXC_RETURN (Thread+PSP) */
    *(--sp) = 0x00000000;            /* r11 */
    *(--sp) = 0x00000000;            /* r10 */
    *(--sp) = 0x00000000;            /* r9 */
    *(--sp) = 0x00000000;            /* r8 */
    *(--sp) = 0x00000000;            /* r7 */
    *(--sp) = 0x00000000;            /* r6 */
    *(--sp) = 0x00000000;            /* r5 */
    *(--sp) = 0x00000000;            /* r4 */

    tcb->sp = sp;                    /* points to r4 (lowest addr) */
    ready_list_add(tcb);

    print("Task '");
    print(name);
    println("' created.");
    return tcb;
}

/* ==========================================================================
 * Simple spinlock mutex
 * ========================================================================== */

void mutex_lock(mutex_t *m) {
    while (1) {
        __asm volatile("cpsid i" : : : "memory");
        if (!m->locked) {
            m->locked = true;
            __asm volatile("cpsie i" : : : "memory");
            return;
        }
        __asm volatile("cpsie i" : : : "memory");
        __asm volatile("nop");
    }
}

void mutex_unlock(mutex_t *m) {
    __asm volatile("cpsid i" : : : "memory");
    m->locked = false;
    __asm volatile("cpsie i" : : : "memory");
}

/* ==========================================================================
 * Kernel init / start
 * ========================================================================== */

void os_kernel_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tcb_pool[i].state = TASK_DEAD;
        tcb_pool[i].sp = NULL;
        tcb_pool[i].next = NULL;
        tcb_pool[i].prev = NULL;
    }
    current_task = NULL;
    ready_head = NULL;
    println("Kernel initialized.");
}

void os_kernel_start(void) {
    if (ready_head == NULL) {
        println("ERROR: No tasks to run!");
        return;
    }

    println("Starting kernel...");

    /* Pick the first task before enabling any interrupts */
    tcb_t *first_task = scheduler();
    if (first_task == NULL) {
        println("ERROR: Scheduler returned NULL!");
        return;
    }

    print("First task: '");
    print(first_task->name);
    println("'");

    current_task = first_task;
    first_task->state = TASK_RUNNING;

    /* Set PSP to the first task's stack before starting Thread mode. */
    __asm volatile("MSR PSP, %0" : : "r" (first_task->sp));

    /* PendSV lowest priority */
    SCB_SHPR(SCB_SHPR_PRI_14_PENDSV) = 0xFF;
    /* SysTick higher than PendSV */
    SCB_SHPR(SCB_SHPR_PRI_15_SYSTICK) = 0x40;

    /* Do NOT enable SysTick here.
     * If SysTick fires before SVC starts the first task, PendSV will save
     * the main() context onto the first task's stack and corrupt it.
     * SysTick is enabled inside sv_call_handler after the first task context
     * is installed. */

    /* Trigger SVC #0. The SVC handler (in context_switch.S) loads
     * the first task's stack frame, enables SysTick, and returns to it. */
    __asm volatile("SVC #0");

    /* Never reached */
    while (1) { }
}

/* ==========================================================================
 * os_delay — Busy-wait for now (full implementation in Step 6)
 * ========================================================================== */

void os_delay(uint64_t ms) {
    uint64_t target = ticks + ms;
    while (ticks < target) { }
}
