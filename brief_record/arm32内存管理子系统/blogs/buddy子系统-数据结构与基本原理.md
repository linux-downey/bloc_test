# buddy 子系统--数据结构与基本原理

## buddy 子系统数据结构

按照惯例，讲解一个模块之前自然需要先分析它对应的数据结构，数据结构中基本上包含了该模块的基本逻辑，对于 buddy 子系统也是一样，对于非 numa 架构的系统，全局变量 struct pglist_data contig_page_data 记录了整个 buddy 子系统相关的信息，访问该全局变量的接口为 NODE_DATA 宏：

```c++
#define NODE_DATA(nid)		(&contig_page_data)
```

其中 nid 表示 numa node 节点的 id，上面是非 numa 架构中的实现，该接口直接指向全局变量 contig_page_data 的地址。

contig_page_data 中包含的数据成员为：

```c++
typedef struct pglist_data {
	struct zone node_zones[MAX_NR_ZONES];
	struct zonelist node_zonelists[MAX_ZONELISTS];
	int nr_zones;
	struct page *node_mem_map;

	unsigned long node_start_pfn;
	unsigned long node_present_pages; 
	unsigned long node_spanned_pages; 
					     
	int node_id;

	unsigned long		totalreserve_pages;

	ZONE_PADDING(_pad1_)
	spinlock_t		lru_lock;
	...
} pg_data_t;
```

因为本系列文章主要针对 buddy 子系统内存初始化以及分配回收的实现，不涉及内存碎片、内存交换、内存压缩的处理，同时考虑到当前系统的硬件特性(比如不支持地址扩展、numa 架构)，因此省去了一些不重要以及不相关的数据成员,只关注一些核心成员。 

* node_zones：buddy 子系统中内存管理是以 zone 为单位的，也就是内存的分配在具体的 zone 内进行，关于 zone 的概念可以参考TODO，对于不同的 zone 有不同的特性，其内存管理的特性也有一些区别，每个 zone 对应一个数组成员，内核中支持多少个 zone 取决于硬件，硬件特性决定了软件配置，imx6ull 中 MAX_NR_ZONES 为 3，表示支持 3 个 zone，分别为 ZONE_NORMAL、ZONE_HIGHMEM 和 ZONE_MOVABLE。
  实际上，struct zone 结构才是 buddy 系统内存管理的体现，下文中将详细介绍。
  
* node_zonelists：当当前的 zone 无法满足内存分配的需求时，zonelists 用于指定备用 node zone 列表，这里的 node 表示 numa node，在非 numa 架构中，不使用 zonelists。

* nr_zones：node 中 zone 的数量，原本我以为这个值就是等于 MAX_NR_ZONES，但是本着严谨的态度看了一眼源代码，发现不是这么回事，nr_zones 这个成员针对的是实际管理了物理内存的 zone 的数量，也就是说，尽管有些 ZONE 被配置到内核中，但是实际上并没有真正地管理内存，就不算在 nr_zones 中。
  比如我手头上的这个 imx6ull 开发板，ZONE_MOVABLE 区域没有管理物理内存，尽管 MAX_ZONELISTS 为 3，但是 nr_zones 为 2，仔细一想也是，如果 nr_zones 恒等于 MAX_NR_ZONES，也就没有使用这个变量的必要了，直接使用宏就好，我猜测使用 nr_zones 记录真实使用的 zone 主要是减少在操作中没必要的遍历。 

* node_mem_map：struct page 数据基地址，每个页面都由一个 struct page 结构来管理，对于 FLATMEM 类型的内存模型，所有页面对应的 struct page 在启动阶段被统一申请，放在连续的物理内存地址上，node_mem_map 指向这片内存的基地址。

* node_start_pfn：该 numa node 中起始页面的页帧号，这里的 node 并非一定是 numa 架构所属，内核中将非 numa 架构抽象为一个 numa node 的 numa 系统，这样可以统一接口。

* node_present_pages：该 node 中已经给出的页面数，也就是实际页面数。

* node_spanned_pages：从当前 zone 的物理内存开始到物理内存结束地址对应的 page 数，包括内存空洞的，没有内存空洞时，等于 node_present_pages，否则大于 node_present_pages。

* node_id：当前 numa 节点的 id，非 numa 结构中该 id 为 0

* totalreserve_pages：所有 zone->lowmem_reserve 的统计值，指保留内存(待研究)。

* ZONE_PADDING：严格来说这不算是一个结构成员，它只是一个针对 cacheline 的填充，系统的 cache 通常是以 cache line 为单位加载内存的，一个 cache line 可能是 8 字节或者 16 字节不等，在多核系统中，多个 CPU 同时加载同一个 cache line 到各自缓存中，这并不是一件好事，尤其是当两个 CPU 同时修改同一个 cache line 中的数据时，将会 invalidate 另一个 CPU 上的 cache，导致另一个 CPU 上的数据需要完全地从内存中加载，这种 cache 一致性问题是比较麻烦的。
  回到正题，这里为什么需要使用 ZONE_PADDING 呢，ZONE_PADDING 本意是填充当前的 cache line，该指令后面的数据将会放到另一个 cache line上，有些类似于结构体中为了对齐而填充的空字节，这种操作的原因在于将  lru_lock 成员放到另一个 cache line 上，因为它经常会被操作到，而前面的某些数据也经常被 CPU 操作到，这两者又并不通常会被一起操作，将它们分到不同的 cache line，这样在不同的 CPU 分别访问这两个数据成员时就不会造成 cache 数据的竞争了。

  

