#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

typedef struct thrdpool_s thrdpool_t;
typedef void (*handler_pt)(void *);

#ifndef __cplusplus
extern "C"
{
#endif

// 对称处理
// 创建线程池
thrdpool_t *thrdpool_create(int thrd_count);

// 销毁线程池
void thrdpool_terminate(thrdpool_t * pool);

// 投递任务
// 向线程池的任务队列中添加一个新任务。这是一个生产者操作。
int thrdpool_post(thrdpool_t *pool, handler_pt func, void *arg);

// 等待线程池的退出
void thrdpool_waitdone(thrdpool_t *pool);

#ifndef __cplusplus
}
#endif

#endif
