# buddy子系统-分配与释放

前面的章节中分析了 buddy 子系统是如何从 memblock 内存分配器过渡而来，同时分析了 buddy 子系统的相关数据结构与基本原理，在这一章中，继续分析 buddy 子系统靠近最靠近用户的一部分：页面的分配与释放。 

尽管前面的文章让我们大概地了解了 buddy 子系统中物理页面的分配与释放是如何工作的，但是实际的实现要更复杂，除了常规情况下的内存申请与分配之外，需要更多地考虑另一个问题：执行内存分配时内存不够怎么办，这涉及到 buddy 跨 zone 甚至跨 node 的分配，以及 zone 中的 watermark 是如何控制页面的分配行为的。

只有弄清楚这些问题之后，才知道在执行页面分配时 buddy 子系统到底在做什么。



## alloc_pages

alloc_pages 用于直接向 buddy 子系统申请完整的页面，该接口接受两个参数，gfp_mask 和 order，一个用于指定分配时的 flag，从而决定分配策略，另一个用于指定页面数量。 

实际上 alloc_pages 是一个宏定义，对应非 NUMA 架构平台，对应的函数为 alloc_pages_node，而 NUMA 架构对应的实现为 alloc_pages_current，取决于 CONFIG_NUMA 内核配置项。

不管上层怎么封装，实际底层的物理页分配对应的接口为 __alloc_pages_nodemask：

```c++
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
			struct zonelist *zonelist, nodemask_t *nodemask);
```

\_\_alloc_pages_nodemask 接受 4 个参数，分别为 mask、order、zonelist、nodemask，zonelist 中保存的是在执行内存分配时扫描的内存节点，其结构体定义如下：

```c++
struct zonelist {
	struct zoneref _zonerefs[MAX_ZONES_PER_ZONELIST + 1];
};
struct zoneref {
	struct zone *zone;	/* Pointer to actual zone */
	int zone_idx;		/* zone_idx(zoneref->zone) */
};
```

每个 zonelist 中包含多个 zoneref，一个 zoneref 用于描述一个 zone，比如 ZONE_NORMAL、ZONE_HIGHMEM，因此，一个 zonelist 中包含了整个内存节点中的所有 zone。**同时需要注意的是，_zonerefs 数组中所保存的 zone 是反序的，比如 _zonerefs[0] 描述的是 ZONE_HIGHMEM、 _zonerefs[1] 描述的是 ZONE_NORMAL，这种顺序在跨 zone 分配时更方便遍历，这部分将在下文中详谈。**

在非 NUMA 架构中，内存节点只有一个，因此 zonelist 中仅有一个成员。



接着看该函数的实现：

```c++
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
			struct zonelist *zonelist, nodemask_t *nodemask)
{
    struct alloc_context ac = {
		.high_zoneidx = gfp_zone(gfp_mask),
		.zonelist = zonelist,
		.nodemask = nodemask,
		.migratetype = gfpflags_to_migratetype(gfp_mask),
	};                                                    .....................1
    
    if (IS_ENABLED(CONFIG_CMA) && ac.migratetype == MIGRATE_MOVABLE)
		alloc_flags |= ALLOC_CMA;                         
    
    ac.spread_dirty_pages = (gfp_mask & __GFP_WRITE);    .....................2
    
    ac.preferred_zoneref = first_zones_zonelist(ac.zonelist,
					ac.high_zoneidx, ac.nodemask);        .....................3
    
    page = get_page_from_freelist(alloc_mask, order, alloc_flags, &ac); 
    if (likely(page))
		goto out;
    page = __alloc_pages_slowpath(alloc_mask, order, &ac);              .......4
    ...
}
```

1. 构建一个内存分配的上下文结构，其实就是把内存分配需要的那些信息打个包，方便传来传去。high_zoneidx 成员表示本次页面分配需要扫描的最上层的 zoneidx，默认是从 ZONE_NORMAL，当指定 ZONE_HIGHMEM 时，也就返回该 zone 对应的 index，用来确定即将扫描的 zone 区域的界限。

   nodemask 成员值为 0，只在 NUMA 架构中用到。
   migratetype 表示页面的类型，包括 MOVABLE/UNMOVABLE/RECLAIMABLE 等，不同的迁移类型对应 free_area 中不同的数组项，同时不同的迁移类型还会导致不同的分配行为。 

