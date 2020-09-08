串口,时钟,ddr 等初始化

gd 直接使用保存在 r9 中的指针。 

```c++
typedef struct global_data {
	bd_t *bd;                      //单板相关的信息
	unsigned long flags;           //标志位，就是传递给 board_init_f 的参数
	unsigned int baudrate;         //波特率
	unsigned long cpu_clk;		   //CPU 的时钟
	unsigned long bus_clk;         // bus 的时钟
	
	unsigned long pci_clk;         //pci 时钟
	unsigned long mem_clk;         //内存时钟


#ifdef CONFIG_BOARD_TYPES
	unsigned long board_type;      //单板类型
#endif
	unsigned long have_console;	   //串口是否已经初始化完成

#if CONFIG_IS_ENABLED(PRE_CONSOLE_BUFFER)
	unsigned long precon_buf_idx;	/* Pre-Console buffer index */
#endif
	unsigned long env_addr;		  //env 结构的地址(可能是保存到 flash 上的地址)
	unsigned long env_valid;	  //env 的校验是否有效

	unsigned long ram_top;		 // uboot 使用的高端地址
	unsigned long relocaddr;	 // uboot 在 ram 中的重定位地址
	phys_size_t ram_size;		 // ram 的大小
	unsigned long mon_len;		 /* monitor len */
	unsigned long irq_sp;		 
	unsigned long start_addr_sp;	//栈指针开始地址
	unsigned long reloc_off;       //重定位偏移项
	struct global_data *new_gd;	   //重定位 gd

#ifdef CONFIG_DM
	struct udevice	*dm_root;	/* Root instance for Driver Model */
	struct udevice	*dm_root_f;	/* Pre-relocation root instance */
	struct list_head uclass_root;	/* Head of core tree */
#endif
#ifdef CONFIG_TIMER
	struct udevice	*timer;		/* Timer instance for Driver Model */
#endif

	const void *fdt_blob;		//设备树 dtb 文件
	void *new_fdt;			 	//重定位之后的 dtb
	unsigned long fdt_size;		// 重定位 dtb 的保留空间
	struct jt_funcs *jt;		// 跳转表 ？
	char env_buf[32];		    // 重定位之前的 buffer？
#ifdef CONFIG_TRACE
	void		*trace_buff;	/* The trace buffer */
#endif
#if defined(CONFIG_SYS_I2C)
	int		cur_i2c_bus;	 /* current used i2c bus */
#endif
#ifdef CONFIG_SYS_I2C_MXC
	void *srdata[10];
#endif
	unsigned long timebase_h;
	unsigned long timebase_l;
#ifdef CONFIG_SYS_MALLOC_F_LEN
	unsigned long malloc_base;	/* base address of early malloc() */
	unsigned long malloc_limit;	/* limit address */
	unsigned long malloc_ptr;	/* current address */
#endif
#ifdef CONFIG_PCI
	struct pci_controller *hose;	/* PCI hose for early use */
	phys_addr_t pci_ram_top;	/* top of region accessible to PCI */
#endif
#ifdef CONFIG_PCI_BOOTDELAY
	int pcidelay_done;
#endif
	struct udevice *cur_serial_dev;	/* current serial device */
	struct arch_global_data arch;	/* architecture-specific data */
#ifdef CONFIG_CONSOLE_RECORD
	struct membuff console_out;	/* console output */
	struct membuff console_in;	/* console input */
#endif
#ifdef CONFIG_DM_VIDEO
	ulong video_top;		/* Top of video frame buffer area */
	ulong video_bottom;		/* Bottom of video frame buffer area */
#endif
} gd_t;
```


board_init_f() 直接调用一个函数 initcall_run_list。
在 initcall_run_list。 中，会进入到这些控制路径：
	CONFIG_OF_CONTROL

	#if defined(CONFIG_BOARD_EARLY_INIT_F)

	#if defined(CONFIG_ARM) || defined(CONFIG_MIPS) || defined(CONFIG_BLACKFIN) || defined(CONFIG_NDS32) || \
	defined(CONFIG_SH) || defined(CONFIG_SPARC)

	#if defined(CONFIG_BOARD_POSTCLK_INIT)

	#if defined(CONFIG_SYS_FSL_CLK) || defined(CONFIG_M68K)

	#if defined(CONFIG_DISPLAY_CPUINFO)

	#if defined(CONFIG_DISPLAY_BOARDINFO)

	#if defined(CONFIG_ARM) || defined(CONFIG_X86) || defined(CONFIG_NDS32) ||    defined(CONFIG_MICROBLAZE) || defined(CONFIG_AVR32) || defined(CONFIG_SH)

	#if !(defined(CONFIG_SYS_ICACHE_OFF) && defined(CONFIG_SYS_DCACHE_OFF)) &&  defined(CONFIG_ARM)

	#if !defined(CONFIG_BLACKFIN) && !defined(CONFIG_XTENSA)

	#ifndef CONFIG_SPL_BUILD

