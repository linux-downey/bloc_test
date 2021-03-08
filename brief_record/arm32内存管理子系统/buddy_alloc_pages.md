# buddy - alloc_pages 接口解析

内存申请的所有起点在 contig_page_data。 

```c++
alloc_pages(gfp_mask, order)
    // 暂时不考虑 numa 架构的 buddy
    alloc_pages_node(numa_node_id(), gfp_mask, order)  
    	__alloc_pages_node(nid, gfp_mask, order)
    		// zone list 主要用于保存一个后备的申请空间,主要用在 numa 中，非 numa 架构就是当前zone
    		__alloc_pages(gfp_mask, order, node_zonelist(nid, gfp_mask))
    			// 因为是非 numa 节点，因此 nodemask 为 NULL，nodemask 主要针对 numa 的节点选择
    			__alloc_pages_nodemask(gfp_mask, order, zonelist, NULL);
					
					
```



## gfp flag

内存申请时可传入的 flag 标志,，类型为 gfp_t：

```c++
GFP_DMA
  作用: 从 ZONE_DMA 内分配内存
GFP_DMA32
  作用: 优先从 ZONE_DMA32 中分配内存，未找到可以到 ZONE_DMA 中分配物理内存
GFP_KERNEL
  作用: 优先从 ZONE_KERNEL 中分配物理内存，未找到可以依次从 ZONE_DMA32 到
        ZONE_DMA 分区中进行查找.
__GFP_HIGHMEM
  作用: 优先从 ZONE_HIGHMEM 中分配物理内存，未找到可以依次从 ZONE_NORMAL
        到 ZONE_DMA32, 最后到 ZONE_DMA 分区中查找可用物理内存.
GFP_ATOMIC
  作用: 分配物理内存不能等待或休眠，如果没有可用物理内存，直接返回。
__GFP_WAIT
  作用: 如果未找到可用物理内存，可以进入休眠等待 Kswap 查找可用物理内存.
__GFP_ZERO
  作用: 分配的物理内存必须被清零.
__GFP_HIGH
  作用: 当系统可用物理很紧缺的情况下，可以从紧急内存池中分配物理内存.
__GFP_NOFAIL:
  作用: 当系统中没有可用物理内存的情况下，可以进入睡眠等待 kswap 交换出更
        多的可用物理内存, 或者等待其他进程释放物理内存，直到分配到可用物理
        内存.
__GFP_COLD:
  作用: 从 ZONE 分区的 PCP 冷页表中获得可用物理页.
__GFP_REPEAT:
  作用: 当 Buddy 分配器没有可用物理内存时，进行 retry 操作以此分配到可用物
        理内存，但由于 retry 的次数有限，可能会失败.
__GFP_NORETRY:
  作用: 当 Buddy 分配器没有可用物理内存时，不进行 retry 操作，直接反馈结果。 
```





## alloc_pages 的核心部分

因为内核中 numa 和非 numa 系统使用兼容的接口，alloc_pages 前面的操作都是针对 numa 进行封装，这里不做解析，主要分析内存申请的部分。 

