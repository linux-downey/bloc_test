# linux内存子系统 - buddy 子系统3 - 分配与释放

前面的章节 [memblock 到 buddy](https://zhuanlan.zhihu.com/p/363925295) 分析了 buddy 子系统是如何从 memblock 内存分配器过渡而来，同时 [buddy实现原理及数据结构](https://zhuanlan.zhihu.com/p/363922807) 中分析了 buddy 子系统的相关数据结构与基本原理，在这一章中，继续分析 buddy 子系统靠近最靠近用户的一部分：页面的分配与释放。 

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

每个 zonelist 中包含多个 zoneref，一个 zoneref 用于描述一个 zone，比如 ZONE_NORMAL、ZONE_HIGHMEM，因此，一个 zonelist 中包含了整个内存节点中的所有 zone。**同时需要注意的是，\_zonerefs 数组中所保存的 zone 是反序的，比如 \_zonerefs[0] 描述的是 ZONE_HIGHMEM、 \_zonerefs[1] 描述的是 ZONE_NORMAL，这种顺序在跨 zone 分配时更方便遍历，这部分将在下文中详谈。**

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

1 ： 构建一个内存分配的上下文结构，其实就是把内存分配需要的那些信息打个包，方便传来传去。high_zoneidx 成员表示本次页面分配需要扫描的最上层的 zoneidx，默认是从 ZONE_NORMAL，当指定 ZONE_HIGHMEM 时，也就返回该 zone 对应的 index，用来确定即将扫描的 zone 区域的界限。

nodemask 成员值为 0，只在 NUMA 架构中用到。
migratetype 表示页面的类型，包括 MOVABLE/UNMOVABLE/RECLAIMABLE 等，不同的迁移类型对应 free_area 中不同的数组项，同时不同的迁移类型还会导致不同的分配行为。 

2 ： 如果用户指定的迁移类型中带有 MIGRATE_MOVABLE 标志，就可以使用系统中的 CMA 区域来分配内存，毕竟 CMA 区域必须只能被分配为移动页面使用。
如果 gfp_mask 中包含 \_\_GFP_WRITE，说明该页面会被修改，buddy 对 zone 中的脏页面有所限制，因此需要统计页面的读写需求。
实际上类似于 \_\_GFP_WRITE 这类标志位并不需要申请者直接指定，而是由内核提供一系列的标志位组合，比如常用的 GFP_KERNEL 就是 (\_\_GFP_RECLAIM | \_\_GFP_IO | \_\_GFP_FS) 这几者的组合 ，通常带双下划线的都不是被用户直接使用的申请标志。

3 ： preferred_zoneref 表示第一个将要被遍历的 zone，通过执行 first_zones_zonelist 函数返回该 zoneref 对象，zone 的选取主要是根据传入 ac.high_zoneidx 对象，从 zonelist->_zonerefs 数组中，找到可用的最接近或等于 ac.high_zoneidx 的那个 zone，对应的源码为：

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

4 ： 物理页面申请的主要实现函数为 get_page_from_freelist，传入 4 个参数：alloc_mask、order、alloc_flags 和 ac，ac 就是当前函数中构建的内存分配上下文结构。该函数返回申请到的 page 页面，但是该函数并不保证内存分配的成功。
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



了解完了 watermark 的相关知识，再回过头来看内存申请的具体实现：

```c++
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
						const struct alloc_context *ac)
{
    ...
	for_next_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
								ac->nodemask) {             ................5
		struct page *page;
		unsigned long mark;
        
		if (ac->spread_dirty_pages) {
			if (last_pgdat_dirty_limit == zone->zone_pgdat)
				continue;

			if (!node_dirty_ok(zone->zone_pgdat)) {         ................6
				last_pgdat_dirty_limit = zone->zone_pgdat;
				continue;
			}
		}

		mark = zone->watermark[alloc_flags & ALLOC_WMARK_MASK];
		if (!zone_watermark_fast(zone, order, mark,
				       ac_classzone_idx(ac), alloc_flags)) { ...............7
			int ret;

			if (alloc_flags & ALLOC_NO_WATERMARKS)
				goto try_this_zone;

			if (node_reclaim_mode == 0 ||
			    !zone_allows_reclaim(ac->preferred_zoneref->zone, zone))
				continue;

			ret = node_reclaim(zone->zone_pgdat, gfp_mask, order);
			switch (ret) {
			case NODE_RECLAIM_NOSCAN:
				continue;
			case NODE_RECLAIM_FULL:
				continue;
			default:
				if (zone_watermark_ok(zone, order, mark,
						ac_classzone_idx(ac), alloc_flags))
					goto try_this_zone;

				continue;
			}
		}

try_this_zone:
		page = buffered_rmqueue(ac->preferred_zoneref->zone, zone, order,
				gfp_mask, alloc_flags, ac->migratetype);   .................8
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

5 ： 执行物理页面分配时，将会按照 zoneref 数组和 zonelist 中的顺序对各个 zone 进行扫描，在一个 node 内，根据当次分配的 gfp mask 确定需要从哪个 zone 分配，然后以从高到低的顺序扫描各个zone，扫描完一个 node 之后，再处理 zonelist 中指定的备用 node。
比如顺序为 ZONE_HIGHMEM->ZONE_NORMAL->ZONE_DMA。

6 ： 当申请页面的 gfp_mask 中包含 \_\_GFP_WRITE 时，表示申请的物理页面将会生成脏页，单个 node 中会对 dirty page 数量存在限制，超出限制时，buddy 就不会在当前 zone 中分配 \_\_GFP_WRITE 类型的物理页面了，转而向扫描下一个 zone 区域。
node_dirty_ok 用于判断 node 中脏页面是否超出限制，被标记为 NR_FILE_DIRTY、NR_UNSTABLE_NFS、NR_WRITEBACK 这几种类型的页面将会被视为 dirty page。

7 ： 执行内存水位的相关处理，在分配之前，先会获取本次内存分配水位线限制，该水位线由 alloc_flags 的最后两位决定，0~2 分别对应 MIN、LOW、HIGH，如果用户没有指定，会默认指定为 LOW。
zone_watermark_fast 用于判断内存水位值是否满足本次申请需求，另一个用于判断内存水位线的函数为 zone_watermark_ok，实际上 zone_watermark_fast 最终也是调用 zone_watermark_ok，从 fast 可以看出，这是内核在尝试走捷径，其实是针对单个页面分配的优化，因为内核中申请单个页面的情形非常普遍(slub/slab通常只申请单个页面)，单个页面时就可以快速判断页面余量并且返回，而多个页面的情况下需要通过相对复杂的计算才能确定。 
zone_watermark_ok 的实现实际上在上文中对 watermark 的介绍中讲得七七八八了，也是水位线控制的核心实现，这里做一个简要的分析：

```c++
bool __zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
			 int classzone_idx, unsigned int alloc_flags,
			 long free_pages)
{
    const bool alloc_harder = (alloc_flags & ALLOC_HARDER);
    free_pages -= (1 << order) - 1;

	if (alloc_flags & ALLOC_HIGH)
		min -= min / 2;
    
    if (likely(!alloc_harder))
		free_pages -= z->nr_reserved_highatomic;
	else
		min -= min / 4;                      ..........................a
            
    if (!(alloc_flags & ALLOC_CMA))
		free_pages -= zone_page_state(z, NR_FREE_CMA_PAGES);
    
    if (free_pages <= min + z->lowmem_reserve[classzone_idx])
		return false;                       ..........................b

	if (!order)
		return true;                         
    
    for (o = order; o < MAX_ORDER; o++) {
		struct free_area *area = &z->free_area[o];
		int mt;

		if (!area->nr_free)
			continue;

		for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
			if (!list_empty(&area->free_list[mt]))
				return true;                  
		}

		if ((alloc_flags & ALLOC_CMA) &&
		    !list_empty(&area->free_list[MIGRATE_CMA])) {
			return true;
		}
        
		if (alloc_harder &&
			!list_empty(&area->free_list[MIGRATE_HIGHATOMIC]))
			return true;                       .........................c
	}
}
```

a.  __zone_watermark_ok 中的 free_pages 参数需要拿出来说一下，这个参数是通过 zone_page_state(zone, NR_FREE_PAGES) 函数获取的，从函数实现来看实际的 freepages 数量是保存在 zone->vm_stat[NR_FREE_PAGES] 中的，zone->vm_stat 是一个 zone 内信息的统计结构，记录 zone 中的国中信息，具体的信息在 enum zone_stat_item 中列出。 
alloc_harder 标志位在用户传入的标志位中带有 \_\_GFP_ATOMIC 且不带有 \_\_GFP_NOMEMALLOC 标志时被置位，这个标志位具有较高的分配优先级，意味着可以在更艰难的情况下同样需要完成内存分配，这个标志位表示可以使用低水位线的内存或者保留内存。 
ALLOC_HIGH 这个标志位同样表示本次内存分配具有较高的优先级，可以使用更低水位线的内存。

对于这两个标志位而言，ALLOC_HIGH 可以使用水位线 1/2 处的内存执行分配，如果 alloc_harder 标志同时被置位，甚至可以使用水位线 1/4 处的内存。
z->nr_reserved_highatomic 中记录了专门为高优先级内存分配请求的保留页面的数量，如果 alloc_harder 标志位没有被置位，在计算 freepage 数量时就需要排除这一部分内存页面。 
这部分 highatomic 页面并不是在 buddy 初始化的时候被保留的，而是在分配完一次 HIGH_ATOMIC 内存页面之后为后续的再次分配保留的，对应的实现在 reserve_highatomic_pageblock 中。



b. 如果在分配时指定内存的迁移类型为 MIGRATE_MOVABLE，同时内核支持 ALLOC_CMA时，就可以使用 CMA 区域执行内存分配，反之如果迁移类型不为 MIGRATE_MOVABLE，就要将 freepage 的数量减去 CMA page 的数量，标志在计算水位时不能将这些 page 算进去。
简单地判断内存水位线是否满足要求的公式为：

```c++
if (free_pages <= min + z->lowmem_reserve[classzone_idx])
		return false;
