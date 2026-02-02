// thrd_pool.c
#include "thrd_pool.h"
#include "spinlock.h" // 包含了 atomic.h
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>   // for NULL definition implicitly
#include "spinlock.h"

// 任务节点
typedef struct task_s {
    void *next;
    handler_pt func;
    void *arg;
} task_t;

// 任务队列
typedef struct task_queue_s {
    void *head;
    void **tail;
    int block;
    spinlock_t lock;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

// 线程池结构
struct thrdpool_s {
    task_queue_t *task_queue;
    volatile int quit; // 修改：使用 volatile int
    int thrd_count;
    pthread_t *threads;
};

// --- 队列操作 (保持不变，已使用 spinlock) ---

static task_queue_t *
__taskqueue_create() {
    task_queue_t *queue = (task_queue_t *)malloc(sizeof(*queue));
    if (queue) {
        int ret = pthread_mutex_init(&queue->mutex, NULL);
        if (ret == 0) {
            ret = pthread_cond_init(&queue->cond, NULL);
            if (ret == 0) {
                spinlock_init(&queue->lock);
                queue->head = NULL;
                queue->tail = &queue->head;
                queue->block = 1;
                return queue;
            }
            pthread_mutex_destroy(&queue->mutex);
        }
        free(queue);
    }
    return NULL;
}

static void 
__nonblock(task_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    queue->block = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
}

static inline void  
__add_task(task_queue_t *queue, void *task) {
    void ** link = (void **) task;
    *link = NULL;

    spinlock_lock(&queue->lock);
    *queue->tail = link;
    queue->tail = link;
    spinlock_unlock(&queue->lock);

    pthread_cond_signal(&queue->cond);
}

static inline void *
__pop_task(task_queue_t *queue) {
    spinlock_lock(&queue->lock);
    if (queue->head == NULL) {
        spinlock_unlock(&queue->lock);
        return NULL;
    }
    task_t *task;
    task = (task_t *)queue->head;

    void **link = (void **)task;
    queue->head = *link;

    if (queue->head == NULL) {
        queue->tail = &queue->head;
    }
    spinlock_unlock(&queue->lock);
    return task;
}

static inline void *
__get_task(task_queue_t *queue) {
    task_t *task;
    // 1. 快速路径
    task = (task_t *)__pop_task(queue);
    if (task) return task;

    // 2. 慢速路径
    pthread_mutex_lock(&queue->mutex);
    while ((task = (task_t *)__pop_task(queue)) == NULL) {
        if (queue->block == 0) {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    pthread_mutex_unlock(&queue->mutex);
    return task;
}

static void 
__taskqueue_destroy(task_queue_t *queue) {
    task_t *task;
    while ((task = (task_t *)__pop_task(queue))) {
        free(task);
    }
    spinlock_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

// --- 线程工作函数 ---

static void *
__thrdpool_worker(void *arg) {
    thrdpool_t *pool = (thrdpool_t *) arg;
    task_t *task;
    void *ctx;

    // 修改：不再使用 atomic_load，直接读取 volatile 变量
    // 因为 quit 只是从 0 变 1，且有 nonblock 的锁保护流程，这里直接判断即可
    while (1) {
        task = (task_t *)__get_task(pool->task_queue);
        if (!task) {
            break;
        } 
        handler_pt func = task->func;
        ctx = task->arg;
        free(task);
        if (func) {
            func(ctx);
        }
    }
    return NULL;
} 

static void 
__threads_terminate(thrdpool_t *pool) {
    // 修改：直接赋值，配合 memory barrier 确保可见性
    pool->quit = 1; 
    memory_barrier(); 

    __nonblock(pool->task_queue);
    
    int i = 0;
    for (; i < pool->thrd_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

static int 
__threads_create(thrdpool_t *pool, size_t thrd_count) {
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret == 0) {
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thrd_count);
        if (pool->threads) {
            int i = 0;
            for (; i < thrd_count; i++) {
                if (pthread_create(&pool->threads[i], &attr, __thrdpool_worker, pool)) {
                    break;
                }
            }
            pool->thrd_count = i;
            pthread_attr_destroy(&attr);
            if (i == thrd_count) {
                return 0;
            }
            __threads_terminate(pool);
            free(pool->threads);
        }
        ret = -1;
    }
    return ret;
}

// --- API ---

thrdpool_t *thrdpool_create(int thrd_count) {
    thrdpool_t *pool;
    if (thrd_count <= 0) thrd_count = 1;
    
    pool = (thrdpool_t *)malloc(sizeof(*pool));
    if (pool) {
        pool->thrd_count = 0;
        pool->quit = 0; // 普通赋值
        pool->task_queue = __taskqueue_create();
        if (pool->task_queue) {
            if (__threads_create(pool, thrd_count) == 0) {
                return pool;
            }
            __taskqueue_destroy(pool->task_queue);
        }
        free(pool);
    }
    return NULL;
}

int thrdpool_post(thrdpool_t *pool, handler_pt func, void *arg) {
    if (pool == NULL || func == NULL) {
        return -1;
    }
    if (pool->quit) { // 普通判断
        return -1;
    }

    task_t *task = (task_t *)malloc(sizeof(task_t));
    if (task == NULL) {
        return -1;
    }

    task->func = func;
    task->arg = arg;
    task->next = NULL;

    __add_task(pool->task_queue, task);
    return 0;
}

void thrdpool_terminate(thrdpool_t *pool) {
    if (pool == NULL) return;
    __threads_terminate(pool);
    __taskqueue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
}

void thrdpool_waitdone(thrdpool_t *pool) {
   thrdpool_terminate(pool);
}