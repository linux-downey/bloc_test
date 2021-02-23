使用 qemu 调试,默认情况下会将镜像加载到 0x10000000,在开启 mmu 之前,就需要先将 .head.text 和 .text 符号表重定向到 0x10008000,相应的内容也在对应的位置上. 



在运行内核代码之前,系统环境为:

MMU = off, D-cache = off, I-cache = dont care, r0 = 0,

r1 = machine nr, r2 = dtb pointer.

```assembly
#ifdef CONFIG_ARM_VIRT_EXT
	bl	__hyp_stub_install                    // 不分析
#endif
	@ ensure svc mode and all interrupts masked
	safe_svcmode_maskall r9                   // 确定当前模式,屏蔽各类中断
	
	// MIDR 寄存器,读取 CPU ID
	// imx6ull - 0x410fc090,没问题.
	mrc	p15, 0, r9, c0, c0		              
	
	// proc info 被静态地定义在 arm/mm/proc-v7.S 中,包括 cortex-a5/cortex-a9/cortex-a8/cortex-a7 的内容
	// __proc_info_begin 为 0x80a88900，__proc_info_end 为 0x80a88b3c
	// 执行完成之后,r5=procinfo,也就是 cortex-a9 对应的信息 r9=cpuid，还是 0x410fc090
	bl	__lookup_processor_type		  
	movs	r10, r5				@ invalid processor (r5=0)?
	beq	__error_p			@ yes, error 'p'


#ifndef CONFIG_XIP_KERNEL    // 如果不是 xip 类型的 kernel
	// 2f 位置放的是 .long .(当前虚拟地址)/.long PAGE_OFFSET,两个四字节
	// PAGE_OFFSET 代表的是用户空间和内核空间的虚拟地址划分,默认 linux 中,PAGE_OFFSET 为 0xc0000000
	adr	r3, 2f               
	ldmia	r3, {r4, r8}    // r4 是虚拟地址 r8 是 PAGE_OFFSET
	sub	r4, r3, r4			// 获取实际运行物理地址和虚拟地址的偏移量
	// PAGE_OFFSET加上偏移量,所以是实际的物理地址+0xc0000000,在当前板子上是 0x80000000
	// r8 的寄存器值为 0x10000000（计算得到的实际加载的物理地址）
	add	r8, r8, r4			
#else
	...
#endif

	/*
	 * r1 = machine no, r2 = atags or dtb,
	 * r8 = phys_offset, r9 = cpuid, r10 = procinfo
	 */
	bl	__vet_atags                      // 检查 r2 传入的 dtb/atags 是否有效,也就是检查 magic
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
	// pgtbl 这个宏用于获取启动页表物理地址
	//.macro	pgtbl, rd, phys
	// TEXT_OFFSET 一共 32 K，0x8000，PG_DIR_SIZE 的值为 0x4000，16K，也就是说 r4 最终的值为 0x10004000，r8 还是保持原来的值 0x10000000
	// 同时定义了 swapper_pg_dir 这个变量，对应的值就是虚拟地址对应的页表地址，0x80004000(通常为 0xc0004000)
	//add	\rd, \phys, #TEXT_OFFSET        r8 加上 TEXT_OFFSET 赋值给 r4,TEXT_OFFSET 在 makefile 中被定义,值为 0x00008000(32k),也正是 .head.text,.text 开始的地方(虚拟地址)
	//sub	\rd, \rd, #PG_DIR_SIZE          r4 再减去 PG_DIR_SIZE(16K)
	//.endm
	
	pgtbl	r4, r8				// r4 是程序的物理地址,已经向页面对齐了.

	// 因为要存放页表，所以需要将这部分内存全部清零
	// r4 的值是页表地址
	mov	r0, r4                  // r4 -> r0
	mov	r3, #0                  // r3 = 0
	add	r6, r0, #PG_DIR_SIZE    // r0 再减去 0x4000
1:	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4
	str	r3, [r0], #4           // r0 地址处的值清零 
	teq	r0, r6                 // 清零整个 16K 的地址空间
	bne	1b

	// 静态定义的 mm_mmuflags 值为 PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_AP_READ | PMD_SECT_AF | PMD_FLAGS_SMP,对应 cortex-a9 为 0xc0e
	ldr	r7, [r10, #PROCINFO_MM_MMUFLAGS] // mm_mmuflags 存到

	// 大名鼎鼎的 identity mapping 部分.
	// 这部分会被 paging_init 移除
	// identity mapping 建立的映射是物理地址和虚拟地址相同的，这部分代码并不多，在这里只有 32 字节，这部分代码的中间部分包括开启 mmu，因此在开启 mmu 前后分别使用的是物理地址和虚拟地址，这种情况下开启 mmu 之后的代码才能更方便地映射到对应的物理地址上。
	
	//该地址上三个部分,虚拟地址/__turn_mmu_on地址/__turn_mmu_on_end地址
	// r0 的值为 0x10008138，保存的 __turn_mmu_on 和 __turn_mmu_on_end 的地址分别为 0x80100000，0x80100020，也就是 .text 段的起始位置，32个字节。
	adr	r0, __turn_mmu_on_loc 
    //r3 - 虚拟地址,r5 - __turn_mmu_on地址,r6 - __turn_mmu_on_end地址
    // pgtable 文件使用的是 /arch/arm/include/asm/pgtable-2level.h 而不是 /arch/arm/include/asm/pgtable-3level.h
	ldmia	r0, {r3, r5, r6}  
	sub	r0, r0, r3			// 获取偏移
	add	r5, r5, r0			// 校正偏移
	add	r6, r6, r0			// 校正偏移
	// r5 和 r6 中保存的都是 __turn_mmu_on/ __turn_mmu_on_end 对应的物理地址 0x10100000/0x10100020
	// SECTION_SHIFT 的值为 20
	// 在执行完下面两条指令之后，r5 和 r6 的结果都变成了 0x101
	mov	r5, r5, lsr #SECTION_SHIFT   
	mov	r6, r6, lsr #SECTION_SHIFT   

	// 这里没有循环，页表基地址是 0x10004000，0x10004404 对应那一页的物理地址？再分析。
1:	orr	r3, r7, r5, lsl #SECTION_SHIFT	@ flags + kernel base   // r3 = 0x10100c0e
	str	r3, [r4, r5, lsl #PMD_ORDER]	@ identity mapping      // 将 0x10100c0e 保存到 0x10004404 的位置
	cmp	r5, r6
	addlo	r5, r5, #1			@ next section
	blo	1b

	/*
	 * Map our RAM from the start to the end of the kernel .bss section.
	 */
	// 最终建立的内核页表如下：
	// 地址：0x10006000 - 0x100060044
	//	0x10006000:	0x10000c0e	0x10100c0e	0x10200c0e	0x10300c0e
		0x10006010:	0x10400c0e	0x10500c0e	0x10600c0e	0x10700c0e
		0x10006020:	0x10800c0e	0x10900c0e	0x10a00c0e	0x10b00c0e
		0x10006030:	0x10c00c0e	0x10d00c0e	0x10e00c0e	0x10f00c0e
		0x10006040:	0x11000c0e	0x11100c0e
	// 这是映射的内核页表，属于物理地址
	add	r0, r4, #PAGE_OFFSET >> (SECTION_SHIFT - PMD_ORDER) // r0 的值为 0x10006000
	ldr	r6, =(_end - 1)         // r6 的值 0x8112a347，内核 image 的 .bss 段的结尾处.
	orr	r3, r8, r7
	add	r6, r4, r6, lsr #(SECTION_SHIFT - PMD_ORDER)
1:	str	r3, [r0], #1 << PMD_ORDER
	add	r3, r3, #1 << SECTION_SHIFT
	cmp	r0, r6
	bls	1b
	
	// 到这里就建立了两段物理地址的映射，identity mapping 和 kernel image mapping
	
	/*
	 * Then map boot params address in r2 if specified.
	 * We map 2 sections in case the ATAGs/DTB crosses a section boundary.
	 */
	// r2 是从 uboot 传递过来的物理地址，在 qemu 的调试中被放在了 0x18000000
	mov	r0, r2, lsr #SECTION_SHIFT     // r0 为 0x180
	movs	r0, r0, lsl #SECTION_SHIFT  // r0 为 0x18000000
	subne	r3, r0, r8                  
	addne	r3, r3, #PAGE_OFFSET
	addne	r3, r4, r3, lsr #(SECTION_SHIFT - PMD_ORDER)
	orrne	r6, r7, r0
	strne	r6, [r3], #1 << PMD_ORDER      // 将 0x18000c0e 存到 0x10006200
	addne	r6, r6, #1 << SECTION_SHIFT    
	strne	r6, [r3]                       // 将 0x18100c0e 存到 0x10006204
	
	ret	lr                                 
ENDPROC(__create_page_tables)
// 到这里，三段页面的 mapping 也就完成了，分别是 identity mapping、kernel imaging mapping、dtb mapping。
// TTBR0 寄存器的值和虚拟地址共同组成了 level1 desc，level1 desc 的最后两个 bit 决定是使用 4K、64K、1M 还是 16M 的映射。
// level1 desc 的组成为：TTBR0[32:14]+inputaddr[32:10]+2bits desc type，因此，第一级的页表需要 16K 的内存来保存，这个地址也就是 0x10004000 到 0x10008000。 
```



