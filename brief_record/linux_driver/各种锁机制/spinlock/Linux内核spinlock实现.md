# linux 内核 spinlock 的实现
在内核中，共享对象的互斥处理向来是个非常麻烦的问题，这种麻烦是源自于 linux 内核中复杂的并发机制，这种并发有可能是单核上多进程（线程）之间的伪并发、也可能是 SMP 架构中的真实并发，再考虑到中断和下半部机制的情况，并发编程变得非常复杂。  

在并发编程中，对共享数据加锁，这是一个最基本的共识，在 Linux 内核中支持各种各样的锁机制，以满足各种各样的并发情况，并没有哪一种锁机制能适用于所有的并发情况，即使有，那么这种锁机制肯定只是能用，而不是适用。毕竟，生活经验告诉我们：兼容性和针对性之间只能二选其一，兼容性带来更广的应用范围，而针对性带来更高的效能发挥。  

在所有的锁机制中，spin lock 和 mutex lock 是应用最为广泛的，如果一个进程请求某个临界区的资源，当请求不到时陷入睡眠状态，直到临界区资源重新开放可用时再进行处理，这是相当实用的做法，节省了 CPU 的运行资源。  

但是在某些情况下，mutex lock 并不合适：

一方面，从微观的角度来说，进程的切换尽管耗时很少，通常在 us 级别，如果程序员确定进入临界区的时间要小于这个时间，比如仅仅是置一个标志位，进程切换的开销相对来说就是不划算的,同时，随着硬件的发展，各种缓存技术的应用，进程切换的开销就不能只放在进程的 switch 过程了，后续还有一些隐性的开销，比如缓存和 TLB 的重新载入。 

另一方面，通常也是开发者不使用 mutex lock 的原因，就是在中断上下文中不能睡眠，这是一个硬性条件，这个时候就需要使用 spinlock 来实现共享对象的互斥访问。  


## spinlock
spinlock 中的 spin 是旋转的意思，当进程对 spinlock 保护的临界区没有访问权，通俗地讲也就是无法获得锁时，就会原地打转等待资源，当然这个等待的过程伴随着不断地去查询是否能获得锁。  


### spinlock 的接口以及使用
spinlock 的使用是相当简单的，主要是三个过程：初始化锁、加锁、解锁。  

对应的编程接口为：

```c++
spinlock_t lock;         //定义 spinlock 变量
spin_lock_init(&lock);   //初始化 spinlock,或者直接使用 DEFINE_SPINLOCK 接口实现 定义+初始化
spin_lock(&lock);        //获取 spin_lock（也有给临界区加锁的说法），该接口会一直等待直到获取到锁
spin_lock(&unlock);      //解锁，被加锁的临界区只有在解锁后其它进程才可以进入。  
```

和 mutex 一样，spinlock 是二义锁，lock 和 unlock 需要成对出现，同时，需要注意的是，不要嵌套使用 spinlock，这会直接导致死锁。  
除了最基本的使用接口之外，还支持以下的接口以供在特殊情况下使用：

```c++
spin_lock_irq()      spin_unlock_irq()
spin_lock_irqsave()  spin_unlock_irqstore()
spin_lock_bh()       spin_unlock_bh()
spin_is_locked()     
```

spin_lock_irq 和 spin_unlock_irq 成对出现，表示在使用 spinlock 的同时禁止中断
spin_lock_irqsave 和 spin_unlock_irqstore 成对出现，同样是禁止中断，并将中断状态保存起来
spin_lock_bh 和 spin_unlock_bh 成对出现，表示禁止下半部
spin_is_locked 接口用于查询当前的 spinlock 是否已经被加锁   

对于 spin_lock_irq 和 spin_lock_irqsave 的区别，这和其它的 *irq 和 *irqsave 接口一样，前者是单纯地禁止本地中断，在 arm 架构上就是通过设置 CPRS 的 I bit 屏蔽中断接收，后者是在禁止本地中断之前保存中断状态，在屏蔽 CPSR 的 I bit 之前，会先保存 CPSR 寄存器的值到变量 flags 中。它们的实际区别在于：如果存在嵌套地 disable 中断行为，就需要使用 spin_lock_irqsave 接口，这是因为 spin_unlock_irq 接口是无条件地开启中断，如果在程序中禁止了两次中断（禁止中断并不一定是调用了 spin_lock_irq 接口，也可能是其它的 *irq 接口），而调用 spin_unlock_irq 一次就开启了中断，这明显是不符合开发者意图的。spin_unlock_irqstore 是回复到上次的中断状态，而不是直接无条件的开启中断。  

