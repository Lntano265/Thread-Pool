// thrd_pool.h
#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

typedef struct thrdpool_s thrdpool_t;
typedef void (*handler_pt)(void *);

#ifdef __cplusplus
extern "C" {
#endif

// 创建线程池
thrdpool_t *thrdpool_create(int thrd_count);

// 销毁线程池
void thrdpool_terminate(thrdpool_t *pool);

// 投递任务
int thrdpool_post(thrdpool_t *pool, handler_pt func, void *arg);

// 等待所有任务完成
void thrdpool_waitdone(thrdpool_t *pool);

#ifdef __cplusplus
}
#endif

#endif