被执行的函数有：

	setup_mon_len,    gd->mon_len = bss_end - _start，表示整个 uboot 镜像的
	
	fdtdec_setup,     
		#if CONFIG_IS_ENABLED(OF_CONTROL)  不满足，只执行 fdtdec_prepare_fdt，赋值 ：gd->fdt_blob 为设备树的 blob，imx6 应该是没有 uboot 的，后期可以测试一下。  
	
	arch_cpu_init,      定义在 arch/arm/cpu/armv7/mx6/soc.c 中，初始化 cpu，需要细究，TODO
 	
	mach_cpu_init,      不做任何事
 	
	 initf_dm,           
		#if defined(CONFIG_DM) && defined(CONFIG_SYS_MALLOC_F_LEN)  满足
		CONFIG_TIMER_EARLY     不满足
		执行 dm_init_and_scan，主要是设备驱动相关的。 
 	
	 arch_cpu_init_dm,       什么也不做
	 	
 	mark_bootstage,          不做

	board_early_init_f,      定义在 board/freescale/mx6ullevk/mx6ullevk.c 中，主要设置一下 uart 的 pinmux

	timer_init,             定义在 arch/arm/imx-common/syscounter.c 中。
	board_postclk_init,     定义在 arch/arm/cpu/armv7/mx6/soc.c 中，设置 ldo
	get_clocks,             定义在 arch/arm/imx-common/speed.c 中,获取 gd->arch.sdhc_clk
	env_init,               定义在 common/env_mmc.c 中,获取默认的 env 参数,不做其它事
	init_baud_rate,         本文件中，获取 gd->baudrate,获取 gd->baudrate
	serial_init,            定义在 drivers/serial/serial.c,初始化 serial.为什么可以使用全局变量?
	console_init_f,         
							定义在 common/console.c,根据环境变量是否存在 silent，pre_console_putc 不做任何事

	display_options,        定义在 /lib/display_options.c 中,打印信息
	display_text_info,      定义在本文件,代码数据bss地址
	print_cpuinfo,          打印 cpu 相关消息，
	show_board_info,        打印board 相关的消息
	INIT_FUNC_WATCHDOG_INIT        初始化看门狗
	INIT_FUNC_WATCHDOG_RESET       reset 看门狗
	announce_dram_init,     本文件，打印 dram 信息
	dram_init,              
							定义在 board/freescale/mx6ullevk/mx6ullevk.c 中，仅仅是获取 gd->ram_size, ddr 不知道在哪里已经被初始化好了？后续再来分析
	INIT_FUNC_WATCHDOG_RESET
	INIT_FUNC_WATCHDOG_RESET
	INIT_FUNC_WATCHDOG_RESET

	setup_dest_addr,		找到内存的顶部：gd->ram_top，并且将 gd->relocaddr 设置到内存的顶部
							/*
							* Now that we have DRAM mapped and working, we can
							* relocate the code and continue running from DRAM.
							*
							* Reserve memory at end of RAM for (top down in that order):
							*  - area that won't get touched by U-Boot and Linux (optional)
							*  - kernel log buffer
							*  - protected RAM
							*  - LCD framebuffer
							*  - monitor code
							*  - board info struct
							*/


	reserve_mmu,           为 tlb 保留空间
	reserve_trace,         如果定义了 CONFIG_TRACE，就为 trace 保留空间
	reserve_uboot,         
	reserve_malloc,
	reserve_board,
	setup_machine,
	reserve_global_data,
	reserve_fdt,
	reserve_arch,
	reserve_stacks,
	setup_dram_config,
	show_dram_config,
	display_new_sp,
	INIT_FUNC_WATCHDOG_RESET
	reloc_fdt,
	setup_reloc,








一直疑惑的一个问题：为什么 bin 文件里面还要初始化 BSS。  

这是因为：在 elf 文件中，各个主要段的排列位置为： 代码段、数据段、bss 段，bss 段是被放在最后的。 

使用 objcopy -O binary X.elf X.bin  时，并没有将 bss 段放到 bin 文件中，而是直接留空(bss 是最后一个段，可以这么做)。所以，即使是
使用 bin 文件中，也是不包含 bss 段的。    

如果修改链接脚本，将 bss 段放到 data 或者 text 前面，因为 bin 文件是连续的，这种情况下 bss 段就会以全 0 的方式被放到
bin 文件中，后面再接 data 等段。但是这种做法并不常见。     


