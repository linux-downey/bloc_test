mutex 和 spinlock 实现的本质：
	1、通过一个全局参数记录锁定/非锁定状态，
	2、记录请求锁的顺序，做到先请求锁的先得到锁
实现的细节有很大的区别

owner 保存的是获取到 lock 的进程 task_struct 指针
因为对齐的关系，最后三位用于保存 flag 标志位：

#define MUTEX_FLAG_WAITERS	0x01  // 表示存在 waiters，unlock 的同时要发送唤醒事件


#define MUTEX_FLAG_HANDOFF	0x02  // 表示解锁后需要将锁交给 top-waiter(应该是：如果后续还有等待的，需要移交)
if ((use_ww_ctx && ww_ctx) || !first) {
	first = __mutex_waiter_is_first(lock, &waiter);
	if (first)
		__mutex_set_flag(lock, MUTEX_FLAG_HANDOFF);
}

#define MUTEX_FLAG_PICKUP	0x04  // 第二步的移交已经完成，需要等待 pickup(不懂？)


struct optimistic_spin_queue {
	/*
	 * Stores an encoded value of the CPU # of the tail node in the queue.
	 * If the queue is empty, then it's set to OSQ_UNLOCKED_VAL.
	 */
	atomic_t tail;
};

struct mutex {
	atomic_long_t		owner;
	spinlock_t		wait_lock;
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
	struct optimistic_spin_queue osq; /* Spinner MCS lock */
#endif
	struct list_head	wait_list;
#ifdef CONFIG_DEBUG_MUTEXES
	void			*magic;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map	dep_map;
#endif
};


#define mutex_init(mutex)						\
do {									\
	static struct lock_class_key __key;				\
									\
	__mutex_init((mutex), #mutex, &__key);				\
} while (0)


void __mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
	atomic_long_set(&lock->owner, 0);
	spin_lock_init(&lock->wait_lock);
	INIT_LIST_HEAD(&lock->wait_list);
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
	osq_lock_init(&lock->osq);
#endif
	debug_mutex_init(lock, name, key);
}



## lock

void __sched mutex_lock(struct mutex *lock)
{
	might_sleep();

	if (!__mutex_trylock_fast(lock))
		__mutex_lock_slowpath(lock);
}


static __always_inline bool __mutex_trylock_fast(struct mutex *lock)
{
	unsigned long curr = (unsigned long)current;

	// 初始化情况下，lock->owner 为 0，如果这个锁没有被别人使用，就将当前进程的 curr 值设置为 lock->owner.  
	// 否则就返回 false，表示上锁失败
	// atomic_long_cmpxchg_acquire 的实现为：将p1(参数1)和p2对比，如果相等，p1=p3,返回 p2，如果不相等，返回 p1._acquire是同步相关的指令后缀
	if (!atomic_long_cmpxchg_acquire(&lock->owner, 0UL, curr))  
		return true;

	return false;
}

如果上锁失败，就走到 slowpath：
static noinline void __sched __mutex_lock_slowpath(struct mutex *lock)
{
	__mutex_lock(lock, TASK_UNINTERRUPTIBLE, 0, NULL, _RET_IP_);
}

static int __sched
__mutex_lock(struct mutex *lock, long state, unsigned int subclass,
	     struct lockdep_map *nest_lock, unsigned long ip)
{
	return __mutex_lock_common(lock, state, subclass, nest_lock, ip, NULL, false);
}


__mutex_trylock：这个函数就是判断当前获取 lock 的 owner 是否有 flag，有 flag 就表示除了当前 owner，还存在 waiter。 


mutex_optimistic_spin：512：


static __always_inline bool
mutex_optimistic_spin(struct mutex *lock, struct ww_acquire_ctx *ww_ctx,
		      const bool use_ww_ctx, struct mutex_waiter *waiter)
{
	if (!waiter) {
		
		// 调用 need_resched 来判断是否需要重新调度，如果是，就返回0
		// 通过 owner->on_cpu 来判断 lock 的当前 owner 是否在 cpu 上执行，如果没有就返回0
		// 通过 !vcpu_is_preempted(task_cpu(owner)) 这里存疑？不懂 vcpu_is_preempted 什么意思？
		// 如果返回零，就表示不能 spin，goto fail
		if (!mutex_can_spin_on_owner(lock))
			goto fail;

		
		if (!osq_lock(&lock->osq))                     // 存疑，没搞懂
			goto fail;
	}
	
	for (;;) {
		struct task_struct *owner;

		/* Try to acquire the mutex... */
		owner = __mutex_trylock_or_owner(lock);
		if (!owner)                                       // owner 为空，表示已经释放了
			break;

		
		if (!mutex_spin_on_owner(lock, owner, ww_ctx, waiter))
			goto fail_unlock;                             // 出现了其它情况，不能继续 spin

		/*
		 * The cpu_relax() call is a compiler barrier which forces
		 * everything in this loop to be re-loaded. We don't need
		 * memory barriers as we'll eventually observe the right
		 * values at the cost of a few extra spins.
		 */
		cpu_relax();
	}

}


static __always_inline int __sched
__mutex_lock_common(struct mutex *lock, long state, unsigned int subclass,
		    struct lockdep_map *nest_lock, unsigned long ip,
		    struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx)
{
	大体思路：检查是否能 spin，如果不能 spin 就进入到睡眠状态。
	细节部分：
		1、检查是否能 spin(思路就是判断锁是不是很快会被释放)：
			1、当前获取锁的 owner 正在其它 CPU 上执行
			2、当前锁上没有其它的等待进程，如果还有其他等待的，就不能 spin
			3、没有设置调度标志(不能完全理解)
			4、vcpu_is_preempted 返回值(不理解这个函数的真实含义)
		2、满足上述条件，进入到 spin，spin过程中可能有三种结果：
			1、锁没有被释放，继续 spin
			2、锁已经被释放，spin 成功，可以获取锁
			3、出现了异常情况，不能继续 spin，异常情况包含上述的 3、4 点。
	
	如果不能 spin 或者 spin 失败的情况：
		1、将 task->blocked_on.list 添加到 lock->wait_list 中
		2、设置进程睡眠状态，然后执行调度函数 schedule
}



## unlock

void __sched mutex_unlock(struct mutex *lock)
{
	// 如果加密进程是当前进程，调用该函数解锁成功，否则解锁失败
	// 如果 owner 不相等，无法解锁
	// 如果有其它的等待进程，即 owner 的低 3 bits 不为0，也无法解锁
	if (__mutex_unlock_fast(lock))
		return;
	
	// _RET_IP_ 是当前函数的返回地址
	__mutex_unlock_slowpath(lock, _RET_IP_);
}

// 判断当前进程是不是加锁的进程，如果是，将 lock->owner 设置为0，表示解锁成功，否则返回 lock->owner,表示解锁失败。 
static __always_inline bool __mutex_unlock_fast(struct mutex *lock)
{
	unsigned long curr = (unsigned long)current;

	if (atomic_long_cmpxchg_release(&lock->owner, curr, 0UL) == curr)
		return true;

	return false;
}



复盘解析：
mutex_optimistic_spin 函数：
	mutex_can_spin_on_owner(lock)：
		if (need_resched())         // 判断有没有更高优先级的进程就绪，如果有，就不 spin，调用 schedule 切换进程
		if (!osq_lock(&lock->osq))  // 尝试获取 osq_lock，如果获取失败，返回失败，不 spin 
		








