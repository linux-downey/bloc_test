# mutex lock
spin lock 是在 linux 内核中实现的一种忙等机制，本质上是对 mutex lock 的一种优化，对于那些执行时间非常短的临界区而言，没有必要让进程进入休眠，因为进程切换的开销可能远大于临界区执行时间，因此就设计了 spinlock 的机制代替 mutex lock 来提升锁的性能。  

而对于更小的共享对象操作，比如单个 int、long 类型的共享变量的操作，则通过原子操作来实现，这是架构相关的。   

mutex lock 本身在操作系统领域是一个通用的概念，不仅仅是 linux 中，对于其它的操作系统同样提供了 mutex lock，其操作接口基本上是一样的：使用互斥的临界区保护共享对象，当临界区被其它进程占用时，尝试进入的临界区的进程进入休眠，等待占用者退出临界区，这个占用和退出的过程对应 mutex 的加锁和解锁。  

对于 mutex 而言，尽管各操作系统之间对外的接口几乎一致，但是其具体的实现一般不同，本章节就是讨论 linux 内核中 mutex lock 的实现。    




# 互斥锁
抛开 linux 的实现，从通用的角度来看 spinlock 和 mutex lock 这种二义性的锁，它们的基本特性也比较简单：



1、加锁和解锁由同一个进(线)程完成

2、使用一个状态标志，通过这个标志来控制、判断加解锁

3、实现排队机制，作为同样等待的进程，先进入等待的先获取锁

通过这三点，可以发现通用的二义性互斥锁的实现其实是比较简单的，使用一个结构体来描述所有成员，结构体的成员包括：



* 当前进(线)程识别号，用于在唤醒时找到对应的进(线)程，对于 spin lock，并不需要这个，因为 spin lock 在加解锁的过程中不允许切换进程(是否允许中断依赖于使用的接口)
* 加解锁状态标志，加解锁主要就是操作这个标志来实现，这里面有一个值得注意的问题是：对这个标志的操作必须是原子化的，因为这个标志也会产生竞态，通常这种原子化由平台实现。  
* 排队结构体，通常是链表，也可以是其它的数据结构。和加解锁状态标志一样存在一个问题，这个结构体同样会产生竞争问题，需要通过特定的方式解决，比如禁止中断、或者使用其它同步机制来保证竞态下的数据安全。   

如果你看一个实时操作系统的互斥锁实现，大体与上面描述的相差不大，但是在 linux 内核中的实现，要显得复杂很多，一方面是因为 linux 中的并行化环境更为复杂，包括中断、中断下半部、内核抢占以及非常棘手的多核环境。另一方面，随着 Linux 的发展，对执行效率提出了更高的要求，在不断地优化中自然带来了更大的复杂性。  

不过话说回来，不管 mutex 处于多复杂的环境，linux 内核对 mutex lock 如何优化，始终离不开它的那三个特性，只是实现的细节变得更加复杂，这一章我们就来看看内核中的 mutex lock 是如何实现的。    




## mutex 使用规则
在解析之前，先了解 mutex 的使用以及一些使用规则：



* 使用接口有三个：mutex_init,mutex_lock,mutex_unlock,至于如何使用应该不用多说
* 同一时间只能由一个 task 占用该锁
* 只有锁的占有者才能解锁
* 不允许重复解锁
* 不允许递归加锁
* mutex lock 必须由 mutex_init 进行初始化，而不是简单地将所有成员置 0，比如使用 memset 或者 memcpy。
* mutex lock 不能在中断或者其它不允许睡眠的场景使用
* 获取锁的进程不能退出，这会导致其它请求锁的进程饿死




## mutex 数据结构
和内核中其它组件一样，数据结构基本就反映了该组件的大部分信息，mutex lock 对应 struct mutex 结构：

```C++
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
```
struct mutex 数据成员中，CONFIG_DEBUG_MUTEXES 和 CONFIG_DEBUG_LOCK_ALLOC 这两个宏属于 debug 相关的，本章节不做分析。其它的主要成员主要是四个：owner，wait_lock，osq，wait_list。  

参照我们上面对 mutex lock 基本特性的分析，从字面上可以看出：owner 用于记录进程信息，而 wait_list 链表用于实现等待进程的排队，但是好像并没有看到专门用于记录锁状态的字段，同时还多了两个字段：一个 spinlock 和一个看不懂的 struct optimistic_spin_queue osq 字段，这里面有什么玄机呢？  

