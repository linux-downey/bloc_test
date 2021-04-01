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
    
    page = get_page_from_freelist(alloc_mask, order, alloc_flags, &ac); .......4
    ...
}
```

1. 构建一个内存分配的上下文结构，其实就是把内存分配需要的那些信息打个包，方便传来传去。high_zoneidx 成员表示本次页面分配需要扫描的最上层的 zoneidx，默认是从 ZONE_NORMAL，当指定 ZONE_HIGHMEM 时，也就返回该 zone 对应的 index，用来确定即将扫描的 zone 区域的界限。

   nodemask 成员值为 0，只在 NUMA 架构中用到。
   migratetype 表示页面的类型，包括 MOVABLE/UNMOVABLE/RECLAIMABLE 等，不同的迁移类型对应 free_area 中不同的数组项，同时不同的迁移类型还会导致不同的分配行为。 

2. 如果用户指定的迁移类型中带有 MIGRATE_MOVABLE 标志，就可以使用系统中的 CMA 区域来分配内存，毕竟 CMA 区域必须只能被分配为移动页面使用。
   如果 gfp_mask 中包含 \_\_GFP_WRITE，说明该页面会被修改，buddy 对 zone 中的脏页面有所限制，因此需要统计页面的读写需求。
   实际上类似于 \_\_GFP_WRITE 这类标志位并不需要申请者直接指定，而是由内核提供一系列的标志位组合，比如常用的 GFP_KERNEL 就是 (\_\_GFP_RECLAIM | \_\_GFP_IO | \_\_GFP_FS) 这几者的组合 ，通常带双下划线的都不是被用户直接使用的申请标志。

3. preferred_zoneref 表示第一个

