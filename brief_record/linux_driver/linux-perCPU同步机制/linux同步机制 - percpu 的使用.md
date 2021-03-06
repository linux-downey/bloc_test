# linux同步机制 - percpu 的使用

## 同步机制
共享数据的同步问题，伴随着整个计算机的发展史，硬件的飞速发展，带动着操作系统和各类软件复杂度日益增长，数据同步问题也变得越来越棘手。   

对于一些微型的系统，即使运行在无操作系统的单片机上，也需要考虑中断与执行主程序链路之间的数据同步问题，这时候的软件设计中的同步方式有很多：锁、volatile关键词、临界区等等。  

操作系统的诞生使得程序分散为更多的执行流，尽管看起来所有进程(线程)并发执行，实际上在同一时刻还是只有一个执行流在工作，数据同步方式和微型系统本质上没有太多区别(从 主程序-中断 双执行流的方式变为多执行流)，只是考虑到系统的优化，细化了各种不同类型的锁、信号量机制。   

随着 SMP(对称多处理器架构) 的发展，在享受处理器更快的执行效率的同时，也为数据同步带来了更大的挑战，因为在 SMP 架构中，程序确实是在并发执行，从单 CPU 只需要考虑调度风险到 SMP 中还需要考虑到多 CPU 中同时执行的风险，而且在单 CPU 下的乱序执行也会在 SMP 架构中带来，从而也衍生了一些新的同步机制，比如：内存屏障、perCPU 机制等等。  




## percpu 机制
在传统软件环境下，优化程序的执行效率主要是针对程序逻辑和编译时指令的优化，随着硬件的发展，高速缓存也成了提高整体性能的一个很好的解决方案，但是高速缓存的命中和一致性一直是非常棘手的问题，这种情况在 SMP 架构中尤为突出。  

在 SMP 架构中，每个 CPU 都拥有自己的高速缓存，通常，L1 cache 是 CPU 独占的，每个 CPU 都有一份，它的速度自然是最快的，而 L2 cache 通常是所有 CPU 共享的高速缓存，当 CPU 载入一个全局数据时，会逐级地查看高速缓存，如果没有在缓存中命中，就从内存中载入，并加入到各级 cache 中，当下次需要读取这个值时，直接读取 cache 将获得非常快的速度，比直接读取内存高出几个数量级。   

对于读而言，cache 带来了巨大的性能提升，当涉及到修改时，也是在缓存中进行操作，然后同步到内存中，对于单 CPU 而言，这没有什么问题，但是对于 SMP 架构而言，一个 CPU 对全局数据的修改将会导致所有其它 CPU 上对该全局数据的缓存失效，需要全部进行更新，这个操作将带来性能上的损失。  

最好的军事战略是不战而屈人之兵，而最好的同步机制就是抑制同步产生的条件，因为每一种显式的同步机制都有着不可忽视的性能开销，percpu 就是这样一种同步机制：为了避免多个 CPU 对全局数据的竞争而导致的性能损失，percpu 直接为每个 CPU 生成一份独有的数据备份，每个数据备份占用独立的内存，CPU 不应该修改不属于自己的这部分数据，这样就避免了多 CPU 对全局数据的竞争问题。    

那么，问题立马就来了，percpu 之间的数据同步怎么做？很容易想到的是：当进程在 CPU0 上操作 percpu 变量，在某个时刻进程被调度到 CPU1 上执行时，CPU0 和 CPU1 上的 percpu 变量值就不同，将会导致严重的问题。   

事实上，percpu 只适合在特殊条件下使用：也就是当它确定在系统的 CPU 上的数据在逻辑上是独立的时候。正因为它的这种应用场景，CPU 之间的 percpu 变量并不需要同步，在整个 percpu 的生命周期内， percpu 变量对应的 CPU 副本都被对应的 CPU 独占使用。   

比较经典的应用场景是作为计数器使用，在网络接收程序中，接收数据包的程序可能先后运行在不同的 CPU 上，采用 percpu 变量进行计数，最后将所有 CPU 上的 percpu 副本相加就可以得到所接收数据包的总和，在这里，每个 CPU 上的 percpu 计数器并没有产生逻辑上的联系。  



## percpu 的存储
在 linux 内核的 Image 中，存在各种不同的 section，在内核代码中也时常可以看到 \_\_attribute\_\_ 关键字自定义 section ：

```
__attribute__(section(".section"),...)
```
\_\_attribute\_\_ 是 gcc 中扩展的关键字，通常作为修饰符，使用方式类似于 const、static，为符号添加各种属性。  

\_\_attribute\_\_(section()) 的作用就是将所修饰的对象放在编译生成二进制文件的指定 section 中，最常见的 section 有 .data/.data/.bss，在程序链接的阶段将会确定每个 section 最终的加载地址。   

对于普通的变量而言，变量的加载地址就是程序中使用的该变量的地址，可以使用取址符获取变量地址，但是对于 percpu 变量而言，该 percpu 的加载地址是不允许访问的。取而代之的，是在内核启动阶段，对于 n 核的 SMP 架构系统，内核将会为每一个 CPU 另行开辟一片内存，然后将该 percpu 变量复制 n 份分别放在每个 CPU 独有的内存区中，内存结构是这样的：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/lock_and_sync/percpu_memory_layout.jpg)


