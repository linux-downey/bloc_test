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
    
	// min 是物理地址的最低点(页帧号)，max_low 是低端内存的最高点，max_high 是高端内存最高点，也就是物理地址的内存终点
    // memblock.memory 中保存的物理内存描述是从低地址到高地址的，因此，获取物理地址的最大值就是找到 memblock.memory 最后一个 regions 即可计算得到。 
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

    // 把物理地址起始页帧、低端内存最高地址、高端内存最高地址保存下来。
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

	// contig_page_data 的部分成员赋值
	pgdat->node_id = nid;
	pgdat->node_start_pfn = node_start_pfn;
	pgdat->per_cpu_nodestats = NULL;

	start_pfn = node_start_pfn;
	// 计算每个 zone 的 total pages，这是基于 memory 节点计算的，包括 reserved 部分。赋值到  contig_page_data 中
	calculate_node_totalpages(pgdat, start_pfn, end_pfn,
				  zones_size, zholes_size);
	
    // 为对应所有物理内存的 struct page 申请内存,包括高端内存
    // 在这里，物理内存长度为 0x10000000 = 256M，每个页面 4K，一共 64K 个物理页面
    // 每个页面的 struct page 占用 0x20 的空间(32字节)，一共占用 0x200000 的空间
    // 由 memblock 申请，保存在 memblock.reserved 区域中。 
    // 将这片内存的首 page 地址保存在 pgdat->node_mem_map 中。 
	alloc_node_mem_map(pgdat);
#ifdef CONFIG_FLAT_NODE_MEM_MAP
	printk(KERN_DEBUG "free_area_init_node: node %d, pgdat %08lx, node_mem_map %08lx\n",
		nid, (unsigned long)pgdat,
		(unsigned long)pgdat->node_mem_map);
#endif

	reset_deferred_meminit(pgdat);
    // 为每个区域的 zone->pageblock_flags 申请内存空间，4 字节。 
    // 针对每个物理内存中的每个 page 进行初始化，包括是否设置为 movable，设置 page 所属的 zone，refcount等等(free_area_init_core->memmap_init->memmap_init_zone)
    // 初始化各个 zone->free_area[order].free_list,这个就是 buddy 子系统中存放页面的地方（init_currently_empty_zone->zone_init_free_lists）
	free_area_init_core(pgdat);
}
```



这个函数结束之后，bootmem_init 函数基本上就已经完成了，因此，bootmem_init 基本上做的事情就是：

初始化 contig_page_data 结构，并赋值，包括 zone_size，zone 中的各个 page 属性，同时为所有 struct  page 结构申请内存，并保存在 contig_page_data->node_mem_map 中。 

每个 page 做一些初始化设置,包括一些 flag 设置，这些设置作用于每个 page 对应的 struct  page 结构，也就是  contig_page_data->node_mem_map 执行的内存部分。

目前使用了两个 zone：normal 和 highmem，前面的 zone_dma 和 zone_dma32 并没有使用，dma 设备可以直接使用 normal 部分的内存。

定义了三个 zone，最后一个 zone 是 movable，没有相关的 page。



b bootmem_init

print/x memblock.memory.regions.size