至于 spinlock 与中断或者其它并行执行流之间的纠葛以及什么时候该使用什么样的接口，我们接着看下文。  


## spinlock 涉及的并发情况
在分析 spinlock 之前，先考虑一个问题：如果让你来实现 linux 中的 spinlock，你会怎么做？  

本质上来说，全局对象需要使用锁来保护的原因是并发的产生，如果是单一的执行流，不需要考虑使用锁来做数据保护，所以第一个需要思考的问题是：spinlock 的使用过程中会有哪些并发的产生？  
* 内核的抢占
* 中断的抢占
* 下半部的抢占
* SMP 架构中的并发

### 内核抢占
linux 默认支持内核抢占，这个特性使得当前进程的执行可以被其它内核进(线)程抢占执行，这种抢占并不是无时不刻都在进行的，而是存在一些抢占点，比如从中断处理函数返回到内核中，比如调用 preempt_enable 使能内核抢占，因此可能出现这样的情况：

内核进程 A 正在执行，并获取了一个 spinlock，此时发生了中断并跳转到中断服务程序中，在中断中一个更高优先级的进程 B 被唤醒，在中断返回时 B 会抢占 A 并执行，如果这时候 B 也请求同一个 spinlock，问题就来了：B 因为请求不到 spinlock 而一直自旋，而占用 spinlock 的 A 因为被 B 抢占而得不到执行，无法释放 spinlock。  

因此，对于自旋锁而言，内核抢占是一个明显的风险点，作为最简单的处理，需要在使用自旋锁之前，就禁止内核抢占。  

## 中断抢占
毫无疑问，中断的优先级肯定是比普通进程的优先级要高，在没有禁止中断的情况下，一旦有外设中断来到，CPU 就会直接跳转到中断向量处执行系统的中断代码以及我们定义的中断处理函数，这一步是硬件操作，软件上无法修改其行为。  

同样的，如果进程 A 执行的时候占用了 spinlock，此时发生了中断，同时中断中也需要请求同一个 spinlock，也会造成中断请求不到锁，而进程 A 无法释放锁的情况，而且中断中长时间的自旋所带来的后果更加严重。  

那么，作为处理，是不是需要在请求自旋锁之前也禁止本地中断呢？  

实际上并不用，中断和内核抢占有本质上的区别。需要注意的是，内核管理所有的硬件，向应用层提供操作接口，是一个服务的提供者，通常需要支持并发访问，而这些并发访问是无法针对性进行处理的，因为对于内核来说它们都是一样的。因此，对于某个驱动程序 A 而言，开发者并不知道同时会有多少个进程对其发起请求，完全可能存在用户进程 X 和 Y 同时发起系统调用向 A 请求服务，这就造成了 X 和 Y 可能同时请求到 A 中的同一个 spinlock，这种情况下如果 X 和 Y 相互抢占，就会导致上述的问题，除了禁止抢占，没有更好的办法。  

而中断不一样，中断服务程序是可知的，或者说开发者是可以明确地知道中断服务程序和普通程序会不会竞争同一个锁，从而进行针对性的处理，如果中断服务程序中会和普通程序中竞争同一个 spinlock，毫无疑问进程在使用 spinlock 的时候需要禁止中断，但是如果在中断中没有使用到 spinlock，就没有必要禁止中断，白白浪费系统响应性能。  

所以，这个过程是可预知、可控制的，如果你的中断程序中使用到与进程竞争的 spinlock，就是用 spin_lock_irq 这一类接口，否则就使用 spin_lock 这种普通接口即可。  

可能有的朋友就要问了，在 SMP 系统中，同一个中断是不是会在不同的 CPU 上并发执行，实际上这个不需要担心，目前 SMP 架构上的 linux 实现，是不支持中断的并发的，同时，如果是不同中断之间使用了同一个 spinlock，也不用担心，新版的（>2.6） linux 是不支持中断嵌套的，也就是中断执行的过程是关中断的。  


## 下半部的抢占
中断下半部的优先级是高于普通进程的，因此同样会出现和中断一样的情况：中断下半部和普通进程发生竞争而导致死锁的问题。  

由于下半部和中断一样同样可以方便地和普通进程区分开来，因此对于中断下半部的处理方式和中断一样：如果中断下半部中存在与普通进程之间的 spinlock 竞争关系，就需要禁止下半部，反之则不用。  

