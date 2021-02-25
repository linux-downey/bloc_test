## Linux 内核宏

### PAGE_OFFSET

内核空间与用户空间在线性地址上的划分，在 arm32 上，通常是 0xc0000000(1:3)，也有配置成 0x80000000(1:1)的。



### TEXT_OFFSET

内核代码运行的地址偏移，通常也是 stext 的偏移地址，在 makefile 中定义，值通常为 0x00008000，也就是从 ram 开始地址的 32K 处开始运行。 



### PHYS_OFFSET

内存的外设基地址，比如 ram 的外设基地址为 0x40000000，这个值就等于 PHYS_OFFSET，每个板子的 RAM 外设基地址都不一样，但是物理地址和虚拟地址的线性映射是：ram 基地址线性映射到内核内存基地址。

对于线性映射，虚拟地址到物理地址的转换就是：vaddr - PAGE_OFFSET + PHYS_OFFSET，反之也是一样。



### vmalloc_min

这个不是宏，这是内核静态定义的，表示 vmalloc 的最低地址。 

```c
static void * __initdata vmalloc_min =
	(void *)(VMALLOC_END - (240 << 20) - VMALLOC_OFFSET);
```

VMALLOC_END  为 0xff800000

VMALLOC_OFFSET  为 0x00800000

240 << 20 为 0x0f000000

vmalloc_min 最终的值为 0xf0000000

VMALLOC_END 为 vmalloc 结束地址，VMALLOC_OFFSET 为 vmalloc 偏移地址 8M。

VMALLOC_START 是起始地址，但是 qemu 中打印出来的起始地址是 0+VMALLOC_OFFSET，这就有点不合理了，需要再测试一下。理论上应该是高端内存开始的地方  high_memory+VMALLOC_OFFSET， 可能是因为 qemu 里面的内存采集有问题导致的。 

### swapper_pg_dir

boot 阶段页表的虚拟地址，在 imx6ull 上的是 0x80004000。



### initial_boot_params

设备数 blob 对应的虚拟地址,imx6ull 上对应 0x88000000



### TASK_SIZE

用户空间的 task 大小

```c
#define TASK_SIZE		(UL(CONFIG_PAGE_OFFSET) - UL(SZ_16M))
```







