#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <sys/idt.h>
#include <mm/vma.h>

#define PROC_DEFAULT_TIME 1 // Roughly 20ms, timer is expected to run at roughly 200hz
#define PROC_MAX_PROCS 2048 // Should be plenty

typedef enum
{
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_TERMINATED
} process_state_t;

typedef struct pcb
{
    struct register_ctx ctx;
    uint64_t pid;
    process_state_t state;
    uint64_t timeslice;
    uint64_t *pagemap;
    vma_context_t *vma_ctx;
    bool in_syscall;
} pcb_t;

void scheduler_init();
uint64_t scheduler_spawn(bool user, void (*entry)(void), uint64_t *pagemap);
void scheduler_tick(struct register_ctx *ctx);
void scheduler_exit(int return_code);
pcb_t *scheduler_get_current();
void scheduler_set_final(void (*final)(void));

#endif // SCHED_H