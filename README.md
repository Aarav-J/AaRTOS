# RTOS from Scratch — STM32F4

A minimal preemptive RTOS for the STM32F4 (ARM Cortex-M4), written in C and ARM assembly. Built to demonstrate deep understanding of the Cortex-M exception model, context switching, scheduling, and synchronization primitives — **no copy-paste from tutorials**.

## Architecture

```
app/src/firmware.c              ← Demo application (tasks, blink, UART)
shared/src/core/kernel.c        ← Scheduler, task management, mutex
shared/src/core/context_switch.S ← PendSV / SVC handlers (assembly)
shared/src/core/system.c        ← SysTick, clock setup
shared/src/core/uart.c          ← UART driver (interrupt-driven RX + ring buffer)
shared/src/core/print.c         ← Tiny printf-free string output
shared/src/core/ring-buffer.c   ← Generic ring buffer
shared/inc/core/                ← Public headers
libopencm3/                     ← CMSIS + HAL library (submodule)
```

## Features

| Feature | Status | Details |
|---|---|---|
| **Preemptive scheduling** | ✅ | Round-robin, 1ms SysTick → PendSV |
| **Context switch** | ✅ | Full register save/restore (r4–r11, PSP, EXC_RETURN) |
| **Task creation** | ✅ | Stack frame setup with exception return |
| **Spinlock mutex** | ✅ | IRQ-safe atomic lock for shared resources (e.g. UART) |
| **`print()`/`println()`** | ✅ | No newlib dependency, just UART writes |
| **Blocking `os_delay()`** | 🚧 | Currently busy-wait; proper tick-based sleep planned |
| **Blocking mutex** | 🚧 | Planned: wait list, task blocking, wake-on-release |
| **Inter-task queue** | 🚧 | Planned: blocking send/receive with ring buffer |
| **Idle task** | 🚧 | Planned: `wfi` loop when no tasks are ready |

## How it works (the interesting parts)

### Context switch (`context_switch.S`)

The PendSV handler (lowest-priority exception) performs the actual task switch:

1. Save callee-saved registers (`r4–r11`) + `EXC_RETURN` onto the current task's PSP
2. Call `save_context()` to stash PSP in the TCB
3. Call `get_next_task()` (round-robin scheduler)
4. Call `load_context()` to get the new task's PSP
5. Restore `r4–r11` + `EXC_RETURN` from the new task's stack
6. `bx lr` — CPU pops the exception frame (`r0–r3`, `r12`, `LR`, `PC`, `xPSR`) automatically

The SVC handler starts the first task by loading its initial stack frame and setting `CONTROL[1]` to switch to PSP in Thread mode.

### Task creation (`kernel.c`)

Each task's stack is pre-initialized to look like it was just preempted:

```
High addr
  xPSR = 0x01000000    ← exception frame (popped by CPU on return)
  PC   = entry
  LR   = 0
  r12  = 0
  r3–r0 = 0 / arg
  LR   = 0xFFFFFFFD    ← callee-saved (popped by PendSV ldmia)
  r11–r4 = 0
Low addr (tcb->sp points here)
```

### Startup race fix

SysTick is **not** enabled in `os_kernel_start()`. Instead it's enabled inside `sv_call_handler`, after the first task's context is installed. This prevents a race where PendSV fires before the first task starts and corrupts its initial stack frame with `main()`'s register state.

## Build & Flash

```bash
# Build the demo application
cd app
make clean && make

# Flash (using st-flash)
make flash
```

Requires:
- `arm-none-eabi-gcc` (GCC ARM embedded toolchain)
- `libopencm3` (included as a git submodule, auto-detected by the Makefile)
- `st-link` / `st-flash` for flashing

## UART output (115200 baud)

After flashing, the firmware boots from a bootloader at `0x08000000`, then jumps to the app at `0x08008000`. On UART (PA2=TX, PA3=RX) you should see:

```
Kernel initialized.
Task 'LED' created.
Task 'PRINT' created.
Starting kernel...
First task: 'LED'
Led on
Led off
Hello, Count:0
Led on
Led off
Hello, Count:1
...
```

The LED toggles on PA5 at 2Hz. Both tasks share the UART via a spinlock mutex to prevent character interleaving.

## Why this is resume-worthy

- **You built a real preemptive kernel**, not a super-loop
- Covers **ARM Cortex-M fundamentals**: PendSV, SysTick, SVC, PSP, EXC_RETURN
- Shows **systems thinking**: interrupts, critical sections, scheduling tradeoffs
- It's a **complete artifact** to point to: "I wrote a working RTOS from scratch for STM32"
