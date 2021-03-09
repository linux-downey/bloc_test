## mm_init 函数解析

contig_page_data



```c++
static void __init mm_init(void)
{
	// 不支持
	page_ext_init_flatmem();
    
	mem_init();
	kmem_cache_init();
	percpu_init_late();
	pgtable_init();
	vmalloc_init();
	ioremap_huge_init();
	kaiser_init();
}
```

__attribute__((optimize("O0")));



```c++
void __init mem_init(void)
{

	set_max_mapnr(pfn_to_page(max_pfn) - mem_map);

	// 没干啥,对于不连续或者稀疏内存可能需要处理
	free_unused_memmap();
    
    // 释放所有的低端内存到 buddy 中，属于核心函数
	free_all_bootmem();


	free_highpages();

	mem_init_print_info(NULL);


	if (PAGE_SIZE >= 16384 && get_num_physpages() <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = OVERCOMMIT_ALWAYS;
	}
}
```



### free_all_bootmem

mm/nobootmem.c 。

```c++
unsigned long __init free_all_bootmem(void)
{
	unsigned long pages;
	// 初始化所有 zone 的 managed_pages 成员
	reset_all_zones_managed_pages();
	
    
	pages = free_low_memory_core_early();
	totalram_pages += pages;

	return pages;
}
```



```c++
static unsigned long __init free_low_memory_core_early(void)
{
	unsigned long count = 0;
	phys_addr_t start, end;
	u64 i;

	memblock_clear_hotplug(0, -1);

    // 标记所有的 reserved 内存
    // 这部分内存不会释放到 buddy 中去，对于系统来说，这部分内存已经消失了
    //所有的 reserved region：
    /*
    {base = 0x10004000, size = 0x4000, flags = 0x0}
    {base = 0x10100000, size = 0x102a348, flags = 0x0}
    {base = 0x18000000, size = 0x17456, flags = 0x0}
    {base = 0x3f30d000, size = 0xd2000, flags = 0x0}
    {base = 0x3f3e1918, size = 0xc1a6e8, flags = 0x0}
    {base = 0x3fffc880, size = 0x3c, flags = 0x0}
    {base = 0x3fffc8c0, size = 0x3c, flags = 0x0}
    {base = 0x3fffc900, size = 0x78, flags = 0x0}
    {base = 0x3fffc980, size = 0x4, flags = 0x0}
    {base = 0x3fffc9c0, size = 0x4, flags = 0x0}
    {base = 0x3fffca00, size = 0x4, flags = 0x0}
    {base = 0x3fffca40, size = 0x4, flags = 0x0}
    {base = 0x3fffca80, size = 0x20, flags = 0x0}
    {base = 0x3fffcac0, size = 0x20, flags = 0x0}
    {base = 0x3fffcb00, size = 0x20, flags = 0x0}
    {base = 0x3fffcb28, size = 0x1b, flags = 0x0}
    {base = 0x3fffcb44, size = 0x1b, flags = 0x0}
    {base = 0x3fffcb60, size = 0x7b, flags = 0x0}
    {base = 0x3fffcbdc, size = 0x1b, flags = 0x0}
    {base = 0x3fffcbf8, size = 0x1b, flags = 0x0}
    {base = 0x3fffcc14, size = 0x1b, flags = 0x0}
    {base = 0x3fffcc30, size = 0x1b, flags = 0x0}
    {base = 0x3fffcc4c, size = 0x1b, flags = 0x0}
    {base = 0x3fffcc68, size = 0x1b, flags = 0x0}
    {base = 0x3fffcc84, size = 0x1b, flags = 0x0}
    {base = 0x3fffcca0, size = 0xa9, flags = 0x0}
    {base = 0x3fffcd4c, size = 0x19, flags = 0x0}
    {base = 0x3fffcd68, size = 0x19, flags = 0x0}
    {base = 0x3fffcd84, size = 0x19, flags = 0x0}
    {base = 0x3fffcda0, size = 0x1d, flags = 0x0}
    {base = 0x3fffcdc0, size = 0x3d, flags = 0x0}
    {base = 0x3fffce00, size = 0x25, flags = 0x0}
    {base = 0x3fffce28, size = 0x24, flags = 0x0}
    {base = 0x3fffce58, size = 0x31a8, flags = 0x0}
    {base = 0x5c000000, size = 0x14000000, flags = 0x0}
    */
	for_each_reserved_mem_region(i, &start, &end)
		reserve_bootmem_region(start, end);
	// 针对每个 range，释放内存，返回值是释放的内存数量，以页为单位。 
    // 不包括 reserved 部分，但是整个内存块被 reserved 部分切分了。 
    // 所有的 memory 内存：
    /*
    	0x10000000,0x10004000
    	0x10008000,0x10100000
    	0x1112a348,0x18000000
    	0x18017456,0x3f30d000
    	0x3f3df000,0x3f3e1918
    	0x3fffc000，0x3fffc880
    	0x3fffc8bc，0x3fffc8c0
    	0x3fffc8fc，0x3fffc900
    	...
    */
    
	for_each_free_mem_range(i, NUMA_NO_NODE, MEMBLOCK_NONE, &start, &end,
				NULL)
		count += __free_memory_core(start, end);

	return count;
}
```









```c++
void __meminit reserve_bootmem_region(phys_addr_t start, phys_addr_t end)
{
	unsigned long start_pfn = PFN_DOWN(start);
	unsigned long end_pfn = PFN_UP(end);

	for (; start_pfn < end_pfn; start_pfn++) {
		if (pfn_valid(start_pfn)) {
			struct page *page = pfn_to_page(start_pfn);
			// 不干啥
			init_reserved_page(start_pfn);

			INIT_LIST_HEAD(&page->lru);
			// 设置 reserved 标志，具体的定义在 include/linux/page-flags.h 中
            // PAGEFLAG(Reserved, reserved, PF_NO_COMPOUND)
			// __CLEARPAGEFLAG(Reserved, reserved, PF_NO_COMPOUND)
            // #define SETPAGEFLAG(uname,lname,policy) static __always_inline void SetPage ## uname(struct page *page) { set_bit(PG_ ## lname, &policy(page, 1)->flags); }
            // 也就是设置 page->PG_reserved ，bit10
			SetPageReserved(page);
		}
	}
}
```