```

free_pages 是当前 zone 中剩余的空闲内存，一个 zone 中总的 freepage 是记录在 zone->vm_stat[NR_FREE_PAGES] 数组成员中的，对于不同的标志位，这个剩余内存用量是不一样的，某些内存用量是为特定场景预留的，因此对于非特定场景的内存申请，传入的 free_pages  需要先根据 order 减去需要分配的内存，再减去这些 page，再进行水位线计算。

min 是水位线值，表示本次内存分配能触及到的内存底限，同样也会在传入的 mark 值基础上进行处理，默认情况下为 LOW 水位线，内存吃紧的情况下可能被设置为 MIN 水位线，对于优先级较高的情况(比如 GFP_ATOMIC)可以触及到水位线以下的位置。

lowmem_reserve 是 zone 中预留的内存，鉴于高  level 的内存分配可以使用低 level 的内存区，但是这种侵占不能是无限制的，不然就可能导致低 level 的 zone 无法满足原本属于该区的内存申请，毕竟低 level 的 zone 对物理内存提出了更高的要求，是更珍贵的，这种情况下会为 zone 保留一部分内存，这部分保留内存并没有独立出来，只是作为一个记录值，在高 level fallback 到低 level zone 时需要注意留下对应数量的内存(这部分关于 classzone_idx 的指定以及 lowmem_reserve 中保存的值比较不好理解，可以结合 setup_per_zone_lowmem_reserve 函数的实现)，具体保留内存的数量可以由 /proc/sys/vm/lowmem_reserve_ratio 查看和配置。



c.  即使 freepage 数量满足了要求，也并不一定意味着水位线的检查就能返回 true，还有一个重要的参数为 order。假设 order 值为 6，也就是一次性需要申请 64 个 page，即使当前 zone 经过计算之后的 freepage 还剩 200 个 page，也不一定能凑出连续的 64 个 page 满足分配要求。
因此，如果 order 为 0，只需要分配一个页面，自然是可以返回 true。如果非零，就需要检查 zone->freearea 的链表，查看是否存在满足分配要求的链表节点。
从 free_area[order] 所属的成员链表开始，一级一级网上，毕竟如果 free_area[order] 中没有满足要求的页面，可以征用 free_area[order+1]中的链表节点。

在 free_area[order] 内部，又分为不同的 MIGRATE_TYPE，默认情况下可以从 UNMOVABLE、MOVABLE、RECLAIMABLE 三组链表中查找，如果 ALLOC_CMA 被设置，可以从 MIGRATE_CMA 中找，如果进一步 alloc_harder 标志位被设置(GFP_ATOMIC)，还可以从 MIGRATE_HIGHATOMIC 链表成员中找。

找到之后返回 true，表示水位线的检查没问题，没找到就继续往 order+n 的方向努力，直到找到最后一个 order，实在没有满足要求的链表节点就返回 false。

如果水位检查失败，返回的是 false，一种可能是，标志位中设置了 ALLOC_NO_WATERMARKS，这个标志位表示忽略水位检查，依旧尝试从当前 zone 中分配内存，使用这个标志位的通常是内存回收进程，用户程序非常不建议使用这个标志位，即使是高优先级的 GFP_ATOMIC 也不会设置该标志位，毕竟这几乎意味着征用最后的内存。
接着调用 zone_allows_reclaim 来检查当前 node 是否支持 reclaim，如果不支持就转而到下一个 zone 中扫描，如果支持，就调用 node_reclaim 执行内存回收的工作，实际上这个函数只有在 NUMA 架构系统中才会有对应的实现，非 NUMA 架构中为空，因此对于非 NUMA 架构而言，如果水位线检查不 ok，就跳转到下一个 zone 尝试分配。



8 ： 在水位线检查没问题的情况下，就可以进入到真正的内存分配阶段了。对应的函数为 buffered_rmqueue，返回分配到的一个或多个页面对应的 struct page 结构，如果是多个页面，struct page 对应第一个页面。
正常的页面分配在前面的章节中有对应的描述，这里也就不贴代码了，直接一步步地进行分析吧：

* 如果需要分配的页面为单页，直接从 percpu 的 pcp page 链表中分配，鉴于内核中单页分配是非常频繁的，因此 buddy 子系统为每个 CPU 缓存了一部分单页，以提高分配效率。
  如果 pcp page 缓存已经被分配完，就从 free_area 中获取页面继续填充到 pcp page 缓存中以完成单页的分配，对应的接口为 rmqueue_bulk，同时单页的缓存被分为 hot page 和 cold page，hot page 是使用完不久被返回到 pcp page 缓存中的，缓存被重复利用的可能性很大，而 cold page 相关，hot 和 cold page 的处理方式为，hot page 被链接在链表的头部，而 cold page 被链接在链表尾部，当然，这个 hot 和 cold 是相对的，也就是释放时间越短的页默认在链表的越前面。返回给申请者的单页也是默认从 hot 到 cold，申请者可以指定 cold flag 来指定申请 cold page。
* 如果需要分配的页面为多页，就需要从 zone->free_area 中申请页面，对应链表节点，普通的内存申请将会用到 UNMOVABLE、MOVABLE、RECLAIMABLE 这三个 migratype 的数组，其中 MOVABLE 类型的申请请求还可以使用 CMA 区域，对于 GFP_ATOMIC 的需求还可以使用 MIGRATE_HIGHATOMIC  数组成员中的页面，如果当前 order 的链表中不存在合适的节点，就扫描 order+1 数组，直到直到合适的页面节点，分配剩下的部分添加到对应 order 的链表节点中。
  对于这部分的分配网上资料非常多，也介绍得比较详细了，这里也就不过多赘述了，有兴趣的可以自己看看源代码。





### slowpath

上述讨论的是 get_page_from_freelist 分配内存的情况，该函数会遍历所有能遍历 zone、node，以期望能找到合理的物理页面，理想状态下当然是返回满足要求的内存，但是也可能分配失败，如果分配失败应该如何处理呢？

自然是不能那么轻易地放弃的，我们再回到最初的内存分配函数中，当 get_page_from_freelist  分配失败时，还会执行另一个函数：

```c++
 page = get_page_from_freelist(alloc_mask, order, alloc_flags, &ac); 
    if (likely(page))
		goto out;
    page = __alloc_pages_slowpath(alloc_mask, order, &ac);              