2. 如果用户指定的迁移类型中带有 MIGRATE_MOVABLE 标志，就可以使用系统中的 CMA 区域来分配内存，毕竟 CMA 区域必须只能被分配为移动页面使用。
   如果 gfp_mask 中包含 \_\_GFP_WRITE，说明该页面会被修改，buddy 对 zone 中的脏页面有所限制，因此需要统计页面的读写需求。
   实际上类似于 \_\_GFP_WRITE 这类标志位并不需要申请者直接指定，而是由内核提供一系列的标志位组合，比如常用的 GFP_KERNEL 就是 (\_\_GFP_RECLAIM | \_\_GFP_IO | \_\_GFP_FS) 这几者的组合 ，通常带双下划线的都不是被用户直接使用的申请标志。

3. preferred_zoneref 表示第一个将要被遍历的 zone，通过执行 first_zones_zonelist 函数返回该 zoneref 对象，zone 的选取主要是根据传入 ac.high_zoneidx 对象，从 zonelist->_zonerefs 数组中，找到可用的最接近或等于 ac.high_zoneidx 的那个 zone，对应的源码为：

   ```c++
   static inline struct zoneref *first_zones_zonelist(struct zonelist *zonelist,
   					enum zone_type highest_zoneidx,
   					nodemask_t *nodes)
   {
   	return next_zones_zonelist(zonelist->_zonerefs,
   							highest_zoneidx, nodes);
   }
   static __always_inline struct zoneref *next_zones_zonelist(struct zoneref *z,
   					enum zone_type highest_zoneidx,
   					nodemask_t *nodes)
   {
   	if (likely(!nodes && zonelist_zone_idx(z) <= highest_zoneidx))
   		return z;
   	return __next_zones_zonelist(z, highest_zoneidx, nodes);
   }
   ```

   在看这部分代码时，百思不得其解，为何是在 zonelist->\_zonerefs 数组中从下(0)往上早，找到小于等于 highest_zoneidx 就返回，后面将 zonelist->\_zonerefs 打印出来才发现，数组中 zone 的排列是反序的，也就是 ZONE_HIGHMEM 为下标 0，依次再是 ZONE_NORMAL 等。

   因此，在物理页分配时，第一个扫描的是最高 level 的 zone，再依次往下，而不能反向往更高 level 延伸。

   这也很好理解，假设 ZONE_HIGHMEM 部分的物理内存不够，可以从 ZONE_NORMAL 申请，ZONE_NORMAL 区的物理内存完全可以满足分配要求，反之则不成立，如果用户指定在 ZONE_NORMAL 申请物理页面，用户通常期望其物理地址是连续的、同时处于线性映射区，但是 ZONE_HIGHMEM 区的物理内存并不满足该要求，因此只能向更低 level 的 ZONE_DMA 延伸(假设存在 ZONE_DMA zone)。 

4. 物理页面申请的主要实现函数为 get_page_from_freelist，传入 4 个参数：alloc_mask、order、alloc_flags 和 ac，ac 就是当前函数中构建的内存分配上下文结构。该函数返回申请到的 page 页面，但是该函数并不保证内存分配的成功。
   get_page_from_freelist 分配内存失败基本上也就意味着整个节点上的内存已经吃紧，或者是分配要求的物理内存太大确实无法满足要求，毕竟在 get_page_from_freelist 中也会尝试执行内存的交换和回收来满足分配需求，即使这样还是分配失败的情况下，会进入到分配内存的 slowpath，在 slowpath 中，会执行一系列的内存操作来整理内存碎片和回收页面，包括 memory compaction、物理页面的 direct reclaim、内存交换等，这一部分比在本章节中暂不讨论。



## get_page_from_freelist

前面的部分基本上是针对物理页面的分配做一些前期的准备工作，而 get_page_from_freelist 才是内存分配真正的开始。

在此之前，需要了解内存分配的一个重要概念：watermark，也就是内存的水位线。

内核对于用户的内存请求，并不是一视同仁的，体现在申请内存时使用的 gfp_mask 上，不同的标志位将导致不同的内存分配策略，比如：

* 有些物理页面被应用于只读、有些应用于读写、而有些应用于执行，这些应用属性的不同导致页面属性的差异，比如页面是否可移动、是否可回收，这涉及到后续的内存碎片整理。
* 有些场景下要求物理地址连续，而有些场景下只要求虚拟地址连续，就会导致从不同的 zone 申请物理页面，同时低 level zone 中物理页面同样可以满足高 level zone 对物理页面的要求。