中断下半部的调度点有：硬件中断处理程序退出时和调用 local_bh_enable 重新使能下半部时，所以禁止中断并不能禁止下半部的执行，因此下半部也有相对应的独立的接口：spin_lock_bh 和 spin_unlock_bh。同样的，如果普通进程和下半部之间没有 spinlock 的竞争，就使用普通的 spin_lock 和 spin_unlock。当然，如果你的程序完全不需要考虑性能，直接使用 spin_lock_bh 或者 spin_lock_irq 也是可以的。  

相对于中断不存在并发执行而言，下半部是不是存在并发执行的情况呢？首先，下半部不会在同一个 CPU 上被下半部抢占，但是 softirq 支持在不同的 CPU 上并发执行，这类情况并不需要担心，比如 CPU0 上执行 softirq 请求了 spinlock，CPU1 同时运行该 softirq 请求 spinlock，但是这并不会造成死锁，实际情况是 CPU0 的 softirq 很快地执行完临界区代码释放 spinlock，从而 CPU1 上的 softirq 得以继续执行。 



## SMP 架构中的并发
SMP（Symmetrical Multi-Processing） 架构与 UP（Unique Processing） 最大的区别在于：SMP 实现了真正的并发执行，而不像单核下的伪并发。  

实际上，对于 SMP 中的并发，和单核下的并发有本质的区别，单核下的 spinlock 并发问题来自于当一个执行流获取到锁，另一个执行流不能抢占当前执行流并同时尝试获取该锁，这会导致原本的执行流无法执行而新的执行流无法获取锁的情况。而在多核下，是实实在在的并发，只要两个同时请求锁的执行流在不同的 CPU 上，后请求的等待前一个请求的释放锁就可以了。 

单核下锁的机制基本上是通过防止并发的方式实现，比如禁中断、禁抢占，单核下的禁止等于系统性地禁止。而多核下不一样，linux 并不提供系统性的禁止行为，比如禁止整个系统上所有 CPU 的中断，并发是必然存在的。

正因为是实实在在的并发，这也会带来另一个问题：多核下需要实现加解锁操作的原子性或者独占性，首先需要了解的是，即使是一个变量的操作，通常都包括取指、操作、写回这三个步骤，假设 CPU0 对锁的操作正进行到操作步骤，准备写回，同时 CPU1 也在进行操作，并准备写回，这时候 CPU0 或者 CPU1 的其中一个操作结果可能会丢失，从而产生错误的逻辑。  
所以，对于多核下的 spinlock，需要考虑锁操作的原子性和独占性。  


## spinlock 的特性总结
根据上面的分析，一个完善的 spinlock 实现应该有以下的特点：
1、申请锁时关内核抢占，释放锁时开启内核抢占
2、如果中断或者进程中出现 spinlock 的竞争，需要进程中的锁操作需要关中断，中断中不需要关中断（默认就是关状态）。
3、如果中断下半部或者进程中出现 spinlock 的竞争，需要进程中的锁关闭中断下半部
4、在多核系统下，加解锁的操作函数需要具备原子性或者独占性。  

实际上，这也是目前 linux 中 spinlock 的实现所考虑的并发处理，接下来就来看看 spinlock 的源代码实现。  


## spinlock 的源码实现
因为 spinlock 的实现涉及到单核、多核的区分，对于多核的支持是架构相关，同时内核支持死锁的检测以及 debug 支持，所以源码的定义带有多层的宏选项，非常地绕，在这里我就基于 arm SMP 平台，在不支持死锁检测(lockdep)和 debug 的情况下对 spinlock 进行分析。  

### spinlock 结构体
spinlock 的结构体为：spinlock_t，定义如下：

```c++
typedef struct spinlock {
		struct raw_spinlock rlock;
} spinlock_t;

typedef struct raw_spinlock {
	arch_spinlock_t raw_lock;
} raw_spinlock_t;

typedef struct {
	union {
		u32 slock;
		struct __raw_tickets {
			u16 owner;
			u16 next;
		} tickets;
	};
} arch_spinlock_t;

```
对应的层次为： spinlock_t -> raw_spinlock_t -> arch_spinlock_t -> tickets，绕来绕去就是定义了一个  4 字节的 union 结构，拆分开来分别是 owner 和 next，spinlock 就是通过这两个变量完成加解锁操作。  


### spinlock 初始化
spinlock 的初始化接口为 spin_lock_init，这是一个宏定义： 

```c++
#define spin_lock_init(_lock)				\
do {							\
	spinlock_check(_lock);				\
	raw_spin_lock_init(&(_lock)->rlock);		\
} while (0)

# define raw_spin_lock_init(lock)				\
	do { *(lock) = {.raw_lock = {0}} } while (0)

```
初始化的第一步是对传入的 spinlock 进行检查，接着就结构体内容的初始化，这里将整个冗长的定义进行了简化，其实就是将结构体中的 slock 设置为 0，也可以看成是将 owner 和 next 两个变量设置为 0。 

