#include <sys/sched.h>
#include <sys/spinlock.h>
#include <mm/kmalloc.h>
#include <util/log.h>
#include <lib/string.h>
#include <sys/cpu.h>
#include <lib/assert.h>

pcb_t **procs;
uint64_t count = 0;
uint64_t current_pid = 0;
spinlock_t lock = {0};

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
    procs = (pcb_t **)kcalloc(sizeof(pcb_t *), PROC_MAX_PROCS);
    if (!procs)
    {
        err("Failed to initialize scheduler process list");
        return;
    }

    info("Initialized scheduler process list, %d bytes (%d max processes)", sizeof(pcb_t *) * PROC_MAX_PROCS, PROC_MAX_PROCS);
}

uint64_t scheduler_spawn(bool user, void (*entry)(void), uint64_t *pagemap)
{
    pcb_t *proc = (pcb_t *)kmalloc(sizeof(pcb_t));
    if (!proc)
    {
        err("Failed to allocate memory for new process");
        return -1;
    }

    proc->pid = count++;
    proc->state = PROCESS_READY;
    proc->ctx.rip = (uint64_t)entry;
    proc->pagemap = pagemap;
    proc->vma_ctx = vma_create_context(proc->pagemap);

    // Setup stack and other shit
    uint64_t stack_size = 4;
    uint64_t map_flags = VMM_PRESENT | VMM_WRITE;
    if (user)
    {
        // map_flags |= VMM_USER;
        // proc->ctx.cs = 0x1B; // User code segment
        // proc->ctx.ss = 0x23; // User data segment
        err("Userspace procs are not supported");
        return 0;
    }
    else
    {
        proc->ctx.cs = 0x08; // Kernel code segment
        proc->ctx.ss = 0x10; // Kernel data segment
    }

    proc->ctx.rsp = (uint64_t)vma_alloc(proc->vma_ctx, stack_size, map_flags) + ((PAGE_SIZE * stack_size) - 1);
    proc->ctx.rflags = 0x202;

    vmm_map(proc->pagemap, (uint64_t)proc, (uint64_t)proc, map_flags);
    map_range_to_pagemap(proc->pagemap, kernel_pagemap, (uint64_t)procs, sizeof(pcb_t *) * PROC_MAX_PROCS, map_flags);
    map_range_to_pagemap(proc->pagemap, kernel_pagemap, 0x1000, 0x10000, map_flags);

    // Set up some default values
    proc->timeslice = PROC_DEFAULT_TIME;
    proc->in_syscall = false;

    return proc->pid;
}

void scheduler_tick(struct register_ctx *ctx)
{
    spinlock_acquire(&lock);
    if (count == 0)
    {
        spinlock_release(&lock);
        return;
    }

    pcb_t *proc = procs[current_pid];
    if (proc && proc->state == PROCESS_RUNNING)
    {
        // Skip decrementing the timeslice if the process is in a syscall.
        if (!proc->in_syscall)
        {
            memcpy(&proc->ctx, ctx, sizeof(struct register_ctx));

            if (--proc->timeslice == 0)
            {
                proc->state = PROCESS_READY;
                proc->timeslice = PROC_DEFAULT_TIME;
                current_pid = (current_pid + 1) % count;
            }
        }
    }

    pcb_t *next_proc = procs[current_pid];
    assert(ctx && next_proc);
    if (next_proc)
    {
        if (next_proc->state == PROCESS_READY)
        {
            next_proc->state = PROCESS_RUNNING;
            assert(next_proc->pagemap);
            assert(ctx);
            assert(&next_proc->ctx);
            memcpy(ctx, &next_proc->ctx, sizeof(struct register_ctx));
            vmm_switch_pagemap(next_proc->pagemap);
        }
        else if (next_proc->state == PROCESS_TERMINATED)
        {
            procs[next_proc->pid] = NULL;
            count--;

            if (count == 0)
                spinlock_release(&lock);
            {
                return;
            }

            current_pid = (current_pid + 1) % count;
        }
    }
    spinlock_release(&lock);
}

void scheduler_exit(int return_code)
{
    pcb_t *proc = procs[current_pid];
    if (proc)
    {
        proc->ctx.rip = 0;
        vmm_destroy_pagemap(proc->pagemap);
        kfree(proc);

        procs[proc->pid] = NULL;
        count--;

        current_pid = (count == 0) ? 0 : (current_pid + 1) % count;
        info("Process %d exited with return code %d", proc->pid, return_code);

        if (count == 0)
        {
            info("No more processes available, freezing scheduler.");
            hlt();
        }
    }
    else
    {
        err("No process to exit (current_pid: %d)", current_pid);
    }
}

pcb_t *scheduler_get_current()
{
    if (procs == NULL)
        return NULL;
    return procs[current_pid];
}
