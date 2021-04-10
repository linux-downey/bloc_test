# linux调度子系统4 - 调度优先级
如何决定一个进程是否得到调度器的更多照顾，获得更多的执行时间或者得到先运行的资格，自然是通过进程的优先级来决定的，在操作系统中，优先级只是个相对宽泛的概念，其具体的实现方式又是多种多样。  

linux 中进程支持多种调度策略，有针对实时进程和非实时进程的，不同的调度策略对应不同的优先级，对于非实时进程而言，其优先级在用户空间的表示方式为 nice 值，nice 值的范围为 -20~19，这是 linux 设计之初就使用的优先级，数值越大，而表示的优先级越小。  

nice 这个词用得很有意思，在英语中，一般使用 nice 来描述 "好" 的意思，为什么这里的进程使用 nice 呢，一个进程越 nice(nice值越大)，表示它越 gentle，也就更懂得礼让，从而优先级越低。  

在内核的发展中，使用 nice 来表示进程优先级渐渐变得不那么方便，内核更倾向于直接使用优先级 prio 来描述进程的优先级，但是由于兼容性的原因，必须保留 nice 值，因此进程就由有两种优先级表示方式 prio 和 nice，对于实时进程而言，优先级 prio 占用 0~99，非实时进程优先级为 100~139，nice 值的 [-20,19] 与 [100,139] 之间建立直接的映射，内核提供两者之间的转换接口，nice 值并不作用于实时进程。  

进程的优先级表示并不仅仅是 prio，而是在 task_struct 中有三个字段：prio、normal_prio、static_prio。  
prio：进程的动态优先级，动态优先级会随着进程的运行而调整，从而决定了进程当前状态下获取 CPU 的能力，需要注意的是，**动态优先级对 cfs 调度器的影响很小，它主要作用于其它调度器**  
static_prio:进程预设的优先级，也就是进程创建之初就给定的一个优先级，这个优先级直接决定了进程在将来的调度行为中获取 CPU 的能力。
normal_prio：对于非实时进程，prio 值越大，其优先级越小，而对于实时进程，prio值越大，其优先级越大，在某些情况下，需要对这两种进程进行统一的优先级操作时，并不方便，因此需要使用一个统一的优先级表示方式，而 normal_prio 就是统一了实时和非实时进程的优先级表示，对于非实时进程而言，normal_prio = static_prio，而对于实时进程而言，需要进行相应的转换，内核中的转换函数为 normal_prio：其实现为：对于 dl 调度进程，normal_prio 为 -1，对于 rt 调度进程，normal_prio = 99 - p->rt_priority，即实时进程的最大值减去 p->rt_priority。




## 调度实体的权重 load weight
根据前面文章的描述，调度的单元并非进程，而是调度实体，调度器类的操作对象为调度实体，因此，对于每一个调度实体也需要使用一个数据成员来描述优先级，每种调度器的调度实体结构都是不一样的，比如 cfs 调度器对应的实体为 sched_entity，而  rt 调度器的调度实体使用 sched_rt_entity 描述。  

进程的优先级是一种通用的描述方式，相对而言，调度实体则更贴近具体的调度器，可以使用和调度器强相关的参数来表示优先级，对于 cfs 调度器，sched_entity 使用 load 数据成员表示优先级，load 表示权重，是 struct load_weight 类型的数据。  

```c++
struct load_weight {
	unsigned long	weight;
	u32				inv_weight;
};
```

在进程被创建时，同时被创建的还有进程对应的 sched_entity，sched_entity->weight 的赋值就是一个查表行为，100~139 优先级的进程对应的 weight 定义在数组 sched_prio_to_weight 中，默认情况下，用户所创建的进程 nice 值为 0，对应调度实体的 weight 值为 1024，相差一个 nice 值，对应的 weight 值之间的差别大约为 1.25 倍。  

weight 直接关系到调度行为，在前面的文章中有提到，进程的调度和 vruntime 的增长速度是相关的，高优先级进程的增长速度要慢于低优先级进程，至于慢多少呢？就是直接根据这个 weight 计算得到的，每相差一个 nice 值 vruntime 的增长速度相差 1.25 倍。  

至于这个 1.25 倍的系数是怎么得到的，可以参考 kernel/sched/core.c 中 sched_prio_to_weight(weight 与 nice 的映射静态数组)处的注释，对于 linux 中 nice 值的设计是：每提高一个进程的 nice 值，则该进程多获得 10% 的运行时间。需要注意的是，这里的 10% 是相对的，而不是绝对的，也就是当系统中某个进程的 nice 值提升 1，该进程会获得更多的时间，其它进程会相对应的减少一些时间，这个 10% 等于当前进程增加时间的百分比 + 其它进程减少时间的百分比。  

比如系统中存在 3 个进程，nice 值都为 0，对应 load weight 为 1024,1024,1024，每个进程占 33% 运行时间，假设将其中一个进程 nice 值减一，对应的 load weight 值为 1277，计算得出该进程占用 CPU 的比值为 1277/(1024+1024+1077) = 38%,其它两个进程占用比值为 1024/(1024+1024+1277) = 31%，则该进程所提升的 CPU 时间为 (33%-31%) \* 2+(38% - 33%)= 10%，通常通过浮点计算出来的结果并不是完全精确的，只是大概在 10% 左右。  