首先，我们暂时抛开 mutex 数据结构，来考虑一个问题：在内核中使用 mutex 的时候，获取不到锁的就一定要陷入睡眠吗？

实际上并不是，在一个进程尝试获取锁时，如果发现占用当前锁的进程正在另一个 CPU 上的临界区运行，就可以乐观地假设该进程会很快地运行完，这种情况下不需要让进程进行休眠，在多核环境下，尤其是锁竞争比较激烈的情况下，这一项优化有非常好的效果，毕竟执行两次调度的时间肯定大于spin 需要的时间。  

linux 内核中对于 mutex 最大的优化就是这一点，这种优化并不是免费的午餐，它同时也会带来一些问题，同时引入了更多的复杂性，基于这一点我们再进入到 mutex 的具体实现。 



## 为什么要避免睡眠

了解操作系统的朋友都清楚，进程睡眠意味着操作系统的进程切换，进程切换的开销并不小，大部分进程相关的运行数据都需要重新加载，而锁的请求者进入睡眠和不进入睡眠的方式相差着两次进程切换，所以如果可以的话，原地自旋是可以成倍地提升锁的性能，这种性能的提升在越是繁忙的系统上体现得越明显。   

另一方面，随着硬件的发展，cache 常常要被考虑到效率的评估中，进程切换意味着 cache 的失效，所有的 cache 都要重新加载(一般是二级或者三级 cache)，而原地自旋的情况下，所有的 cache line 都是热乎的，执行起来也就非常地顺畅，在内核中，cache 的命中和一致性在效率评估中通常要被考虑进去。  



## 优化的代价

如果你正在阅读 mutex lock 的源代码，就会发现这种优化会带来一个问题：破坏了 mutex lock 的公平性，也就是等待者的顺序在某些情况下无法得到保证。   

在没有优化的情况下,mutex lock 的顺序是由 wait_list 来保证的，所有等待的进程逐一地挂到 wait_list 上，链表结构可以保证先后顺序，从而保证所有等待者的公平性。  

一旦引入 spin 的特性，情况是怎样的呢，所有选择原地自旋的锁因为有 osq lock(见下文) 的限制，可以保证锁顺序，它们之间可以保证公平性。  

另一方面，所有因等待而进入睡眠的进程都会被挂在 lock->wait_lock 上，它们同样可以保持公平性。 

但问题是，可能出现这样一种情况：当 mutex lock 被进程 A 获取时，进程 A 在执行的时候被中断打断，执行中断去了，此时进程 B 尝试获取该锁，因为判断到进程 A 并没有执行，进程 B 进入睡眠。这时进程 A 从中断返回并执行，紧随着进程 C 尝试获取该锁，因为判断到进程 A 正在执行，从而进程 C 只是自旋等待。问题是：当进程 A 释放锁时，谁会获得锁呢？

答案是进程 C，因为进程 C 在自旋，而进程 B 需要唤醒，而这时候没有一种机制来判断进程 B 和 C 的先后顺序，自然是进程 C 捷足先登，这就明显不符合公平性，而且在锁竞争非常激烈的情况下，进程 B 甚至可能饿死。  



### 解决方案

对于这种优化所带来的不公平性，解决方法当然是很多，但是要做到既不增加额外的负担又要完美地解决这种公平性的问题，目前还没有完美的解决方案，如果要确切地记录每个进程的顺序，又得引入新的字段，这样明显得不偿失，所以内核采用了一种折中的方案：等待链表中的第一个进程被唤醒时(后续节点不会唤醒，链表还是可以保持顺序的)，如果没有"抢到"锁，同时它也可以通过判断当前锁持有者是否正在运行从而决定是否再次进入睡眠，同时也会设置一个 HANDOFF 标志，这个标志的作用在于强制要求 mutex lock 在下次释放时交给这个进程，相当于开一个后门，从而不用再去争抢锁。  

这种开后门方式的弊端在于：可能很多正在自旋等待的请求者需要等待该进程 被唤醒->获取锁->执行->释放锁的漫长过程，这也是为了避免进程饿死所付出的代价。    

稍稍思考就能发现，这种解决方案并不完美，只是解决了极端情况下进程饿死的情况下，并没有严格地保证顺序，但是从应用角度来看，先等待的锁进入睡眠，后等待的锁自旋等待的情况并不多，同时，进入睡眠的锁被唤醒后即使没有抢到锁，它也是可以通过判断来决定是否再次睡眠的，也可能它也会加入到自旋的队伍中，因此，这种强制的顺序调整从最终结果来看影响并不会很大。  

