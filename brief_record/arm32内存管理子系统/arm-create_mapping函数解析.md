# arm-create_mapping 函数解析



重要的两个接口：

pgd = pgd_offset(mm, addr)：通过虚拟地址 addr 获取 pgd 的位置，这个 pgd 可能是 section 映射，也可能是 page table 映射。

\#define pte_offset_kernel(pmd,addr)： 通过 pmd 和 addr 获取 pte 的地址。 



## struct map_desc

```c
struct map_desc {
	unsigned long virtual;        // 虚拟地址
	unsigned long pfn;            // 物理页帧号
	unsigned long length;         // 地址长度
	unsigned int type;            // 内存类型
};
```

```c

static void __init create_mapping(struct map_desc *md)
{
    // 判断是不是 user region，是则返回
    // 判断是不是超出 vmalloc 范围，是则警告，继续工作。
    __create_mapping(&init_mm, md, early_alloc, false);
}
```

__create_mapping 需要申请内存，对应的申请函数为 early_alloc，因为这个时候伙伴系统还没有完成初始化，因此还是得使用 memblock，而且申请的内存都是低端内存，高端内存还没准备好。 

同时，这个时候的 mapping 还没有完全建立完成，因此要注意虚拟地址的访问是否合法。 

```c
memblock.bottom_up 为 true 表示从低地址向高地址分配，而 false 则反之。 
static void __init *early_alloc_aligned(unsigned long sz, unsigned long align)
{
    // 申请了之后需要将申请到的物理地址转换为虚拟地址
	void *ptr = __va(memblock_alloc(sz, align));  
    // 整个页面会被清零
	memset(ptr, 0, sz);
	return ptr;
}

static void __init *early_alloc(unsigned long sz)
{
	return early_alloc_aligned(sz, sz);
}
```

alloc 函数暂时不分析，主要工作就是在 memblock.memory.regions 中找到一片物理内存，然后将这片内存加入到 reserved 链表中，再返回。 



```c++
static void __init __create_mapping(struct mm_struct *mm, struct map_desc *md,
				    void *(*alloc)(unsigned long sz),
				    bool ng)
{
	unsigned long addr, length, end;
	phys_addr_t phys;
	const struct mem_type *type;
	pgd_t *pgd;

	type = &mem_types[md->type];

	addr = md->virtual & PAGE_MASK;
	phys = __pfn_to_phys(md->pfn);
	length = PAGE_ALIGN(md->length + (md->virtual & ~PAGE_MASK));

	if (type->prot_l1 == 0 && ((addr | phys | length) & ~SECTION_MASK)) {
		pr_warn("BUG: map for 0x%08llx at 0x%08lx can not be mapped using pages, ignoring.\n",
			(long long)__pfn_to_phys(md->pfn), addr);
		return;
	}
	// init_mm->pgd 的值为 0x80004000
    // 这里计算的是相对 addr 的偏移值，如果这个值为 0x80000000，就是 0x80006000
	pgd = pgd_offset(mm, addr);
	end = addr + length;
	do {
		unsigned long next = pgd_addr_end(addr, end);

		alloc_init_pud(pgd, addr, next, phys, type, alloc, ng);

		phys += next - addr;
		addr = next;
	} while (pgd++, addr != end);
}
```

如果对应的 mem 设置了 type->prot_sect，说明这段内存可以映射为 section，同时，如果需要映射的内存向 1M 的边界对齐的话，就会做 section 映射，在 map_lowmem 中，所有内存都被映射为 section。  

mapping section 比较简单，因为只有一层，直接在 0x80006000 的基础上修改值就行了。



pte 地址的计算：

```c++
#define pte_offset_kernel(pmd,addr)	(pmd_page_vaddr(*(pmd)) + pte_index(addr))

```