实际上的计算并不是像上面的计算方法一样带有除法，出于兼容性以及运算速率的考虑，内核中是不支持浮点运算的，因此需要将除法转换成乘法进行计算，而 struct load_weight 的另一个成员 inv_weight 就是保存的乘法转换的系数，这个系数同样是静态定义在数组 sched_prio_to_wmult 中，其中 40 个系数值，对应非实时进程的 100~139 优先级。  




## 调度优先级的初始化
在调用 fork 创建进程的时候，会对进程的优先级进行初始化，进程的静态优先级是继承了父进程的静态优先级，而动态优先级 p->prio 继承了父进程的 normal 优先级，这是为了防止在某些情况下父进程的优先级反转传递给子进程，而子进程的 normal_prio 则是根据不同调度器进行计算得来，可以参考上文中的描述。  

进程的初始化同时伴随着调度实体的初始化，毕竟调度器只认调度实体而不认进程，对于 se->load 的初始化接口为：set_load_weight。 


_do_fork->copy_process->sched_fork->set_load_weight
```c++
static void set_load_weight(struct task_struct *p)
{
	int prio = p->static_prio - MAX_RT_PRIO;
	struct load_weight *load = &p->se.load;
	// 当进程的 policy 为 idle 时(不一定是 IDLE 进程，可以设置普通进程的调度策略为 IDLE)，设置 load weight 值为 3，inv_weight 为 1431655765
	if (idle_policy(p->policy)) {
		load->weight = scale_load(WEIGHT_IDLEPRIO);
		load->inv_weight = WMULT_IDLEPRIO;
		return;
	}

	// 如果是正常的进程，通过 prio 在数组中查找对应的值，prio 和 nice 值可以相互转换 
	// 用户创建的默认进程的 nice 值为 0，对应 load weight 为1024，inv_weight 为 4194304，其实这里说的默认进程是不大准确的，有种好像不指定 nice 值就为 0 的感觉，实际上子进程都是继承了父进程的优先级，只是一般的 linux shell 环境一般 nice 值为 0.
	load->weight = scale_load(sched_prio_to_weight[prio]);
	load->inv_weight = sched_prio_to_wmult[prio];
}
```

下面是两个系统静态定义的 load weight 和 inv_weight 数组：

```c++
// const int sched_prio_to_weight[40] = {
	 /* -20 */     88761,     71755,     56483,     46273,     36291,
	 /* -15 */     29154,     23254,     18705,     14949,     11916,
	 /* -10 */      9548,      7620,      6100,      4904,      3906,
	 /*  -5 */      3121,      2501,      1991,      1586,      1277,
	 /*   0 */      1024,       820,       655,       526,       423,
	 /*   5 */       335,       272,       215,       172,       137,
	 /*  10 */       110,        87,        70,        56,        45,
	 /*  15 */        36,        29,        23,        18,        15,
	};
	const u32 sched_prio_to_wmult[40] = {
	 /* -20 */     48388,     59856,     76040,     92818,    118348,
	 /* -15 */    147320,    184698,    229616,    287308,    360437,
	 /* -10 */    449829,    563644,    704093,    875809,   1099582,
	 /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
	 /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
	 /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
	 /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
	 /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
	};
```

其实，数值看起来并没有太多意义，它们之间都是以 1.25 的倍数，也不是完全精确的，而是存在一些误差，这些误差并不都是精度造成的，而是人为地执行了一些调整，毕竟最终需要满足的是一个 nice 值对应 10% 的 CPU，这个 1.25 的倍数也只是一个约值。  

等比数列有指数增长的特性，因此，相差 10 个 nice 值，两个进程之间运行时间相差 9 倍，而相差 20 个 nice 值这个倍数迅速扩大到将近 90 倍，因此，别小看一个 nice 值造成的进程之间的差别。 




## load_weight 的应用
调度实体的优先级使用权重 weight 表示，而这个 weight 和系数都是系统预定义的，那么，它们是如何作用于调度算法的？  

当一个进程投入运行，在多处函数中会对该进程进行 vruntime 的更新，正如上文中提到的，vruntime 的增长速度取决于 load weight 的值，更新 vruntime 的代码如下：

```c++
static void update_curr(struct cfs_rq *cfs_rq){
	...
	curr->vruntime += calc_delta_fair(delta_exec, curr);
	...
}

static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
	if (unlikely(se->load.weight != NICE_0_LOAD))
		delta = __calc_delta(delta, NICE_0_LOAD, &se->load);

	return delta;
}
```

calc_delta_fair 函数传入的第一个参数是进程当次运行的真实时间，返回的是根据 load(优先级)计算出来的 vruntime 值，附加到 curr->vruntime 上，也就完成了 vruntime 的更新。  

核心的计算工作由 __calc_delta 完成，这个函数带有三个参数，分别为 delta、weight(参考值)、load_weight（目标 load weight），其运算公式就是：

```c++
return (delta_exec * weight / load_weight.weight)
```

比如计算 vruntime 时，参考的 weight 为 nice0 对应的 load-1024，因为 nice0 进程的 vruntime 直接对应真实时间，假设一个 nice 为 1 的进程，返回的 vruntime 为 1.25*delta，如果 nice 为 n，则是 delta *(1.25^n)，因此 nice 为 1 的 进程 vruntime 比 nice 为 10 的进程要慢很多。

而 __calc_delta 的实现并不简单，因为实际是不能使用除法的，因此需要引入 inv_weight 进行计算，实现就变得相对复杂。  







### 参考

[蜗窝科技：进程优先级](http://www.wowotech.net/process_management/process-priority.html)

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。