### struct  zone

实际的内存管理是在每个 zone 内部进行的，因此继续深入到 struct zone 结构中：

```c++
struct zone {
	/* zone watermarks, access with *_wmark_pages(zone) macros */
	unsigned long watermark[NR_WMARK];

	long lowmem_reserve[MAX_NR_ZONES];

	struct pglist_data	*zone_pgdat;
	struct per_cpu_pageset __percpu *pageset;

#ifndef CONFIG_SPARSEMEM
	/*
	 * Flags for a pageblock_nr_pages block. See pageblock-flags.h.
	 * In SPARSEMEM, this map is stored in struct mem_section
	 */
	unsigned long		*pageblock_flags;
#endif /* CONFIG_SPARSEMEM */

	/* zone_start_pfn == zone_start_paddr >> PAGE_SHIFT */
	unsigned long		zone_start_pfn;

	/*
	 * spanned_pages is the total pages spanned by the zone, including
	 * holes, which is calculated as:
	 * 	spanned_pages = zone_end_pfn - zone_start_pfn;
	 *
	 * present_pages is physical pages existing within the zone, which
	 * is calculated as:
	 *	present_pages = spanned_pages - absent_pages(pages in holes);
	 *
	 * managed_pages is present pages managed by the buddy system, which
	 * is calculated as (reserved_pages includes pages allocated by the
	 * bootmem allocator):
	 *	managed_pages = present_pages - reserved_pages;
	 *
	 * So present_pages may be used by memory hotplug or memory power
	 * management logic to figure out unmanaged pages by checking
	 * (present_pages - managed_pages). And managed_pages should be used
	 * by page allocator and vm scanner to calculate all kinds of watermarks
	 * and thresholds.
	 *
	 * Locking rules:
	 *
	 * zone_start_pfn and spanned_pages are protected by span_seqlock.
	 * It is a seqlock because it has to be read outside of zone->lock,
	 * and it is done in the main allocator path.  But, it is written
	 * quite infrequently.
	 *
	 * The span_seq lock is declared along with zone->lock because it is
	 * frequently read in proximity to zone->lock.  It's good to
	 * give them a chance of being in the same cacheline.
	 *
	 * Write access to present_pages at runtime should be protected by
	 * mem_hotplug_begin/end(). Any reader who can't tolerant drift of
	 * present_pages should get_online_mems() to get a stable value.
	 *
	 * Read access to managed_pages should be safe because it's unsigned
	 * long. Write access to zone->managed_pages and totalram_pages are
	 * protected by managed_page_count_lock at runtime. Idealy only
	 * adjust_managed_page_count() should be used instead of directly
	 * touching zone->managed_pages and totalram_pages.
	 */
	unsigned long		managed_pages;
	unsigned long		spanned_pages;
	unsigned long		present_pages;

	const char		*name;

	int initialized;

	/* Write-intensive fields used from the page allocator */
	ZONE_PADDING(_pad1_)

	/* free areas of different sizes */
	struct free_area	free_area[MAX_ORDER];

	/* zone flags, see below */
	unsigned long		flags;

	/* Primarily protects free_area */
	spinlock_t		lock;

	/* Write-intensive fields used by compaction and vmstats. */
	ZONE_PADDING(_pad2_)


} ____cacheline_internodealigned_in_smp;
```

* watermark：水位值，这是一个数组，数组中包含三个成员：WMARK_MIN、WMARK_LOW、WMARK_HIHG，水位表示的是当前 zone 中剩余物理内存的占比，剩余物理内存占比高时，水位值就高，不难看出这个值就是为了记录当前 zone 中的剩余内存是否到达某个临界点，以便执行相应的操作。
  当水位值为 high 时，内存申请可以毫无压力地进行，当水位低到 LOW 时，这时候就要小心了，WMARK_LOW 相当于一个警戒水位线，某些内存申请操作可能不支持，如果水位到达了 WMARK_MIN，这时候说明物理内存的确是吃紧了，将会触发内存回收或者内存交换等机制。以避免触发 OOM(out of memory)。
* lowmem_reserve：在内存分配过程中，如果没有通过 gfp 标志位指定内存分配标志，当高一级的 zone 内存不足时，允许到低一级的 zone 中分配内存，比如 ZONE_HIGHMEM 可以从 ZONE_NORMAL 中分配内存，当然，这种策略是不得已而为之的行为，为了防止高一级的 zone 过渡挤压自己的内存，在 water mark 之外，低一级的 zone 会保留一些内存页面，放到 lowmem reserve 中。
* zone_pgdat：指向 contig_page_data  全局数据结构的指针
* pageset：这是 percpu 类型的结构，以链表的形式记录了一些物理单页，原本 buddy 子系统只使用 free_area 来组织并记录 zone 中所有的页面，pageset 是一种优化措施，一方面，内核中对应单页的申请比多页的申请要更频繁，因此该结构相当于对物理单页的缓存，减少单页的分配时间。另一方面，将这些单页与特定的 CPU 相关联，比如程序中频繁地申请释放单页时，可以更好地利用 CPU 缓存。而 percpu 机制可以比较好地解决多核之间的数据同步问题，有效利用 CPU 缓存。
  





water mark 参考：https://zhuanlan.zhihu.com/p/73539328

lowmem_reserve 参考：https://zhuanlan.zhihu.com/p/81961211