HANDOFF 标志并没有使用独立的字段来描述，而是使用了 owner 的 bit1，bit2 是 PICKUP 标志，同样是用于享受"开后门"待遇相关的标志位。  

而 bit0 为 WAITERS 标志，和当前功能无关，它表示当前 lock->wait_list 上有没有等待者，这里顺带提一句。



## 数据结构成员
在回过头来看 mutex 的数据成员，通过数据成员对 mutex 的实现进行简单的概括：

### owner
owner 是一个 task_struct 成员，保存了进程描述符指针值，记录一个锁的属主，用于实现 mutex 的第一个特性：加锁和解锁由同一个进(线)程完成。   

同时，owner 的作用不仅于此，它还被用来控制加解锁，如果 owner 不等于 0，表示当前锁已经被占用，反之，则表示该锁未被占用。   

而且，因为进程描述符指针总是 8 字节对齐的，即 owner 的 bit0～bit2 恒为 0，内核同时使用了这三位作为标志位，在加解锁过程中记录一些标志信息，这些标志位对应关系为：
* bit0 -- MUTEX_FLAG_WAITERS：指示当前是否有睡眠的等待者	
* bit1 -- MUTEX_FLAG_HANDOFF：指示下一次解锁需要将锁指定交给某个进程
* bit2 --  MUTEX_FLAG_PICKUP：和 bit1 配合，指定交给某个进程之后需要进程确认的标志位

也就是说，一个 owner 字段，巧妙地记录了三种信息，对于 mutex 这种非常常用的组件而言，节省哪怕一个 byte 的内存都是非常有意义的。



### wait_lock
wait_lock 是一个自旋锁，mutex 主要用来保证链表操作的安全性，因为排队链表同样会带来竞争问题。  



### osq
osq 的数据类型为：struct optimistic_spin_queue，这个数据类型并不常见。  

在上文中说到，当一个进程尝试获取锁但获取不到时，它会先判断当前持有锁的进程是否正在另一个 CPU 的临界区上运行，如果是，它会原地等待。  

如果此时又来了一个进程请求该 mutex lock，那它会怎么做呢？第二个进程是可以知道该 mutex lock 存在等待者的，但是在 linux 内核的实现中，它并不会选择进入睡眠，而是同样假设第一个等待者会很快地执行完，从而轮到自己，于是第二个等待进程作出和第一个进程相同的动作：判断锁的持有者是否正在某个 CPU 上执行，如果是，就原地自旋。   

也就是，在竞争比较激烈的情况下，可能存在多个锁在自旋等待 mutex lock 的释放，这会带来一个问题，在多核环境下，需要尽量避免多个核上运行的进程共享一个全局对象，因为一旦这个全局对象更新，就会导致其它所有 CPU 上对于该全局对象的 cache 失效，而这种  cache 失效带来的后果就是 CPU 需要重新从内存中将数据载入到多级 cache 中，开销很大。  

既然多个进程等待一个锁无法避免，那么就修改等待方式，即通过使用 percpu 的机制让每个等待进程只阻塞在本地锁变量上，而不是阻塞在 lock->owner 这个全局锁变量上，只有获取到 osq 锁的进程才阻塞在 lock->owner 上，这样就保证了 lock->owner 的修改不会导致所有 CPU 的 cache miss。  



## wait_list
wait_list 就是用来保存因无法获取锁而进入睡眠的进程信息，在必要的时候通过该链表唤醒其中的等待进程，wait_list 同样是全局对象，所以需要使用 wait_lock 来进行数据同步。  






## mutex 的实现框架梳理
鉴于 mutex 的具体实现有些复杂，我将整个实现过程进行了梳理：

mutex 加锁：



* 尝试直接加锁，只有在锁没有被占用且没有等待者的情况下才会成功，成功直接返回，失败跳到第二步
* 判断占用锁的进程是否正在执行(主要条件)，如果是，自旋等待锁释放，否则跳到第三步
  * 在进入自旋之前，如果当前已经有进程正在自旋，会因为获取不到 osq 锁而切换自旋方式，阻塞在本地 CPU 上而不是阻塞在 mutex lock 上
  * 自旋过程中循环判断自旋条件是否满足，如果不满足，返回并跳到第三步睡眠等待

* 将当前进程添加到 wait_list,进入睡眠，睡眠模式为 INTERRUPTIBLE
  * 进程被唤醒时，如果依旧获取不到锁，设置 HANDOFF 标志，强制下次获取到锁