每个 CPU 都有一份真实的数据在内存中，具体的访问方式就是通过原始的存在于加载地址的变量作为跳板。  

也就是说，在为 percpu 分配内存的时候，原始的变量 var 与 percpu 变量内存偏移值 offset 被保存了下来，每个 CPU 对应的 percpu 变量地址为 (&var + offset)，当然真实情况要比这个复杂，将在后文中讲解。  



## percpu 变量的使用
讲了那么多繁杂的概念，驱动工程师最关心的还是怎么使用它，秉承 linux 内核的一贯设计风格，percpu 变量的接口非常易用，总共包含两个部分：
* 初始化(定义)
* 读写操作
* 如果是动态申请方式的初始化，需要释放。

接下来我们就来看看 percpu 机制所提供的接口。  



### percpu 的定义
percpu 变量的定义分为两类：静态定义和动态定义：

```
DEFINE_PER_CPU(type, name)
```
静态地定义一个 percpu 变量，type 是变量类型，name 是变量名。  

```
type __percpu *ptr alloc_percpu(type);
```

使用 alloc_percpu 动态地分配一个 percpu 变量，返回 percpu 变量的地址，但是这个返回的地址并非是可以直接使用的变量地址，就像静态定义的那样，这只是一个原始数据，真正被使用的数据被 copy 成 n(n=CPU数量) 份分别保存在每个 CPU 独占的地址空间中，在访问 percpu 变量时就是对每个副本进行访问。  



### percpu 的读写
对于静态的变量读写，通常使用以下的接口：

```
get_cpu_var(name)
put_cpu_var(name)
```
get_cpu_var 参数是定义时使用的变量名，通过这个函数获取当前执行这段代码的 CPU 所对应的 percpu 变量，而 put_cpu_var 表示当前操作的结束。它们的使用通常是这样的：

```
DEFINE_PER_CPU(int, val);
...
int *pint = &get_cpu_var(val)
*pint++;
put_cpu_var(val)
```
通常获取当前 CPU 的 percpu 变量的地址进行操作，在只读的情况下可以只获取变量值。  

事实上，put_cpu_var 并非字面上理解的：将变量放回内存，事实上它仅仅是使能了在 get_cpu_var 函数中关闭的内核抢占。  

所以，put_cpu_var 和 get_cpu_var 是成对出现的，因为这段期间内内核抢占处于关闭状态，它们之间的代码不宜执行太长时间。为什么在调用 get_cpu_var 时，第一步是禁止内核抢占呢?  

想想这样一个场景，进程 A 在 CPU0 上执行，读取了 percpu 变量到寄存器中，这时候进程被高优先级进程抢占，继续执行的时候可能被转移到 CPU1 上执行，这时候在 CPU1 执行的代码操作的仍旧是 CPU0 上的 percpu 变量，这显然是错误的。  


```
per_cpu_ptr(ptr, cpu)
```
这个接口是动态 percpu 变量操作的版本，与静态变量的操作接口不一样，这个接口允许指定 CPU ，不再是只能获取当前 CPU 的值，而第一个参数是动态申请时返回的指针，通过该指针加上 offset 来获取 percpu 变量的地址。通常操作是这样的:

```
int *pint = alloc_percpu(int);
...
int *p = per_cpu_ptr(pint,raw_smp_processor_id());
(*p)++;
```

其中，raw_smp_processor_id() 函数返回当前 CPU num，这个示例也就是操作当前 CPU 的 percpu 变量，这个接口并不需要禁止内核抢占，因为不管进程被切换到哪个 CPU 上执行，它所操作的都是第二个参数提供的 CPU。  



### 遍历 percpu 变量
尽管 percpu 变量的原则是 CPU 只操作自己所独占的 percpu 变量，但是软件上并没有做任何限制，每个 CPU 都可以通过接口或者是特定手段获取其他 percpu 变量的值并对其进行操作，内核提供遍历的接口：

```
for_each_cpu(cpu, mask)	
    per-cpu-operation..
```
for_each_cpu 第一个参数为 cpu,这只是一个临时变量，用于遍历时的赋值，第二个参数 mask 是 CPU 的位图(掩码)，类型为 struct cpumask，它的定义为：

```
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;
```

其中 NR_CPUS 表示当前硬件中的 CPU 数量,在 64 位系统中，struct cpumask 通常是一个 unsigned long(u64) 类型的 map，每一位代表一个 CPU ，一个 u64 的 map 最多表示 64 个 CPU 的状态(如果核心数大于 64,就是一个 u64 类型的数组)，比如对于四核架构，就是低四位被置 1 ，其他位为0 。   

对于 CPU 的掩码分为多种，分别有 \_\_cpu_possible_mask、\_\_cpu_online_mask、\_\_cpu_present_mask、\_\_cpu_active_mask，其掩码所描述的功能从字面就可以看出，值得一提的是，当 CPU 支持热插拔时，\_\_cpu_possible_mask 的值为 ~0。如果你对 CPU 的掩码有兴趣，可以在 linux/cpumask.h 中查看所有的类型，也可以参考 [cpu 初始化](https://zhuanlan.zhihu.com/p/363799713)。





### 参考

http://www.wowotech.net/kernel_synchronization/per-cpu.html

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

