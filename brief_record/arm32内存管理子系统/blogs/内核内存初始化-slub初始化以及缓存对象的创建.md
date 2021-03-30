# 内核内存初始化 -  slub初始化

slub 的初始化函数在 buddy 子系统初始化完成之后，核心的初始化函数为 start_kernel -> mm_init-> kmem_cache_init，slub 的初始化主要分为两个部分：

* bootstrap，自举阶段，自举这个概念总结起来就是指：要完成一项工作需要 A 资源，但是初始化 A 资源的过程中又依赖于 A 资源本身，这种矛盾在计算机中经常见到，比如程序需要运行在内存中，但是初始化内存又需要先将程序加载到内存中，又比如前几章中提到的动态申请内存需要先初始化内存管理器，但是内存管理器初始化本身又需要动态申请内存，对于这种矛盾，一般使用特殊的方式先进行资源的初始化，再完成具体的工作。
  在 slub 中，这种矛盾在于：struct kmem_cache 和 struct kmem_cache_node 两个结构是 slub 的管理数据结构，这两者的内存申请也依赖于 slub 为其创建对应的缓存对象，但是 slub 创建缓存对象又依赖于这两个缓存对象，因此 slub 也无法创建任何缓存对象。
  解决方案是：先静态地分别定义 struct kmem_cache 和 struct kmem_cache_node 两个结构，临时用于创建 
  struct kmem_cache 和 struct kmem_cache_node 这两个缓存对象本身，再就可以创建其它对象了。
* 初始化第二部分就是初始化一些常用的 slub 缓存 ，比如给 kmalloc 使用的 8 bytes/16 bytes/32 bytes 等常用 size 的缓存对象，对于只指定 size 的 kmalloc 调用，使用的就是这些缓存对象，不过对于频繁申请释放的数据结构，可以使用 kmem_cache_create 创建专用的 slub 缓存。 
  这部分对应的函数接口为 kmem_cache_init -> create_kmalloc_caches，具体创建了哪些 kmalloc 对应的缓存对应，可以通过 /sys/kernel/slab/ 目录查看，以 kmalloc-* 命名的 slab 都是在此创建的。



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



### create_boot_cache

create_boot_cache 所实现的功能和 kmem_cache_create 函数实现功能差不多，都是创建一个新的缓存对象，以及构建一个新的 kmem_cache 结构对该缓存对象进行管理，create_boot_cache 的特别之处在于执行时用于管理的 kmem_cache 和 kmem_cache_node 缓存对象还没有被创建，只能传入静态参数或使用特殊 alloc 方式，不能正常地动态申请。 

因此 create_boot_cache 函数需要传入一个 kmem_cache 实例参数，在该实例的基础上执行初始化。

初始化函数先会对传入的 kmem_cache 实例初始化 name、size、align 等参数，接着调用 __kmem_cache_create 接口创建对象。



### kmem_cache_create

既然涉及到了创建缓存对象，顺带着也解析一下通用的创建缓存对象的接口：kmem_cache_create。

```c++
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
		  unsigned long flags, void (*ctor)(void *))
{
	struct kmem_cache *s = NULL;
    s = create_cache(cache_name, size, size,
			 calculate_alignment(flags, align, size),
			 flags, ctor, NULL, NULL);
   	return s;
}

static struct kmem_cache *create_cache(const char *name,
		size_t object_size, size_t size, size_t align,
		unsigned long flags, void (*ctor)(void *),
		struct mem_cgroup *memcg, struct kmem_cache *root_cache)
{
    struct kmem_cache *s;
    s = kmem_cache_zalloc(kmem_cache, GFP_KERNEL);
    s->name = name;
	s->object_size = object_size;
	s->size = size;
	s->align = align;
	s->ctor = ctor;
    
    err = __kmem_cache_create(s, flags);
    s->refcount = 1;
	list_add(&s->list, &slab_caches);
}
```

