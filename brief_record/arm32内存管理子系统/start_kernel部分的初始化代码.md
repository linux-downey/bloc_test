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

* 调用 setup_processor,进行处理器的相关初始化工作,在该函数中,先获取编译时确定的 procinfo,比如 cpu_name,architecture ,  调用 cpu_init 设置 CPU 每个模式下的栈以及设置 per_cpu_offset 到 TPIDRPRW 寄存器中
* 调用 setup_machine_fdt,主要是针对设备树的处理,首先就是对设备树的 compatible 属性进行匹配,获取对应的 machine_desc 结构.再调用 early_init_dt_scan_nodes 进行设备树的扫描工作,这里需要关注的节点有 chosen ,memory, chosen 主要是 bootargs,这个可以是 uboot 修改内核 dtb 产生的 chosen 节点,也可以是在编译内核时 dts 中指定的 chosen 节点.
  early_init_dt_scan_nodes 中会执行三次设备树的扫描,第一次扫描 /chosen,生成 boot_command_line,第二次会扫描 size,address cell,初始化数据宽度,第三次扫描 memory.
  所有扫描到的 memory 区域都会通过 memblock_add(参数为 base,size) 添加到 memblock 中.  memblock 是一个静态定义的全局,
* 初始化 init_mm 的 start_code,end_code,end_data,brk 相关参数.
* 