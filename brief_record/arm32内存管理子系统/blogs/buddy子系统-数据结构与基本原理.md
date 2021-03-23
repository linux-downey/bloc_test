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
	unsigned long node_present_pages; /* total number of physical pages */
	unsigned long node_spanned_pages; /* total size of physical page
					     range, including holes */
	int node_id;

	/*
	 * This is a per-node reserve of pages that are not available
	 * to userspace allocations.
	 */
	unsigned long		totalreserve_pages;

	/* Write-intensive fields used by page reclaim */
	ZONE_PADDING(_pad1_)
	spinlock_t		lru_lock;

	unsigned long		flags;

	ZONE_PADDING(_pad2_)

	/* Per-node vmstats */
	struct per_cpu_nodestat __percpu *per_cpu_nodestats;
	atomic_long_t		vm_stat[NR_VM_NODE_STAT_ITEMS];
} pg_data_t;
```

因为本系列文章主要针对 buddy 子系统内存初始化以及分配回收的实现，不涉及内存碎片、内存交换、内存压缩的处理，同时考虑到当前系统的硬件特性(比如不支持地址扩展、numa 架构)，因此省去了一些不重要以及不相关的数据成员。 



https://blog.csdn.net/zhoutaopower/article/details/86736504