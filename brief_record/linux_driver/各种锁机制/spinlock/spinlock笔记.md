# spin_lock

spin_lock 的结构：

```c++
typedef struct spinlock {
	union {
		struct raw_spinlock rlock;
	};
} spinlock_t;
```

struct raw_spinlock 的结构：

```c++
typedef struct raw_spinlock {
	arch_spinlock_t raw_lock;
} raw_spinlock_t;
```

arch_spinlock_t 的结构,对于 arm64，定义在 arch/arm64/include/asm/spinlock_types.h 中：

```c++
typedef struct {
	u16 owner;
	u16 next;
} __aligned(4) arch_spinlock_t;
```


spinlock 的初始化：
```c++
#define spin_lock_init(_lock)				\
do {							\
	spinlock_check(_lock);				\
	raw_spin_lock_init(&(_lock)->rlock);		\
} while (0)
```

```c++  只是检查以下
static __always_inline raw_spinlock_t *spinlock_check(spinlock_t *lock)
{
	return &lock->rlock;
}
```

```c++ include/linux/spinlock.h 中。
# define raw_spin_lock_init(lock)				\
	do { *(lock) = __RAW_SPIN_LOCK_UNLOCKED(lock); } while (0)
```


```c++ include/linux/spinlock_types.h 中。 
#define __RAW_SPIN_LOCK_UNLOCKED(lockname)	\
	(raw_spinlock_t) __RAW_SPIN_LOCK_INITIALIZER(lockname)

#define __RAW_SPIN_LOCK_INITIALIZER(lockname)	\
	{					\
		.raw_lock = __ARCH_SPIN_LOCK_UNLOCKED,	\
		SPIN_DEBUG_INIT(lockname)		\
		SPIN_DEP_MAP_INIT(lockname) 
	}

展开就是：
	(lock->rlock = {
		.raw_lock = __ARCH_SPIN_LOCK_UNLOCKED, 
		SPIN_DEBUG_INIT(lockname)	          // debug 的情况下才会使用，否则为空
		SPIN_DEP_MAP_INIT(lockname)           // debug 的情况下才会使用，否则为空
	})

```

```c++    定义在架构文件中：arch/arm64/include/asm/spinlock_types.h
#define __ARCH_SPIN_LOCK_UNLOCKED	{ 0 , 0 }  // 就是将架构定义的 owner 和 next 设置为 0；
```

------------------------------------spinlock_init--------------------------------------------

# spin_lock 函数

```c++
static __always_inline void spin_lock(spinlock_t *lock)
{
	raw_spin_lock(&lock->rlock);
}

#define raw_spin_lock(lock)	_raw_spin_lock(lock)


// kernel/locking/spinlock.c
// __lockfunc 前缀： #define __lockfunc __attribute__((section(".spinlock.text")))，锁的代码实现保存在特定段。  
// kernel/locking/spinlock.c 中。
void __lockfunc _raw_spin_lock(raw_spinlock_t *lock)  
{
	__raw_spin_lock(lock);
}

// include/linux/spinlock_api_smp.h 中
static inline void __raw_spin_lock(raw_spinlock_t *lock)  // include/linux/spinlock_api_smp.h
{
	preempt_disable();                                  //禁止内核抢占
	spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);       //定义在 include/linux/lockdep.h 中，只有定义了 lockdep 时，才会执行
	LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);  //
}

// 在没有定义 CONFIG_LOCK_STAT 时，执行第三个参数 lock，并以第一个参数为函数参数传递到 lock() 中，也就是 do_raw_spin_lock。
#define LOCK_CONTENDED(_lock, try, lock) \
	lock(_lock)



static inline void do_raw_spin_lock(raw_spinlock_t *lock) __acquires(lock)
{
	__acquire(lock);
	arch_spin_lock(&lock->raw_lock);
}

arch_spin_lock 就是调用到最后的架构相关的 spinlock 实现了。定义在 arch/arm64/include/asm/spinlock.h 中， 



```






