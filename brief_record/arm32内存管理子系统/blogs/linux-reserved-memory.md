# linux reserved memory
随着内核的运行，内核中的物理内存越来越趋向于碎片化，但是某些特定的设备在使用时用到的 DMA 需要大量的连续物理内存，这可能导致设备在真正使用的时候因为申请不到满足要求的物理内存而无法使用，这显然是不能接受的。
最简单的方式就是为特定设备保留一部分物理内存专用，这部分内存不受系统的管理，绑定到特定的设备，在设备需要使用的时候再对这部分内存进行管理，这就是内核中提供的 reserved memory 机制，这是内核中比较传统的内存保留机制。
在实际应用中，为特定设备保留内存虽然实现和操作都相对简单，但是有个明显的缺点：正因为这部分内存不能被系统管理，也就造成了一定的浪费，在设备不使用的时候，这部分内存就无法利用的，而最理想的情况应该是：当被保留的内存在不使用的时候同样可以被系统利用，而设备需要使用的时候就返回给其绑定的设备，这种保留内存的机制从实现上来说要相对复杂一些，但是大大地提高了内存利用率，这就是内核中的 CMA 机制。  


## 配置 reserved memory
在早期的内核中，保留内存操作需要通过命令行传递到内核，而在内核支持设备树之后，就可以在设备树中进行配置，下面是一个保留内存的设备树配置示例：
```
/ {
	#address-cells = <1>;
	#size-cells = <1>;

	memory {
		reg = <0x40000000 0x40000000>;
	};
    reserved-memory {
		#address-cells = <1>;
		#size-cells = <1>;
		ranges;
        display_reserved: framebuffer@78000000 {
			reg = <0x78000000 0x800000>;
		};
    }
    fb0: video@12300000 {
		memory-region = <&display_reserved>;
	};
}
```
address-cells 和 size-cells 在设备树中属于比较重要的概念，用于指定其同一级节点中描述一个单元的信息所需要的字节数，cell 值为 1 表示使用 4 个 bytes 描述，比如在 memory 节点中，reg 中一共有 8 个字节，表示描述两个单元，第一个 4 字节表示物理内存起始地址，而第二个 4 字节表示物理内存的 size，reg 属性的解析方式是由内核规定的，设备树解析相关的知识点可以参考TODO，这里就不过多赘述了。
内核在启动时通过扫描 memory 节点可以知道当前系统中所有物理内存的相关信息。

reserved-memory 节点正是用来设置保留内存的相关信息，在该节点下的所有子节点都表示需要保留的一段内存，其中 reg 用来指定保留内存的起始地址和 size。

最后，这片保留内存需要和特定的设备建立联系，就需要在对应设备节点中提供 memory-region 属性，对保留内存节点进行引用，表明当前设备需要使用到对应的保留内存。

这种设备与保留内存之间的节点引用，可以让设备更方便地使用相应的设备树接口获取该片物理内存的相关信息，即使没有建立这种设备节点之间的关联，只要特定设备在启用时能够知道保留内存地相关信息，也是可以直接使用，毕竟这片物理内不会被系统征用，至于该设备在启动时如何使用这片物理内存，取决于具体地业务场景。

### 设置保留内存的选项
上面只是一个 hello world 示例，在实际的使用中，存在一些配置选项对保留内存进行灵活的配置。

#### 静态和动态保留内存选项
静态保留内存就是在编写设备树时就直接确定了保留内存的地址以及 size，如果要设置静态的保留内存，就必须提供 reg 属性指定相关信息。

相对应的，动态保留内存只需要指定一个保留内存的 size，而具体保留哪一片内存由内核自行决定，这种操作具有更高的灵活性，不会造成任何冲突，除了必须提供的 size 属性，还可以提供两个属性：
* alignment：对其参数，保留内存的起始地址需要向该参数对齐
* alloc-ranges：指定可以用来申请动态保留内存的区间
alignment 和 alloc-ranges 是可选参数。

在设置保留内存时，如果 reg 属性和 size 同时被给出，reg 属性具有更高的优先级，因此这种情况下会建立静态的保留内存。

