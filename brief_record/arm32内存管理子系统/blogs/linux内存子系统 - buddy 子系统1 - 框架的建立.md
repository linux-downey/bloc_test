# linux内存子系统 - buddy 子系统1 - 框架的建立

在内核的内存初始化阶段，memblock 在完成了一些最基本的物理内存信息收集以及必要的内存分配之后，就需要着手开始向 buddy 系统进行迁移了，毕竟 buddy 子系统越早启动，内核就越早进入到内存管理的正轨。

但是这个过程并不是一步就位的，内存所涉及到的方方面面本身是非常复杂的，尽管 buddy 系统已经足够简单，那也只是相对其它的内存管理器而言，本章着重于分析 buddy 子系统的初始化的早期部分，也就是一些准备工作。 



## 基本概念

memblock 足够简单，简单到将所有的物理内存一视同仁，但实际上鉴于内核亦或是硬件的一些特性，内核如果想要更高的效率和利用率，比如对内存管理工作进行细化。在了解 buddy 所做的这些工作之前，我们需要先了解几个概念。



### struct page 和 pfn

对于 buddy 而言，最小的管理单元为一个物理页面，每个页面都由 struct page 结构来描述，该结构包含页面属性、是否分配、引用计数等页面相关信息，struct page 结构和页面本身是独立的，一片物理内存所对应的所有 struct page 结构通常会被统一分配，占用连续的物理内存。 

同时，为了管理方便，每个物理页面还映射到一个页帧号 pfn，页面基地址和 pfn 基于线性映射，4K 页面映射的系统中  pfn = page_vaddr >> 12。



### 内存模型

内存模型这个概念其实是有些抽象的，没办法，对应的英文就是 memory model，这是针对整体物理内存管理的策略，包括两个方面：

* 每个页面使用 struct page 进行描述，所有页面对应的 struct page 结构应该如何被组织
* struct page 结构和对应的物理页面之间如何建立映射，通过 struct page 找到物理页面是简单的，大不了使用一个结构成员指针即可，但是如何通过物理页面索引到 struct page 结构呢？要知道物理页面中是不会保存任何管理数据的，唯一知道的就是页面对应的地址范围。



内核中的内存模型分为三种：

* FLATMEM：平坦内存模型
* DISCONTIGMEM：非连续内存模型
* SPARSEMEM：稀疏内存模型

软件总归是基于硬件进行抽象，内核使用哪种内存模型取决于系统的物理内存是如何分布的，在嵌入式系统中，物理内存通常是连续的，这种情况下的物理内存管理是最简单的，平坦内存模型适用于这种情况。

在内核中，既然要对每个物理页面进行管理，那么自然是需要使用特定的数据结构对其进行描述，记录页面的相关信息，内核中使用 struct page 结构来描述一个页面，在平坦内存模型中，所有的页面对应的 struct page 结构可以存放在一片连续的内存中。

这种结构带来的好处在于：物理页帧(号)与 struct page 之间的关联实现非常简单，这种关联在于通过一方可以快速地索引到另一方，连续的内存可以直接使用偏移地址获得对方的地址。 

实际的情况也可能并不理想，系统中连接的物理内存可能并不是连续的，被分为一块或者多块，这种情况在 numa 架构中多见，页面依旧使用 struct page 来表示，但是如果还是按照原来的方案(连续内存)来保存所有物理内存的 struct page，这种物理内存上的空洞必定会带来内存空洞处对应的 struct page 是没有对应物理页面的，也就是会造成内存的浪费，为了避免这种浪费，非连续内存模型被提出，顾名思义，这种内存模型用来管理非连续的物理内存。理想的情况是，在 numa 架构中，每个节点中的物理内存是连续的，这样对每个 numa 节点使用独立的数据结构来描述，一方面可以节省内存，另一方面同样可以做到 struct page 与物理页面之间的简单映射，但是需要增加判断内存所在节点的成本，同时这种方案对内存的热插拔支持性不好，如果单节点中出现非连续的内存(尽管很少见)，则更不好处理，因此，DISCONTIGMEM 只在内核中存在较短的时间。