mutex 解锁：



* 尝试直接解锁，只有在没有等待者的情况下才会成功，否则跳到第二步
* 解锁，如果没有设置 HANDOFF 标志且 wait_list 上有等待者，唤醒等待者，否则跳到第三步
* 存在 HANDOFF 标志，将锁交给 wait_list 上的第一个等待者，以确保其它自旋等待者无法获得





## mutex 的源码实现
通过实现框架以及上文中对 mutex 的分析，我们已经对 mutex 的实现有了一个基本的了解，接下来我们就进入到 mutex 的实现源码中，详细地分析 mutex 的每一部分。(源码基于 4.14 内核，源码位于 kernel/locking/mutex.c)。

 


## mutex_init 初始化
mutex lock 的不允许自行初始化，需要使用内核提供的接口：mutex_init.

```c++
#define mutex_init(mutex)						\
do {									\
	static struct lock_class_key __key;				\
									\
	__mutex_init((mutex), #mutex, &__key);				\
} while (0)

void __mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
	atomic_long_set(&lock->owner, 0);        // owner 初始化为 0
	spin_lock_init(&lock->wait_lock);        // 初始化 spin lock
	INIT_LIST_HEAD(&lock->wait_list);        // 初始化链表
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
	osq_lock_init(&lock->osq);               // 初始化 osq lock
#endif
	debug_mutex_init(lock, name, key);      // debug 相关的接口，暂不讨论
}
};
```

mutex lock 的初始化规规矩矩，就是依次对每个成员初始化。   



## mutex_lock 加锁
加解锁是 mutex lock 的核心部分，加锁被分为三个部分：



* 尝试直接加锁，成功就返回，失败进入下一步
* 尝试自旋等待，成功就返回，失败进入下一步
* 添加到等待链表，进入睡眠

同时对比 mutex_lock 的源代码：

```c++
void __sched mutex_lock(struct mutex *lock)
{
	might_sleep();

	if (!__mutex_trylock_fast(lock))
		__mutex_lock_slowpath(lock);
}
```
其中，\_\_mutex_trylock_fast 对应尝试直接加锁，而失败之后调用的 __mutex_lock_slowpath 则对应尝试自旋等待和睡眠等待两个部分

鉴于 mutex lock 的加锁整体实现比较长，我们就按照上述的三个部分对源码进行分析。  



### 直接加锁：__mutex_trylock_fast

```C++

static bool __mutex_trylock_fast(struct mutex *lock)
{
	unsigned long curr = (unsigned long)current;

	if (!atomic_long_cmpxchg_acquire(&lock->owner, 0UL, curr))
		return true;

	return false;
}

```

fast 部分的代码相对比较简单，主要是两步：



* 获取当前进程的 task_stuct 指针
* 调用 atomic_long_cmpxchg_acquire 函数尝试更新 lock->owner,这个函数是一个原子操作函数，因为 lock->owner 是全局变量，所以这里需要用到原子操作。这个接口的定义为：将 p1 (第一个参数)和 p2 作比较，如果相等，则 p1=p3，返回 p2，否则不执行赋值，直接返回 p1。

当 lock->owner 为 0 时，表示既没有其它进程获取锁也没有等待者,就可以直接获取到锁并返回。  

锁刚好释放了，此时有一个 spin，会不会导致该锁直接获得。 



### 尝试自旋和睡眠等待
尝试自旋和睡眠由另一个函数控制：__mutex_lock_slowpath

```c++
static noinline void __sched __mutex_lock_slowpath(struct mutex *lock)
{
	__mutex_lock(lock, TASK_UNINTERRUPTIBLE, 0, NULL, _RET_IP_);
}

static int __sched __mutex_lock(struct mutex *lock, long state, unsigned int subclass,
	     struct lockdep_map *nest_lock, unsigned long ip)
{
	return __mutex_lock_common(lock, state, subclass, nest_lock, ip, NULL, false);
}

static __always_inline int __sched
__mutex_lock_common(struct mutex *lock, long state, unsigned int subclass,
		    struct lockdep_map *nest_lock, unsigned long ip,
		    struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx)
{
	...
}

```
\_\_mutex_lock_slowpath 最后调用到 \_\_mutex_lock_common 这个函数，由于该函数篇幅比较大，需要分步介绍，这里先介绍它的参数：