### spinlock 的加锁
spinlock 的核心实现就是加锁部分，它的调用层次是这样的：

```c++
static void spin_lock(spinlock_t *lock)
{
	raw_spin_lock(&lock->rlock);
}

#define raw_spin_lock(lock)	_raw_spin_lock(lock)

void _raw_spin_lock(raw_spinlock_t *lock)  
{
	__raw_spin_lock(lock);
}

static inline void __raw_spin_lock(raw_spinlock_t *lock) 
{
	preempt_disable();                                  //禁止内核抢占
	LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);  
}

#define LOCK_CONTENDED(_lock, try, lock) \
	lock(_lock)


static inline void do_raw_spin_lock(raw_spinlock_t *lock) __acquires(lock)
{
	arch_spin_lock(&lock->raw_lock);
}
```

整个跟踪的过程又臭又长，还要仔细的分辨各个宏的走向，不过总算是有不小的收获，在 __raw_spin_lock 的函数调用中，即真正的加锁之前，调用了 preempt_disable 函数禁止了内核抢占，这和我们上面的分析是一致的，然后调用 LOCK_CONTENDED，最终的执行结果是：以传入的 raw_spinlock_t 类型的 lock 指针为参数执行 do_raw_spin_lock。  

而 do_raw_spin_lock 的函数实现中调用了 arch_spin_lock(&lock->raw_lock)，看到这个前缀 arch 知道这是一个架构相关的函数，需要到 arch/ 目录下找对应的实现。  

因为是 arm 架构，在 arch/arm/include/asm/spinlock.h 中找到了 arch_spin_lock 对应的实现：

```c++
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned long tmp;
	u32 newval;
	arch_spinlock_t lockval;

	prefetchw(&lock->slock);    // gcc 内置预取指令，指定读取到最近的缓存以加速执行
	__asm__ __volatile__(
"1:	ldrex	%0, [%3]\n"         // lockval = &lock->slock
"	add	%1, %0, %4\n"           // newval = lockval + 1 << 16,等于 lockval.tickets.next +1；
"	strex	%2, %1, [%3]\n"     // 将 lock->slock = newval
"	teq	%2, #0\n"               // 测试上一步操作的执行结果
"	bne	1b"                     // 如果执行 lock->slock = newval 失败，则跳到标号 1 处从头执行
	: "=&r" (lockval), "=&r" (newval), "=&r" (tmp)
	: "r" (&lock->slock), "I" (1 << TICKET_SHIFT)
	: "cc");

    // 没进行 +1 操作前的 lockval.tickets.next 是否等于 lockval.tickets.owner
    // 不相等时，调用 wfe 指令进入 idle 状态，等待 CPU event，被唤醒后继续判断锁变量是否相等
	while (lockval.tickets.next != lockval.tickets.owner) { 
		wfe();
		lockval.tickets.owner = ACCESS_ONCE(lock->tickets.owner);
	}
    // 内存屏障 
	smp_mb();
}
```
arch_spin_lock 的实现并不简单，夹杂着一部分嵌入汇编程序，关于嵌入汇编的教程可以参考我的这一篇博客TODO，鉴于有些朋友的汇编基础薄弱，我特意加上了等价的 C 语言注释。   

从上述的注释中大概可以看出加锁的实现：
1、先将 spinlock 结构体中的 next 变量 + 1，不管是否能够获得锁
2、判断 +1 操作之前，next 是否等于 owner，只有在 next 与 owner 相等时，才能完成加锁，否则就循环等待，从这里也可以看到，自旋锁并不是完全地自旋，而是使用了 wfe 指令。

要完整地理解加锁过程，就必须要提到解锁，因为这两者是相对的，解锁的实现很简单：就是将 spinlock 结构体中的 owner 进行 +1 操作，因此，当一个 spinlock 初始化时，next 和 onwer 都为 0。某个执行流 A 获得锁，next + 1，此时在其它执行流 B 眼中，next ！= owner，所以 B 等待。当 A 调用 spin_unlock 时，owner + 1。  

此时 next == owner，所以 B 可以欢快地继续往下执行，这就是加解锁的逻辑。需要注意的是，B 在请求锁时会先将 next + 1，此时的 next 为 new_next,而用于判断是否等于 owner 的那个 next 是 old next，这里需要注意。  

