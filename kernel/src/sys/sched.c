#include <sys/sched.h>
#include <mm/kmalloc.h>
#include <lib/assert.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/spinlock.h>
#include <lib/string.h>

#define PRINT_PROC(tpid)                                                                                                                                 \
    do                                                                                                                                                   \
    {                                                                                                                                                    \
        if (tpid < count && procs[tpid] != NULL)                                                                                                         \
        {                                                                                                                                                \
            pcb_t *p = procs[tpid];                                                                                                                      \
            info("{\"pid\":%llu,\"state\":%d,\"entry\": \"0x%llx\",\"pagemap\": \"0x%llx\",\"stack\": \"0x%llx\",\"timeslice\":%llu,\"in_syscall\":%d}", \
                 p->pid, p->state, p->ctx.rip, p->pagemap, p->ctx.rsp, p->timeslice, p->in_syscall);                                                     \
        }                                                                                                                                                \
        else                                                                                                                                             \
        {                                                                                                                                                \
            err("Invalid pid %llu or null process", tpid);                                                                                               \
        }                                                                                                                                                \
    } while (0)

static pcb_t **procs = NULL;
static uint64_t count = 0;
static uint64_t current_pid = 0;
static spinlock_t lock = {0};
static void (*die_func)(void) = NULL;

void map_range_to_pagemap(uint64_t *dest_pagemap, uint64_t *src_pagemap, uint64_t start, uint64_t size, uint64_t flags)
{
    if (!dest_pagemap || !src_pagemap)
    {
        err("Invalid pagemap parameters");
        return;
    }

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
        kpanic(NULL, "Failed to allocate scheduler process list");
    }

    count = 0;
    current_pid = 0;
    spinlock_init(&lock);
    info("Initialized scheduler with %d max processes (%d bytes)", PROC_MAX_PROCS, sizeof(pcb_t *) * PROC_MAX_PROCS);
}

uint64_t scheduler_spawn(bool user, void (*entry)(void), uint64_t *pagemap)
{
    if (!entry || !pagemap)
    {
        err("Invalid entry point or pagemap");
        return -1;
    }

    spinlock_acquire(&lock);

    if (count >= PROC_MAX_PROCS)
    {
        spinlock_release(&lock);
        err("Maximum process limit reached");
        return -1;
    }

    pcb_t *proc = (pcb_t *)kmalloc(sizeof(pcb_t));
    if (!proc)
    {
        spinlock_release(&lock);
        err("Failed to allocate process control block");
        return -1;
    }

    memset(proc, 0, sizeof(pcb_t));
    proc->pid = count++;
    proc->state = PROCESS_READY;
    proc->ctx.rip = (uint64_t)entry;
    proc->pagemap = pagemap;
    proc->vma_ctx = vma_create_context(pagemap);

    if (!proc->vma_ctx)
    {
        kfree(proc);
        spinlock_release(&lock);
        err("Failed to create VMA context");
        return -1;
    }

    const uint64_t stack_size = 4;
    uint64_t map_flags = VMM_PRESENT | VMM_WRITE;
    if (user)
    {
        map_flags |= VMM_USER;
        proc->ctx.cs = 0x1B;
        proc->ctx.ss = 0x23;
    }
    else
    {
        proc->ctx.cs = 0x08;
        proc->ctx.ss = 0x10;
    }

    proc->ctx.rsp = (uint64_t)vma_alloc(proc->vma_ctx, stack_size, map_flags);
    if (!proc->ctx.rsp)
    {
        vma_destroy_context(proc->vma_ctx);
        kfree(proc);
        spinlock_release(&lock);
        err("Failed to allocate stack");
        return -1;
    }
    proc->ctx.rsp += (PAGE_SIZE * stack_size) - 1;
    proc->ctx.rflags = 0x202;

    vmm_map(proc->pagemap, (uint64_t)proc, (uint64_t)proc, map_flags);
    map_range_to_pagemap(proc->pagemap, kernel_pagemap, (uint64_t)procs, sizeof(pcb_t *) * PROC_MAX_PROCS, map_flags);
    map_range_to_pagemap(proc->pagemap, kernel_pagemap, 0x1000, 0x10000, map_flags);

    proc->timeslice = PROC_DEFAULT_TIME;
    proc->in_syscall = false;
    procs[proc->pid] = proc;

    info("Spawned process %llu (%p) with entry %p, pagemap %p", proc->pid, proc, entry, pagemap);
    spinlock_release(&lock);
    return proc->pid;
}