```

也就是内存分配的 slopath，既然已经确定了现存的物理内存无法满足要求，那么就尝试对内存进行整理，尝试找到一片合适的内存。

这就像你买了一个新家具回家，乍一看没有地方放，你自然是不会直接把新买的家具扔了，而是整理屋子中现有的家具，该移动的移动、该扔的扔，或者将一些原本零散的东西放到一起，以腾出一片大的空间来存放新家具，buddy 系统的内存整理也是这样一个原理，其中包含几个部分：

* 允许使用更低水位线部分的内存，但针对不同等级的内存分配依旧有不同限制。
* 内存回收，在系统的实际运行中，内核会在内存中缓存大量的数据，尤其是针对文件系统，毕竟那些内存闲着也是闲着，这样可以提供内核运行效率，在内存吃紧时可以回收这部分内存。
  内存回收可以是同步的也可以是异步的，当内存较少但是并不是非常严重时，可以只是唤醒内存回收进程，将其加入到调度队列执行，该分配的内存继续分配。而当物理内存的分配实在无以为继时，就会停止本次内存分配动作，直到内存回收完成，这也是同步内存回收，这也可以保证在内存回收期间普通的内存申请不能再分配内存从而导致进一步的内存问题。
* memory compaction：compaction 翻译为压紧、压实，可以理解为通过内存迁移的方式将零散的空闲内存整合成大片的物理内存，典型的对象是用户空间程序所占用的页面，这些页面可以移动，只需要修改页表映射即可。
* 内存交换：将没用到的内存暂时交换到硬盘中，这样会严重影响运行效率，毕竟硬盘读写速率很慢，但是如果内存低到系统无法正常运行了，也不得不这么做。
* 直接挑一个不那么重要的进程 kill 掉，到这一步基本上已经是弹尽粮绝了，杀一个进程祭天，从而回收该进程的内存继续使用，俗话说死道友不死贫道，这也是有道理的，如果内核运行不下去了，进程也都不存在了。
  当然，怎么选择被 kill 的进程也是遵照一些特定算法的。



对于内存回收部分，博主还只是通过源代码理出一个大概框架，后续如果有时间，将会有后续的文章来专门描述这一部分。



### free_pages

buddy 子系统的释放相对于内存申请要简单很多，因为不需要考虑内存不足的问题，只需要简单地将对应的页面放回到 buddy 子系统中。

```c++
void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		__free_pages(virt_to_page((void *)addr), order);
	}
}
void __free_pages(struct page *page, unsigned int order)
{
	if (put_page_testzero(page)) {
		if (order == 0)
			free_hot_cold_page(page, false);
		else
			__free_pages_ok(page, order);
	}
}
```

free_pages 接受两个参数，虚拟地址和需要释放的页面数，内核通过 virt_to_page 宏将虚拟地址转换为对应页面的 struct page 结构，virt_to_page  这个函数就是通过 mem_map 这个全局变量加上页面偏移获取对应的 struct page 结构。

在释放物理页面之前，需要先将对应 page 的 refcount 减去 1，然后测试是否为 0，以确定该页面没有其它的使用者，如果该页面正在被使用，是释放不了的。

```c++
从源代码来看，如果 put_page_testzero 返回 false，也就是该页面依旧被引用，会直接退出该函数，也没有错误返回值，这就意味着用户在调用 free_pages 之后并不知道是不是真的释放了该 page，一旦出现这种情况，可能造成内存泄漏，而且不太好检测。
```

当释放的页面是单页时，调用 free_hot_cold_page 函数，单页的返回会直接放到 pcp page 链表上，free_hot_cold_page 第二个参数 flag 表示将该页面当成 cold page 还是 hot page，如果是 cold page，放在 pcp page 链表的尾端，否则放到前端，相对 hot 的 page 将在下次分配时优先使用。

如果 pcp page 链表上的页面太多，也会将部分单页释放会 buddy 子系统中，触发释放的页面数量阈值保存在 pcp->high 中，pcp 上的 page 一旦超过这个值，就调用 free_pcppages_bulk 释放页面到 buddy 子系统中，这个释放自然只会释放一部分，如果全部释放，下次申请单页的时候又得从 buddy 中获取，非常影响效率。

如果释放的不是单页，也就是 order 不为 0，就需要直接操作 buddy 的 free_area，将页面挂到对应的 free_area\[order\]\[migratype\] 链表上， 同时会尝试进行 merge 操作，也就是两个 page 块能组成一个更大的 page 块，就将这两个 page 块合并，放到更高阶(order+1) free_area 链表中。

当一个 page 块插入到链表中时，通过 __find_buddy_index 尝试寻找当前 page 块的 buddy，该函数的原理如下：

```c++
{
    ...
	page_idx = pfn & ((1 << MAX_ORDER) - 1);
	buddy_idx = __find_buddy_index(page_idx, order);
    ...
}

