#include <sys/sched.h>
#include <mm/kmalloc.h>
#include <lib/assert.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/spinlock.h>
#include <lib/string.h>

#define KERNEL_STACK_SIZE (4 * PAGE_SIZE)
#define USER_STACK_SIZE (8 * PAGE_SIZE)

static pcb_t **procs = NULL;
static uint64_t proc_count = 0;
static uint64_t current_pid = 0;
static spinlock_t scheduler_lock = {0};
static void (*die_func)(void) = NULL;

void map_range_to_pagemap(uint64_t *dest_pagemap, uint64_t *src_pagemap, uint64_t start, uint64_t size, uint64_t flags)
{
    for (uint64_t offset = 0; offset < size; offset += PAGE_SIZE)
    {
        uint64_t phys = virt_to_phys(src_pagemap, start + offset);
        if (phys)
        {
            vmm_map(dest_pagemap, start + offset, phys, flags);
        }
    }
}

void scheduler_init()
{
    procs = (pcb_t **)kcalloc(PROC_MAX_PROCS, sizeof(pcb_t *));
    if (!procs)
    {
        err("Failed to allocate scheduler process table");
        kpanic(NULL, "Scheduler initialization failed");
    }

    proc_count = 0;
    current_pid = 0;
    spinlock_init(&scheduler_lock);
    info("Scheduler initialized with %d max processes", PROC_MAX_PROCS);
}

uint64_t scheduler_spawn(bool user, void (*entry)(void), uint64_t *pagemap)
{
    spinlock_acquire(&scheduler_lock);

    if (proc_count >= PROC_MAX_PROCS)
    {
        spinlock_release(&scheduler_lock);
        err("Maximum process limit reached");
        return -1;
    }

    pcb_t *proc = (pcb_t *)kmalloc(sizeof(pcb_t));
    if (!proc)
    {
        spinlock_release(&scheduler_lock);
        err("Failed to allocate process control block");
        return -1;
    }

    memset(proc, 0, sizeof(pcb_t));
    proc->pid = proc_count++;
    proc->state = PROCESS_READY;
    proc->timeslice = PROC_DEFAULT_TIME;
    proc->pagemap = pagemap ? pagemap : vmm_new_pagemap();
    if (!proc->pagemap)
    {
        kfree(proc);
        proc_count--;
        spinlock_release(&scheduler_lock);
        err("Failed to create pagemap for process");
        return -1;
    }

    proc->vma_ctx = vma_create_context(proc->pagemap);
    if (!proc->vma_ctx)
    {
        vmm_destroy_pagemap(proc->pagemap);
        kfree(proc);
        proc_count--;
        spinlock_release(&scheduler_lock);
        err("Failed to create VMA context");
        return -1;
    }

    uint64_t stack_size = user ? USER_STACK_SIZE : KERNEL_STACK_SIZE;
    uint64_t map_flags = VMM_PRESENT | VMM_WRITE | (user ? VMM_USER : 0);
    void *stack = vma_alloc(proc->vma_ctx, stack_size / PAGE_SIZE, map_flags);
    if (!stack)
    {
        vma_destroy_context(proc->vma_ctx);
        vmm_destroy_pagemap(proc->pagemap);
        kfree(proc);
        proc_count--;
        spinlock_release(&scheduler_lock);
        err("Failed to allocate stack");
        return -1;
    }

    proc->ctx.rip = (uint64_t)entry;
    proc->ctx.rsp = (uint64_t)stack + stack_size - 8;
    proc->ctx.rflags = 0x202;
    proc->ctx.cs = user ? 0x1B : 0x08;
    proc->ctx.ss = user ? 0x23 : 0x10;

    vmm_map(proc->pagemap, (uint64_t)proc, (uint64_t)proc, map_flags);
    map_range_to_pagemap(proc->pagemap, kernel_pagemap, (uint64_t)procs, sizeof(pcb_t *) * PROC_MAX_PROCS, map_flags);
    map_range_to_pagemap(proc->pagemap, kernel_pagemap, 0x1000, 0x10000, map_flags);

    proc->in_syscall = false;
    procs[proc->pid] = proc;

    info("Spawned process %d (user=%d, entry=%p, pagemap=%p)", proc->pid, user, entry, proc->pagemap);
    spinlock_release(&scheduler_lock);
    return proc->pid;
}

static uint64_t scheduler_find_next_runnable(void)
{
    uint64_t start_pid = (current_pid + 1) % proc_count;
    for (uint64_t i = 0; i < proc_count; i++)
    {
        uint64_t pid = (start_pid + i) % proc_count;
        if (procs[pid] && procs[pid]->state == PROCESS_READY)
        {
            return pid;
        }
    }
    return start_pid;
}

static void scheduler_cleanup_process(pcb_t *proc)
{
    if (!proc)
        return;

    if (proc->vma_ctx)
        vma_destroy_context(proc->vma_ctx);
    if (proc->pagemap)
        vmm_destroy_pagemap(proc->pagemap);

    procs[proc->pid] = NULL;
    kfree(proc);
    proc_count--;
}

void scheduler_tick(struct register_ctx *ctx)
{
    spinlock_acquire(&scheduler_lock);

    if (proc_count == 0)
    {
        spinlock_release(&scheduler_lock);
        return;
    }

    pcb_t *current = procs[current_pid];
    if (current && current->state == PROCESS_RUNNING)
    {
        if (!current->in_syscall)
        {
            memcpy(&current->ctx, ctx, sizeof(struct register_ctx));
            if (--current->timeslice == 0)
            {
                current->state = PROCESS_READY;
                current->timeslice = PROC_DEFAULT_TIME;
                current_pid = scheduler_find_next_runnable();
            }
        }
    }

    pcb_t *next = procs[current_pid];
    if (next && next->state == PROCESS_READY)
    {
        next->state = PROCESS_RUNNING;
        memcpy(ctx, &next->ctx, sizeof(struct register_ctx));
        vmm_switch_pagemap(next->pagemap);
    }
    else if (next && next->state == PROCESS_TERMINATED)
    {
        scheduler_cleanup_process(next);
        current_pid = scheduler_find_next_runnable();
    }

    spinlock_release(&scheduler_lock);
}

void scheduler_exit(int return_code)
{
    spinlock_acquire(&scheduler_lock);

    pcb_t *proc = procs[current_pid];
    if (!proc)
    {
        spinlock_release(&scheduler_lock);
        err("No process to exit (pid: %d)", current_pid);
        return;
    }

    proc->state = PROCESS_TERMINATED;
    info("Process %d exited with code %d", proc->pid, return_code);

    if (proc_count == 1)
    {
        scheduler_cleanup_process(proc);
        spinlock_release(&scheduler_lock);
        if (die_func)
            die_func();
        hlt();
    }

    current_pid = scheduler_find_next_runnable();
    spinlock_release(&scheduler_lock);
}

pcb_t *scheduler_get_current()
{
    spinlock_acquire(&scheduler_lock);
    pcb_t *proc = procs ? procs[current_pid] : NULL;
    spinlock_release(&scheduler_lock);
    return proc;
}

void scheduler_set_final(void (*final)(void))
{
    spinlock_acquire(&scheduler_lock);
    die_func = final;
    spinlock_release(&scheduler_lock);
}