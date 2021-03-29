# 内核内存初始化 -  slub初始化

slub 的初始化函数在 buddy 子系统初始化完成之后，核心的初始化函数为 start_kernel -> mm_init-> kmem_cache_init，slub 的初始化主要分为两个部分：

* bootstrap，自举阶段，自举这个概念总结起来就是指：要完成一项工作需要 A 资源，但是初始化 A 资源的过程中又依赖于 A 资源本身，这种矛盾在计算机中经常见到，比如程序需要运行在内存中，但是初始化内存又需要先将程序加载到内存中，又比如前几章中提到的动态申请内存需要先初始化内存管理器，但是内存管理器初始化本身又需要动态申请内存，对于这种矛盾，一般使用特殊的方式先进行资源的初始化，再完成具体的工作。
  在 slub 中，这种矛盾在于：struct kmem_cache 和 struct kmem_cache_node 两个结构是 slub 的管理数据结构，这两者的内存申请也依赖于 slub 为其创建对应的缓存对象，但是 slub 创建缓存对象又依赖于这两个缓存对象，因此 slub 也无法创建任何缓存对象。
  解决方案是：先静态地分别定义 struct kmem_cache 和 struct kmem_cache_node 两个结构，临时用于创建 
  struct kmem_cache 和 struct kmem_cache_node 这两个缓存对象本身，再就可以创建其它对象了。
* 初始化第二部分就是初始化一些常用的 slub 缓存 ，比如给 kmalloc 使用的 8 bytes/16 bytes/32 bytes 等常用 size 的缓存对象，对于只指定 size 的 kmalloc 调用，使用的就是这些缓存对象，不过对于频繁申请释放的数据结构，可以使用 kmem_cache_create 创建专用的 slub 缓存。 



## 自举阶段

自举阶段的源码如下：

```c++
void __init kmem_cache_init(void)
{
	static __initdata struct kmem_cache boot_kmem_cache,
		boot_kmem_cache_node;                               ......................1
	kmem_cache_node = &boot_kmem_cache_node;
	kmem_cache = &boot_kmem_cache;
	
	create_boot_cache(kmem_cache_node, "kmem_cache_node",   
		sizeof(struct kmem_cache_node), SLAB_HWCACHE_ALIGN);

	create_boot_cache(kmem_cache, "kmem_cache",
			offsetof(struct kmem_cache, node) +
				nr_node_ids * sizeof(struct kmem_cache_node *),
		       SLAB_HWCACHE_ALIGN);                         ......................2
	kmem_cache = bootstrap(&boot_kmem_cache);
	kmem_cache_node = bootstrap(&boot_kmem_cache_node);     ......................3
    ....
}
```

1. 静态地定义两个 struct kmem_cache 结构，准备为 struct kmem_cache 结构和 struct kmem_cache_node 结构创建缓存对象，从 __initdata 前缀可以看出，这两个静态结构体被放在内核中的 .init 段中，在内核初始化后期将会被 buddy 子系统回收，因此这两者只是临时存在的，实际上，在创建完这两个缓存对象之后，就会分别申请一个 object 放置这两个原始的 kmem_cache 结构。
2. 给 kmem_cache_node 创建 slub 缓存，先给这个结构创建缓存对象是因为在后面创建 kmem_cache 的缓存中需要使用到该缓存对象，而在创建 kmem_cache_node 缓存对象时，同样也需要申请 kmem_cache_node  结构的内存，因此也需要使用其它特殊的方式来处理。
   而在创建 kmem_cache 缓存对象时，kmem_cache_node 缓存对象已经存在，只需要自举本身。
   关于 create_boot_cache 的源码实现在下文中详细讨论。
3. 到这一阶段，slub 已经可以依据上一阶段创建的两个 kmem_cache 结构创建任何对象缓存了。
   但是由于之前创建的两个缓存对象信息保存在 boot_kmem_cache 和 boot_kmem_cache_node 这两个静态变量中，这两者在后期需要被回收，因此需要从刚刚创建的两个缓存中分别分配一个 object 出来，将这两个静态变量的内容 copy 到 object 中，也就完成了整个自举过程。
   同时，该函数做的另一件事就是将 slab deactivate，执行初始化动作，同时将当前 kmem_cache 链接到 slab_caches 链表中，内核中所有缓存对象对应的 kmem_cache 结构都将被链接在 slab_caches 中。