* lock：mutex lock 结构体
* subclass：死锁检测相关
* nest_lock：死锁检测 lockdep 相关结构体
* ip：当前函数返回地址，_RET_IP_ 是 gcc 的內建函数，表示当前函数的返回地址
* ww_ctx：wound wait 结构体，wound wait 用于解决死锁问题
* use_ww_ctx：wound wait 相关

lockdep 是内核中提供的一个死锁检测模块，它可以用来检测内核中的各种死锁问题，主要用于调试阶段。当死锁的风险被检测到时，一种解决方案是调整执行逻辑，防止死锁的产生，另一种方案就是使用内核提供的死锁解决方案，而 wound wait 就是其中一种。    

存在两个进程 p1、p2 和两个 mutex lock A、B，存在这样一种情况：p1 获得 mutex A 之后，切换到 p2 执行，p2 获得 mutex B 之后，再请求 mutex A，此时会被阻塞，再切换到 p1 执行，此时如果 p1 在持有 mutex A 的同时再请求 mutex B，这就造成一个结果：p1 持有 mutex A 请求 mutex B，p2 持有 mutex B 请求 mutex A，两个进程都因为获取不到锁而阻塞，很明显这种阻塞是无法解开的。  

如果此时使用的是 wound wait 接口，系统就会自动检测这种死锁行为，然后让其中一个进程主动释放锁，等另一个进程执行完释放锁之后回头执行之前的进程，以解决死锁的问题。wound wait 翻译过来为"受伤地等待"，表示持锁的双方有一方要做出的牺牲。至于 wound wait 的详细实现，这一章节就不过多赘述。  




### 尝试自旋
尝试自旋的代码在 __mutex_lock_common 函数中：

```c++
static __always_inline int __sched
__mutex_lock_common(struct mutex *lock, long state, unsigned int subclass,
		    struct lockdep_map *nest_lock, unsigned long ip,
		    struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx)
{
	...

	preempt_disable();
	
	if (__mutex_trylock(lock) ||
		mutex_optimistic_spin(lock, ww_ctx, use_ww_ctx, NULL)) {
		
		lock_acquired(&lock->dep_map, ip);
		if (use_ww_ctx && ww_ctx)
			ww_mutex_set_context_fastpath(ww, ww_ctx);

		preempt_enable();
		return 0;
	}
}

```

在操作之前需要先关闭内核抢占，以免 mutex 的执行过程被其它进程抢占并重复执行，需要注意的是，这里只是 mutex 的 lock 过程的关抢占，并不是像 spin lock 一样整个加锁到解锁的过程都关抢占，这是有本质区别的，mutex 在获取锁之后是可以被调度出去的，只是获取锁的过程中需要关抢占。  

尝试自旋的核心函数为 \_\_mutex_trylock 和 mutex_optimistic_spin，顾名思义，\_\_mutex_trylock 是尝试获取锁，而 mutex_optimistic_spin 是尝试自旋的动作，当这两者的返回结果有一个为真时，表示已经获取到锁，就重新开启抢占，然后功成身退，否则就继续执行后续的睡眠代码(后文中分析)。  



接下来看看 __mutex_trylock 和 mutex_optimistic_spin 的实现：

```c++
static inline bool __mutex_trylock(struct mutex *lock)
{
	return !__mutex_trylock_or_owner(lock);
}

static inline struct task_struct *__mutex_trylock_or_owner(struct mutex *lock)
{
	// 获取当前的 task_struct 结构
	unsigned long owner, curr = (unsigned long)current;
	// 获取 lock 中的 owner，owner 由 task_struct 和低三位的标志位组成
	owner = atomic_long_read(&lock->owner);
	
	for (;;) { 
		unsigned long old, flags = __owner_flags(owner);

		// 获取去除标志位之后的 owner，也就是占用锁的进程的 task_struct 
		unsigned long task = owner & ~MUTEX_FLAGS;

		// 根据 task 的值进行处理，第一分支
		if (task) {
			if (likely(task != curr))
				break;

			if (likely(!(flags & MUTEX_FLAG_PICKUP)))
				break;

			flags &= ~MUTEX_FLAG_PICKUP;
		} else {
		}

		flags &= ~MUTEX_FLAG_HANDOFF;

		// 尝试直接获取锁，第二分支
		old = atomic_long_cmpxchg_acquire(&lock->owner, owner, curr | flags);
		if (old == owner)
			return NULL;

		owner = old;
	}

	return __owner_task(owner);
}
```

\_\_mutex_trylock 的实现稍微有点复杂，同时也涉及到解锁的逻辑，\_\_mutex_trylock 调用了 \_\_mutex_trylock_or_owner，这是主要的函数。  

