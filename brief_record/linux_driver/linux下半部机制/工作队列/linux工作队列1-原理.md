# linux 工作队列的实现

在驱动工程师的印象中，工作队列就是将中断中不方便执行的任务延迟执行，将工作挂到内核某个队列上，在将来的某个阶段内核执行该工作。  

最简单的实现就是：内核启动时创建并维护一个工作队列，该队列由内核线程实现，没有任务执行时就陷入睡眠。在用户调用 schedule_work 时，将 work 挂到该工作队列的链表或者队列中，唤醒该内核线程并执行该 work。在有必要的时候，用户也可以自己创建一个 workqueue 来使用。   

如果涉及到 SMP 架构，事情可能就要复杂一点，考虑到 cpu 缓存的问题，如果多个 cpu 共用同一个队列，会导致过多的缓存失效从而带来效率上的问题。那么，就让每个 cpu 维护一个队列，cpu 之间的队列互不干涉，用户的工作被均匀分配给各个 cpu 的队列。   

同时，采用优先级机制，将用户的工作区分优先级，同样的也创建不同的优先级队列，这样就可以照顾到一些实时性较高的工作。   


上述这种实现就是传统的 workqueue 实现方式，出现在 2.6 之前，这种实现在通常情况下是可以接受的，但是也有一些缺陷所在：

* 每个 workqueue 上连接的 work 都是串行化执行的，如果某一个 work 需要长时间执行甚至睡眠，后续的 work 将被迫等待前面的 work 执行完成，由此还可能造成死锁问题。 
* 无法限制用户创建 workqueue 的数量，从而无法控制内核线程的数量，内核线程过多将导致内核执行效率降低。  

针对传统 workqueue 的限制，linux 在 2.6 版本的基础上对其进行了更新，引入了 CMWQ(Concurrency Managed Workqueue),即并发管理的工作队列。  

传统 workqueue 出现的问题大部分都是由于工作队列的排队机制引起的，即在同一个工作队列中，后就绪的 work 需要等待前面的 work 执行完才能执行，要解决这个问题，很容易想到的一个方法，就是支持动态地将工作进行迁移，当前面的工作执行有延迟或者阻塞时，将后面的工作动态迁移到其他的工作队列上。但是这样也有一个问题，一个阻塞的工作将占用一个工作队列，系统中添加的工作负担比较重的时候，还是会出现堵塞的问题，而且，用户创建自定义的工作队列导致内核线程过多的问题也得不到解决。  

另一个方法，也就是 linux 当前使用的方法：将工作队列和线程分离，每一个工作队列并不对应特定的内核线程，而是引入一个 worker_pool 的东西，worker 用来描述一个内核线程，顾名思义 worker_pool 就是一个 worker 池，这个 worker_pool 最大的特点是其伸缩性。  

当一个 workqueue 中有多个 work 需要执行时，理论上它们在工作 workqueue 中是顺序执行的，当前面的 work 阻塞时，workqueue 可以检测到并且动态地开启一个新的线程来执行后续的 work，如果是非常耗时的计算工作，由用户设置相应的标志位，该 workqueue 也将会腾出专门一个内核线程来执行这个 work。  

对于用户创建工作队列而言，因为工作队列的创建不再严格对应内核线程的创建(需要就创建，不需要就删除)，所以也解决了这个内核线程过多的问题。  

新的工作队列实现机制较传统的版本实现更为复杂，其源代码在 kernel/workqueue.c 中，目前(4.14)将近 6000 行代码，相比于传统的实现(800多行)多了数倍。  

## 关键结构体

每一个子模块都会涉及到一个或几个结构体来描述整个模块，包括数据、操作，workqueue 自然也不例外，我们来看看 workqueue 中几个主要的结构体：

* struct workqueue_struct：主要用于描述一个工作队列，在创建工作队列时返回该结构。
* struct work_stuct : 描述需要执行的工作
* struct worker_pool：工作队列池
* struct worker：描述真正执行工作的内核线程
* struct pool_workqueue：起一个连接作用，负责关联 workqueue_struct 和 worker_pool。

当我第一次打开 workqueue 的源代码，看到上述几个结构体以及他们相互交织以实现 workqueue 的功能，也是非常头疼，但是既然源代码都在眼前，唯一要做的就是不断地深入分析，经过一段时间的死磕，对于 workqueue 这几个核心结构体，博主有了一些自己的总结，他们之间的对应关系是这样的：