static inline unsigned long
__find_buddy_index(unsigned long page_idx, unsigned int order)
{
	return page_idx ^ (1 << order);
}
```

首先获取待释放页面的 page_idx，page_idx 就是将页帧号 pfn 取最低的 MAX_ORDER 位，然后使用计算得出的 page_idx 和 order 作为参数计算当前 page(块) 的 buddy page(块).

假设 MAX_ORDER 为 11，释放页面的 order 为 2，PAGE_OFFSET 为 12，同时知道需要释放的页面虚拟地址，先通过 virt_to_page 获取虚拟地址对应页面的 struct page 结构，再通过 page_to_pfn 获取对应的物理页帧号，假设为 0x10012，其计算得出的 page_idx 为 0x12。

于是将 page_idx(0x12) 和 order(2) 作为参数传递给 \_\_find_buddy_index，返回值为：

```c++
page_idx ^ (1 << order) = 0x12 ^ (1<<2) = 0x12 ^ 0x4 = 0x16
```

因此，可以得出物理页帧号为 0x10012，order 为 2 的 page 块的 buddy page 块对应的页帧号为 0x10016。

通过 \_\_find_buddy_index 获取到 buddy 之后，这时候该 buddy 页面块可能存在以下情况：

* 正在被占用
* 在页面分配过程中被分成更小 order 的页面，部分被占用
* 整个 buddy 都处于空闲

当当前释放的 page(块)的 buddy 也正好空闲时，这时候就可以进行 merging 了，合成更大的内存块，放到更高阶的 free_area 链表中。这种 merging 操作会递归向上，直到在某一阶无法执行 merging，就不会向上继续扫描，完成本次页面释放并返回。



### 参考

4.9.88 源码

https://zhuanlan.zhihu.com/p/73539328

https://my.oschina.net/u/4410101/blog/4881290

https://zhuanlan.zhihu.com/p/81961211

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