在讲解 __mutex_trylock_or_owner 之前，需要再次强调 lock->owner 的组成：lock->owner 是当前获得锁进程的 task_struct 指针值，其中低三位还被用作标志位，对应关系为：



* bit0 -- MUTEX_FLAG_WAITERS：指示当前是否有睡眠的等待者	
* bit1 -- MUTEX_FLAG_HANDOFF：指示下一次解锁需要将锁指定交给某个进程
* bit2 --  MUTEX_FLAG_PICKUP：和 bit1 配合，指定交给某个进程之后需要进程确认的标志位

接着看上面的代码分析：在 \_\_mutex_trylock_or_owner 中，大体上分为两种情况：

一种情况是：lock->owner 屏蔽掉 flags 之后，task 不为空，一方面，可能是有其它的进程正在占用这个锁，也就是第一个判断 (task != curr) 为真，跳出循环，不再操作。另一方面，也可能是当前进程设置了 HANDOFF 标志，上一个锁拥有者释放锁的时候将锁指定交给了当前进程，从而 (task != curr) 和 (flags & MUTEX_FLAG_PICKUP) 这两个条件都不成立，不跳出循环，继续执行，这种情况下，当前进程就可以通过 atomic_long_cmpxchg_acquire 获得锁。   

另一种情况就是：lock->owner 屏蔽掉 flags 之后为空，其实这时候并不意味着 lock 已经完全空闲了，可能只是上一个进程释放锁之后，自旋的进程还没有抢到锁，所以屏蔽掉 flags 的 owner 临时处于空的状态，这时候，因为上一个进程解锁之后没有指定接手的进程，所以 spin 的进程大概率可以抢到锁，需要注意的是，在 lock 上 spin 的进程只有一个，其它 spin 的进程其实是在 osq lock 上(即本地CPU) 上 spin，这种情况下可以保证顺序，但是被唤醒的进程不受 osq lock 的影响，它可以直接参与到 lock 的 spin 过程。    


上一部分是尝试获得锁的部分，当获取失败时，接着调用另一个函数：mutex_optimistic_spin，也就是真正地进入 spin 阶段：

```c++
static __always_inline bool
mutex_optimistic_spin(struct mutex *lock, struct ww_acquire_ctx *ww_ctx,
		      const bool use_ww_ctx, struct mutex_waiter *waiter)
{
	// waiter 是否为 NULL
	if (!waiter) {
		if (!mutex_can_spin_on_owner(lock))
			goto fail;

		if (!osq_lock(&lock->osq))
			goto fail;
	}

	for (;;) {
		struct task_struct *owner;
		
		// 尝试是否能获得锁
		owner = __mutex_trylock_or_owner(lock);
		if (!owner)
			break;
		
		// 进入自旋的操作
		if (!mutex_spin_on_owner(lock, owner, ww_ctx, waiter))
			goto fail_unlock;

		cpu_relax();
	}

	if (!waiter)
		osq_unlock(&lock->osq);

	return true;


fail_unlock:
	if (!waiter)
		osq_unlock(&lock->osq);

fail:
	if (need_resched()) {
		__set_current_state(TASK_RUNNING);
		schedule_preempt_disabled();
	}

	return false;
}
```
该函数同样分为两个部分：

区分在于 waiter 参数是否为空，waiter 用来描述一个因等待而陷入睡眠的请求锁进程，数据类型为 struct mutex_waiter，其中包括一个 list 链表节点用于链接到 lock->wait_list 上，一个 task_struct 结构记录进程，还有一个 ww_ctx 成员记录 wound_wait 相关参数。  

当请求锁的进程直接进入到 spin 时，waiter 参数为 NULL，需要先请求 osq lock，当请求锁的进程是从睡眠中(因请求不到锁而睡眠)被唤醒而进入 spin 时，waiter 参数为非空，该进程就不受  osq lock 的限制。  

当 waiter 参数为 NULL 时，先判断当前请求锁的进程是否能进入到 spin，判断的条件是：



* 持有锁的进程正在其它的 CPU 上运行，否则不能 spin，应该睡眠等待
* 是否有更高优先级的进程就绪，通过调用 need_resched 来确定
* 调用 vcpu_is_preempted() 函数