* work_pool 存在多个，通常是对应 CPU 而存在的，系统默认会为每个 CPU 创建两个 worker_pool，一个普通的一个高优先级的 pool，这些 pool 是和对应 CPU 严格绑定的，同时还会创建两个 ubound 类型的 pool，也就是不与 CPU 绑定的 pool。  

* worker 用于描述内核线程，由 worker pool 产生，一个 worker pool 可以产生多个 worker。

* workqueue_struct：属于上层对外的接口，一个 workqueue_struct 描述一个工作队列。如果把整个工作队列看成是一个黑盒子。  

* pool_workqueue:属于 worker_pool 和 workqueue_struct 之间的中介，负责将 workqueue_struct 和 worker_pool 联系起来，一个 workqueue_struct 对应多个 pool_workqueue。  

* work：由用户添加，可以是多个，一个 workqueue 对应多个 work


上述的描述还是很抽象，我知道你还是没有搞清楚他们之间的关系，为了更方便地理解它们之间的关系，我们以一个简单的例子来讲解它们之间的联系：

workqueue_struct 相当于公司的销售人员，对外揽活儿，揽进来的活儿呢，就是 work，销售人员可以有多个，活也有很多，一个销售人员可以接多个活，而一个活只能由一个销售人员经手，所以 workqueue_struct  和 work 是一对多的关系。  

销售人员揽进来的活自己是干不了的，那这个分给谁干呢？当然是技术部门来做，交给哪个技术部门呢？自然是销售目前所对接的技术部门，技术部门负责分配技术员来做这个活，如果一个技术员搞不定，就需要分配多个技术员。相对应的,这个技术部门就是 woker_pool，而这个技术员就是 woker，表示内核线程。

当开发者调用 schedule_work 将 work 添加到工作队列时，实际上是添加到了当前执行这段代码的 CPU 的 worker_pool->work_list 成员中(通常情况下)，这时候 worker_pool 就派出 worker 开始干活了，如果发现一个 worker 干不过来，就会派出另一个 worker 进行协助，活干完了就释放 worker。   

这个活干得好不好，进度怎么样，是否需要技术支持，这就需要一个项目管理来进行协调，向上连接销售人员，向下连接技术部门，这个项目管理起到一个联系作用，它就是 pool_workqueue。  

传统的模式是：一个销售(workqueue_struct)固定绑定一个或多个技术员(worker)，来了活儿技术员直接干，活儿太多也得排队等着，如果活儿太少，技术人员就自己在那儿玩，多招入一个销售，就得相对应地多分配出一个技术人员。  

CMWQ 模式是：一个销售人员(workqueue_struct)并不固定对应哪个技术部门(worker_pool)，也不对应哪个技术员(worker)，有事做的时候才请求技术部门(worker_pool)分配技术人员(worker)，没事做的时候就不需要，活儿多的时候就可以申请更多地技术人员(worker)，这种动态申请的方式无疑是提高了资源的利用率。  

看了这个例子，不知道你对这几个核心结构体有没有更深的了解，如果没有，接着看下面的图片，因为了解核心结构体是了解整个 workqueue 实现原理的关键所在：







## 结构体源码
下面我们来看看这几个核心数据结构中几个主要的结构体成员：

### workqueue_struct 

```C
struct workqueue_struct {
	struct list_head	pwqs;		//链表头，该链表中挂着所有与当前 workqueue_struct 相关的 pool_workqueue
	struct list_head	list;		//链表节点，通过该链表节点将该 workqueue_struct 链接到全局链表(workqueues)中，内核通过该全局链表统一管理 workqueue_struct。

	struct mutex		mutex;		//内核互斥锁
	...

	struct list_head	maydays;	//需要 rescue 执行的工作链表
	struct worker		*rescuer;	// rescue 内核线程，这是一个独立的内核线程

	struct workqueue_attrs	*unbound_attrs;	// ubound 类型 workqueue_struct 的属性，该属性定义了一个 workqueue 的 nice 值、numa 节点等，只针对 unbound 类型。
	struct pool_workqueue	*dfl_pwq;	//同样只针对 unbound 类型，pool_workqueue。

#ifdef CONFIG_SYSFS
	struct wq_device	*wq_dev;	//针对 sysfs 的属性，是否导出接口到 sysfs 中，通常是 /sys 目录
#endif

	char			name[WQ_NAME_LEN]; // 名称

	
	unsigned int		flags ____cacheline_aligned;  //针对不同情况下的标志位，后续文章中将会讲到
	
    struct pool_workqueue __percpu *cpu_pwqs;    // 指向 percpu 类型的 pool_workqueue
	struct pool_workqueue __rcu *numa_pwq_tbl[]; //指向 pernode 类型的 pool_workqueue
};

```