#### 其它选项
* compatible(可选)：通常情况下，保留内存并不需要 compatible 属性，因为保留内存并不需要相应的驱动程序来处理它，一个特殊的情况就是上面提到的 CMA 保留内存，CMA 内存在作为保留内存的同时还需要可以被 buddy 子系统管理，需要做一些特殊的设置，因此需要编写相应的驱动程序，使用 compatible 属性来关联相应的驱动程序，并在内核的启动阶段调用，完成 CMA 保留内存的初始化。
CMA 内存对应的 compatible 属性为：shared-dma-pool，这是内核规定的一个固定值。
当然，在一些特殊情况下，如果保留内存需要驱动程序进行一些特殊处理，也可以提供自定义的 compatible 属性以在内核启动阶段执行相应的驱动程序，这都是很灵活的。
* no-map(可选)：在 32 位的系统上，由于内核线性地址的限制，物理内存被分为两个部分：高端内存和低端内存，低端内存会被直接线性映射到内核地址空间中，而高端内存暂时不会进行映射，只有在需要使用到的时候再动态地映射到内核中。而在 64 位系统中，由于内核线性地址完全足够，就会直接将所有物理内存映射到内核虚拟地址中。
默认情况下，32 位系统地低端内存和 64 位系统所有物理内存都将会被映射到内核中，而且通过简单地线性转换就可以通过物理地址找到对应地虚拟地址，执行内存访问。
如果在保留内存处设置了 no-map 属性，这部分保留内存将不会建立虚拟地址到物理地址的映射，也就是即使获取了这部分保留内存，也是不能直接访问的，而是需要使用者自己建立页表映射，通常直接使用 ioremap 来重新建立映射再访问，为保留内存添加 no-map 属性可以提供更高的安全性和灵活性，毕竟只有在特定设备建立映射之后才能访问，同时在建立页表映射时可以根据不同的业务场景自行指定该片内存的缓存策略，话说回来，没有添加 no-map 属性的保留内存同样也可以在高端内存区重新建立映射，只是这种情况很少见。
既然讲到这儿，就多说两句底层实现吧，内核初始化阶段使用的是 memblock 内存分配器，对于 32 位系统，memblock 只会管理低端内存，memblock 负责收集系统的物理内存信息，主要是通过设备树。memblock.memory 节点用于保存 memory 节点提供的物理内存信息，对于已经被分配掉的或者需要保留的内存区间将会被额外地添加到 memblock.reserved 中(memblock.reserved 中的内存段依旧保存在 memblock.memory 中)，同时，memblock.memory 中所有的物理内存都会建立虚拟映射，在内存初始化的后期阶段，memblock.memory 除去 memblock.reserved 中的所有内存页面都会被移交给 buddy 子系统。
再回过头看保留内存的设置，默认的保留内存会被添加到 memblock.reserved 中，表示不会被移交给 buddy 子系统，但是依旧会被 memblock 管理，进行一些初始化设置，对于设置了 no-map 属性的保留内存，memblock 会将这部分内存从 memblock.memory 中删除，就好像物理上并没有提供这片内存一样，后续的内存管理自然不会管理到这片内存。

而对于指定位 CMA 类型的保留内存，和其他保留内存不一样的是，它还是会被移交给 buddy 子系统，只是将这片内存标记为 MIGRATE_CMA，表示这是 CMA 内存，只有用户在申请 MOVABLE 类型的内存时才能使用这部分内存，因为当特定设备需要使用这部分内存的时候需要把原本占用该内存的内容移出去，达到使用时再分配的目的。

* reusable(可选)：保留内存中指定该属性表示这片内存可以被 buddy 子系统利用，CMA 就属于这种，也可以自定义其它的框架来实现保留内存的重复利用。对于一个保留内存节点，不能同时指定 no-map 和 reusable 属性。
* 如果 cma 节点指定了 linux,cma-default 属性，内核在分配 cma 内存时会将这片内存当成默认的 cma 分配池使用，执行内存申请时如果没有指定对应的 cma 就使用默认 cma pool。
* 如果用作 dma 的保留内存指定了 linux,dma-default 属性，内核在分配 dma 内存时将会默认使用该片内存作为 dma 分配池。