```c++
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
			struct zonelist *zonelist, nodemask_t *nodemask)
{
	struct page *page;
	unsigned int alloc_flags = ALLOC_WMARK_LOW;
	gfp_t alloc_mask = gfp_mask; 
	struct alloc_context ac = {
        // 从 gfp_mask 获取优先在哪个 zone 中申请内存。 
        // 而且这个 zone 是最大的 zone idx，如果当前 zone 不合适，就去其它的 zone idx 申请。
		.high_zoneidx = gfp_zone(gfp_mask),
        // 非 numa 架构中 zonelist 中只包含当前 zone
		.zonelist = zonelist,
        // numa node 相关的 nodemask
		.nodemask = nodemask,
        // 从 gfp_mask 中解析出申请的页面属于哪种类型的页面，__GFP_RECLAIMABLE 还是 __GFP_MOVABLE
        // __GFP_MOVABLE 表示可移动的页面，__GFP_RECLAIMABLE 表示可回收的页面
		.migratetype = gfpflags_to_migratetype(gfp_mask),
	};
	
	// cpuset 相关的，不支持...

    // 可以使用的 mask
	gfp_mask &= gfp_allowed_mask;

	// 如果申请标志位中包含 __GFP_DIRECT_RECLAIM，可能导致睡眠
	might_sleep_if(gfp_mask & __GFP_DIRECT_RECLAIM);

	// zonelist 如果为空，自然是无法申请的
	if (unlikely(!zonelist->_zonerefs->zone))
		return NULL;
	
    // 是否可以使用 CMA 区域，需要页面是可移动的，因为 CMA 需要被驱动用到的时候需要迁移页面
	if (IS_ENABLED(CONFIG_CMA) && ac.migratetype == MIGRATE_MOVABLE)
		alloc_flags |= ALLOC_CMA;

	// 根据 __GFP_WRITE 设置 ac.spread_dirty_pages 成员，具体是干嘛的待研究
	ac.spread_dirty_pages = (gfp_mask & __GFP_WRITE);

	// 获取优先选择的 zonelist，也就是 zonelist 中第一个 zone
	ac.preferred_zoneref = first_zones_zonelist(ac.zonelist,
					ac.high_zoneidx, ac.nodemask);
	if (!ac.preferred_zoneref->zone) {
		page = NULL;
		// 没有 preferred zone 的情况
		goto no_zone;
	}
	// 核心的申请函数，申请成功之后就可以返回，page 用来描述第一个页面
	page = get_page_from_freelist(alloc_mask, order, alloc_flags, &ac);
	if (likely(page))
		goto out;

no_zone:
	
	alloc_mask = memalloc_noio_flags(gfp_mask);
	ac.spread_dirty_pages = false;

	if (unlikely(ac.nodemask != nodemask))
		ac.nodemask = nodemask;
	// 申请内存的 slow path，显然优先的 zone 并不能提供合适的页面
	page = __alloc_pages_slowpath(alloc_mask, order, &ac);

out:
	if (memcg_kmem_enabled() && (gfp_mask & __GFP_ACCOUNT) && page &&
	    unlikely(memcg_kmem_charge(page, gfp_mask, order) != 0)) {
		__free_pages(page, order);
		page = NULL;
	}

	if (kmemcheck_enabled && page)
		kmemcheck_pagealloc_alloc(page, order, gfp_mask);

	trace_mm_page_alloc(page, order, alloc_mask, ac.migratetype);
    
	// 返回 page，可能申请成功，也可能申请失败
	return page;
}
```



## fast_past 申请内存



```c++
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
						const struct alloc_context *ac)
{
	struct zoneref *z = ac->preferred_zoneref;
	struct zone *zone;
	struct pglist_data *last_pgdat_dirty_limit = NULL;

	// 逐个地扫描 zone，以找到合适的 page
    // zone_idx 的优先级越大，数组越小，如果当前 zone 找不到合适的，那就到 idx++ 中找。 
	for_next_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
								ac->nodemask) {
		struct page *page;
		unsigned long mark;

		/*
		 * When allocating a page cache page for writing, we
		 * want to get it from a node that is within its dirty
		 * limit, such that no single node holds more than its
		 * proportional share of globally allowed dirty pages.
		 * The dirty limits take into account the node's
		 * lowmem reserves and high watermark so that kswapd
		 * should be able to balance it without having to
		 * write pages from its LRU list.
		 *
		 * XXX: For now, allow allocations to potentially
		 * exceed the per-node dirty limit in the slowpath
		 * (spread_dirty_pages unset) before going into reclaim,
		 * which is important when on a NUMA setup the allowed
		 * nodes are together not big enough to reach the
		 * global limit.  The proper fix for these situations
		 * will require awareness of nodes in the
		 * dirty-throttling and the flusher threads.
		 */
        // 暂时没搞懂
		if (ac->spread_dirty_pages) {
			if (last_pgdat_dirty_limit == zone->zone_pgdat)
				continue;

			if (!node_dirty_ok(zone->zone_pgdat)) {
				last_pgdat_dirty_limit = zone->zone_pgdat;
				continue;
			}
		}
		// 获取当前 zone 的 watermark 值
        // 如果 watermark 报警就做相关处理，否则不进入该分支
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
        // 内存的申请工作
		page = buffered_rmqueue(ac->preferred_zoneref->zone, zone, order,
				gfp_mask, alloc_flags, ac->migratetype);
		if (page) {
            // 特定 page 需要做一些处理
			prep_new_page(page, order, gfp_mask, alloc_flags);

			if (unlikely(order && (alloc_flags & ALLOC_HARDER)))
				reserve_highatomic_pageblock(page, zone, order);

			return page;
		}
	}

	return NULL;
}
```



## fast_path->buffered_rmqueue

