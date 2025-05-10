#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    volatile uint32_t lock; // 0 = unlocked, 1 = locked
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock)
{
    lock->lock = 0;
}

static inline void spinlock_acquire(spinlock_t *lock)
{
    while (__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE))
    {
        asm volatile("pause" ::: "memory");
    }
}

static inline void spinlock_release(spinlock_t *lock)
{
    __atomic_clear(&lock->lock, __ATOMIC_RELEASE);
}

static inline bool spinlock_try_acquire(spinlock_t *lock)
{
    return !__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE);
}

static inline bool spinlock_held(spinlock_t *lock)
{
    return __atomic_load_n(&lock->lock, __ATOMIC_RELAXED) != 0;
}

#endif // SPINLOCK_H