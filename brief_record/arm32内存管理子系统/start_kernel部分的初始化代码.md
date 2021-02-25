# start_kernel

在 start_kernel 之前,主要的工作是建立 image 的页表,确保在开启 mmu 之后内核可以正常运行起来.

开启 mmu 之后,代码就跳转到了 start_kernel 部分. 

image 的内存已经被 mapping 了,设备树的内存地址是不是也被 mapping 了?

在内核中,静态定义了一个 task,也就是 init_task.



start_kernel 的实现在 init/main.c 中:

* 调用 set_task_stack_end_magic,在 init 进程的 stack_start + sizeof(thread_info) 处放置一个 magic(0x57ac6e9d)防止栈溢出(仿真的结果). 
* 调用 smp_setup_processor_id,通过读取 mpidr 寄存器获取系统中所有 CPU 的信息并编号,保存在 cpu_logical_map 中.
* 调用 debug_objects_early_init 初始化 hash buckets.忽略
* 调用 boot_init_stack_canary,初始化栈 canary,防止栈溢出攻击,获取一个随机值,放在栈溢出检测的位置,如果这个 canary 被修改,说明发生了栈溢出
* 调用 cgroup_init_early: cgroup 的早期初始化,不讨论
* 调用 local_irq_disable 关中断,并将 early_boot_irqs_disabled 全局变量置为 true
* 调用 boot_cpu_init ,设置 boot cpu 的 CPU_mask,在 smp 系统下,通常是 boot0 作为 boot cpu,其它 cpu 停在汇编阶段.
* 调用 page_address_init: 初始化高端内存的 page 地址相关参数,暂时不研究.
* 调用 setup_arch,这是个非常重要的函数,参数是 command_line





## setup_arch

* 调用 setup_processor,进行处理器的相关初始化工作,在该函数中,先获取编译时确定的 procinfo,赋值到 list 中，这里的 procinfo 和刚开始的汇编部分获取的 procinfo 是一样的,比如 cpu_name,architecture .

  需要注意的是，procinfo 里面设置了 struct processor proc 相关的数据，其中有一系列的回调函数，包括 set_pte_ext 这个用于设置 pte 页表的回调函数，switch_mm 等。
  调用 cpu_init 设置 CPU 每个模式下的栈以及设置 per_cpu_offset 到 TPIDRPRW 寄存器中

* 调用 setup_machine_fdt,主要是针对设备树的处理,首先就是对设备树的 compatible 属性进行匹配,获取对应的 machine_desc 结构.
  
  machine desc 用于描述特定于硬件的信息，管理和架构相关的参数，比如 architecture、irq 、l2 cache 等。
  machine desc 是被静态定义的，放在镜像的 .init.arch.info 段中，并在链接脚本中定义了两个变量\_\_arch_info_begin 和 \_\_arch_info_end 来指定 mdesc 的位置。
  machine desc 的静态定义可以使用 MACHINE_START 和 MACHINE_END 包括起来，或者使用 DT_MACHINE_END 和 MACHINE_END 包括起来定义。
  静态定义的 machine desc 中包含一个 dt_compat 定义，用于在寻找 mdesc 的时候和 dtb 中的 compatible 进行匹配。 
  
  fdt blob 的虚拟地址被存放到 initial_boot_params 全局变量中。
  
  再调用 early_init_dt_scan_nodes 进行设备树的扫描工作,这里需要关注的节点有 chosen ,memory, chosen 主要是 bootargs,这个可以是 uboot 修改内核 dtb 产生的 chosen 节点,也可以是在编译内核时 dts 中指定的 chosen 节点.
  early_init_dt_scan_nodes 中会执行三次设备树的扫描,第一次扫描 /chosen,生成 boot_command_line,
  第二次会扫描 size,address cell,初始化数据宽度,赋值给 dt_root_address_cells 和 dt_root_addr_cells
  第三次扫描 memory.对应的设备数节点属性为 memory@n 或者 memory 属性
  所有扫描到的 memory 区域都会通过 early_init_dt_add_memory_arch->memblock_add(参数为 base,size) 添加到 memblock.memory.region 中.  memblock 是一个静态定义的全局,
  reserved memory 由 arm_memblock_init 指定,memblock 数据结构：
  
  ```c++
  struct memblock {
  	bool bottom_up;  /* is bottom up direction? */
  	phys_addr_t current_limit;
  	struct memblock_type memory;
  	struct memblock_type reserved;
  };
  struct memblock_type {
  	unsigned long cnt;	/* number of regions */
  	unsigned long max;	/* size of the allocated array */
  	phys_addr_t total_size;	/* size of all regions */
  	struct memblock_region *regions;
  };
  struct memblock_region {
  	phys_addr_t base;
  	phys_addr_t size;
  	unsigned long flags;
  };
  ```
  
  region 的初始化会指向一个 INIT_MEMBLOCK_REGIONS 个成员的 region 数组，后续添加的 region 都会加到该数组中，reserved 则表示保留内存。
  
