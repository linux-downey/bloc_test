在运行内核代码之前,系统环境为:

MMU = off, D-cache = off, I-cache = dont care, r0 = 0,

r1 = machine nr, r2 = dtb pointer.

```assembly
#ifdef CONFIG_ARM_VIRT_EXT
	bl	__hyp_stub_install                    // 不分析
#endif
	@ ensure svc mode and all interrupts masked
	safe_svcmode_maskall r9                   // 确定当前模式,屏蔽各类中断

	mrc	p15, 0, r9, c0, c0		              // MIDR 寄存器,读取 CPU ID
	bl	__lookup_processor_type		  //执行完成之后,r5=procinfo r9=cpuid
	movs	r10, r5				@ invalid processor (r5=0)?
 THUMB( it	eq )		@ force fixup-able long branch encoding
	beq	__error_p			@ yes, error 'p'


#ifndef CONFIG_XIP_KERNEL    // 如果不是 xip 类型的 kernel
	// 2f 位置放的是 .long .(当前虚拟地址)/.long PAGE_OFFSET,两个四字节
	// PAGE_OFFSET 代表的是用户空间和内核空间的虚拟地址划分,默认 linux 中,PAGE_OFFSET 为 0xc0000000
	adr	r3, 2f               
	ldmia	r3, {r4, r8}    // r4 是虚拟地址 r8 是 PAGE_OFFSET
	sub	r4, r3, r4			// 获取实际运行物理地址和虚拟地址的偏移量
	add	r8, r8, r4			// PAGE_OFFSET加上偏移量,所以是实际的物理地址+0xc0000000
#else
	...
#endif

	/*
	 * r1 = machine no, r2 = atags or dtb,
	 * r8 = phys_offset, r9 = cpuid, r10 = procinfo
	 */
	bl	__vet_atags                      // 检查 r2 传入的 dtb/atags 是否有效
#ifdef CONFIG_SMP_ON_UP
	bl	__fixup_smp                      //如果运行SMP内核在单处理器系统中启动，做适当调整
#endif
#ifdef CONFIG_ARM_PATCH_PHYS_VIRT
	bl	__fixup_pv_table            // 根据内核在内存中的位置修正物理地址与虚拟地址的转换机制
#endif
	bl	__create_page_tables
```



```assembly
__hyp_stub_install:
ENTRY(__hyp_stub_install)
	store_primary_cpu_mode	r4, r5, r6
ENDPROC(__hyp_stub_install)

.macro	store_primary_cpu_mode	reg1, reg2, reg3
    mrs	\reg1, cpsr                         //cpsr 寄存器放到 r4 中
    and	\reg1, \reg1, #MODE_MASK            // r4 & 0x1f,只保留模式位
    adr	\reg2, .L__boot_cpu_mode_offset     // r5 保存 __boot_cpu_mode_offset 的地址
    ldr	\reg3, [\reg2]                      // r6 保存 r5 地址上的值
    str	\reg1, [\reg2, \reg3]               // 将 r4 保存到 r5-r6 的位置上
.endm
```

```assembly
safe_svcmode_maskall r9:
.macro safe_svcmode_maskall reg:req
#if __LINUX_ARM_ARCH__ >= 6 && !defined(CONFIG_CPU_V7M)
	mrs	\reg , cpsr                         // cpsr 保存到 r9
	eor	\reg, \reg, #HYP_MODE               // r9 ^ 0x1a,保存到 r9
	tst	\reg, #MODE_MASK                    // tst 执行位与操作,如果是 hyp mode,r9 为 0.
	bic	\reg , \reg , #MODE_MASK            // 清除 r9 的所有 mode bits
	orr	\reg , \reg , #PSR_I_BIT | PSR_F_BIT | SVC_MODE  //设置 I/F bit,设置模式为 svc
THUMB(	orr	\reg , \reg , #PSR_T_BIT	)
	bne	1f                                  // 如果不是 hyp 模式,跳转到标号 1 处
	orr	\reg, \reg, #PSR_A_BIT              // 如果是 hyp 模式,执行其它操作,暂不讨论
	badr	lr, 2f
	msr	spsr_cxsf, \reg
	__MSR_ELR_HYP(14)
	__ERET
1:	msr	cpsr_c, \reg        // cpsr_c 表示低 8 位,更新 cpsr 的低八位,也会切换到 svc 模式
2:
#else
	// armv6 之前的,或者 v7m,直接关中断,设置 svc 模式
	setmode	PSR_F_BIT | PSR_I_BIT | SVC_MODE, \reg  
#endif
.endm
```

