// atomic.h
#ifndef _ATOMIC_H_
#define _ATOMIC_H_

/* 
 * 封装编译器内置原子操作 (GCC/Clang)
 * 对应汇编指令如 lock xadd, lock cmpxchg 等
 */

// 1. 比较并交换 (CAS - Compare And Swap)
// 如果 *ptr == old_val，则将 *ptr 设置为 new_val，返回 true
// 否则返回 false
#define atomic_cmp_set(ptr, old_val, new_val) \
    __sync_bool_compare_and_swap(ptr, old_val, new_val)

// 2. 获取并添加 (Fetch and Add)
// 返回加之前的旧值
#define atomic_fetch_add(ptr, val) \
    __sync_fetch_and_add(ptr, val)

// 3. 测试并置位 (Test and Set) - 专门用于自旋锁
// 将 *ptr 设为 1，并返回操作前的值
#define atomic_test_and_set(ptr) \
    __sync_lock_test_and_set(ptr, 1)

// 4. 锁释放
// 将 *ptr 设为 0
#define atomic_clear(ptr) \
    __sync_lock_release(ptr)

// 5. 内存屏障 (Memory Barrier)
// 防止编译器或 CPU 对指令重排序
#define memory_barrier() \
    __sync_synchronize()

#endif