从 kmem_cache_create 的源码不难看出，该函数和 create_boot_cache 的差别在于不需要传入 kmem_cache 实例，而是会返回一个可用的 kmem_cache，这个 kmem_cache 结构是通过 kmem_cache_zalloc 从 kmem_cache 缓存对象分配的。

针对 kmem_cache 的操作差不多，同样是初始化 name、size、align 等参数，将 kmem_cache 链接到 slab_caches 链表中。调用 __kmem_cache_create 继续缓存对象的创建。

这里需要额外讨论的一个成员变量就是 align 成员，在调用 kmem_cache_create 函数时提供一个 align 参数，传入的 align 值并不一定作为该缓存对象的 align 值，还取决于传入的 flags 参数。

通常，创建缓存对象时会设置 flags | SLAB_HWCACHE_ALIGN，这个标志位表明向硬件缓存行对齐，也就意味着 object 成员在使用时会被加载到同一个缓存行中，有利于提高缓存的命中率以及降低多核下的缓存失效问题。

向缓存行对齐在特定情况下并不一定合适，毕竟为了对齐而填充无效字节意味着空间的浪费，如果一个缓存对象只有 8 bytes，但是一个缓存行为 64 字节，这种对齐明显有些得不偿失，因此可以不指定 SLAB_HWCACHE_ALIGN 标志位，这种情况下如果传入的对齐参数小于 ARCH_SLAB_MINALIGN 就会使用 ARCH_SLAB_MINALIGN 作为对齐参数，该值通常为 sizeof(unsigned long long)

如果传入的对齐参数大于 ARCH_SLAB_MINALIGN 又不设置 SLAB_HWCACHE_ALIGN标志位，那么就将传入的 align 值向 sizeof(void\*) 上界(ROUND_UP)对齐，这不大好理解，举个例子，在 32 位平台上，如果 align 为 10，sizeof(void\*) 为 4，那么最终就向 12 字节对齐，如果 align 为 7，就向 8 字节对齐。 

### __kmem_cache_create

create_boot_cache 和 kmem_cache_create 底层都是调用了 __kmem_cache_create 函数执行缓存对象的创建：

```c++
int __kmem_cache_create(struct kmem_cache *s, unsigned long flags)
{
	int err;
    ...
	err = kmem_cache_open(s, flags);
	err = sysfs_slab_add(s);
    ...
	return err;
}
```



该函数主要包括两个部分：

* kmem_cache_open 用来进一步创建缓存对象
* 调用 sysfs_slab_add，将当前缓存对象的调试信息导出到 /sys 目录下，比如 objects、align、size、cpu_partial 等信息，通常对应的目录为  /sys/kernel/slab/${NAME}。



kmem_cache_open 函数中执行 kmem_cache 成员变量值的计算以及初始化，在此之前，已经设置完 align、size、name 参数，kmem_cache_open 具体实现为：

* kmem_cache 的 inuse 成员设置为传入的 size 成员(向 sizeof(void*) 对齐后的)，注意区分 page->inuse 和 kmem_cache->inuse 的区别，前者记录的是 page 中的被使用的 object 数量。
* 如果传入的 flags 中设置了 SLAB_POISON| SLAB_DESTROY_BY_RCU bit，或者指定了 ctor 回调函数(该回调函数会在每次分配调用，在调用 kmem_cache_create 时传入)，就不能使用 object 中的内存来保存下一个空闲 object 的地址，这时候需要在 object 后面新开辟 4 个字节用来保存下一个空闲 object 地址
  相对应的，size 需要加 4，offset 成员需要设置为 sizeof(object)。
  如果没有设置上述操作，那么 offset 就为 0，表示空闲 object 的前 4 个字节保存下一个空闲 object 的地址。
  poison 是 slub 实现的一种保护和调试机制，它会使用特殊的字段(0x5a)来初始化每个 object。