```assembly
此时 r9 = cpu_id
__lookup_processor_type:
	__lookup_processor_type:
	adr	r3, __lookup_processor_type_data  // r3 赋值为地址,adr 指令是根据当前 pc 的偏移值
	ldmia	r3, {r4 - r6}     //r4=procinfo 的虚拟地址,r5=procinfo start,r6=procinfo end
	//获取 procinfo 实际地址与虚拟地址之间的偏移值,r3 表示实际的运行物理地址,r4 是编译的虚拟地址(.这个地址定位符是在链接过程中解析的,而 adr 是根据当前运行的 pc 的偏移值)
	sub	r3, r3, r4			  
	add	r5, r5, r3			  //偏移值加上虚拟地址,才是真正的 procinfo 所在地址,保存到 r5
	add	r6, r6, r3			  //r5 是起始地址,r6 是结束地址
1:	ldmia	r5, {r3, r4}			@ value, mask
	and	r4, r4, r9			@ mask wanted bits
	teq	r3, r4
	beq	2f
	add	r5, r5, #PROC_INFO_SZ		@ sizeof(proc_info_list)
	cmp	r5, r6
	blo	1b
	mov	r5, #0				@ unknown processor     // 返回 0 表示无效的 processor
2:	ret	lr
ENDPROC(__lookup_processor_type)

```

```assembly
// 第一个字节是当前地址,第二个字节是 procinfo 开始地址,第三字节是 procinfo 结束地址.
// __proc_info_begin 和 __proc_info_end 在链接脚本中被定义,procinfo 被定义在 .proc.info.init 段中,通常是静态定义的.
__lookup_processor_type_data:
	.long	.
	.long	__proc_info_begin
	.long	__proc_info_end
	.size	__lookup_processor_type_data, . - __lookup_processor_type_data
```

procinfo 定义在 armv7.S 中,结构体如下: 

```c++
struct proc_info_list {
	unsigned int		cpu_val;
	unsigned int		cpu_mask;
	unsigned long		__cpu_mm_mmu_flags;	/* used by head.S */
	unsigned long		__cpu_io_mmu_flags;	/* used by head.S */
	unsigned long		__cpu_flush;		/* used by head.S */
	const char		*arch_name;
	const char		*elf_name;
	unsigned int		elf_hwcap;
	const char		*cpu_name;
	struct processor	*proc;
	struct cpu_tlb_fns	*tlb;
	struct cpu_user_fns	*user;
	struct cpu_cache_fns	*cache;
};
```



```assembly
__vet_atags:
	tst	r2, #0x3			@ aligned?
	bne	1f                          // 如果没有 4 字节对齐,将 r2 赋值为 0,返回

	ldr	r5, [r2, #0]               // r2 中的第一个字取出到 r5
#ifdef CONFIG_OF_FLATTREE
	ldr	r6, =OF_DT_MAGIC		    // 判断 r2 中的值是不是 dtb 的 magic value
	cmp	r5, r6                      
	beq	2f                          // 确实是 dtb 文件
#endif
	// 只分析 dtb 部分,这部分暂不分析.
	cmp	r5, #ATAG_CORE_SIZE		    // 对比是不是 ATAG_CORE_SIZE
	cmpne	r5, #ATAG_CORE_SIZE_EMPTY
	bne	1f
	ldr	r5, [r2, #4]
	ldr	r6, =ATAG_CORE
	cmp	r5, r6
	bne	1f

2:	ret	lr				@ atag/dtb pointer is ok

1:	mov	r2, #0
	ret	lr
ENDPROC(__vet_atags)
```

## 重点部分,创建页表

r8 - 物理偏移地址(0xc0000000+offset), r9- cpuid, r10 - procinfo