## spinlock 的疑问
尽管了解了 spinlock 的实现方式，我想很多朋友其实还是一头雾水，脑子里面盘旋着几个疑问：
1、既然就是判断变量同步来确定加解锁状态，用一个变量就行了啊，加锁的时候变量为 1，表示 lock，释放的时候变量递减到 0，表示 unlock，只有当变量为 0 时，进程才能获得锁，这种方式不是一样的吗？   
2、加锁时的 owner 和 next 的加减操作为什么还要这么麻烦地使用汇编实现？

对于第一个疑问：很多实时操作系统甚至老版本的 linux 中就是这么实现的，使用一个变量来管理锁状态就好。但是，问题就在于：这种没有状态记录的管理无法对锁的请求进行排序。  

这种实现方式就相当于你去银行取钱，当窗口忙碌的时候大家都在边上等着，上一位顾客办理完业务，所有等待的顾客一拥而上，而跑得快的或者离窗口近的人更有优势，但是我们都知道，这是不对的，正确的做法应该是先到先得，按照排队的顺序处理业务。  

在上述的 spinlock 的实现中体现了排队的思想：假设三个进程 A、B、C，A 先获得锁，B 请求锁，接着 C 请求锁，在 B 看来，spinlock 的 next-owner = 1，而在 C 看来，next- owner = 2（B 执行了 next+1 操作），当 A 释放时， owner + 1，此时在 B 看来，next==owner，B 可以继续往下执行，而 C 需要继续等待。这样就实现了先到先得的排队机制。不管能不能获得锁，先将 next + 1，这个操作相当于获取一个号码牌用于后续的排队。 


对于第二个问题，为什么要使用汇编代码来实现，一方面，汇编代码执行效率更快，另一方面， next+1 这个操作需要独占访问，为什么呢？同样举一个例子：CPU0 的进程 A 和 CPU1 的进程 B 同时请求锁，此时 A 获取到 next 的值为 0，B 获取到的 next 值也是 0，A 和 B 分别对 next 进行 +1 操作，两个自增操作最终产生的 next 值都为 1，然后再写回到内存，可以想到，此时 next 最终的值为 1，而不是我们预期中的 2.  

正常的操作流程应该是， CPU0 上的 A 取 next，对 next 自增，然后将 next 写回。即使同时 CPU1 上的 B 也要访问 next，也要等 CPU0 上的 A 操作完之后才能继续操作。可惜的是，这种多核中的独占访问并没有 C 语言的实现，而是需要硬件架构的支持，由 CPU 提供特殊的独占指令或者原子操作指令，在 arm 中，strex 和 ldrex 就是实现了独占访问，当处理器 A 使用了 ldrex 指令独占访问了 next 时，处理器 B 使用 ldrex 回写 next 时就会失败，然后重新读取 next 的值再更新，这也解释了上面的汇编代码的实现。  

对于 CPU 间的独占指令，每个架构的实现不一样，这也是为什么在 SMP 架构中，spinlock 的源码实现在 arch 目录下，因为它是硬件相关的。  

## spinlock 的释放锁
对于 spinlock 的释放，在加锁部分就已经有提及，相对应的，它的函数实现为：arch_spin_unlock，具体实现为：

```c++
static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	smp_mb();    // 内存屏障
	lock->tickets.owner++;  // 将 owner + 1 
    dsb_sev();
}
```
实现很简单，至于调用路径就不过多赘述了，那么，为什么 arch_spin_lock 中对 next 的操作需要使用汇编实现独占访问，而释放锁时对 owner 的操作不需要独占访问呢？其实这也很简单，存在多个并发的进程申请锁，但是并不存在并发地释放锁，因为锁始终只会被一个执行流所拥有，也就不存在并发地释放锁这种概念了。  

至于内存屏障，是为了防止 CPU 的乱序执行而导致的问题，这里就不展开讨论了。 

最后的 dsb_sev()，包含两条处理器相关的指令：dsb 和 sev，dsb 是一条数据屏障指令，而 sev 则是一条唤醒指令，它可以唤醒 WFE 状态的 CPU，这条指令和 arch_spin_lock 中的 wfe() 是相互呼应的，wfe 是让处理器处于 idle 状态，不再处理常规任务，尽管看起来它也是一种睡眠机制，但是只是节省能源而已，并没有实现效率上的提升，这和软件实现的睡眠机制是有本质区别的。  

同时，wfe 状态还会被中断等异常唤醒，如果对 wfe 机制有兴趣，可以参考 armv7-a 手册。  


# 关中断的接口中为什么还要关抢占
TODO

## 单核下的 spinlock
上文中讨论的 spinlock 基本上是基于多核的，你可以思考一下，单核下的 spinlock 应该如何实现？  



