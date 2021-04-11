# linux 读写锁 rwlock
在 [spinlock 实现](https://zhuanlan.zhihu.com/p/363993550) 介绍了 spinlock 的实现，相对来说，spinlock 是一种比较通用的实现，适用于大部分快速执行的临界区数据保护。  

相对于 spinlock 的不区分读写操作，如果对于共享对象的读写操作明显不均衡的时候，是不是可以针对读写做进一步的优化，比如在多读少写的情况下，读操作之间的互斥是不是真的有必要，毕竟如果一个共享对象是只读的，都不需要使用复杂的锁机制进行保护，只需要防止编译器优化即可。  

因此，对于多读少写的情况，理想的状态是读之间不需要做互斥，只有读写和写写的情况下需要。而对于多写少读的情况，使用 spinlock 就可以了，毕竟写之间的互斥是必须的。  

这就引出了两种新的、针对读写不平衡的锁：读写锁 rwlock 和顺序锁 seqlock。这两种锁都实现了同步读的无锁方案以提高读取效率，而区别在于对锁的优先级定义。这一章我们主要将 rwlock，seqlock 放到下一章。  



## rwlock 的特点
rwlock 主要有以下几种特征：



* 多进程对临界区的读不互斥，可同步进行，互不影响
* 如果要执行写，需要等所有的读者退出才能执行写操作
* 如果正在执行写操作且未完成，这一阶段发生的读操作会被阻塞，即读写互斥
* 如果正在执行写操作且未完成，这一阶段发生的读操作会被阻塞，即写写互斥
* 不造成睡眠，等待形式是自旋

这种场景有点像行人过马路，公交车司机必须停在斑马线前等待所有行人过完马路才能继续往前开，在繁忙的时段，不断地有行人走过，就会导致公交车一直止步不前，甚至造成堵车。  

这也是 rwlock 的一大缺点：写者优先级太低，在极端情况下甚至出现饿死的情况。  



## rwlock 的实现
对于这类简单的锁，通常是使用一个变量来记录锁的状态，所有加解锁的行为都根据这个变量来进行操作，同时，因为这个变量也是共享数据，对变量的操作需要是原子的、或者说是互斥的，在 spinlock 章节介绍了两个指令：ldrex 和 strex，这两条指令可以实现内存操作的独占性，也就解决了锁变量的同步问题。  

同时，不同于 spinlock 的对读写一视同仁，rwlock 需要区分读写操作，所以需要实现四种接口：读加锁、读解锁、写加锁、写解锁。  

至于具体的，内核中的 rwlock 是这样实现的：



* 定义一个 lock 变量，加解锁的操作基于该变量完成
* 读加锁对 lock 变量加 1，对应的，读解锁对变量减 1.
* 写加锁对 lock 变量的最高位置 1，对应的，写解锁清除 lock 变量最高位。 
* 针对读写互斥，当读者判断 lock 变量最高位为 1 时，需要等待最高位被清除再继续操作，也就是等正在执行的写者写完再操作。 
* 针对写写互斥，当写者判断 lock 变量最高位为 1 时，需要等待最高位被清除再继续操作，也就是等正在执行的写者写完再操作。
* 针对写优先级最低的情况，当写者判断 lock 变量不为 0 时，表示还有读者没退出，需要等待 lock 变量为 0 才能进入操作。 

上面是 rwlock 的主要执行逻辑，除了主要逻辑之外，还需要注意三个部分：



* 当发生读写互斥、写写互斥或者写者等待读时，等待过程并不是简单地自旋(或者说是 busy loop)，arm 平台中使用 wfe 指令让 CPU 进行节能模式，在特定的时间被唤醒，损失了效率节省了功率，这也算是一种心理安慰吧（当然这不是主动的拿效率换功率，只是一种无奈的补救方法），注意这里的 CPU 的休眠和唤醒和进程的休眠唤醒完全是两个概念，CPU 的休眠是让 CPU 大部分部件停止工作以降低功耗。  
* 对于读加锁和写加锁都要禁止抢占，原因也很简单，假设在不禁止抢占的读写锁实现中，进程 A 对锁进行了读加锁，立刻被 B 抢占，在 B 中对锁进行写加锁操作，因为读者 A 没有释放，所以 B 得等到 A 释放锁，但是 A 进程并没有运行，而正在运行的 B 只能忙等，这种情况明显是不允许的。  
* 除了要禁止抢占，是否要禁止中断和中断下半部呢？答案其实和 spinlock 的实现一样，如果你在中断或者中断下半部中会和普通进程抢占同一把锁，那么进程中的锁操作接口就需要禁止中断或者中断下半部。当然，你可以不分青红皂白地直接使用禁止中断的接口，这不会有什么问题，只是会带来效率的损失。对应的接口为：read_lock_irq/write_lock_irq 等。 




## rwlock 的内核源码
了解了 rwlock 的实现，再来看看 rwlock 中的内核源码实现，需要注意的是，针对内核中的大部分锁机制，都支持 lockdep 死锁检测机制，本章节并不会 lockdep 作深入讨论，所以会省略一些源码，只保留关键的部分进行分析。  

### 数据结构
rwlock 的数据结构为：

```c++
typedef struct {
	arch_rwlock_t raw_lock;
    ...
} rwlock_t;

// arch/arm
typedef struct {
	u32 lock;
} arch_rwlock_t;
```

rwlock 的结构为 rwlock_t，主要的成员为 arch_rwlock_t raw_lock，因为对于锁变量的共享操作实现是架构相关的，比如 arm 中是使用 ldrex 和 strex 指令的组合，而 x86 使用的是 LOCK 总线的方式，所以不论是最终的数据定义还是接口的底层实现都是由架构相关的文件提供的，而上层的 rwlock 只是提供一个抽象的接口，这一点和 spinlock 是相同的。  

在 arm 架构中，arch_rwlock_t 结构中就一个 32 位的锁变量成员 lock。  

关于 rwlock_t 其它的成员，基本上是和 debug 调试相关的，这里就不再赘述了。  



### rwlock 初始化
既然默认配置下的 rwlock 只有 raw_lock 这个字段，初始化自然也是针对这个成员，arm 下是一个 u32 的 lock 变量，就是将这个变量赋值为 0.对应的接口为 rwlock_init(lock)。

### 读加锁 read_lock
read_lock 的调用路径为：

```c++
read_lock(lock)
    _raw_read_lock(lock)
        __raw_read_lock(lock);
            preempt_disable();           // 关闭抢占
            LOCK_CONTENDED(lock, do_raw_read_trylock, do_raw_read_lock);  
                do_raw_read_lock(lock)
                    arch_read_lock(&lock->raw_lock);
```

从 read_lock 到 arch_read_lock 经过了层层调用，中间过程中关闭了内核抢占，关闭抢占的理由在上文中也已经讲过了，接下来看看 arch_read_lock 的实现：

```c++
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2;
	prefetchw(&rw->lock);         // 预取指令，显式指定加载到缓存提高效率
	__asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"           // tmp = rw->lock
"	adds	%0, %0, #1\n"         // tmp = tmp + 1,并将结果更新到 cpsr
"	strexpl	%1, %0, [%2]\n"       // rw->lock = tmp,执行结果放在 tmp2 中，pl 后缀判断 cpsr 中的 flag，因为上条指令使用后缀 s 更新了 cpsr flag，所以表示上条指令结果大于等于 0 才执行当前指令
	WFE("mi")                     // mi 表示 cpsr 中 N flag 等于 1，表示 (adds	%0, %0, #1) 这条指令结果小于 0
"	rsbpls	%0, %1, #0\n"         // tmp = 0 - tmp2. 后缀分为两部分：第一部分 pl 表示结果大于等于 0 才执行当前指令，第二部分 s 表示将结果更新到 cpsr。 
"	bmi	1b"                       // 如果结果小于 0，跳到标号 1 处
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&rw->lock)
	: "cc");
	smp_mb();                     // 内存屏障         
}
```
在 arm 架构中，arch_read_lock 是由嵌入汇编实现的，它执行了以下的动作：



* rw->lock 对应内存的值 +1，使用 ldrex 和 strex 独占内存，在 spinlock 的文章中有对这两条指令做基本的介绍。 
* 如果 rw->lock 在加 1 之后小于 0，则让 CPU 进入低功耗，因为写加锁操作会将 lock 变量的最高位置为 1，如果 rw->lock 小于 0 意味着有其它线程在该锁上执行写操作，要登上一段事件，所以干脆让 CPU 进入低功耗。当然，写者在 unlock 的时候会发送唤醒指令。 
* 如果没有写者或者被唤醒之后，判断 strex 更新内存操作是否成功，当 tmp2 为 0 表示成功，失败则跳转到标号 1 处从头执行。 
* 内存屏障防止编译器乱序，让整个加锁过程对于其它线程完全可见，也就是说：加锁代码后面的指令不能乱序排到加锁之前执行，这明显是不对的。 

这段汇编代码涉及到的 armv7 指令集知识有点多，想深入研究的朋友建议参考 armv7 指令集架构手册。  



### 读解锁 read_unlock
读解锁的接口为 read_unlock，同样的，最后的执行代码也是定义在体系架构的文件中，需要注意的是，在解锁操作执行之后，需要打开内核抢占。具体的调用路径我就不贴出来了，直接看对应的架构相关的读解锁接口实现：

```c++
static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2;

	smp_mb();                   // 内存屏障

	prefetchw(&rw->lock);       // 预取指令，显式指定加载到缓存提高效率
	__asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"           // tmp = rw->lock
"	sub	%0, %0, #1\n"             // tmp = tmp - 1 
"	strex	%1, %0, [%2]\n"       // rw->lock = tmp, strex 执行结果放在 tmp2 中，更新成功时 tmp2 为 0，否则为 1
"	teq	%1, #0\n"                 // 判断 tmp2 是否等于 0
"	bne	1b"                       // 如果 tmp2 不等于 0，表示 strex 更新失败，需要重新来一次。  
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&rw->lock)
	: "cc");

	if (tmp == 0)
		dsb_sev();               // 如果 tmp 等于 0，发送唤醒事件。 
}
```

read_unlock 部分代码主要是以下几个部分：



* 内存屏障，保证解锁代码不会乱序排到解锁前面的代码之前执行
* 执行 rw->lock 减一的操作，同样使用 ldrex 和 strex 指令组合，并判断是否成功。 
* 当 tmp == 0，也就是 rw->lock 的值为 0 时，需要执行 sev 指令，该指令发送 CPU 唤醒事件，这个唤醒事件是为了唤醒写者的，因为写者需要等待所有读者执行完毕，等待期间会让 CPU 进入休眠，而 rw->lock 为 0 也就表示所有读者执行完毕，可以唤醒对应的 CPU 上的写者。  




### 写加锁 write_lock
写接口为 write_lock(lock)，同样的，在写之前关闭内核抢占，直接看对应的 arm 架构的写实现：

```c++
static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned long tmp;

	prefetchw(&rw->lock);        // 预取指令，显式指定加载到缓存提高效率
	__asm__ __volatile__(
"1:	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"                 // 测试 rw->lock 变量是否为 0，即检查是否有读者或写者，teq 指令默认更新 cpsr 的 flag
	WFE("ne")                     // 如果 rw->lock 不为 0，表明有其它进程在操作，直接让 CPU 休眠
"	strexeq	%0, %2, [%1]\n"      // rw->lock = 0x80000000
"	teq	%0, #0\n"                 // 判断 strex 是否执行成功
"	bne	1b"
	: "=&r" (tmp)
	: "r" (&rw->lock), "r" (0x80000000)
	: "cc");

	smp_mb();                    // 内存屏障
}
```
write_lock 的实现正如上文中介绍的那样：



* 判断并等待 rw->lock 为 0，如果不为 0，表示有其它进程正在读或者写，这时候让 CPU 休眠，read_unlock 和 write_unlock 都会在适当的时机发送唤醒事件
* 如果 rw->lock 为 0，就将 rw->lock 的最高位设置为 1，表示当前进程正在操作，你们等着吧。 
* 最后，添加一个内存屏障，表示加锁后的代码不会乱序排到加锁之前执行。  



### 写解锁 write_unlock
写解锁的接口是 write_unlock，同样在写解锁操作之后需要添加一个使能内核抢占的操作，写解锁的底层接口为：

```c++
static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	smp_mb();

	__asm__ __volatile__(
	"str	%1, [%0]\n"
	:
	: "r" (&rw->lock), "r" (0)
	: "cc");

	dsb_sev();
}
```

写解锁非常简单，写完成之后 rw->lock 的值肯定是 0，而且写解锁不存在并发，只是需要发送一个 CPU 唤醒事件以唤醒那些等待在写操作之后的 CPU。  

## 单核下的 rwlock
上述所讨论的都是多核下的 rwlock 实现，你可以想想单核下的 rwlock 应该如何实现。  

## rwlock 的性能
相对于通用的 spinlock 而言，读写锁的好处在于对读者的优化，而这种优化带来的后果是写者的低优先级，这也就注定了它只能在一些多读少写且写优先级低的情况应用，实际上，rwlock 尽管相对于 spinlock 有一些优势，它也并不常用在它的优势场景，而是被 RCU 所替代。  



### 参考

4.14.98 源码

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