#### 引用保留内存节点选项
设备节点可以通过设备树引用保留内存节点以建立设备与保留内存之间的关联，可以使用的节点属性为：
* memory-region(可选)：被引用节点的 phandle 值，每个节点都存在一个唯一的 ID 用于识别，这个 ID 就是 phandle 值，不过在编写设备树节点的时候通常都使用 &NODE_NAME 表示引用节点，设备树编译器会自动分配 phandle 并建立联系。
* memory-region-names(可选)：设备节点需要引用的保留内存节点名，可以是一个列表。


## CMA 的初始化
对于系统中默认的保留内存方式，在上面介绍得七七八八了，无非就是初始化的时候留出一块物理内存不让系统用，然后在需要的时候再拿出这片物理内存，如果这片内存没有建立映射，再使用 ioremap 建立虚拟地址到物理地址的映射，像访问外设内存一样访问它，只是因为这片内存没有被系统接管，因此需要使用者自己来实现页面的管理。

需要拿出来讲一讲的是 CMA 机制，正如上文所说，通俗地讲，CMA 机制就是实现了让保留内存需要用的时候由指定设备使用，不需要用的时候被系统征用，接下来看看 CMA 机制的初始化以及使用。 

指定 cma 内存还是从设备树开始：
```c++
/ {
    reserved-memory {
        ...
        linux,cma {
            compatible = "shared-dma-pool";
            reusable;
            size = <0x4000000>;
            alignment = <0x2000>;
            linux,cma-default;
        };
    }
}
```
其中，resuable 属性表示 cma 内存可被 buddy 子系统使用，只指定了 size 属性表示该 cma 内存是动态申请的，linux,cma-default 属性表示当前 cma 内存将会作为默认的 cma pool 用于 cma 内存的申请。

在内核 start_kernel->setup_arch 中，内核会扫描设备树，此时内核中的内存管理器还是 memblock，memblock 扫描完 memory 节点之后，接着扫描 reserved-memory 节点，获取其中所有设置为保留内存的区域，对于提供了 reg 属性的保留内存，直接添加到 memblock.reserved 中，对应的实现在 \_\_fdt_scan_reserved_mem->\_\_reserved_mem_reserve_reg 函数中。
而对于只提供了 size 的动态内存而言，内核使用一个静态数组 reserved_mem 用来保存动态的保留内存,先会获取需要保留内存的 size 值，reserved_mem_count 值用来记录内核中动态保留内存的数量，具体的实现在 \_\_fdt_scan_reserved_mem->fdt_reserved_mem_save_node 函数中，这个函数仅仅是做一个统计，并没有分配内存，实际分配保留内存在 fdt_init_reserved_mem->__reserved_mem_alloc_size 函数中实现，该函数通过 memblock 内存分配器为用户分配大小为 size 的物理内存，分配完之后的内存页会被记录在 memblock.reserved 中。

紧接着，因为内核为 cma 内存指定了 compatible 属性，也就意味着内核中存在针对 cma 的 driver 部分，一旦设备树中指定了同名的 compatible 节点，该 driver 代码就会被调用，driver 对应的源码在 drivers/base/dma-contiguous.c 中：

```c++
static int __init rmem_cma_setup(struct reserved_mem *rmem)
{
    ...
    if (!of_get_flat_dt_prop(node, "reusable", NULL) ||
	    of_get_flat_dt_prop(node, "no-map", NULL))
		return -EINVAL;

    err = cma_init_reserved_mem(rmem->base, rmem->size, 0, &cma);
    ...
}
RESERVEDMEM_OF_DECLARE(cma, "shared-dma-pool", rmem_cma_setup);
```

从源代码的解析不难看出，cma 节点中 reusable 属性是必须的，同时不能有 no-map 属性，初始化 cma 的函数为 cma_init_reserved_mem。

```c++
int __init cma_init_reserved_mem(phys_addr_t base, phys_addr_t size,
				 unsigned int order_per_bit,
				 struct cma **res_cma)
{
    ...
    if (!size || !memblock_is_region_reserved(base, size))
		return -EINVAL;
    
    cma = &cma_areas[cma_area_count];
	cma->base_pfn = PFN_DOWN(base);
	cma->count = size >> PAGE_SHIFT;
	cma->order_per_bit = order_per_bit;
	*res_cma = cma;
	cma_area_count++;
	totalcma_pages += (size / PAGE_SIZE);
    ...
}
```
在初始化函数中，可以看到内核同样使用一个全局数组 cma_areas 来保存系统中的所有 cma 区域，cma_area_count 表示系统中 cma 区域的数量。在这个初始化函数中也只是登记了一下 cma 区域，设置一些参数，并没有做其它动作。