## 页表建立之后的工作

```assembly
/*
	 * The following calls CPU specific code in a position independent
	 * manner.  See arch/arm/mm/proc-*.S for details.  r10 = base of
	 * xxx_proc_info structure selected by __lookup_processor_type
	 * above.
	 *
	 * The processor init function will be called with:
	 *  r1 - machine type
	 *  r2 - boot data (atags/dt) pointer
	 *  r4 - translation table base (low word)
	 *  r5 - translation table base (high word, if LPAE)
	 *  r8 - translation table base 1 (pfn if LPAE)
	 *  r9 - cpuid
	 *  r13 - virtual address for __enable_mmu -> __turn_mmu_on
	 *
	 * On return, the CPU will be ready for the MMU to be turned on,
	 * r0 will hold the CPU control register value, r1, r2, r4, and
	 * r9 will be preserved.  r5 will also be preserved if LPAE.
	 */
	ldr	r13, =__mmap_switched		 // 使用 sp 保存 __mmap_switched 的地址
						@ mmu has been enabled
	badr	lr, 1f				 // 设置返回地址到 lr 中，返回地址为 b	__enable_mmu

	mov	r8, r4				    // 都设置为 0x10004000
	
	ldr	r12, [r10, #PROCINFO_INITFUNC]  // 
	add	r12, r12, r10            // 值为 0x10118adc
	// 执行 proc info 中的 initfunction，执行完成之后会跳转到 1f 处。
	// 对应 cortex-a9 的函数为：__v7_ca9mp_setup
	ret	r12                      
1:	b	__enable_mmu
ENDPROC(stext)
	.ltorg
#ifndef CONFIG_XIP_KERNEL
2:	.long	.
	.long	PAGE_OFFSET
```