一个更好的方案是稀疏内存模型(SPARSEMEM)，这是目前最受欢迎的内存模型，64位平台中通常将这种内存模型设为默认值，内存模型是否合适依旧是那两个判断标准，一是是否在支持非连续内存的同时不造成内存的过多浪费，二是该内存模型是否能很好地处理 struct page 与物理页面之间的映射，显然 SPARSEMEM 是做到了这两点。

SPARSEMEM 在内存的描述中使用了 section 的概念(注意这里的section 和 MMU 的 section 映射不是同一个概念)，section 的大小由体系结构决定，由原来对整块内存的操作变成对多个内存 section 的操作，分配 struct page 相关的操作更加灵活，可以做到按需分配。

为了在物理页帧号 pfh 和 strcut page之间进行高效转换，物理页帧号 pfn 的几个高位用于索引sections数组，解决了从物理页面到 struct page 的映射，另一方向上，段号被编码在 struct page 中，由 struct page 到物理页面的映射也就解决了。

在实际的应用中，第二种内存模型(DISCONTIGMEM )基本上已经被抛弃了，在较为复杂的内存系统中(存在非连续内存的系统)，通常使用SPARSEMEM ，而对于只提供连续内存的系统，当然是要使用 FLATMEM，简单且快捷。 

在我当前使用的平台上，使用的是 FLATMEM，毕竟大部分嵌入式平台不支持 numa 或者内存热插拔等比较高级的内存特性。

在内核的初始化阶段，将会一次性为所有的物理页面 struct page 结构体，占用连续的内存，基地址保存在全局变量 struct page* mem_map 中，因此，struct page 和屋里页帧号之间的映射实现为：

```c++
#define __page_to_pfn(page)	((unsigned long)((page) - mem_map) + \
				 ARCH_PFN_OFFSET)
#define __pfn_to_page(pfn)	(mem_map + ((pfn) - ARCH_PFN_OFFSET))
```

实现非常简单，从原理上来说，也就是物理页面相对于物理内存基地址的偏移等于 struct page 结构相对 mem_map 基地址的偏移，基于这个进行双向的映射。