* 初始化 init_mm 的 start_code,end_code,end_data,brk 相关参数.

* early_fixmap_init/early_ioremap_init/parse_early_param 暂时不分析，early_paging_init 直接返回了,adjust_lowmem_bounds 也差不多直接返回

* adjust_lowmem_bounds 调整 memblock 的物理映射，后面再分析。

* arm_memblock_init:
  首先，调用 memblock_reserve 保留内核本身所占的空间
  如果定义了 initrd，也要为 initrd 保留对应的内核空间(initrd 为什么不会被回收？)
  调用 arm_mm_memblock_reserve,为 swapper_pg_dir 保留内存空间
  调用架构相关的  mdesc->reserve() 来保留内存，imx6ull 并没有指定
  为设备树 blob 保存物理地址空间
  early_init_fdt_scan_reserved_mem -> __fdt_scan_reserved_mem,扫描设备树中的保留内存(reserved_memory指定的)，如果指定了 nomap，直接将这部分保留内存从 memblock 中删除，否则添加到 memblock 的 reserved 节点中。    调用 fdt_init_reserved_mem,处理真正由开发者指定的 memory，reserved_mem_count 指定数量，reserved_mem 是一个 16 成员的数组，放置指定的保留内存，还处理 CMA 相关的保留内存，这部分后续再细究。
  dma_contiguous_reserve 保留 DMA 的连续内存地址
  这一部分主要就是设置保留内存。 
  
* 再次执行 adjust_lowmem_bounds ，先不管

* paging_init，这是很重要的函数了,下面单独讲。 



arm32 的页表介绍：

https://people.kernel.org/linusw/arm32-page-tables

### paging_init



#### 页表相关的宏

##### pgd_index(addr)

```c
#define pgd_index(addr)		((addr) >> PGDIR_SHIFT)
```

对于二级页表的 arm32 而言，PGDIR_SHIFT 为 21

获取虚拟地址 addr 对应 pgd 的偏移地址



##### pgd_offset

```c
#define pgd_offset(mm, addr)    ((mm)->pgd + pgd_index(addr))
```

获取进程虚拟地址对应的 pgd 项地址。



##### pgd_offset_k

```c
#define pgd_offset_k(addr)	pgd_offset(&init_mm, addr)
```

init_task 相当于内核的初始化进程，init_mm 也就是内核对应的 mm 结构

这个宏也就是 init_mm->pgd + addr >> 21.

init_mm->pgd  通常为 内核起始地址 + 0x0004000，也就是 16K 处。 



##### pud_offset

arm32 不支持 pud，因此 pud_offset 就等于 pgd_offset。



##### pmd_offset

arm32  如果不支持 LAPE 扩展地址，pmd = pud = pgd

不讨论扩展地址的情况



##### pmd_off_k

```c
static inline pmd_t *pmd_off_k(unsigned long virt)
{
	return pmd_offset(pud_offset(pgd_offset_k(virt), virt), virt);
}
```

因此，pmd_off_k  返回的就是 0x80004000 + 偏移地址 处对应的 L1 表项。 

因为内核 image 的映射是在 0x80006000 地址，暂时不会清除掉内核的页表映射。 



#### map_lowmem

映射低端的物理内存，内核 image 之前，内核 image ，内核 image 到 lowmem_limit.



#### dma_contiguous_remap

map dma 的保留内存。



#### early_fixmap_shutdown

重新 map fixmap 相关的页面。

arm32 好像没有 fixmap？



#### devicemaps_init

device 的 mapping.

向量表的 mapping，从 memblock 申请两个页面用来存放 vector 相关代码。 



### bootmem_init

