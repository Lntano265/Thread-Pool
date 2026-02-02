// main.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "thrd_pool.h"
#include "spinlock.h"  

// 共享资源，用于测试线程安全
static int g_counter = 0;
static spinlock_t g_lock; 

// 任务1：模拟耗时操作
void task_heavy(void *arg) {
    int id = *(int *)arg;
    free(arg); 
    usleep(10000); // 10ms
}

// 任务2：并发累加
void task_counter(void *arg) {
    (void)arg;
    spinlock_lock(&g_lock);
    g_counter++;
    spinlock_unlock(&g_lock);
}

int main() {
    int n_threads = 4;
    int n_tasks = 1000;
    
    printf("=== High Performance Thread Pool Test ===\n");
    printf("Pool Threads: %d\n", n_threads);
    printf("Total Tasks:  %d\n", n_tasks);

    thrdpool_t *pool = thrdpool_create(n_threads);
    if (!pool) {
        fprintf(stderr, "Create pool failed\n");
        return -1;
    }
    printf("[1] Pool created.\n");

    spinlock_init(&g_lock);

    printf("[2] Posting %d counter tasks...\n", n_tasks);
    for (int i = 0; i < n_tasks; i++) {
        thrdpool_post(pool, task_counter, NULL);
    }

    for (int i = 0; i < 10; i++) {
        int *arg = (int *)malloc(sizeof(int));
        *arg = i;
        thrdpool_post(pool, task_heavy, arg);
    }
    printf("[3] Tasks posted. Waiting for completion...\n");

    thrdpool_waitdone(pool);
    
    printf("[4] Pool destroyed.\n");

    if (g_counter == n_tasks) {
        printf("SUCCESS: Counter = %d (Expected %d)\n", g_counter, n_tasks);
    } else {
        printf("FAILED: Counter = %d (Expected %d)\n", g_counter, n_tasks);
    }

    return 0;
}