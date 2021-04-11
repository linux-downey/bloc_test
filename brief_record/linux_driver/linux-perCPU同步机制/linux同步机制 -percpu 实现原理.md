# linux同步机制 -percpu 实现原理

[上一章](https://zhuanlan.zhihu.com/p/363969903)对 percpu 同步机制进行了简单的介绍，并讨论了如何使用该机制，这一章，我们将要继续深入，探寻它背后的实现原理。  



## percpu 变量的定义
percpu 变量的静态定义是通过 DEFINE_PER_CPU(type,name) 接口，它的源代码实现如下：

```C
#define DEFINE_PER_CPU(type, name)					\
	DEFINE_PER_CPU_SECTION(type, name, "")

#define DEFINE_PER_CPU_SECTION(type, name, sec)				\
	__PCPU_ATTRS(sec) PER_CPU_DEF_ATTRIBUTES			\
	__typeof__(type) name

#define __PCPU_ATTRS(sec)						\
	__percpu __attribute__((section(PER_CPU_BASE_SECTION sec)))	\
	PER_CPU_ATTRIBUTES

#ifdef CONFIG_SMP
    #define PER_CPU_BASE_SECTION ".data..percpu"
#else
    #define PER_CPU_BASE_SECTION ".data"
#endif

```
经过一番兜兜转转的追踪，找到了 DEFINE_PER_CPU 的定义，将宏展开的结果就是：

```C
#ifdef CONFIG_SMP
#define DEFINE_PER_CPU(type, name)					\                               
    __percpu __attribute__((section(".data..percpu"))) type name;  \ 
#else   \
    __percpu __attribute__((section(".data"))) type name; \
#endif
```
整个实现翻译过来就是：在 SMP 架构下，被定义的 percpu 变量在编译后放在 .data..percpu 这个 section 中，在单核系统中， percpu 变量被放在 .data 也就是数据段中，所以单核系统下的 percpu 变量直接被当成普通变量进行处理，事实上，所有基于 percpu 的讨论都是基于 CONFIG_SMP 被设置的情况下，也就是 SMP 架构下。这里我们主要关注的是 SMP 架构下的 percpu 变量处理。  




## section的处理与链接

既然涉及到 section 的定位，自然需要查看 linux 的链接处理，链接处理和平台是强相关的，这里我们以 arm 平台为例：  

在链接脚本 arch/arm/kernel/vmlinux.lds 中，对 .data..percpu section 进行了重定位(为该 section 分别加载地址)。

```
...
. = ALIGN((1 << 12)); 
.data..percpu : AT(ADDR(.data..percpu) - 0) {
     __per_cpu_load = .; __per_cpu_start = .; __per_cpu_user_mapped_start = .; 
     *(.data..percpu..first) . = ALIGN((1 << 6)); 
     *(.data..percpu..user_mapped) 
     *(.data..percpu..user_mapped..shared_aligned) . = ALIGN((1 << 12)); 
     *(.data..percpu..user_mapped..page_aligned) __per_cpu_user_mapped_end = .; . = ALIGN((1 << 12)); 
     *(.data..percpu..page_aligned) . = ALIGN((1 << 6)); 
     *(.data..percpu..read_mostly) . = ALIGN((1 << 6)); 
     *(.data..percpu) 
     *(.data..percpu..shared_aligned) __per_cpu_end = .; 
}
```
其中，"." 在链接脚本中用作定位符，也就是通过 "." 来确定符号的加载位置，"." 的值是自动更新的，比如下列的脚本：

```
. = 0x1000
.text : { *(.text) }
```
在执行完这个指令之后 . 的位置就成了 0x1000 + sizeof(*.text)。

在链接脚本的前面部分 "." 已经被赋值了，而且添加了不少的段，我们暂时不去分析 "." 具体的值(注1)，主要看段的内容：

在重定位中，.data..percpu 段被重新定义，从开始的位置依次存放源代码代码中定义的放在 .data..percpu..first(section)、.data..percpu..user_mapped(section)、...、.data..percpu(section)、.data..percpu..shared_aligned(section) 这些自定义段中的 percpu 变量。  

同时，定义了几个变量：\_\_per_cpu_load、\_\_per_cpu_start、\_\_per_cpu_user_mapped_start、\_\_per_cpu_end，分别对应 percpu 的加载地址、percpu 变量内存开始地址、用户映射开始地址、percpu 变量内存结束地址。  

在上文的介绍中，我们只讨论了 DEFINE_PER_CPU 这个系统提供的最基础的也是用得最广泛的接口，事实上除了这个，内核还提供一些特殊场景下使用的接口：

```
//该接口定义的 percpu 变量将被放在 .data..percpu..first section 中，这些 percpu 变量始终放在 percpu 内存的头部位置
DEFINE_PER_CPU_FIRST(type, name)

//该接口定义的 percpu 变量将被放在 .data..percpu..user_mapped..shared_aligned 中，这些 percpu 变量在 SMP 的情况下将会对齐到 L1 cache，单核 CPU 的情况下则不对齐
DEFINE_PER_CPU_SHARED_ALIGNED(type, name)

//该接口定义的 percpu 变量将被放在 .data..percpu..page_aligned 中，无论 SMP 还是单核CPU，都会对齐到 L1 cache。
DEFINE_PER_CPU_ALIGNED(type, name)


//该接口定义的 percpu 变量将被放在 .data..percpu..read_mostly 中。  
DEFINE_PER_CPU_READ_MOSTLY(type, name)
```
这些接口定义的 percpu 变量都将被加载到 .data..percpu section 开始的内存。   

注1：如果你想确认当前平台下 .data..percpu section 重定位的位置，除了从链接脚本进行推理，也可以直接在 System.map 文件中搜索 \_\_per_cpu_load 或者 \_\_per_cpu_start 符号对应的地址即可，这个地址和内核在 ram 中的加载位置相关联。  




## percpu 内存的初始化

在[上一章](https://zhuanlan.zhihu.com/p/363969903)中，我们提到：.data..percpu section 中 percpu 变量加载到内存中后，并不能被直接使用，而是为每个 cpu 分配一个独立的 percpu 内存，将变量拷贝到每个 cpu 内存中，每个 cpu 只访问属于自己的 percpu 内存区，以这种分隔的形式来保证数据的独立。   

具体的实现我们深入到源代码中查看：主要的初始化函数为 setup_per_cpu_areas()，被定义在 mm/percpu.c 中，这个函数在 start_kernel 中被直接调用。下面是该函数的实现。    

```
void __init setup_per_cpu_areas(void)
{
	unsigned long delta;
	unsigned int cpu;
	int rc;
    //申请并初始化内存块
	rc = pcpu_embed_first_chunk(PERCPU_MODULE_RESERVE,
				    PERCPU_DYNAMIC_RESERVE, PAGE_SIZE, NULL,
				    pcpu_dfl_fc_alloc, pcpu_dfl_fc_free);

	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;

    //将每个 cpu 相对于源内存地址的偏移量。
	for_each_possible_cpu(cpu)
		__per_cpu_offset[cpu] = delta + pcpu_unit_offsets[cpu];
}
```

该函数主要有两个部分:



* 为每个 cpu 分配对应的 percpu 内存并进行初始化。
* 保存每个 cpu percpu 内存相对于源 percpu 内存的偏移量，当使用 get_cpu_var(name) 获取变量地址时，首先找到的是源变量，然后通过地址偏移找到对应 cpu 的 percpu 变量。  


接下来我们简要的分析 pcpu_embed_first_chunk 的实现：

```C
int __init pcpu_embed_first_chunk(size_t reserved_size, size_t dyn_size,
				  size_t atom_size,
				  pcpu_fc_cpu_distance_fn_t cpu_distance_fn,
				  pcpu_fc_alloc_fn_t alloc_fn,
				  pcpu_fc_free_fn_t free_fn)
{
	void *base = (void *)ULONG_MAX;
	void **areas = NULL;
	struct pcpu_alloc_info *ai;
	size_t size_sum, areas_size;
	unsigned long max_distance;
	int group, i, highest_group, rc;

    //申请一个 struct pcpu_alloc_info 结构成员，该结构成员负责描述系统内 percpu 内存。
	ai = pcpu_build_alloc_info(reserved_size, dyn_size, atom_size,
				   cpu_distance_fn);

    //计算 percpu 内存总大小
	size_sum = ai->static_size + ai->reserved_size + ai->dyn_size;
	areas_size = PFN_ALIGN(ai->nr_groups * sizeof(void *));

	areas = memblock_virt_alloc_nopanic(areas_size, 0);

	highest_group = 0;
	for (group = 0; group < ai->nr_groups; group++) {
		struct pcpu_group_info *gi = &ai->groups[group];
		unsigned int cpu = NR_CPUS;
		void *ptr;

		for (i = 0; i < gi->nr_units && cpu == NR_CPUS; i++)
			cpu = gi->cpu_map[i];
		BUG_ON(cpu == NR_CPUS);
        
        //申请内存地址
		ptr = alloc_fn(cpu, gi->nr_units * ai->unit_size, atom_size);
		
		kmemleak_free(ptr);
		areas[group] = ptr;

        //获取 percpu 内存基地址，赋值给 base
		base = min(ptr, base);
		if (ptr > areas[highest_group])
			highest_group = group;
	}
	...
    // cpu 分组
	for (group = 0; group < ai->nr_groups; group++) {
		struct pcpu_group_info *gi = &ai->groups[group];
		void *ptr = areas[group];

		for (i = 0; i < gi->nr_units; i++， ptr += ai->unit_size) {
			if (gi->cpu_map[i] == NR_CPUS) {
				/* unused unit, free whole */
				free_fn(ptr, ai->unit_size);
				continue;
			}
			memcpy(ptr, __per_cpu_load, ai->static_size);
			free_fn(ptr + size_sum, ai->unit_size - size_sum);
		}
	}

	for (group = 0; group < ai->nr_groups; group++) {
		ai->groups[group].base_offset = areas[group] - base;
	}

    //初始化分配的内存空间
	rc = pcpu_setup_first_chunk(ai, base);
    ...
	return rc;
}
```

对于 pcpu_embed_first_chunk 的实现，从函数名可以看出，这是内核创建并嵌入的第一个 chunk(内存块)，主要是以下几个部分：



1：传入的参数有：保留空间的 size，动态申请的 size，以及申请和释放内存空间的回调函数，这个时候的 buddy、slab内存分配系统还没有初始化完成，所有的申请空间都是在 bootmem 上完成。

2：申请并填充 struct pcpu_alloc_info 结构成员，该结构用于 percpu 内存的描述，主要的成员如下：

```
struct pcpu_alloc_info {
    size_t			static_size;  //静态 percpu 变量的 size，等于 __per_cpu_end - __per_cpu_start，这两个变量的定义可以参考上文中的链接部分。
    size_t			reserved_size;  //保留内存区
    size_t			dyn_size;       //动态申请内存区
    size_t			unit_size;      //每个 cpu 所占的 percpu 内存是一个 unit，该成员表示 unit 的大小。
    size_t			alloc_size;     //申请内存的size
    int			    nr_groups;	    //cpu 分组个数
    struct pcpu_group_info	groups[];  //每组 cpu 对应的 percpu 内存描述信息
};
```
这一部分相当于全局记录信息，那么，真正对应的内存是如何管理的呢？内核中使用 struct pcpu_chunk 来描述每一片 percpu 内存：

```C
struct pcpu_chunk {

    struct list_head	list;		//链表节点，当前 chunk 被连接到 pcpu_slot 链表中
    int			free_bytes;	        //当前 chunk 空闲的空间
    
    void			*base_addr;	    //当前 chunk 的起始地址

    void			*data;		//chunk 数据
    ...
};
```
在 pcpu_build_alloc_info 接口中，申请了一个 struct pcpu_alloc_info 的 ai 实例结构，同时计算并记录了各种类型的 size(静态、动态、保留、unit等) 到结构体 ai 中，以备后续的内存申请操作，同时管理 cpu 的分组信息，在 SMP 架构中，每个 cpu 并非是完全平等且独立的，对于某些系统，进一步将多个 cpu 进行分组，划分出不同的域，这在进程调度、安全性方面都有一定的提升。

3：调用内存申请函数为整个 percpu 申请一大片内存。

4：调用 pcpu_setup_first_chunk(ai,base) 函数进一步细化地对整片内存进行划分，传入的两个参数分别为 struct pcpu_alloc_info 类型和申请内存的基地址，在该函数中，还将 base 赋值给 全局变量 pcpu_base_addr。在主函数 setup_per_cpu_areas 中，为每个 cpu 确定 percpu 内存偏移地址(__per_cpu_offset)时就是根据 percpu 变量的加载地址和 percpu 内存起始地址计算的，而每个 cpu 之间的偏移则是由 unit_size 指定。

举个例子：静态 percpu 内存加载地址为 0x1000，申请的 percpu 内存基地址为 0x8000，unit_size 为 0x2000，所以 cpu0 的 percpu 内存偏移为 (0x8000-0x1000)+0\*0x2000 = 0x7000 ,cpu1 的 percpu 内存偏移为 (0x8000-0x1000)+1*0x2000 = 0x9000,以此类推。  

当访问一个静态 percpu 变量时，它的地址可能位于 0x1004, cpu0 对应的该变量副本地址就是 0x1004+0x7000=0x8004,cpu1 对应的该变量副本地址就是 0x1004+0x7000+0x2000=0xA004。    

当然，这只是个示例，实际情况下的地址分配并没有这么随心所欲。  




## get_cpu_var 的实现
get_cpu_var 的实现原理实际上在上文的示例中已经讲到了，这里我们来看看它的源码实现：


```C
#define get_cpu_var(var)				    \
(*({						               	\
	preempt_disable();						\
	this_cpu_ptr(&var);						\
}))
```

首先，preempt_disable 禁用内核抢占，然后使用 this_cpu_ptr 接口获取当前 cpu 上对应的 var 变量地址。  


```C
#define this_cpu_ptr(ptr) raw_cpu_ptr(ptr)

#define raw_cpu_ptr(ptr)						\
({									            \
	__verify_pcpu_ptr(ptr);						\
	arch_raw_cpu_ptr(ptr);						\
})

```
对该 ptr 作指针检查，确定该指针是 percpu 类型指针，然后调用 arch_raw_cpu_ptr：


```C
#define arch_raw_cpu_ptr(ptr) SHIFT_PERCPU_PTR(ptr, __my_cpu_offset)
```

来到关键的部分：arch_raw_cpu_ptr 宏依赖于 SHIFT_PERCPU_PTR(ptr, \_\_my_cpu_offset)，我们先来看看 __my_cpu_offset 的定义：

```
#define __my_cpu_offset per_cpu_offset(raw_smp_processor_id())
#define per_cpu_offset(x) (__per_cpu_offset[x])
```
可以看到，raw_smp_processor_id() 的值是当前 cpu 的 id 号，当在 CPU0 上执行这段代码时，\_\_my_cpu_offset 相当于 \_\_per_cpu_offset[0]，对于 \_\_per_cpu_offset，不知道你还记不记得，这是在 percpu 内存初始化时确定的每个cpu 中 percpu 变量内存的偏移值(在当前页面搜索关键词即可看到)。  

此时 arch_raw_cpu_ptr 可以被简化为 ：SHIFT_PERCPU_PTR(ptr,__per_cpu_offset[n]),n 对应 cpu id，我们接着来看 SHIFT_PERCPU_PTR 的实现：

```
#define SHIFT_PERCPU_PTR(__p, __offset)					\
	RELOC_HIDE((typeof(*(__p)) __kernel __force *)(__p)， (__offset)) 

# define RELOC_HIDE(ptr, off)					\
  ({ unsigned long __ptr;					\
     __ptr = (unsigned long) (ptr);				\
    (typeof(ptr)) (__ptr + (off)); })
```

可以看到，SHIFT_PERCPU_PTR 最后返回的结果就是 ptr + offset，ptr 是传入的源变量名的地址，offset 就是每个 cpu 相对于源变量的偏移地址，所以，最后简化下来，get_cpu_var 的实现就是：

```
#define get_cpu_var(var)				    \
(*({						               	\
	preempt_disable();						\
	&var + __per_cpu_offset[raw_smp_processor_id()]  \
}))
```

结合括号前的 "\*" 号，即取到了当前 cpu 上的 percpu 变量值。  

使用完变量之后记得调用 put_cpu_var 以使能内核抢占功能，恢复系统状态。  



## percpu 的误区
前文中提到，percpu 是伴随着 SMP 而生的，它是为了解决特殊条件下(全局数据逻辑独立)多 cpu 并发访问全局数据的问题，但是并不代表 percpu 变量不存在同步问题，在这一章中我们讨论了 percpu 机制的实现原理，如果你真正地理解了它，你会发现 percpu 机制的作用类似于将 SMP 架构下的全局数据转换成了单 cpu 下的全局数据(percpu 数据和对应的 cpu 绑定，不对外暴露)。   

使用 percpu 机制，并不意味着 percpu 全局数据访问就是安全的，percpu 机制只是将全局数据从多核环境下转换到了单核环境下，在单核环境下是如何处理数据保护的，针对 percpu 变量照样需要，比如使用关中断来防止中断与进程之间的竞争。

尽管从理论上来说，percpu 变量是绑定到 cpu 的，但是并不代表 CPU 不能跨 CPU 访问 percpu 数据，也就是 cpu0 也可以访问 cpu1 的副本，内核是完全允许的，而且在某些情况下是必须的。



### 参考

https://blog.csdn.net/wh8_2011/article/details/53138377

http://www.wowotech.net/kernel_synchronization/per-cpu.html

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。