## 处理器的初始化工作

主要的工作：初始化 TLB、Caches，同时设置 MMU 的状态以便开启 MMU,将会返回到 b	__enable_mmu

```assembly
/*
 *	__v7_setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 control register setting.
 *
 *	r1, r2, r4, r5, r9, r13 must be preserved - r13 is not a stack
 *	r4: TTBR0 (low word)
 *	r5: TTBR0 (high word if LPAE)
 *	r8: TTBR1
 *	r9: Main ID register
 *
 *	This should be able to cover all ARMv7 cores.
 *
 *	It is assumed that:
 *	- cache type register is implemented
 */
__v7_ca5mp_setup:
__v7_ca9mp_setup:
__v7_cr7mp_setup:
	mov	r10, #(1 << 0)			@ Cache/TLB ops broadcasting
	b	1f

	...
	
1:	adr	r0, __v7_setup_stack_ptr      // r0 = 0x10118c08
	ldr	r12, [r0]                     // r12 = 0x00f9a850
	add	r12, r12, r0			      // r12 = 0x110b3458
	stmia	r12, {r1-r6, lr}		  // 保存到临时的栈上，保护现场
	bl      v7_invalidate_l1          // invalidate L1 cache
	ldmia	r12, {r1-r6, lr}          // 恢复现场
#ifdef CONFIG_SMP
	orr	r10, r10, #(1 << 6)		@ Enable SMP/nAMP mode
	ALT_SMP(mrc	p15, 0, r0, c1, c0, 1)
	ALT_UP(mov	r0, r10)		@ fake it for UP
	orr	r10, r10, r0			@ Set required bits
	teq	r10, r0				@ Were they already set?
	mcrne	p15, 0, r10, c1, c0, 1		
#endif
	b	__v7_setup_cont                // 

```