到目前为止，实际上 cma 和其它的保留内存一样放在 memblock.reserved 中，因此在随后的 memblock 到 buddy 子系统的过渡过程中，cma 区域和其它保留内存一样没有被移交给 buddy 子系统，此时并不会被系统使用到。

对 cma 真正的初始化在 mm/cma.c 中：

```c++
static int __init cma_init_reserved_areas(void)
{
    ...
	for (i = 0; i < cma_area_count; i++) {
		int ret = cma_activate_area(&cma_areas[i]);
	}
    ...
}
core_initcall(cma_init_reserved_areas);
```
core_initcall 这个内核宏并不陌生，使用 xx_initcall 描述的初始化函数将会在内核初始化的后期被调用(关于内核 initcall 机制可以参考 TODO)，在 cma_init_reserved_areas 函数中，将会遍历 cma_areas 数组中记录了 cma 的区域，执行 cma_activate_area 函数。
在该函数中，才是 cma 真正的核心初始化部分所在：
* 首先，鉴于 cma 区域是通过 bitmap 记录使用情况，因此，需要通过 kzalloc 函数申请 bitmap 占用的内存(此时 slub 已经可用)。
* 接着，针对每个 cma page，将 page 设置的 reserved 标志位清除，且将 migratype 设置为 MIGRATE_CMA，表明这些 page 都属于 cma 区域，需要特殊处理。
* 调用 \_\_free_pages 函数，将 cma 中所有的 page 释放到 buddy 子系统中，自此，cma 页面就已经被 buddy 系统管理起来，用户申请到的页面中可能就是原本属于 cma 的页面，只是这一切对于使用者来说都是透明的。

在 32 位系统中还存在另一种情况，如果 cma 区域指定的是高端内存区域，而 memblock 并不会管理到高端内存，这种情况下释放 page 到 buddy 子系统这部分是相同的，只是中间不需要经过 memblock 的管理，同时使用高端内存区的物理内存时还需要重新建立持久的映射，毕竟高端内存区默认是不会被映射的。


### cma 区域的使用
cma 区域被初始化之后，就可以通过对应的接口使用了，申请和释放 cma 区域内存的接口为：

```c++
struct page *cma_alloc(struct cma *cma, size_t count, unsigned int align);
bool cma_release(struct cma *cma, const struct page *pages, unsigned int count);
```
第一个参数 cma 是在 cma 初始化的时候生成的，如果特定设备的设备树节点有引用，可以通过标准接口 dev_get_cma_area(dev) 获取设备对应的 dev->cma_area 成员，而如果 dev_get_cma_area 传入的参数为 NULL 或者无效的 dev，就会使用默认的 cma poll。

count 表示需要申请的数量，align 表示对齐。

实际上，cma_alloc 和 cma_release 接口并不会被直接调用，而是隐藏在 dma 的接口背后，在使用 dma 申请对应的物理内存时，使用 dma_alloc_from_contiguous 接口，该接口直接调用 cma_alloc 函数执行物理内存的分配。

```c++
struct page *dma_alloc_from_contiguous(struct device *dev, size_t count,
				       unsigned int align)
{	
	...
	return cma_alloc(dev_get_cma_area(dev), count, align);
}

```

在 cma 页面交给 buddy 管理时，页面管理遵循 buddy 分配算法，但是将 cma 区域独立出来使用时，就需要对 cma 区域执行单独的页面管理算法了，cma 区域中使用的记录方式为 bitmap，这种方式相对简单，每个页面占用一个 bit，通过遍历位图就可以找到哪些页面处于分配状态，哪些页面处于未分配状态，然后根据具体的情况再执行页面分配，看起来 cma 区域并没有实现页内的分配算法，也没太多必要。

同时，在执行 cma 分配的时候一个比较重要的过程是执行页面迁移，毕竟 cma 区域很可能已经被占用，毕竟文件系统通常要使用大量的内存空间来缓存数据，因此在使用这些 cma 页面之前，需要将对应的内存先清空，然后再使用。



参考文章：[官方文档](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/reserved-memory/reserved-memory.txt)

