# linux内存子系统 - memblock 内存分配器

在 linux 内核的启动早期，并没有直接使用 buddy 分配器对物理内存进行管理，而是使用了 memblock 分配器对物理内存进行管理。

其原因在于：无论是哪种内存分配器，其本身也是需要占用内存的，如果该内存分配器本身占用的内存需要动态分配，那么这就是一个悖论，因为内存分配器初始化之前无法动态分配内存。因此，内核中的第一个内存分配器只能是静态定义的。 

对于 buddy 而言，由于它的管理数据存在一定的复杂性，比如需要 percpu 内存的支持，比如node_mem_map 用来保存所有物理 page 的 struct page 结构，这些需要在初始化过程中动态生成，所以 buddy 不能作为启动中使用的内存分配器。 

在内核中，buddy 系统正式启动之后的核心内存分配器，因此，内核的策略为：在内核的初始化阶段，先定义一个简单的内存分配器，该分配器本身所占用的内存要足够小，因为只是初始化阶段使用，自然不需要太复杂，毕竟在初始化之后，就直接过渡到了 buddy 分配器。

内核早期使用的是 bootmem 内存分配器，由于 bootmem 在使用中比如位图的管理等一些问题，较新版本的内核使用 memblock 作为内核早期的分配器。 



## 数据结构

分析驱动的经验告诉我们，如果一个模块弄懂了其相关的数据结构，那么这个模块就已经弄懂了一半，数据结构中的成员也是代码逻辑的体现。 

在 memblock 中，数据结构为：

```c++
struct memblock {
	bool bottom_up;       
	phys_addr_t current_limit;
	struct memblock_type memory;
	struct memblock_type reserved;
};
```

bottom_up：执行内存分配时，该值为 0 时表示从物理内存的高地址向低地址进行内存分配，这两种分配方式相差不大，我所接触的大多 arm 系统都是选择从高地址向低地址分配，我的猜测是：内核镜像和 dtb 在低地址处，如果从低地址开始，内存分配将带来额外的复杂性，同时更可能让内存碎片化。 

current_limit：memblock 管理内存的限制地址，这个值通常是低端内存和高端内存的临界点，memblock 的分配只会限制在起始物理内存与该 limit 之间。

memory：memblock 的核心管理数据结构，该结构用来描述系统提供的物理内存，基地址、大小以及数量。

reserved：同样是核心管理数据结构，该结构用来保存设备树中设定保留的物理内存，同时保存 memblock 已经分配出去的内存。



```c++
struct memblock_type {
	unsigned long cnt;	
	unsigned long max;	
	phys_addr_t total_size;	
	struct memblock_region *regions;
};
```

cnt：当前 memory type 中 regions 的数量

max：当前 memory type 中最大的 regions 数量，如果 region 超过这个数量，将会以翻倍的方式对其进行内存扩展，这个时候已经可以简单地实现内存分配。

total_size：当前 memory type 中的内存总量

regions：这是一个结构体数组，内核默认静态定义了 INIT_MEMBLOCK_REGIONS(默认128) 个结构体成员，因此可以描述 128 片区域。



```c++
struct memblock_region {
	phys_addr_t base;
	phys_addr_t size;
	unsigned long flags;
};
```

memblock_region 结构非常容易理解，既然是描述一片内存，主要是两个属性，一个是内存起始地址 base，另一个是内存大小 size。

flag 是该内存的标志位，主要是记录了一些属性,下面是相关标志位的值：

```c++
enum {
	MEMBLOCK_NONE		= 0x0,	// 没有特殊要求
	MEMBLOCK_HOTPLUG	= 0x1,	// 支持热插拔的区域
	MEMBLOCK_MIRROR		= 0x2,	// 镜像区域
	MEMBLOCK_NOMAP		= 0x4,	// 表示这部分内存不要添加到内核直接映射区
};
```



从数据结构不难发现，memblock 分配器是非常简单的，以 regions 为单位来管理一片内存，该内存分配器占用的静态内存仅有 3K，内核初始化前期本身也不存在频繁地内存释放(有不少的内存申请需求)，在这种前提下，memblock 完全是够用的。



## memblock 接口

### memblock 内存申请

memblock 申请内存的接口为：

```c++
phys_addr_t __init memblock_alloc(phys_addr_t size, phys_addr_t align)
```

memblock_alloc 用来申请一片 size 大小的物理内存块，同时指定内存的对齐参数 align。

