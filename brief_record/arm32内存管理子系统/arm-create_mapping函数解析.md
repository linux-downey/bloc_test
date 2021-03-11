# arm-create_mapping 函数解析



重要的两个接口：

pgd = pgd_offset(mm, addr)：通过虚拟地址 addr 获取 pgd 的位置，这个 pgd 可能是 section 映射，也可能是 page table 映射。

每个 pgd 占用 8 个字节。 

\#define pte_offset_kernel(pmd,addr)： 通过 pmd 和 addr 获取 pte 的地址。 



arm 页表结构的博客文章：https://blog.csdn.net/geshifei/article/details/89574508



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





### pte 地址的计算：

```c++
#define pte_offset_kernel(pmd,addr)	(pmd_page_vaddr(*(pmd)) + pte_index(addr))

static inline pte_t *pmd_page_vaddr(pmd_t pmd)
{
	return __va(pmd_val(pmd) & PHYS_MASK & (s32)PAGE_MASK);
}

#define pte_index(addr)		(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
```

示例：pmd = 0x80007ff8，*pmd = 0x1fffd861，pte = 0x8fffd7c0

计算过程：

PHYS_MASK = 0xffffffff

PAGE_MASK =  ~((1<<12)-1)

pmd_val(pmd) =  pmd

 PAGE_SHIFT = 12

PTRS_PER_PTE = 0x200

因此，pte_offset_kernel(0x80007ff8，0xffff0000) 的值为：

__va( *0x80007ff8  & 0xffffffff & 0xfffff000) + (0xffff0000 >> 12 & 0x1ff )

= __va( 0x1fffd861& 0xfffff000 ) + 1f0

= __va( 0x1fffd000 ) + 1f0

= (pte_t*)0x8fffd000 + 1f0        

= 0x8fffd7c0     // 指针 +1 相当于 + sizeof(int*)



### pgd 地址的获取

```c++
typedef pmdval_t pgd_t[2];
#define pgd_offset_k(addr)	pgd_offset(&init_mm, addr)
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)
```

通过进程的 mm 和 addr 获取对应的 pgd。

对于内核而言，对应的 mm 是 init_mm，init_mm->pgd 的值为 0x80004000

pgd_index 就是 addr >> 21 ，比如 0x80000000 >> 21 = 0x400

鉴于 pgd_t 类型为 typedef pmdval_t pgd_t[2];，也就是 2 成员数组类型，pgd_t* 为2成员数组指针，占用 8 字节，因此 + 0x400 相当于 + 0x400 * 8 = 0x2000。

(pgd_t*)0x80004000 + 0x400 = 0x80006000



### pmd 的申请

```c++
if (pmd_none(*pmd)) {
		pte_t *pte = alloc(PTE_HWTABLE_OFF + PTE_HWTABLE_SIZE);
		__pmd_populate(pmd, __pa(pte), prot);
	}
```

在 arm32 中，pgd 和 pmd 是一样的属于 level1 的页表项，mmu 通过 level1 的页表项找到 level2 的页表项，一个 l1 对应多个 l2 页表项，多个 l2 页表项需要连续的内存地址存放，也就需要申请页面。 

```c++
#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		1
#define PTRS_PER_PGD		2048

#define PTE_HWTABLE_PTRS	(PTRS_PER_PTE)
#define PTE_HWTABLE_OFF		(PTE_HWTABLE_PTRS * sizeof(pte_t))
#define PTE_HWTABLE_SIZE	(PTRS_PER_PTE * sizeof(u32))
```





## 设置 pte 入口项的函数

函数被定义在 proc info 中，struct proc_list  list->processor->set_pte_ext

对应的函数实现为  cpu_v7_set_pte_ext，在 arch/arm/mm/proc-v7-2level.S 中。 



```assembly
/*
 *	cpu_v7_set_pte_ext(ptep, pte)
 *
 *	Set a level 2 translation table entry.
 *
 *	- ptep  - pointer to level 2 translation table entry
 *		  (hardware version is stored at +2048 bytes)
 *	- pte   - PTE value to store
 *	- ext	- value for extended PTE bits
 */
ENTRY(cpu_v7_set_pte_ext)
// 示例1：r0 = 0xefffd7c0
//       r1 = 0x3fffe1cf
//       r2 = 0
#ifdef CONFIG_MMU
	str	r1, [r0]			     // 将 r1 保存到 r0 位置，也就是把 entry 值写入对应位置

	bic	r3, r1, #0x000003f0      // 将 r1 的 bit12-bit4 清零，r3 = 0x3fffe00f
	bic	r3, r3, #PTE_TYPE_MASK   // ,r3 = 0x3fffe00c,PTE_TYPE_MASK = 3,表示 pte 的类型，决定是 4K 映射还是 64K 还是 1M.
	orr	r3, r3, r2                // r3 = 0x3fffe00c,设置最后一位 ng bit
	orr	r3, r3, #PTE_EXT_AP0 | 2  // r3 = 0x3fffe01e,access pointer 控制

	tst	r1, #1 << 4               // 测试 r1 的 bit4 是不是为 1,r1 = 0x3fffe1cf
	orrne	r3, r3, #PTE_EXT_TEX(1)  // r3 = 0x3fffe01e

	eor	r1, r1, #L_PTE_DIRTY      // r1 = 0x3fffe18f
	tst	r1, #L_PTE_RDONLY | L_PTE_DIRTY   
	orrne	r3, r3, #PTE_EXT_APX    //被执行，r3 = 0x3fffe21e

	tst	r1, #L_PTE_USER             // 
	orrne	r3, r3, #PTE_EXT_AP1    // 被执行，r3 = 0x3fffe23e

	tst	r1, #L_PTE_XN               // 
	orrne	r3, r3, #PTE_EXT_XN     // 被执行，r3 = 0x3fffe23e

	tst	r1, #L_PTE_YOUNG           
	tstne	r1, #L_PTE_VALID        // 被执行，r1 =0x3fffe18f
	eorne	r1, r1, #L_PTE_NONE     // 被执行，r1 = 0x3fffe98f
	tstne	r1, #L_PTE_NONE         // 被执行，r1 = 0x3fffe98f
	moveq	r3, #0                  // r3 = 0x3fffe23e

 ARM(	str	r3, [r0, #2048]! )      // 把 r3 存到 0xefffdfc0(r0+2048)
 THUMB(	add	r0, r0, #2048 )
 THUMB(	str	r3, [r0] )
	ALT_SMP(W(nop))
	ALT_UP (mcr	p15, 0, r0, c7, c10, 1)		@ flush_pte
#endif
	bx	lr
ENDPROC(cpu_v7_set_pte_ext)
```

set_pte_ext 支持三个参数：

1、pte ，也就是 entry 需要存放的地址

2、pte value，也就是 pte 对应的值

3、ng flag，和缓存策略相关的参数。

后续再来详细研究这个页表结构。