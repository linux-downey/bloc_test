bootmem_init 函数在 map_lowmem 和 devicemaps_init 之后调用，在这个时候，所有的低端内存都已经被映射完成，向量表的页面也已经被初始化完成。 

bootmem_init 主要的功能将会初始化 struct pglist_data contig_page_data 这个结构体。



```c++
typedef struct pglist_data {
    // zone 区域的个数
	struct zone node_zones[MAX_NR_ZONES];
    // 可分配区域的 zone，包含本区域的 zone_list 的同时包含一个后备区域的 zone_list，不过只有在 numa 系统下才会支持后备区域，非 NUMA 系统下 MAX_ZONELISTS 就是 1，只有一个 zone_list 
	struct zonelist node_zonelists[MAX_ZONELISTS];
    // zone 的个数
	int nr_zones;
    // 当前系统下是 flat 类型的内存模型，还有一种常用的为 SPARSE 的稀疏内存模型 
#ifdef CONFIG_FLAT_NODE_MEM_MAP	/* means !SPARSEMEM */
    // 指向当前 node 中所有 page struct 构成的 mem map 数组，该数组占用几个 page？
	struct page *node_mem_map;
	
    // 该 node 开始的页帧号
	unsigned long node_start_pfn;
    // 系统存在的 pages
	unsigned long node_present_pages; /* total number of physical pages */
    // 线性地址范围内的 pages，包括 holes,如果没有 holes，通常等于 node_present_pages
	unsigned long node_spanned_pages; 
	// numa node id，非 numa 系统下为 0
	int node_id;
	wait_queue_head_t kswapd_wait;
	wait_queue_head_t pfmemalloc_wait;
	struct task_struct *kswapd;	
    
	int kswapd_order;
	
    enum zone_type kswapd_classzone_idx;

	int kswapd_failures;		

#ifdef CONFIG_COMPACTION
	int kcompactd_max_order;
	enum zone_type kcompactd_classzone_idx;
	wait_queue_head_t kcompactd_wait;
	struct task_struct *kcompactd;
#endif

	// 该 node 下总共多少个 reserved pages
	unsigned long		totalreserve_pages;

	/* Write-intensive fields used by page reclaim */
	ZONE_PADDING(_pad1_)
	spinlock_t		lru_lock;


	/* Fields commonly accessed by the page reclaim scanner */
	struct lruvec		lruvec;

	/*
	 * The target ratio of ACTIVE_ANON to INACTIVE_ANON pages on
	 * this node's LRU.  Maintained by the pageout code.
	 */
	unsigned int inactive_ratio;

	unsigned long		flags;

	ZONE_PADDING(_pad2_)

	/* Per-node vmstats */
	struct per_cpu_nodestat __percpu *per_cpu_nodestats;
	atomic_long_t		vm_stat[NR_VM_NODE_STAT_ITEMS];
} pg_data_t;
```



### bootmem_init 源码

```c++
void __init bootmem_init(void)
{
	unsigned long min, max_low, max_high;

	memblock_allow_resize();
	max_low = max_high = 0;
    
	// min 是物理地址的最低点(页帧号)，max_low 是低端内存的最高点，max_high 是高端内存最高点
	find_limits(&min, &max_low, &max_high);

	early_memtest((phys_addr_t)min << PAGE_SHIFT,
		      (phys_addr_t)max_low << PAGE_SHIFT);
	
    // 空函数，sparse 内存模型才有的 
	arm_memory_present();
	sparse_init();

	/*
	 * Now free the memory - free_area_init_node needs
	 * the sparse mem_map arrays initialized by sparse_init()
	 * for memmap_init_zone(), otherwise all PFNs are invalid.
	 */
	zone_sizes_init(min, max_low, max_high);

	min_low_pfn = min;
	max_low_pfn = max_low;
	max_pfn = max_high;
}
```

### zone_sizes_init

```c++
static void __init zone_sizes_init(unsigned long min, unsigned long max_low,
	unsigned long max_high)
{
    // zone_size 保存每个 zone 的 size，单位为页帧
    // zhole_size 为每个 zone 的 hole size，该平台的物理内存中没有 hole
	unsigned long zone_size[MAX_NR_ZONES], zhole_size[MAX_NR_ZONES];
	struct memblock_region *reg;

	memset(zone_size, 0, sizeof(zone_size));

	zone_size[0] = max_low - min;
#ifdef CONFIG_HIGHMEM
	zone_size[ZONE_HIGHMEM] = max_high - max_low;
#endif

	memcpy(zhole_size, zone_size, sizeof(zhole_size));
	for_each_memblock(memory, reg) {
		unsigned long start = memblock_region_memory_base_pfn(reg);
		unsigned long end = memblock_region_memory_end_pfn(reg);

		if (start < max_low) {
			unsigned long low_end = min(end, max_low);
			zhole_size[0] -= low_end - start;
		}
#ifdef CONFIG_HIGHMEM
		if (end > max_low) {
			unsigned long high_start = max(start, max_low);
			zhole_size[ZONE_HIGHMEM] -= end - high_start;
		}
#endif
	}

	free_area_init_node(0, zone_size, min, zhole_size);
}
```

### free_area_init_node

在这个函数中，开始初始化 contig_page_data  这个结构体。

```c++
void __paginginit free_area_init_node(int nid, unsigned long *zones_size,
		unsigned long node_start_pfn, unsigned long *zholes_size)
{
    // #define NODE_DATA(nid) (&contig_page_data)
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned long start_pfn = 0;
	unsigned long end_pfn = 0;


	pgdat->node_id = nid;
	pgdat->node_start_pfn = node_start_pfn;
	pgdat->per_cpu_nodestats = NULL;

	start_pfn = node_start_pfn;
	// 计算每个 zone 的 total pages，这是基于 memory 节点计算的，包括 reserved 部分。 
	calculate_node_totalpages(pgdat, start_pfn, end_pfn,
				  zones_size, zholes_size);

	alloc_node_mem_map(pgdat);
#ifdef CONFIG_FLAT_NODE_MEM_MAP
	printk(KERN_DEBUG "free_area_init_node: node %d, pgdat %08lx, node_mem_map %08lx\n",
		nid, (unsigned long)pgdat,
		(unsigned long)pgdat->node_mem_map);
#endif

	reset_deferred_meminit(pgdat);
	free_area_init_core(pgdat);
}
```