而 DISCONTIGMEM 和 SPARSEMEM  类型的映射，同样可以参考 include/asm-generic/memory_model.h 头文件。  

 **需要注意的是，对于内核初始化阶段来说，内存空洞实际上指的是 memblock.memory 成员中保存的物理内存不连续，这种情况可能是本身提供的物理内存不连续造成的，也可能是用户 reserved nomap 类型的内存造成的，关于 reserved memory 参考：[内核保留内存.](https://zhuanlan.zhihu.com/p/363911384)**



### 内存域 - zone

内核中所有内存并不是被一视同仁的，它们会根据硬件的特性被划分成不同的内存域，对于 32 位系统而言，内存区域的限制将会更加明显。 

目前内核中支持的内存域如下：

```c++
enum zone_type {
#ifdef CONFIG_ZONE_DMA
	ZONE_DMA,
#endif
#ifdef CONFIG_ZONE_DMA32
	ZONE_DMA32,
#endif
	ZONE_NORMAL,
#ifdef CONFIG_HIGHMEM
	ZONE_HIGHMEM,
#endif
	ZONE_MOVABLE,
#ifdef CONFIG_ZONE_DEVICE
	ZONE_DEVICE,
#endif
	__MAX_NR_ZONES
};
```

这是一个枚举列表，包含 6 个有效成员，最后一个 \_\_MAX_NR_ZONES 用于记录 zone 的最大数量，用于在遍历的时候作为结束判断条件，或者定义数组时设置成员数量，在内核中这种手法是很常见的。

从各种条件宏不难看出，上面所列出的所有 zone 除了 ZONE_NORMAL 和 ZONE_MOVABLE 之外，其它的 zone 都是可配置的，主要和硬件相关。 

* ZONE_DMA：在一个完整的硬件系统中，CPU 并不是系统总线的唯一掌控者，DMA 同样可以操作系统总线来访问内存，其主要的作用就是协助 CPU 完成一些预定义的数据拷贝工作，让 CPU 腾出手来做其它更多的事，从而提高程序执行效率。
  
  在一个 32 位的系统中，CPU 延伸出的系统总线宽度为 32 位(非地址扩展的情况)，但是 DMA 硬件控制器并不一定满足 32 位的地址总线宽度，比如 x86 架构中的某些 DMA 设置只支持 24 位的地址线，这就意味着该 DMA 只能访问 16MB 的内存空间。
  
  同时，鉴于 linux 需要兼容这些硬件设备，软件上就需要统一做一些限制：在 DMA 控制器总线宽度不满足的情况下，分配给 DMA 的内存不能超过 16MB 的限制，否则会出现 DMA 无法正常操作内存的情况，因此内核中将内核基地址的 0-16MB 内存划分成 DMA_ZONE，在申请 DMA 内存时从这个区中申请才是安全的。
  
  随着硬件的发展，或者在非 x86 的其它架构中，DMA 只支持 24 位地址线的情况并不多见，因此，DMA_ZONE 在内核中越来越少见，取决于 CONFIG_ZONE_DMA 是否被配置，当然，最根本的还是取决于实际硬件的情况。
  
* ZONE_DMA32：和 ZONE_DMA 相类似的概念，这里考虑的是 64 位系统的情况，如果系统中使用的 DMA 硬件控制器只支持 32 位的地址线，那么也就需要将 DMA 内存申请控制在内核地址开始的 0-4G 空间内，也就在 64 位系统中划分出  ZONE_DMA32 区。

* ZONE_NORMAL：正常的内存区域，也是常用的内存管理区，这里的 NORMAL 是相对于其它 zone 的概念，因为其它 zone 都有些特殊，比如 DMA 需要连续物理内存，HIGHMEM 区不支持持久映射。 

* ZONE_HIGHMEM：高端内存，对于 linux 要了解的一个基本概念为：所有硬件资源都由内核管理，物理内存自然也不例外，从用户空间的角度来说，看起来是对于内存的操作，但是实际上所有用户使用的内存空间都是先由内核分配，再执行映射的。
  
  默认的配置下，32位系统中内核和用户空间的占比为 1：3，也就是内核只占 1G 的线性地址，如果此时系统中存在 2G 的物理内存，那么在使用内存时，内核无法完整地映射所有的物理内存，因为一个页面如果需要被使用，它必须要被映射到内核区域中，需要注意的是，这里说的是无法直接映射访问，并不是无法管理。
  
  因此，在物理内存无法直接全部映射的情况下，将一部分区域直接线性映射到内核中，而另一部分区域留出来，执行动态地映射，动态映射也就是当那些多出来的无法执行直接映射的内存区域需要使用时，临时建立页面到内核中的映射，当不用时，就解除映射，以这种方式来动态地管理多余的内存，而动态映射区也就被划分为 ZONE_HIGHMEM。
  
  在 32 位内核中，高端内存与低端内存的分界点一般配置为 896MB 或者 768MB 处。
  
  对于 64 位系统而言，不存在这个问题，毕竟内核的线性空间可以划分为非常大(TB级)的地址，不会存在物理地址大于内核线性地址的问题，因此在 64 位系统中，所有物理内存都可以实现线性映射。
  
* ZONE_MOVABLE：可迁移的页面，内存和磁盘不一样，对于磁盘而言，数据可以随意地迁移来迁移去，但是对于某些内存就不行，程序代码通常对地址具有依赖性的。比如内核镜像部分的代码是不能随意移动的，但是由于虚拟内存机制提供的抽象，用户空间程序、磁盘文件的缓存等是可以移动的，对于用户空间程序而言，本来就不在乎其运行的物理地址在哪里，只需要满足其虚拟地址正确对应就行，移动它们之后只需要修改以下页表项，重新建立映射即可。
  
  设置这个区域主要是针对内核的内存回收以及碎片处理。
  
* ZONE_DEVICE：与特定设备相关的内存区域，不能被普通的内存操作接口访问



内核中以 ZONE 为单位，对物理内存进行管理，不同的 ZONE 自然有不同的属性，也针对不同的用途，比如在 imx6ull 上，配置了三个 ZONE：ZONE_NORMAL、ZONE_HIGHMEM、ZONE_MOVABLE，其中 ZONE_NORMAL 区域的内存执行线性映射，DMA 申请的内存也属于这个区域。ZONE_HIGHMEM 用于动态映射。



### NUMA 节点

尽管本系列文章并不涉及到 numa 系统的分析，但是在内核中，buddy 子系统会将所有机器抽象为 numa 架构，非 numa 系统被内核视为单节点 numa 系统，这样就可以实现接口的统一，在分析内存时还是不能忽略这个概念。

因此，不难推出，一个 numa 节点就是一个完整的内存管理区域，每个 numa 节点包含一个 struct pglist_data 结构，同样是以 pgdat  -> zones  -> pages 的树形结构来管理一片完整的物理内存。 



## buddy 启动前期



### buddy 初始化背景

memblock 作为内核早期的内存管理器，有几个重要的阶段：

* 在内核中被静态定义，整个内存管理器由 memory 和 reserved 两个数组来管理物理内存

* 扫描设备树，获取系统中所有物理内存信息
* 为内核镜像、dtb、设备树中指定内存等区域设置保留空间，并针对一些必要的部分分配物理内存
* 为所有物理内存建立页表，不过建立页表的工作并不完全算 memblock 来做的，它只是提供相应的内存信息，总之，在此之后，系统中所提供的所有物理内存及其属性都已经确定，线性映射区可以访问了。

在这种情况下，buddy 子系统已经具备了启动的环境，memblock 开始慢慢地将内存管理的重任交接给 buddy 子系统。



## buddy 管理器框架的建立

在 paging_init 函数中建立完页表之后，参考[内核页表的建立](https://zhuanlan.zhihu.com/p/363907867)，在 paging_init 函数的后半段执行 bootmem_init，对 zone 执行初始化工作。  

```c++
void __init bootmem_init(void)
{
    unsigned long min, max_low, max_high;
	find_limits(&min, &max_low, &max_high);
    zone_sizes_init(min, max_low, max_high);
    ...
}
```

find_limits 函数将会找到三个物理内存边界：

* min：物理内存的起始页帧
* max_low：memblock 的 limit 参数，也对应低端内存与高端内存的分界点，通常比例为 3:1(768:256)  或者 7:1(896:128)，假设物理起始页面号为 0x10000，低端内存与高端内存比例为 3:1，max_low 的值就是 0x40000。
* max_high：物理地址的结束页帧号

这三部分信息就是当前 imx6ull 平台用于初始化 zone 的内存信息，实际上只用到两个 zone 空间：NORMAL 和 HIGHMEM，ZONE_MOVEABLE 区域并没有实际管理内存。

需要注意的是，包括 paging_init 函数在内，都是属于 setup_arch 的子函数，这些都是和硬件架构强相关的，不同的硬件完全可能使用不同的 zone 空间，比如 x86 下就可能针对 ZONE_DMA 进行初始化，64 位系统需要使用到 ZONE_DMA32 而不需要使用 ZONE_HIGHMEM。

zone_sizes_init 函数负责初始化部分 zone 相关的信息，从函数名就可以看出，该函数负责初始化和统计 zone 相关的信息，主要是两个方面：

* 统计 hole 信息，也就是内存空洞，用的是反向统计的方式，先假设某个 zone 内全部空间都为 hole，再扫描 memblock，减去 memblock 中提供的物理内存区间，也就是 zone 区域内的内存空洞
* 将物理内存划分给不同的 zone，比如在 imx6ull 中，低 768M 物理内存划分为 ZONE_NORMAL 管理的内存，除此之外的划分给高端内存区 ZONE_HIGHMEM， 



将统计完的所有 zone 信息收集起来，作为调用 free_area_init_node 的参数，在 buddy子系统[数据结构与基本原理](https://zhuanlan.zhihu.com/p/363922807)中说到，整个 buddy 子系统由数据结构 contig_page_data 描述，这是一个 struct pglist_data 类型的数据结构，其中包含 buddy 子系统的所有管理信息，既然要建立 buddy 子系统的框架，自然主要就是针对该结构体的初始化操作，对该结构的初始化操作就在 free_area_init_node  中完成：

* 初始化一些常规结构，比如 NUMA node 节点 id node_id，该节点下的第一个物理页面 node_start_pfn 等等。

* 根据 zone_sizes_init  计算得到的 zone_size(所有 zone 的 size 总和) 和 zhole_size 计算得到总的 page 数量，计算出来有什么用呢？

  一方面是做个记录，给 contig_page_data 的 node_spanned_pages(线性区域内总的 page 数量，包括 hole) 和 node_present_pages(线性区域内提供的物理 page 数量，除去 hole) 赋值。

  另一个更重要的事情是：给所有 page 对应的 struct page 结构申请内存，在 imx6ull(arm32)平台上，一个 struct page 的 size 为 0x20，因此如果物理上提供了 2G 内存，就需要申请 2G / 4K * 0x20 = 16M 内存空间，**申请到的内存块的虚拟基地址保存在 contig_page_data ->node_mem_map 中，尽管此时使用 memblock 申请到的内存为物理内存，因为该内存属于线性映射区，可以直接通过 phys_to_virt 转换成虚拟地址**

* 因为之前已经分别统计了各个 zone 中的页面相关信息，因此调用 free_area_init_core 对 zone 中的管理成员进行初始化，包括 zone->zone_start_pfn、zone->spanned_pages、zone->present_pages，初始化 zone 中一些管理成员比如 lock、name、zone_pgdata 成员等。

* 初始化 zone 中的 pcp page，对应 zone->pageset 结构体成员，这个成员在 buddy 中是非常重要的，pcp 的意思是 per cpu page，为了更高效地执行单页的内存分配，buddy 子系统除了维护主要的 zone->free_area 之外，还维护了一个单页链表，毕竟内核中单页的申请是非常频繁的，因此缓存一些单页以提高分配效率是非常要必要的，同时使用 percpu 机制可以避免多核操作所带来的缓存问题。 

* 接着，在 free_area_init_core->init_currently_empty_zone 中调用了 zone_init_free_lists，该函数用于初始化 buddy 子系统的主角，zone->free_area 结构，free_area 的结构是这样的：
  ![image-20210324215731256](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/linux-memory-subsystem/free_area%E8%AF%A6%E7%BB%86%E7%BB%93%E6%9E%84.png)
   其实初始化的过程很简单，就是逐级遍历 free_area 中每一项，调用 INIT_LIST_HEAD 对每个链表头进行初始化，同时将每个 order 数据成员的 nr_free 初始化为 0
  
* 在 buddy 框架初始化的最后呢，就是对每个页面结构(struct page) 进行初始化，其中包括设置部分页面迁移属性为 MIGRATE_MOVABLE，为所有页面设置 关联的 zone、ref count、lru 链表头等。
在这里会初始化页面的 refcount 为 1，并不是标记所有页面正在使用，而是因为在 memblock 将页面移交给 buddy 子系统时，使用的是页面释放的接口 free_page，该接口会先将 refcount 减一，然后判断是否为 0，只有 refcount 为 0 的页面才会被 buddy 子系统回收。
  
  

不难发现，在 paging_init -> bootmem_init 函数中，几乎完成了 buddy 子系统框架的建立，其中包括所有页面结构 struct page 内存的申请、buddy 全局管理结构 contig_page_data  的初始化、zone 以及 page 针对 zone 的分配、pcp page 以及 free_area 结构的初始化，buddy 的雏形已经完成，接下来的工作就是将 memblock 管理的所有页面提交给 buddy 子系统，完成内存管理工作的交接，关于这一部分我们下节再讨论。 





### 参考

https://zhuanlan.zhihu.com/p/220068494

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。