必须同时满足这三个条件，才可以乐观地认为锁很快会被释放，才能进入到 spin 中(注意这里说的 spin 是原地自旋等待这个动作，而不是指 spinlock 导致的 spin)。同时，所有参与 spin 的进程会竞争 osq 锁，确保只有一个进程在 lock 上阻塞，而其它的在本地 CPU 上阻塞以优化 cache，osq lock 基于 per CPU 的机制实现，在 kernel/locking/osq_lock.c 中，osq 只在限定的场景下使用，比如 mutex 和信号量，暂时还不支持通用的场景，具体的实现这里就不再赘述。   

如果当前进程满足进入 spin 的条件，就会在一个死循环中调用 __mutex_trylock_or_owner 和 mutex_spin_on_owner 进入 spin，这两个函数比较简单，也就是循环检测上面提到的三个条件，整个 spin 的过程还会不断地尝试请求锁，同时循环地检查这三个条件，如果这三个条件中任意一个不满足，spin 过程就会返回失败，切换到睡眠状态中。     



### 进入睡眠
当 spin 的三个检查条件不满足时，就会进入到睡眠的分支:

```c++
static __always_inline int __sched
__mutex_lock_common(struct mutex *lock, long state, unsigned int subclass,
		    struct lockdep_map *nest_lock, unsigned long ip,
		    struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx)
{
	// 使用自旋锁对全局数据进行保护
	spin_lock(&lock->wait_lock);
	// 尝试获取锁
	if (__mutex_trylock(lock)) {
		if (use_ww_ctx && ww_ctx)
			__ww_mutex_wakeup_for_backoff(lock, ww_ctx);

		goto skip_wait;
	}

	// 初始化一个 waiter，将当前 task_stuct 保存到 waiter 中，并链接在 lock->wait_list 中
	debug_mutex_lock_common(lock, &waiter);
	debug_mutex_add_waiter(lock, &waiter, current);

	lock_contended(&lock->dep_map, ip);

	if (!use_ww_ctx) {
		list_add_tail(&waiter.list, &lock->wait_list);
	}

	waiter.task = current;

	// 设置 MUTEX_FLAG_WAITERS 标志位
	if (__mutex_waiter_is_first(lock, &waiter))
		__mutex_set_flag(lock, MUTEX_FLAG_WAITERS);

	// 设置当前进程状态
	set_current_state(state);
	for (;;) {
		if (__mutex_trylock(lock))
			goto acquired;

		// 信号处理
		if (unlikely(signal_pending_state(state, current))) {
			ret = -EINTR;
			goto err;
		}

		spin_unlock(&lock->wait_lock);

		// 执行进程切换
		schedule_preempt_disabled();

		// 设置 HANDOFF 标志位
		if ((use_ww_ctx && ww_ctx) || !first) {
			first = __mutex_waiter_is_first(lock, &waiter);
			if (first)
				__mutex_set_flag(lock, MUTEX_FLAG_HANDOFF);
		}

		set_current_state(state);

		if (__mutex_trylock(lock) ||
		    (first && mutex_optimistic_spin(lock, ww_ctx, use_ww_ctx, &waiter)))
			break;

		spin_lock(&lock->wait_lock);
	}
	spin_lock(&lock->wait_lock);

	// 获取到锁之后会执行这部分，做一些清理并返回
acquired:
	__set_current_state(TASK_RUNNING);

	mutex_remove_waiter(lock, &waiter, current);
	if (likely(list_empty(&lock->wait_list)))
		__mutex_clear_flag(lock, MUTEX_FLAGS);

	debug_mutex_free_waiter(&waiter);

	// 一些清理操作
	...
}
```

对于进程的睡眠操作，主要是以下几个步骤：



* 使用 spinlock 保护 lock 中 wait_list 的操作
* 初始化 waiter，并将该 waiter 链接到 lock->wait_list 中
* 如果当前 waiter 是挂载 lock->wait_list 中的第一个 waiter，就设置一个 MUTEX_FLAG_WAITERS 标志，表示当前 lock 存在睡眠的等待者
* 死循环，该循环包括：
	* 尝试获取锁
	* 设置当前进程状态为传入的参数，即 TASK_INTERRUPTIBLE，表示在等待锁的睡眠状态下可以被信号唤醒并处理信号
	* 调用 schedule_preempt_disabled 执行进程切换，进入睡眠。当然，在当前进程被唤醒时，程序也是从这个断点开始运行
	* 被唤醒一次之后设置 MUTEX_FLAG_HANDOFF 标志，表示当前锁持有者释放锁时指定交给当前进程，然后如果此次唤醒获取到了锁，就清除标志位，跳出循环
	* 尝试获取锁或者尝试进入 spin 状态，如果不能 spin 则继续睡眠，被唤醒的进程不会和其它直接 spin 的进程竞争 osq 锁
