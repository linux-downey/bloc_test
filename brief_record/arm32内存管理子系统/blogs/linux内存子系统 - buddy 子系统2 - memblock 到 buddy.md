# linux内存子系统 - buddy 子系统2 - memblock 到 buddy

在 [buddy 框架的建立](https://zhuanlan.zhihu.com/p/363923438) 中介绍到，paging_init -> bootmem_init 函数中已经构建好了 buddy 子系统的基本框架，但是实际的页面管理工作还是由 memblock 在管理，接下来的工作就是将 memblock 管理的物理页面移交给 buddy 子系统。 



 ## 页面移交

页面移交工作的代码主要在 start_kernel -> mm_init 中，这个函数中包含了整个内核中内存管理子系统的初始化，除了 buddy 子系统的初始化之外还有同样重要的 slab 管理器的初始化，本章只对 buddy 进行分析，对应的接口为 mm_init->mem_init，而 slab 相关的初始化将在后续的文章中介绍。

mem_init 函数中的主要工作就是将 memblock 中管理的页面释放到 buddy 中，鉴于 buddy 子系统对内存页的管理是以 zone 为单位进行的，因此页面的处理也以 zone 为单位，在当前的平台(imx6ull)上包含 3 个 zone 区间，分别为 ZONE_NORMAL、ZONE_HIGHMEM、ZONE_MOVABLE，其中第三个 zone 区域并没有使用，因此页面的处理分为两部分：低端内存部分和高端内存部分。

实际上，页面从 memblock 到 buddy 中可以直接理解为页面的释放，这和用户空间使用完内存之后释放到 buddy 子系统中没什么区别，可以使用共同的接口。



## mem_init

mem_init 的源码实现如下：

```c++
void __init mem_init(void)
{
    ...
	free_unused_memmap();              
    free_all_bootmem();
    free_highpages();
    ...
}
```

free_unused_memmap 函数主要针对在 bootmem_init 中申请的 struct page 内存，对于某些内存空洞，如果为其申请了对应的 struct page 结构，在该函数中就应该把它检测出来并释放，在平坦内存模型(FLAT_MEM)中，struct page 和对应物理页面之间存在某种线性的关系，因此通过扫描 memblock.memory 找到其中的空洞，再释放掉对应的 struct page 所占的内存即可，memblock 的释放操作也很简单，就是从 memblock.reserved 中删除这部分内存即可。 

free_all_bootmem 和 free_highpages 就是释放页面的函数，从函数名可以看出，一个是释放低端内存区(也是线性映射区)，一个是释放高端内存区。 



## 页面释放

总的来说，memblock 中记录的内存包含两部分：一是空闲的内存，二是被分配的内存。

memblock.memory 成员中保存了内核管理的所有物理内存信息，这部分信息通过扫描设备树获得。

memblock.reserved 中保存了已经分配出去的信息，比如内核 image、dtb、页表等占用的空间，这些 persistant 内存自然是不能释放到 buddy 子系统中的。

memblock 对于移交物理内存的策略是：对于已经分配出去的被保存在 memblock.reserved 中的内存，设置保留标志，这部分内存页面暂时不释放给 buddy 子系统，但是也并不绝对，比如对于内核镜像的后半部分，比如 .init/.exit/.data.percpu 这些段而言，在内核初始化完成之后会被释放到 buddy 子系统中，只有 memblock 期间分配的 persistant 内存才不会被释放。

内核初始化到当前，物理内存的相关信息为：

![image-20210327163644425](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/linux-memory-subsystem/%E7%89%A9%E7%90%86%E5%86%85%E5%AD%98%E5%8D%A0%E7%94%A8%E5%B8%83%E5%B1%80.png)

为了让图片看起来更舒服，这里并没有按照真实的比例来画这个图，理论上上面所有分配出去的内存占比应该非常小，比如内核页表只占 16K，而内核镜像只占用不到 20M，总的内存通常是以 G 为单位。

这个图只是描述了通常情况下的物理内存布局，实际内存情况根据真实的硬件而定，比如存在以下几种不同的情况：

* dtb 所放置的内存空间相对来说是比较随意的，只要能被内核访问到即可，并没有太多限制
* 保留内存区域也就是 memblock 分配的内存区域属于 memblock 所管理内存的 ending 处，这是由 memblock 分配器中的 bottom_up 成员决定的，默认配置为从 ending 处分配内存，由高向低增长，当配置为从头部申请内存时，保留内存将会从开始处分配内存。
* 最后一片物理内存是 memblock 所管理的物理内存之后的区域，通常会被内核中的高端内存区域所映射，如果硬件上提供的物理内存并不多，线性映射就足够，那么就不存在这片区域。



看到上面的物理内存分布图，memblock 需要移交的页面就是所有的空闲内存页面，尽管页面移交直接使用的是内存释放的接口，但是它的逻辑要比内存释放简单很多，毕竟内存释放时需要考虑释放后的内存页是否可以合并，而这里不需要考虑。

### buddy 管理结构

再次复习一下 buddy 子系统对内存管理的相关概念：buddy 子系统针对内存的管理以 zone 为单位，对应 zone 区域将所有管理的物理页面大部分存放在 free_area 成员中，free_area 的结构为：

![image-20210327164751786](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/linux-memory-subsystem/free_area%E8%AF%A6%E7%BB%86%E7%BB%93%E6%9E%84.png)

出于分配效率的考虑，内核还会为每个 zone 维护一个 percpu 类型的单页链表，对应的成员结构为 zone->pageset，其存储结构为：

![image-20210327173342097](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/linux-memory-subsystem/pcplist%E7%BB%93%E6%9E%84.png)

pcp 页面和普通内存页面不一样的是，pcp 页面只包含 UNMOVABLE、MOVABLE、RECLAIMABLE 类型页面的缓存，不支持 CMA 等页面类型。

 

### 线性映射区的释放

了解了 buddy 子系统针对页面的管理结构，页面移交工作就简单了：将空闲的物理内存尝试以最大的 order 值进行分割，假设 max order 被设置为 11，就以 2^11 = 2048 个页面为单位分割物理内存，链接到对应的 free_area 成员链表中。

当剩下的物理内存不满足 size 要求时，逐渐降低 order 值，直到所有的物理页面都分配到对应的 free_area 成员链表中，比如剩下 10 个页面未分配时，这 10 个页面就会被分割成 8+2，分别被链入 free_area[3] 和 free_area[1] 中。 

而单独的一个页面的释放并不会直接释放到 free_area 中，而是放到 pcp 链表上，当然，pcp 页面作为一种缓存机制，并不会无限制地缓存单页，而是当单页超过一定的数量时，将会将部分单页释放回 free_area 中，始终保持一种动态的平衡。 



### 高端内存区域的释放

相比于线性映射区页面的释放，高端内存区域的释放有一些不同，主要是因为线性映射区中对应的 struct page 结构已经在此前初始化完成，而高端内存区并没有，因此对于高端内存区域的页面是逐一释放的，因为需要对每个页面设置相应的属性，比如 reserved 标志、refcount 等，逐页释放到 buddy 之后，buddy 子系统会再对这些页面进行"组装"，连续的内存页合起来存放到更高的 order 链表中。



### 参考

4.9.88 源码

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。