* 调用 calculate_order 计算当前缓存对象对应的 order 值，尝试找到一个合适的 order 值，对于内存占用较小的缓存对象，通常 order 值为 0，而且 slub 更倾向于使用单页来管理缓存对象，这样能比跨页操作效率更高，但是对于一些比较大又不到 4K 的缓存对象，如果使用单页放置，会造成内存空间的浪费，比如一个缓存对象占用 1.4K，那么就有 1.2K 的内存被浪费(4 - 1.4*2 = 1.2)，slub 的标准是如果这种浪费超过 1/16，就使用多页来管理，尽管这会带来一系列的问题：比如页面之间的频繁访问、lock 的时间过长，但是相比起内存的大量浪费而言，两相比较取其轻。
  order 值的计算以及一个 page 中 object 的数量计算还有不少实现细节，有兴趣的朋友可以深入研究研究 calculate_order 这个函数，不过其本质就是在执行效率以及内存利用率之间做权衡。 
* 通过 order 和 object_size 构建 kmem_cache -> oo、 kmem_cache -> min、 kmem_cache -> max 成员。 
* 调用 set_min_partial 函数设置 kmem_cache -> min_partial 参数，等于  ilog2(size) / 2，该参数取决于传入的 size，min_partial 的最大值为和最小值分别为 MIN_PARTIAL(5)、MAX_PARTIAL(10)。
  object size 越大，也就越希望能将多的页面保留在 node partial 链表上备用。
* 同样的，通过传入的 size 计算并设置 cpu_patial， cpu_partial 指的是 slab_cpu 的 partial 链表上的最大链表数量，size 越大，cpu_partial 值越小。 
* 调用 init_kmem_cache_nodes 申请并初始化一个 kmem_cache_node 结构，详情见下文
*  调用 alloc_kmem_cache_cpus 申请并初始化 slab_cpu，percpu 类型变量的内存申请并不通过slub，而是通用专用的 percpu 内存池，详情见下文。
* 而 kmem_cache 结构的申请在 kmem_cache_create 初期就已经完成了，其初始化贯穿整个 kmem_cache_create 函数。 

### init_kmem_cache_nodes

init_kmem_cache_nodes 用于 kmem_cache->node(struct kmem_cache_node类型) 的初始化，该函数的执行存在两种情况：

* create_boot_cache 函数流程中，kmem_cache_node 缓存对象还没有被创建，需要向 buddy 子系统申请新的页面并初始化为 slab 对象，从 slab 中分配一个空闲 object 使用，这部分操作并不直接依托于 slub 分配，而是手动地创建一个对应的 slab 缓存块组再执行分配，属于特殊操作。
* kmem_cache_node 缓存对象被创建之后，可以直接使用 kmem_cache_alloc_node 申请一个 kmem_cache_node 类型的 object，slub 申请 object 的实现将在后续详细讨论。



node 申请之后，就调用 init_kmem_cache_node 进行初始化，初始化流程比较简单，就是初始化 nr_partial 为 0，初始化 list_lock 和 partial 链表头。



### alloc_kmem_cache_cpus

alloc_kmem_cache_cpus 用于申请 slab_cpu，percpu 类型的内存分配需要使用 __alloc_percpu 接口，同时只会分配当前 CPU 所属的 slab_cpu ，当其它 CPU 第一次尝试从 slab_cpu 分配内存时，同样需要申请其 CPU 所属的 slab_cpu，才能执行分配，关于 percpu 内存的分配可以参考 TODO。

slab_cpu 申请完成之后，再调用 init_kmem_cache_cpus 初始化该结构。



创建缓存对象的源码属于内核中相对细节的实现，也就没有直接贴代码了，毕竟贴上也没人看，重点在于把整个过程讲清楚，代码是会随着内核的升级而变化，背后的原理总归不会变。 





## 创建常用的 slub 缓存对象

上文针对 slub 的自举阶段以及创建缓存对象进行了解析，slub 初始化的第二部分为创建一系列常用的 slub 缓存对象，主要提供给 kmalloc 使用。

```
void __init kmem_cache_init(void)
{
	...
	setup_kmalloc_cache_index_table();
	create_kmalloc_caches(0);
	...
}
```





