高性能 C 语言线程池 (High Performance C Thread Pool)
这是一个基于 C 语言实现的轻量级、高性能线程池。它不依赖 C11 标准库 (<stdatomic.h>)，而是直接使用 GCC/Clang 编译器内置的原子指令 (__sync_* built-ins)，因此具有极佳的移植性（兼容老版本编译器）和执行效率。
📁 项目文件结构
main.c: 测试程序，包含并发计数测试和模拟耗时任务。
thrd_pool.c / thrd_pool.h: 线程池的核心实现与接口定义。
spinlock.h: 基于原子操作手写的自旋锁实现。
atomic.h: 封装编译器内置原子操作，替代标准库。
🚀 编译与运行
本项目仅依赖 POSIX 线程库 (pthread)。
1. 编译
使用 GCC 或 Clang 进行编译，需要链接 pthread 库：
code
Bash
gcc -o threadpool_test main.c thrd_pool.c -lpthread -O2
-O2: 开启优化，能更好地发挥内联函数 (inline) 和自旋锁的性能。
-lpthread: 链接 POSIX 线程库。
2. 运行
编译成功后，执行生成的可执行文件：
code
Bash
./threadpool_test
📊 预期输出结果
如果代码运行正确且无竞态条件（Race Condition），你应该看到如下输出。重点在于最后的 SUCCESS，表示并发累加的计数结果与预期一致。
code
Text
=== High Performance Thread Pool Test ===
Pool Threads: 4
Total Tasks:  1000
[1] Pool created.
[2] Posting 1000 counter tasks...
[3] Tasks posted. Waiting for completion...
[4] Pool destroyed.
SUCCESS: Counter = 1000 (Expected 1000)
(注意：由于多线程执行顺序的不确定性，具体的打印顺序可能微调，但最终结果必须正确)
💡 代码实现深度讲解
本实现的"高性能"主要体现在以下三个核心设计上：
1. 手写原子操作与脱离 libc 依赖 (atomic.h)
为了在不支持 C11 <stdatomic.h> 的环境（如旧版 GCC 或嵌入式系统）中运行，且为了追求极致性能，我们封装了编译器内置函数：
实现原理：映射到汇编指令（如 lock xadd, lock cmpxchg）。
关键宏：
atomic_cmp_set: 比较并交换 (CAS)，无锁编程的核心。
atomic_test_and_set: 用于实现自旋锁。
memory_barrier: 防止编译器指令重排。
2. 混合锁机制 (Hybrid Locking Strategy)
这是性能优化的关键。线程池并未全程使用重量级的 pthread_mutex，而是结合了 自旋锁 (Spinlock) 和 互斥锁 (Mutex)。
快路径 (Fast Path) - 自旋锁:
场景：任务队列的 push (投递) 和 pop (获取) 操作。
理由：链表节点的指针修改操作极快（纳秒级）。使用 pthread_mutex 会导致线程进入内核态并发生上下文切换（微秒级），开销巨大。使用自旋锁（用户态忙等待）能大幅提升吞吐量。
慢路径 (Slow Path) - 互斥锁 + 条件变量:
场景：当队列为空，Worker 线程需要等待任务时。
理由：如果队列长时间为空，自旋锁会导致 CPU 空转（占满 100%）。此时必须使用 pthread_cond_wait 让线程挂起休眠，释放 CPU 资源。
3. 双重检查锁定 (Double-Check Locking)
在 __get_task 函数中，解决了一个经典的丢失唤醒 (Lost Wakeup) 问题：
code
C
// 伪代码逻辑演示
task = try_pop_with_spinlock();
if (!task) {
    lock(mutex); // 获取互斥锁准备休眠
    
    // [关键点]：在持有互斥锁后，必须再次检查队列！
    // 因为在 "尝试获取锁" 的间隙，可能有新任务被投递并发送了 signal。
    // 如果不重试，那个 signal 就会被错过，导致线程即使有任务也在死等。
    while ((task = pop_with_spinlock()) == NULL) {
        if (shutdown) unlock & return;
        wait(cond, mutex);
    }
    unlock(mutex);
}
4. 优雅退出 (Graceful Shutdown)
不同于粗暴的 kill，本线程池保证 "关门不赶人"：
用户调用 thrdpool_terminate。
设置全局标志 quit = 1。
唤醒所有正在休眠的线程 (broadcast)。
Worker 线程被唤醒后，会继续处理队列中剩余的任务。
只有当 quit=1 且 队列为空 时，Worker 线程才会真正的退出。
这种机制确保了已经提交的业务数据不会丢失。