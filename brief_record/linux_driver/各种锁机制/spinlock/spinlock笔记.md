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

arch_spinlock_t 的结构：

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

```c++
static __always_inline raw_spinlock_t *spinlock_check(spinlock_t *lock)
{
	return &lock->rlock;
}
```