```c++
static inline
struct page *buffered_rmqueue(struct zone *preferred_zone,
			struct zone *zone, unsigned int order,
			gfp_t gfp_flags, unsigned int alloc_flags,
			int migratetype)
{
	unsigned long flags;
	struct page *page;
    // 如果用户在申请的时候指定了 __GFP_COLD，则指定在 pcp 的 code page 中申请
	bool cold = ((gfp_flags & __GFP_COLD) != 0);
	
    // 只是申请一页的情况，申请一页只需要在 pcp 区域申请
	if (likely(order == 0)) {
		struct per_cpu_pages *pcp;
		struct list_head *list;

		local_irq_save(flags);
		do {
            // pcp 页面在 zone->pageset->pcp 链表上。
            /* 该结构的定义如下：struct per_cpu_pages {
                    int count;		
                    int high;		
                    int batch;		
                    // MIGRATE_PCPTYPES 有三个成员：
                    	MIGRATE_UNMOVABLE，MIGRATE_MOVABLE，MIGRATE_RECLAIMABLE
                    	每种 migratype 维护一个链表
					struct list_head lists[MIGRATE_PCPTYPES];
			};*/
			pcp = &this_cpu_ptr(zone->pageset)->pcp;
            // 根据传入的 migratetype 找到对应的链表头
			list = &pcp->lists[migratetype];
            // 如果 pcp 链表为空，就重新向 buddy
			if (list_empty(list)) {
                // 向伙伴系统申请 pcp->batch 个页面放到当前的链表上
				pcp->count += rmqueue_bulk(zone, 0,
						pcp->batch, list,
						migratetype, cold);
				if (unlikely(list_empty(list)))
					goto failed;
			}
			// 如果指定了 cold，就分配 code 页面，否则默认分配 hot 页面
			if (cold)
				page = list_last_entry(list, struct page, lru);
			else
				page = list_first_entry(list, struct page, lru);

			list_del(&page->lru);
            // 如果申请成功，pcp->count 递减
			pcp->count--;

		} while (check_new_pcp(page));
	} else
        // 如果不是申请一个页面，就走这个分支
    	{
		// 如果需要分配多个页面，同时设置了 __GFP_NOFAIL 的话，提出警告。
        // 不建议这么干，可能导致内核一直分配不出合适的页面
		WARN_ON_ONCE((gfp_flags & __GFP_NOFAIL) && (order > 1));
		spin_lock_irqsave(&zone->lock, flags);

		do {
			page = NULL;
            /*
                ALLOC_WMARK_MIN 在当前分配区使用zone->pages_min进行检查。
                ALLOC_WMARK_LOW 在当前分配区使用zone->pages_low进行检查。
                ALLOC_WMAEK_HIGH 在当前分配区使用zone->pages_high进行检查
                ALLOC_HARDER 通知伙伴系统放宽检查限制，其实就是对水印给定的值乘以一个系数 3/4
                ALLOC_HIGH 比HARDER更紧急的分配请求，进一步放宽限制
                ALLOC_CPUSET 只能在当前节点相关连的内存节点进行分配。
                
            */
			if (alloc_flags & ALLOC_HARDER) {
                // __rmqueue_smallest 直接从 free area 中找 page
				page = __rmqueue_smallest(zone, order, MIGRATE_HIGHATOMIC);
				if (page)
					trace_mm_page_alloc_zone_locked(page, order, migratetype);
			}
			if (!page)
                // __rmqueue 先会尝试在 free area 中找 page
                // 如果没有合适的 page，从 cma 区域申请
                // 如果没有合适的 page，从 fallback 中申请
				page = __rmqueue(zone, order, migratetype);
		} while (page && check_new_pages(page, order));
		spin_unlock(&zone->lock);
		if (!page)
			goto failed;
		__mod_zone_freepage_state(zone, -(1 << order),
					  get_pcppage_migratetype(page));
	}

	__count_zid_vm_events(PGALLOC, page_zonenum(page), 1 << order);
	zone_statistics(preferred_zone, zone, gfp_flags);
	local_irq_restore(flags);

	VM_BUG_ON_PAGE(bad_range(zone, page), page);
	return page;

failed:
	local_irq_restore(flags);
	return NULL;
}
```





alloc_page 的参考网址：https://blog.csdn.net/kickxxx/article/details/9287003

MIGRATE_HIGHATOMIC的说明：https://blog.csdn.net/frank_zyp/article/details/89249469