### pool_workqueue

```C
struct pool_workqueue {
	struct worker_pool	*pool;		//当前 pool_workqueue 对应的 worker pool。
	struct workqueue_struct *wq;	//当前 pool_workqueue 对应的 workqueue_struct。
	
	int			refcnt;		//引用计数，内核的回收机制
						
	int			nr_active;	//正处于活动状态的 work 数量。
	int			max_active;	//处于活动状态的 work 最大值

	struct list_head	delayed_works;	// delayed_work 链表，delayed_work 因为有个延时，需要记录起来
	struct list_head	pwqs_node;	  //节点，用于将当前的 pool_workqueue 结构链接到 wq->pwqs 中。
	struct list_head	mayday_node;	//当前节点会被链接到 wq->maydays 链表中。

	struct work_struct	unbound_release_work;
} __aligned(1 << WORK_STRUCT_FLAG_BITS);
```


### worker_pool

```
struct worker_pool {
	spinlock_t		lock;		//该 worker_pool 的自旋锁
	int			cpu;		    //该 worker_pool 对应的 cpu
	int			node;		    //对应的 numa node
	int			id;		        //当前的 pool ID.
	unsigned int		flags;	//标志位

	unsigned long		watchdog_ts;	//看门狗，主要用作超时检测

	struct list_head	worklist;	  //核心的链表结构，当 work 添加到队列中时，实际上就是添加到了当前的链表节点上。
	int			nr_workers;	          //当前 worker pool 上工作的数量

	
	int			nr_idle;	    //休眠状态下的数量

	struct list_head	idle_list;	//链表，记录所有处于 idle 的 worker。
	struct timer_list	idle_timer;	  //内核定时器用于记录 idle worker 的超时
	struct timer_list	mayday_timer;	// SOS timer

	DECLARE_HASHTABLE(busy_hash, BUSY_WORKER_HASH_ORDER);  //记录正运行的 worker

	struct worker		*manager;	// 管理线程
	struct list_head	workers;	// 当前 worker pool 上的 worker 线程。
	struct completion	*detach_completion;  //管理所有 worker 的 detach


	struct workqueue_attrs	*attrs;		// worker 的属性
	int			refcnt;		//引用计数
} ____cacheline_aligned_in_smp;
```


### worker

```C
struct worker {
	
	union {
		struct list_head	entry;    // 如果当前 worker 处于 idle 状态，就使用这个节点链接到 worker_pool 的 idle 链表
		struct hlist_node	hentry;	  // 如果当前 worker 处于 busy 状态，就使用这个节点链接到 worker_pool 的 busy_hash 中。
	};

	struct work_struct	*current_work;	//当前需要执行的 worker
	work_func_t		current_func;	    //当前执行 worker 的函数
	struct pool_workqueue	*current_pwq;  //当前 worker 对应的 pwq

	struct list_head	scheduled;	    //链表头节点，当准备或者运行一个 worker 的时候，将 work 连接到当前的链表中

	

	struct task_struct	*task;		// 内核线程对应的 task_struct 结构
	struct worker_pool	*pool;		// 该 worker 对应的 worker_pool
						

	unsigned long		last_active;	//上一个活动 worker 时设置的 timestamp
	unsigned int		flags;		//运行标志位
	int			id;		 //worker id 

	char			desc[WORKER_DESC_LEN]; // work 的描述信息，主要是在 debug 时使用

	struct workqueue_struct	*rescue_wq;	 // 针对 rescue 使用的 workqueue。  
};
```


在这一章节中，我们主要讲解了五个核心的结构体以及他们之间的关系，理清楚了这些关键的数据结构，我们再来一步步解析 workqueue 的实现流程。







参考:http://www.wowotech.net/irq_subsystem/cmwq-intro.html