##  __enable_mmu

```assembly
/*
 * Setup common bits before finally enabling the MMU.  Essentially
 * this is just loading the page table pointer and domain access
 * registers.  All these registers need to be preserved by the
 * processor setup function (or set in the case of r0)
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r2  = atags or dtb pointer
 *  r4  = TTBR pointer (low word)
 *  r5  = TTBR pointer (high word if LPAE)
 *  r9  = processor ID
 *  r13 = *virtual* address to jump to upon completion
 */
 
 // 此时 TTBCR 寄存器的值为 0x0
 // TTBCR 最高位表示 PAE 位，0 表示使用 32bits short description。
 // TTBCR 低三位为 N 位，决定了 level1/2 描述符中的对齐，0 表示按照 2^0 = 1bit 进行对齐，同时说明 TTBR1 寄存器没有用到
__enable_mmu:
	bic	r0, r0, #CR_A      
	mov	r5, #DACR_INIT    
	mcr	p15, 0, r5, c3, c0, 0		// r5 的值为 0x51
	// TTBR0 的值为 0x10004059，其中 0x10004000 为地址，0x59 为功能位,主要就是设置属性。
	// 
	mcr	p15, 0, r4, c2, c0, 0		// r4 的值为 0x10004059
	
#endif
	b	__turn_mmu_on
ENDPROC(__enable_mmu)
```



```assembly
	.align	5
	.pushsection	.idmap.text, "ax"
ENTRY(__turn_mmu_on)
	mov	r0, r0
	instr_sync
	mcr	p15, 0, r0, c1, c0, 0		
	mrc	p15, 0, r3, c0, c0, 0		// 到这里 mmu 就已经开启了
	instr_sync
	mov	r3, r3
	mov	r3, r13
	ret	r3                          // 为什么这里 pc 没有变成 0x8xx，还是 0x1xx
__turn_mmu_on_end:
ENDPROC(__turn_mmu_on)
```

## 开启 MMU 之后的工作

开启 MMU 之后，程序就跳转到了 __mmap_switched_data，这个部分的代码既不属于 .head.text 也不属于 .text 段。 

```assembly
/*
 * The following fragment of code is executed with the MMU on in MMU mode,
 * and uses absolute addresses; this is not position independent.
 *
 *  r0  = cp#15 control register
 *  r1  = machine ID
 *  r2  = atags/dtb pointer
 *  r9  = processor ID
 */
	__INIT
__mmap_switched:
	adr	r3, __mmap_switched_data
	// 在这里清除 bss 段
	
	ldmia	r3!, {r4, r5, r6, r7}
	cmp	r4, r5				@ Copy data segment if needed
1:	cmpne	r5, r6
	ldrne	fp, [r4], #4
	strne	fp, [r5], #4
	bne	1b

	mov	fp, #0				@ Clear BSS (and zero fp)
1:	cmp	r6, r7
	strcc	fp, [r6],#4
	bcc	1b

 // 最主要的工作还是设置了 sp，将 sp 设置为 0x81000000(init_thread_union)+8184(两个页面长度 -8) = 0x81001ff8.这也是 init_task 的栈空间，因此，从 start_kernel 开始，实际上就相当于在执行 init_task 进程。 
 ARM(	ldmia	r3, {r4, r5, r6, r7, sp})
 THUMB(	ldmia	r3, {r4, r5, r6, r7}	)
 THUMB(	ldr	sp, [r3, #16]		)
	str	r9, [r4]			@ Save processor ID
	str	r1, [r5]			@ Save machine type
	str	r2, [r6]			@ Save atags pointer
	cmp	r7, #0
	strne	r0, [r7]			
	b	start_kernel
ENDPROC(__mmap_switched)

	.align	2
	.type	__mmap_switched_data, %object
__mmap_switched_data:
	.long	__data_loc			
	.long	_sdata				
	.long	__bss_start			@ r6
	.long	_end				@ r7
	.long	processor_id			@ r4
	.long	__machine_arch_type		@ r5
	.long	__atags_pointer			@ r6
#ifdef CONFIG_CPU_CP15
	.long	cr_alignment			@ r7
#else
	.long	0				@ r7
#endif
	.long	init_thread_union + THREAD_START_SP @ sp
	.size	__mmap_switched_data, . - __mmap_switched_data

```