无论是 NUMA 架构还是非 NUMA 架构，都可以直接使用 memblock_alloc，接口上是兼容的，在 NUMA 系统中，默认情况下软件不需要指定 CPU 和 NUMA 节点的关联，这个关系可以通过其它硬件的方式来确定的，因此对于 NUMA 系统，内存申请也只需要提供 size 和 align 两个参数。

memblock_alloc 的实现逻辑很简单，memblock.memory 总是表示系统提供的整块物理内存(可能多个)，而 memblock.reserved 中保存的是已经被分配的，从高地址到低地址反向遍历，尝试从 memblock.memory(除去 memblock.reserved 占用的内存) 中找到一片满足 size 以及 align 要求的内存。 

获取到物理内存之后，再将这片内存以 regions 表示，添加到 memblock.reserved 中，然后返回。

因此可以看出，对于 memblock 而言，memblock.memory 只用来描述系统提供的物理内存，不参与分配释放的管理工作，而 memblock.reserved 负责记录已经分配的内存区域。 

从返回参数类型 phys_addr_t 可以看出，memblock 实际上是针对物理内存的分配管理器，直接返回分配内存的物理地址，返回的这片物理内存当前是不能直接使用的，需要软件上建立页表之后才能通过其对应的虚拟地址进行访问。



### memblock 释放接口

memblock 释放内存的接口相对简单：

```c++
int __init_memblock memblock_free(phys_addr_t base, phys_addr_t size)
```

释放时给定需要释放的物理内存起始地址，以及对应的 size，该接口会将这片物理内存对应的 regions 从 memblock.reserved 中删除。



### memblock 添加/删除内存：

管理内存的前提是知道系统中有多少内存，再基于系统提供的物理内存上进行内存的分配与释放，在内核初始化时通过扫描 dtb 文件获取物理内存信息，然后将其添加到 memblock 中，对应的接口为：

```c++
int __init_memblock memblock_add(phys_addr_t base, phys_addr_t size)
```

与之相对的，移除物理内存的接口为：

```c++
int __init_memblock memblock_remove(phys_addr_t base, phys_addr_t size)
```

这两个接口操作的是 memblock.memory 区域，系统提供多少段物理内存，也就存在多少个 region，memblock 在这基础上实现内存的申请与释放。



### 其它接口

当 memblock 中存在两片连续且属性相同(flags相等)的内存块时，可以使用下面的接口将这两者合并：

```c++
static void __init_memblock memblock_merge_regions(struct memblock_type *type)
```

实际上这个接口会在多种情况下调用，该接口将会轮询 memblock_type 中的每一个 regions，查看是否可以合并其中的项。

这种合并是有意义的，以一大块内存提供给 buddy 系统的方式比提供两块连续小内存到 buddy 系统会让 buddy 更乐于接受，内存管理从来都不喜欢碎片化的东西。  



memblock 内存分配器是静态定义的，系统定义了 memblock.memory 和 memblock.reserved 两个 memtype，默认每个管理 128 块内存，用 memory region 描述，这 128 块对于 memblock.memory 大概率是够用了，但是对于 memblock.reserved 却不一定，每执行一次内存分配，memblock.reserved 都会使用掉一个 regions，因此，当 regions 用完的时候，就需要进行扩展，扩展的方式也很简单，直接以加倍的方式进行，对应的接口为：

```c++
static int __init_memblock memblock_double_array(struct memblock_type *type,
						phys_addr_t new_area_start,
						phys_addr_t new_area_size)
```

通常对应的调用情形是这样的：

```c++
while (type->cnt + nr_new > type->max)
			if (memblock_double_array(type, obase, size) < 0)
```

扩展的管理数据自然也是动态分配的。

在 memblock 中，只有 memtype.regions 可以扩展，而 memtype 是不支持扩展的，暂时也没这个必要。



## memblock 到 buddy 系统

尽管 memblock 劳苦功高，但是始终只是昙花一现，毕竟 memblock 对于内存的管理太过于简单粗暴，不能用作 linux 内核的主内存管理器，因此还是需要过渡到 buddy 系统。 

从 memblock 到 buddy 说起来也简单，就是将 memblock 管理的内存信息全部提交给 buddy 系统，协助 buddy 系统建立起对整个系统中物理内存的管理框架，memblock 也就功成身退了。

当然，这里面会涉及到一些管理细节，将会在 buddy 系统的初始化中继续讨论。 



### 参考

https://biscuitos.github.io/blog/MMU-ARM32-MEMBLOCK-memblock_information/

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