void scheduler_tick(struct register_ctx *ctx)
{
    if (!procs || !ctx)
    {
        err("Scheduler not initialized or invalid context");
        return;
    }

    spinlock_acquire(&lock);

    if (count == 0)
    {
        spinlock_release(&lock);
        if (die_func)
        {
            die_func();
        }
        info("No processes available, freezing scheduler");
        hlt();
        return;
    }

    PRINT_PROC(current_pid);

    pcb_t *current = procs[current_pid];
    if (current)
    {
        if (current->state == PROCESS_RUNNING && !current->in_syscall)
        {
            memcpy(&current->ctx, ctx, sizeof(struct register_ctx));
            if (--current->timeslice == 0)
            {
                current->state = PROCESS_READY;
                current->timeslice = PROC_DEFAULT_TIME;
                current_pid = (current_pid + 1) % count;
            }
        }

        if (current->state == PROCESS_CORRUPT)
        {
            scheduler_exit(SCHEDULER_RETURN_CORRUPT);
            spinlock_release(&lock);
            return;
        }

        if (current->state == PROCESS_TERMINATED)
        {
            procs[current_pid] = NULL;
            count--;
            kfree(current);
            current_pid = (current_pid + 1) % count;
        }
    }

    uint64_t start_pid = current_pid;
    pcb_t *next_proc = NULL;
    do
    {
        next_proc = procs[current_pid];
        if (next_proc && next_proc->state == PROCESS_READY)
        {
            break;
        }
        current_pid = (current_pid + 1) % count;
    } while (current_pid != start_pid && count > 0);

    if (next_proc && next_proc->state == PROCESS_READY)
    {
        next_proc->state = PROCESS_RUNNING;
        memcpy(ctx, &next_proc->ctx, sizeof(struct register_ctx));
        vmm_switch_pagemap(next_proc->pagemap);
    }

    spinlock_release(&lock);
}

void scheduler_exit(int return_code)
{
    spinlock_acquire(&lock);

    pcb_t *proc = procs[current_pid];
    if (!proc)
    {
        spinlock_release(&lock);
        err("No process to exit (pid: %llu)", current_pid);
        return;
    }

    info("Process %llu exiting with code %d", proc->pid, return_code);

    proc->state = PROCESS_TERMINATED;
    proc->ctx.rip = 0;
    vmm_destroy_pagemap(proc->pagemap);
    vma_destroy_context(proc->vma_ctx);

    if (count == 1)
    {
        procs[current_pid] = NULL;
        count = 0;
        kfree(proc);
        current_pid = 0;
        spinlock_release(&lock);

        info("Last process terminated, freezing scheduler");
        if (die_func)
        {
            die_func();
        }
        hlt();
        return;
    }

    procs[current_pid] = NULL;
    count--;
    kfree(proc);
    current_pid = (current_pid + 1) % count;

    spinlock_release(&lock);
}

pcb_t *scheduler_get_current(void)
{
    spinlock_acquire(&lock);
    pcb_t *proc = (procs && current_pid < count) ? procs[current_pid] : NULL;
    spinlock_release(&lock);
    return proc;
}

void scheduler_set_final(void (*final)(void))
{
    spinlock_acquire(&lock);
    die_func = final;
    spinlock_release(&lock);
}