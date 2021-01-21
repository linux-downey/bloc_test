## __gic_init_bases

__gic_init_bases -> gic_init_chip : 主要是 chip 相关的初始化,包括 chip 的回调函数设置. 

 

## gic_init_bases

gic_init_bases(gic, irq_start, handle);

函数调用背景: 在 gic_of_init 中调用,每个 gic 控制器都会调用一次,其中 irq_start 传入的参数总是 -1.



在这个函数中,通过读取 GIC_DIST_CTR 寄存器,也就是 TYPE 寄存器的低 5 位可以获取当前 gic 可以支持多少个 interrupt(cortex A7 手册中定义地址). 先确定支持的 interrupt 数量.  

下一步确定 irq_start,也就是当前 gic domain 下的起始 irq num,有了起始 irq 和 irq nums 就可以对当前的 domain 进行映射. 对于 root irq 而言,hwirq 的前面 32 个 irq num 是不用到的. 因此,在做映射的时候,就不需要对前面 32 个 irq 进行映射.   

获取了  irq_start 和 irq_num,就调用 irq_alloc_descs(),需要注意的是, domain 主要区分 hwirq 和逻辑 irq,  逻辑 irq 是全局的, desc 也是统一的.  

所有已经申请完的 desc 都记录在 bit map  allocated_irqs 中,重新申请 desc 会从该 bit map 中找到空闲的bit 开始,空闲 bit 的下标就是 irq 映射的 start. 

调用 alloc_descs 来申请 desc ,如果定义了 CONFIG_SPARSE_IRQ , desc 的映射关系不再是线性的,而是通过 radix tree 来组织. 

desc 分配完成之后,返回第一个分配的地址,也就是当前 domain 下的第一个逻辑 irq_num,如果不是 radix tree 维护,这个逻辑 irq 自然就一一对应当前 gic 的 hwirq. 通过返回的逻辑 irq 再映射 hwirq,调用的接口为 irq_domain_add_legacy.

irq_domain_add_legacy 会先初始化并添加一个 irq domain,接着调用 irq_domain_associate_many 接口,传入的参数为 first_irq,first_hwirq,size,就会对当前 domain 的 hwirq 和逻辑 irq 进行映射,调用的是 irq_domain_set_mapping 函数(因此,irq_find_mapping 是一个反过程),调用传入的 map 回调函数,map 对应 gic_irq_domain_map,在这里面就会设置对应的 High level irq handle,为 handle_fasteoi_irq. 

需要记得的是,domain 只针对 irq num 的映射,irq_desc 是全局的,不属于 irq domain 管理. 它可以通过静态数组和 radix tree 两种方式进行管理.  

gic_init_bases 接着就是调用 gic_dist_init 和 gic_cpu_init 做一些初始化设置,主要还是 GIC 的寄存器设置.  