* 如果获取到了锁，做一些清理工作并返回

这就是 mutex 的整个加锁过程，与之相对的，继续分析它的解锁过程。 



## mutex_unlock

```c++
void __sched mutex_unlock(struct mutex *lock)
{
#ifndef CONFIG_DEBUG_LOCK_ALLOC
	if (__mutex_unlock_fast(lock))
		return;
#endif
	__mutex_unlock_slowpath(lock, _RET_IP_);
}
```

解锁相对于加锁要简单一些，并不会存在多个进程同时解锁的情况，而且加解锁是同一个进程，在开启 deubg 的情况下，直接调用 __mutex_unlock_fast 尝试解锁，该函数的实现为：

```c++
static __always_inline bool __mutex_unlock_fast(struct mutex *lock)
{
	unsigned long curr = (unsigned long)current;

	if (atomic_long_cmpxchg_release(&lock->owner, curr, 0UL) == curr)
		return true;

	return false;
}
```
调用 atomic_long_cmpxchg_release 尝试直接解锁，如果 lock->owner 和 curr 相等，直接将 0 赋值给 lock->owner，也就完成了解锁，有盆友就问了，正常情况下，既然加解锁是同一个进程，lock->owner 和 curr 肯定是代表同一个进程，是的，但是你别忘了 lock->owner 还有 bit0~bit2 的标志位，所以只有当标志位没有被置位时，才能直接解锁成功。  

如果没有配置 debug 或者直接解锁失败，就走向另一个分支：__mutex_unlock_slowpath

```c++
static noinline void __sched __mutex_unlock_slowpath(struct mutex *lock, unsigned long ip)
{
	struct task_struct *next = NULL;
	DEFINE_WAKE_Q(wake_q);
	unsigned long owner;

	mutex_release(&lock->dep_map, 1, ip);

	owner = atomic_long_read(&lock->owner);
	for (;;) {
		unsigned long old;

		// 如果没有设置 MUTEX_FLAG_HANDOFF，跳出循环
		if (owner & MUTEX_FLAG_HANDOFF)
			break;

		old = atomic_long_cmpxchg_release(&lock->owner, owner,
						  __owner_flags(owner));
		if (old == owner) {
			if (owner & MUTEX_FLAG_WAITERS)
				break;

			return;
		}

		owner = old;
	}

	// 唤醒队列上第一个等待的进程(即 waiters)
	spin_lock(&lock->wait_lock);
	debug_mutex_unlock(lock);
	if (!list_empty(&lock->wait_list)) {
		struct mutex_waiter *waiter =
			list_first_entry(&lock->wait_list,
					 struct mutex_waiter, list);

		next = waiter->task;

		debug_mutex_wake_waiter(lock, waiter);
		wake_q_add(&wake_q, next);
	}

	if (owner & MUTEX_FLAG_HANDOFF)
		__mutex_handoff(lock, next);

	spin_unlock(&lock->wait_lock);

	wake_up_q(&wake_q);
}
```

当设置了 MUTEX_FLAG_HANDOFF 标志位的时候，首先执行解锁的行为，给 lock->owner 赋新值，一般情况下，这时候 MUTEX_FLAG_WAITERS 被置位，表示等待链表上有进程在等待，此时唤醒链表上的第一个进程，然后将 lock->owner 递交到第一个进程。  

唤醒的过程为：



* 找到 wait_list 上第一个成员作为待唤醒成员
* 取出待唤醒成员的 task_struct 结构
* 将待唤醒成员的 task_struct 结构添加到 wake_q 中
* 调用 wake_up_q 函数将进程放入唤醒队列

而锁递交的过程就是调用 __mutex_handoff，这个函数的实现过程为：



* 将 lock->owner 的 task_struct 字段赋值为被递交的进程
* 设置 MUTEX_FLAG_PICKUP 标志，用于和被递交的进程进行确认，以实现精准递交

当没有设置 MUTEX_FLAG_HANDOFF 时，如果等待队列上有进程等待的进程，唤醒第一个成员即可。 



### 参考

https://zhuanlan.zhihu.com/p/95068855
https://zhuanlan.zhihu.com/p/90508284
https://lwn.net/Articles/401911/
https://lwn.net/Articles/699784/   

[mcs 锁和 Q 自旋锁](https://zhuanlan.zhihu.com/p/23850340)

4.14.98 源码

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。






