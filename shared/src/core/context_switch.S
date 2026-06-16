.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

/* External symbols from kernel.c / system.c */
.extern current_task
.extern save_context
.extern get_next_task
.extern load_context
.extern systick_start

/* =========================================================================
 * PendSV Handler — Preemptive context switch
 *
 * Called every SysTick tick (via SCB_ICSR_PENDSVSET).
 * Saves the current task's registers onto its PSP stack,
 * switches to the next task, and restores its context.
 *
 * On entry:
 *   - CPU in Handler mode, using MSP
 *   - Hardware pushed exception frame (r0-r3, r12, LR, PC, xPSR) onto PSP
 *   - LR contains EXC_RETURN value from the exception entry
 *
 * On exit:
 *   - CPU returns to Thread mode using PSP with new task's context
 * ========================================================================= */
.text
.align 2
.globl pend_sv_handler
.type pend_sv_handler,%function
.thumb_func
pend_sv_handler:
    /* Save the current task's callee-saved registers (r4-r11) and
     * the EXC_RETURN value (r14/LR) onto the PROCESS stack (PSP).
     *
     * stmdb = store multiple, decrement before (push downward).
     * r0 = PSP value - we modify r0 and then store it back to PSP. */
    mrs r0, psp              /* r0 = current task's PSP (points to exception frame) */
    stmdb r0!, {r4-r11, lr}  /* Push r4-r11 + EXC_RETURN onto PSP, r0 = new PSP */

    /* Save the updated PSP into current_task->sp */
    push {lr}
    bl save_context          /* save_context(psp) where psp is in r0 (set by stmdb) */
    pop {lr}

    /* Get the next task to run */
    push {lr}
    bl get_next_task         /* returns next_task TCB pointer in r0 */
    pop {lr}

    /* If NULL, restore the old task */
    cbz r0, .Lrestore

    /* Load the new task's context — returns the new PSP */
    push {lr}
    bl load_context          /* r0 = next_task, returns new PSP in r0 */
    pop {lr}

    /* r0 now contains the new PSP value */
    /* Restore r4-r11 and EXC_RETURN from the new task's stack */
    ldmia r0!, {r4-r11, lr}  /* Pop r4-r11 + EXC_RETURN from PSP, r0 = updated PSP */
    msr psp, r0              /* PSP = new task's stack (pointing past saved regs) */

.Lrestore:
    /* Exception return:
     * The CPU automatically pops r0-r3, r12, LR, PC, xPSR from PSP.
     * LR contains the EXC_RETURN value for the new task. */
    bx lr
.size pend_sv_handler, .-pend_sv_handler

/* =========================================================================
 * SVC Handler — Used to start the first task
 *
 * os_kernel_start() sets current_task and triggers SVC #0.
 * The SVC handler loads the first task's context and returns to it.
 * ========================================================================= */
.globl sv_call_handler
.type sv_call_handler,%function
.thumb_func
sv_call_handler:
    /* Enable SysTick only now, while we're still inside SVC.
     * This avoids a startup race where SysTick/PendSV fires before the first
     * task has started and corrupts its initial stack frame with main()'s
     * context. SysTick cannot preempt SVC here because SVC has higher priority. */
    push {lr}
    bl systick_start
    pop {lr}

    /* current_task was set by os_kernel_start() before SVC was called.
     * Load its SP (first field in TCB struct). */
    ldr r0, =current_task
    ldr r0, [r0]              /* r0 = current_task (TCB pointer) */
    ldr r0, [r0, #0]          /* r0 = current_task->sp (offset 0) */

    /* r0 now contains the PSP value to use.
     * This PSP points to the saved r4-r11 + EXC_RETURN area.
     * Pop those first, then exception return pops the exception frame. */
    ldmia r0!, {r4-r11, lr}   /* Pop r4-r11 + EXC_RETURN from stack */
    msr psp, r0               /* PSP = past the saved regs (points to exception frame) */

    /* Set CONTROL[1] = 1 to use PSP in Thread mode */
    mrs r1, control
    orr r1, r1, #2
    msr control, r1
    isb

    /* Exception return: CPU pops r0-r3, r12, LR, PC, xPSR from PSP.
     * The EXC_RETURN value is already in LR from the ldmia above. */
    bx lr
.size sv_call_handler, .-sv_call_handler
