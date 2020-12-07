# linux 顺序锁 seqlock
在内核中，顺序锁和读写锁比较相似，都是针对多读少写且快速处理的临界区的锁机制，对于 rwlock 而言，因为写者的优先级很低，非常影响写进程的执行效率，极端情况下甚至可能出现写进程饿死的情况。  

而顺序锁和读写锁的区别就在与顺序所对读写进程的优先级做了调整，让写者的优先级始终高于读者，从而实现写者可以任意时刻打断读者，因此顺序锁更适用于写者优先级更高的应用场景。  

## 顺序锁的特性
在使用顺序锁之前，先要了解它对应的特性：
* 多进程对临界区的读不互斥，可同步进行，互不影响
* 当进程对临界区写时，可以直接执行写，至于读写之间的同步由读者去完成
* 写者之间是互斥的
* 不造成睡眠，等待形式是自旋

顺序锁的特点是写者优先，需要注意它的适用条件是多读少写的情况，如果写频繁的话，会导致读进程的执行效率降低，同样是不可取的。  

## 顺序锁的内核实现
对于顺序锁而言，主要是解决两个问题:
* 写者任意写，必然就会造成读者出现数据同步问题，比如读到旧数据，甚至读到一部分老数据一部分新数据，比如读者读取一个结构体，读到一半被另一个写者更新了(临界区数据通常是复合型数据，因为简单类型的数据可以直接使用原子操作而不需要用锁)，因此在读端需要确保读到的所有数据都是在写者处理完之后的新数据。  
* 同步写之间的互斥问题

对于上述需要解决的问题，linux 中顺序锁的实现是这样的：
* 使用一个锁变量来记录锁的状态，该锁变量是循环递增的，读者只读取锁变量，不对锁变量进行任何操作
* 读加锁不使用同步机制，甚至都不需要禁止内核抢占，因为读者只读取锁变量。
* 写者在写之前将锁变量加 1，执行完写之后将锁变量再加 1，意味着当锁变量为偶数时，表示没有写者正在执行，当变量为奇数时，表示写者正在执行操作。
* 读者实现同步的方式为：当锁变量为奇数时，读者自旋等待，只有当锁变量为偶数时，读者才执行读操作，同时在读之前记录锁变量的值，在读完成之后再对比锁变量的值，如果不一致，表示在读的过程中有写者更新，返回特定值。 
* 写操作直接使用 spinlock 进行互斥，以保证多进程或者多CPU之间不存在同步地写操作。 


## 顺序锁的内核源码

### 数据结构
顺序锁的数据结构为 seqlock_t，对应的源码为：

```c++
typedef struct {
	struct seqcount seqcount;
	spinlock_t lock;
} seqlock_t;

typedef struct seqcount {
	unsigned sequence;
    ...
} seqcount_t;
```  

数据结构存在两个成员，一个是锁变量 sequence，另一个是 spinlock 成员，该 spinlock 是用作写者之间的互斥的。  

需要注意的是，顺序锁的实现并不是架构相关的，因为数据结构中直接封装了 spinlock 用来互斥，不再需要其它架构相关的同步操作。   

### 初始化
顺序锁在使用之前需要进行初始化操作，直接调用 seqlock_init 即可，它会初始化 seqlock_t 类型锁的 seqcount 和 spinlock 成员。  

### 读加解锁
和 spinlock、rwlock 不同的是，顺序锁的读操作不需要更新锁变量，自始至终都是读取而已，所以并不需要进行数据保护，也不需要关抢占。  

读临界区操作的接口为：read_seqbegin 和 read_seqretry，从命名就可以看出，内核并没有将顺序锁的读加解锁作为真正的锁操作。   

它们的实现是这样的：

进入读临界区：
```c++
static inline unsigned read_seqbegin(const seqlock_t *sl)
{
	return read_seqcount_begin(&sl->seqcount);
}

read_seqbegin(s)
    read_seqcount_begin(s)
        raw_read_seqcount_begin(s)
            __read_seqcount_begin(s)
            smp_rmb();
                __read_seqcount_begin(s)


static inline unsigned __read_seqcount_begin(const seqcount_t *s)
{
	unsigned ret;
repeat:
	ret = READ_ONCE(s->sequence);
	if (unlikely(ret & 1)) {
		cpu_relax();
		goto repeat;
	}
	return ret;
}

```
在真正读取 seqcount 变量之前，需要设置一个读内存屏障，以防止乱序操作带来的问题，接着就是循环地读取 seqcount，确保在 seqcount 为偶数的时候再读取，如果 seqcount 为奇数时，表示当前有写者正在执行，读取旧值是没有意义的。  

同时，返回读取到的值。  

尝试退出读临界区：
```c++
static inline unsigned read_seqretry(const seqlock_t *sl, unsigned start)
{
	return read_seqcount_retry(&sl->seqcount, start);
}

read_seqretry(s, start)
    read_seqcount_retry(s, start)
        smp_rmb();
        __read_seqcount_retry(s, start)

static inline int __read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	return unlikely(s->sequence != start);
}

```
对于退出读临界区，同样需要设置读内存屏障，同时尝试退出操作也很简单，就是对比 read_seqbegin 的返回值和当前的 seqcount 是否相等，返回结果。顺序锁并不对读取操作做任何保证，它只是返回操作结果，可能成功，可能失败，所以，内核中顺序锁读临界区操作的使用方式是这样的：

```c++
retry：
    seq = read_seqbegin(&seqlock);
    ...
    if (read_seqretry(&seqlock, seq))
		goto retry;
```

### 写加解锁
对于顺序锁的写临界区实现，实际上也非常简单，因为写操作直接使用 spinlock 进行互斥，也就保证了写操作的独占性。写加解锁的接口为：write_seqlock、write_sequnlock。  

```c++
static inline void write_seqlock(seqlock_t *sl)
{
	spin_lock(&sl->lock);
	write_seqcount_begin(&sl->seqcount);
}

static inline void write_seqcount_begin_nested(seqcount_t *s, int subclass)
{
	raw_write_seqcount_begin(s);
    ...
}

static inline void write_seqcount_begin(seqcount_t *s)
{
	write_seqcount_begin_nested(s, 0);
}

static inline void raw_write_seqcount_begin(seqcount_t *s)
{
	s->sequence++;
	smp_wmb();
}
```
从源代码可以看出，进入写临界区之前进入 spinlock 临界区，然后直接对 seqcount 进行 +1 操作，最后设置一个内存屏障以保证加锁操作对所有 CPU 可见。  

而退出写临界区的操作几乎和进入临界区操作一样，差别在于内存屏障的设置位置，这里就不过多赘述了。  


和 rwlock、spinlock 一样，req lock 也提供 _bh/_irq/_irqsave 版本来禁止本地中断或者中断上下文。  