```assembly
__create_page_tables:
	
	//.macro	pgtbl, rd, phys
	//add	\rd, \phys, #TEXT_OFFSET        r8 加上 TEXT_OFFSET 赋值给 r4,TEXT_OFFSET 在 makefile 中被定义,值为 0x00008000(32k),也正是 .head.text,.text 开始的地方(虚拟地址)
	//sub	\rd, \rd, #PG_DIR_SIZE          r4 再减去 PG_DIR_SIZE(16K)
	//.endm
	
	pgtbl	r4, r8				// r4 是程序的物理地址,已经向页面对齐了.

	mov	r0, r4                  // r4 -> r0
	mov	r3, #0                  // r3 = 0
	add	r6, r0, #PG_DIR_SIZE    // r0 再减去 0x4000
1:	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4           // r0 地址处的值清零 
	teq	r0, r6                 // 清零整个 16K 的地址空间
	bne	1b
/*************************************************************/
#ifdef CONFIG_ARM_LPAE         // 地址扩展部分,不管
	/*
	 * Build the PGD table (first level) to point to the PMD table. A PGD
	 * entry is 64-bit wide.
	 */
	mov	r0, r4
	add	r3, r4, #0x1000			@ first PMD table address
	orr	r3, r3, #3			@ PGD block type
	mov	r6, #4				@ PTRS_PER_PGD
	mov	r7, #1 << (55 - 32)		@ L_PGD_SWAPPER
1:
#ifdef CONFIG_CPU_ENDIAN_BE8
	str	r7, [r0], #4			@ set top PGD entry bits
	str	r3, [r0], #4			@ set bottom PGD entry bits
#else
	str	r3, [r0], #4			@ set bottom PGD entry bits
	str	r7, [r0], #4			@ set top PGD entry bits
#endif
	add	r3, r3, #0x1000			@ next PMD table
	subs	r6, r6, #1
	bne	1b

	add	r4, r4, #0x1000			@ point to the PMD tables
#ifdef CONFIG_CPU_ENDIAN_BE8
	add	r4, r4, #4			@ we only write the bottom word
#endif
#endif
/*************************************************************/

	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS] // mm_mmuflags 存到

	// 大名鼎鼎的 identity mapping 部分.
	// 这部分会被 paging_init 移除
	
	//该地址上三个部分,虚拟地址/__turn_mmu_on地址/__turn_mmu_on_end地址
	adr	r0, __turn_mmu_on_loc 
    //r3 - 虚拟地址,r5 - __turn_mmu_on地址,r6 - __turn_mmu_on_end地址
	ldmia	r0, {r3, r5, r6}  
	sub	r0, r0, r3			// 获取偏移
	add	r5, r5, r0			// 校正偏移
	add	r6, r6, r0			// 校正偏移
	mov	r5, r5, lsr #SECTION_SHIFT   // 
	mov	r6, r6, lsr #SECTION_SHIFT   // 

1:	orr	r3, r7, r5, lsl #SECTION_SHIFT	@ flags + kernel base
	str	r3, [r4, r5, lsl #PMD_ORDER]	@ identity mapping
	cmp	r5, r6
	addlo	r5, r5, #1			@ next section
	blo	1b

	/*
	 * Map our RAM from the start to the end of the kernel .bss section.
	 */
	add	r0, r4, #PAGE_OFFSET >> (SECTION_SHIFT - PMD_ORDER)
	ldr	r6, =(_end - 1)
	orr	r3, r8, r7
	add	r6, r4, r6, lsr #(SECTION_SHIFT - PMD_ORDER)
1:	str	r3, [r0], #1 << PMD_ORDER
	add	r3, r3, #1 << SECTION_SHIFT
	cmp	r0, r6
	bls	1b

#ifdef CONFIG_XIP_KERNEL
	/*
	 * Map the kernel image separately as it is not located in RAM.
	 */
#define XIP_START XIP_VIRT_ADDR(CONFIG_XIP_PHYS_ADDR)
	mov	r3, pc
	mov	r3, r3, lsr #SECTION_SHIFT
	orr	r3, r7, r3, lsl #SECTION_SHIFT
	add	r0, r4,  #(XIP_START & 0xff000000) >> (SECTION_SHIFT - PMD_ORDER)
	str	r3, [r0, #((XIP_START & 0x00f00000) >> SECTION_SHIFT) << PMD_ORDER]!
	ldr	r6, =(_edata_loc - 1)
	add	r0, r0, #1 << PMD_ORDER
	add	r6, r4, r6, lsr #(SECTION_SHIFT - PMD_ORDER)
1:	cmp	r0, r6
	add	r3, r3, #1 << SECTION_SHIFT
	strls	r3, [r0], #1 << PMD_ORDER
	bls	1b
#endif

	/*
	 * Then map boot params address in r2 if specified.
	 * We map 2 sections in case the ATAGs/DTB crosses a section boundary.
	 */
	mov	r0, r2, lsr #SECTION_SHIFT
	movs	r0, r0, lsl #SECTION_SHIFT
	subne	r3, r0, r8
	addne	r3, r3, #PAGE_OFFSET
	addne	r3, r4, r3, lsr #(SECTION_SHIFT - PMD_ORDER)
	orrne	r6, r7, r0
	strne	r6, [r3], #1 << PMD_ORDER
	addne	r6, r6, #1 << SECTION_SHIFT
	strne	r6, [r3]

#if defined(CONFIG_ARM_LPAE) && defined(CONFIG_CPU_ENDIAN_BE8)
	sub	r4, r4, #4			@ Fixup page table pointer
						@ for 64-bit descriptors
#endif

#ifdef CONFIG_DEBUG_LL
#if !defined(CONFIG_DEBUG_ICEDCC) && !defined(CONFIG_DEBUG_SEMIHOSTING)
	/*
	 * Map in IO space for serial debugging.
	 * This allows debug messages to be output
	 * via a serial console before paging_init.
	 */
	addruart r7, r3, r0

	mov	r3, r3, lsr #SECTION_SHIFT
	mov	r3, r3, lsl #PMD_ORDER

	add	r0, r4, r3
	mov	r3, r7, lsr #SECTION_SHIFT
	ldr	r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
	orr	r3, r7, r3, lsl #SECTION_SHIFT
#ifdef CONFIG_ARM_LPAE
	mov	r7, #1 << (54 - 32)		@ XN
#ifdef CONFIG_CPU_ENDIAN_BE8
	str	r7, [r0], #4
	str	r3, [r0], #4
#else
	str	r3, [r0], #4
	str	r7, [r0], #4
#endif
#else
	orr	r3, r3, #PMD_SECT_XN
	str	r3, [r0], #4
#endif

#else /* CONFIG_DEBUG_ICEDCC || CONFIG_DEBUG_SEMIHOSTING */
	/* we don't need any serial debugging mappings */
	ldr	r7, [r10, #PROCINFO_IO_MMUFLAGS] @ io_mmuflags
#endif

#if defined(CONFIG_ARCH_NETWINDER) || defined(CONFIG_ARCH_CATS)
	/*
	 * If we're using the NetWinder or CATS, we also need to map
	 * in the 16550-type serial port for the debug messages
	 */
	add	r0, r4, #0xff000000 >> (SECTION_SHIFT - PMD_ORDER)
	orr	r3, r7, #0x7c000000
	str	r3, [r0]
#endif
#ifdef CONFIG_ARCH_RPC
	/*
	 * Map in screen at 0x02000000 & SCREEN2_BASE
	 * Similar reasons here - for debug.  This is
	 * only for Acorn RiscPC architectures.
	 */
	add	r0, r4, #0x02000000 >> (SECTION_SHIFT - PMD_ORDER)
	orr	r3, r7, #0x02000000
	str	r3, [r0]
	add	r0, r4, #0xd8000000 >> (SECTION_SHIFT - PMD_ORDER)
	str	r3, [r0]
#endif
#endif
#ifdef CONFIG_ARM_LPAE
	sub	r4, r4, #0x1000		@ point to the PGD table
#endif
	ret	lr
ENDPROC(__create_page_tables)
```

