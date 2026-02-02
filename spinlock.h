// spinlock.h
#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "atomic.h"

typedef struct {
    volatile int lock;
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock) {
    lock->lock = 0;
}

static inline void spinlock_lock(spinlock_t *lock) {
    while (atomic_test_and_set(&lock->lock)) {
    }
}

static inline void spinlock_unlock(spinlock_t *lock) {
    atomic_clear(&lock->lock);
}

static inline void spinlock_destroy(spinlock_t *lock) {
    (void)lock;
}

#endif