* CMA：CMA 实际上是为保留内存重复利用而实现的机制，尽管 CMA 区域支持 buddy 对页面的重复利用，但是其基本需求还是需要被保证：在特定 device 需要用到 CMA 区域的时候该区域要被清空，留给 device 使用，如果这一点保证不了，也就不叫保留内存了，因此 CMA 区域中分配出去的内存迁移类型必须是 MOVABLE。
* 内核中大部分内存是可以被迁移、整理以及回收的，这也就涉及到一个内存管理器设计问题：什么时候来执行这些操作？以什么方式来执行这些操作？
* GFP_ATOMIC：正常情况下，内存申请是可能导致睡眠的，当 zone 中的内存量低到一定的值时，需要睡眠等待内存回收才能继续执行。但是在中断或者不能睡眠的场景下，是不能睡眠的，这就是 GFP_ATOMIC 所代表的含义：内存分配阶段不能睡眠。这也就意味着，当内存吃紧时，不能直接触发内存回收，那内存从哪儿来呢？那就只能是为 GFP_ATOMIC 这种高优先级的内存分配请求预留一些内存
* GFP_ATOMIC 并不是最高优先级的分配需求，它依旧可能失败。在前面就提到过，当内存吃紧时将会触发内存回收，内存回收本身也是需要使用内存的，因此必须要预留最后一部分内存用来执行内存回收程序，不然内存回收无法执行，那就回天乏术了。

从上面可以看出，内存的管理面对各种各样的情况，因此记录内存使用量并根据内存使用情况采取相对应的措施是非常有必要的，，这也就是每个 zone 中 watermark 成员的作用。

watermark 分为三个参考值，high、low、min，通常的比例为：TODO，可以通过 cat /proc/zoneinfo 查看每个 zone 对应的 watermark 值，其计算方式在函数 \_\_setup_per_zone_wmarks 中。

high 表示安全的水位线，代表此时的内存还有富余，当水位线接触到 low 时，说明内存有些吃紧，当本次内存分配完之后，内存会低于 low 但是高于 min，此时就会唤醒 kswapd 内核线程，开始异步的内存回收工作，内存回收的目标是让水位线达到 high。

当水位线低于 min 时，问题就比较严重了，说明内存已经严重不足，一般情况下，申请内存的进程会阻塞等待内存回收完成，也就是 direct reclaim，这个过程中普通的内存申请将不会导致内存水位的下降。

既然保留了 min 以下的内存不被普通的内存申请使用，自然有其特殊用途，其特殊用途主要就是上面所说的两个：GFP_ATOMIC 和内存回收程序发起的内存申请，对应的标志位为 \_\_GFP_MEMALLOC，这个标志位通常不会被普通程序使用。



了解完了 watermark 的相关知识，再回过头来看内存申请：

```c++
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
						const struct alloc_context *ac)
{
	struct zoneref *z = ac->preferred_zoneref;
	struct zone *zone;
	struct pglist_data *last_pgdat_dirty_limit = NULL;

	for_next_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
								ac->nodemask) {
		struct page *page;
		unsigned long mark;

		if (cpusets_enabled() &&
			(alloc_flags & ALLOC_CPUSET) &&
			!__cpuset_zone_allowed(zone, gfp_mask))
				continue;
        
		if (ac->spread_dirty_pages) {
			if (last_pgdat_dirty_limit == zone->zone_pgdat)
				continue;

			if (!node_dirty_ok(zone->zone_pgdat)) {
				last_pgdat_dirty_limit = zone->zone_pgdat;
				continue;
			}
		}

		mark = zone->watermark[alloc_flags & ALLOC_WMARK_MASK];
		if (!zone_watermark_fast(zone, order, mark,
				       ac_classzone_idx(ac), alloc_flags)) {
			int ret;

			/* Checked here to keep the fast path fast */
			BUILD_BUG_ON(ALLOC_NO_WATERMARKS < NR_WMARK);
			if (alloc_flags & ALLOC_NO_WATERMARKS)
				goto try_this_zone;

			if (node_reclaim_mode == 0 ||
			    !zone_allows_reclaim(ac->preferred_zoneref->zone, zone))
				continue;

			ret = node_reclaim(zone->zone_pgdat, gfp_mask, order);
			switch (ret) {
			case NODE_RECLAIM_NOSCAN:
				/* did not scan */
				continue;
			case NODE_RECLAIM_FULL:
				/* scanned but unreclaimable */
				continue;
			default:
				/* did we reclaim enough */
				if (zone_watermark_ok(zone, order, mark,
						ac_classzone_idx(ac), alloc_flags))
					goto try_this_zone;

				continue;
			}
		}

try_this_zone:
		page = buffered_rmqueue(ac->preferred_zoneref->zone, zone, order,
				gfp_mask, alloc_flags, ac->migratetype);
		if (page) {
			prep_new_page(page, order, gfp_mask, alloc_flags);
			if (unlikely(order && (alloc_flags & ALLOC_HARDER)))
				reserve_highatomic_pageblock(page, zone, order);

			return page;
		}
	}

	return NULL;
}
```





参考：watermark：https://zhuanlan.zhihu.com/p/73539328

GFP_ATOMIC  ：https://my.oschina.net/u/4410101/blog/4881290