# kmem_cache_init 解析



```c++
struct kmem_cache {
	// percpu 的 slab cache，相当于一个本地的缓存池
    struct kmem_cache_cpu __percpu *cpu_slab;
	// 分配掩码
	unsigned long flags;
    //限制struct kmem_cache_node中的partial链表slab的数量。虽说是mini_partial，但是代码的本意告诉我这个变量是kmem_cache_node中partial链表最大slab数量，如果大于这个mini_partial的值，那么多余的slab就会被释放。
	unsigned long min_partial;
    // object 的全部大小，包括元数据和对齐
	int size;		
    // object 的实际大小
	int object_size;
    /*slub分配在管理object的时候采用的方法是：既然每个object在没有分配之前不在乎每个object中存储的内容，那么完全可以在每个object中存储下一个object内存首地址，就形成了一个单链表。很巧妙的设计。那么这个地址数据存储在object什么位置呢？offset就是存储下个object地址数据相对于这个object首地址的偏移。*/
	int offset;		
    // per cpu partial中所有slab的free object的数量的最大值，超过这个值就会将所有的slab转移到kmem_cache_node的partial链表。
    
	int cpu_partial;	
    
    //低16位代表一个slab中所有object的数量（oo & ((1 << 16) - 1)），高16位代表一个slab管理的page数量（(2^(oo  16)) pages）
	struct kmem_cache_order_objects oo;

	struct kmem_cache_order_objects max;
	struct kmem_cache_order_objects min;
    // 从伙伴系统分配内存掩码
	gfp_t allocflags;	
    
	int refcount;	
	void (*ctor)(void *);
    // object_size按照word对齐之后的大小。
	int inuse;		
	int align;		
	int reserved;		
	const char *name;	
    /*kmem_cache的链表结构，通过此成员串在slab_caches链表上*/
	struct list_head list;	
	int red_left_pad;	/* Left redzone padding size */
#ifdef CONFIG_SYSFS
	struct kobject kobj;	/* For sysfs */
#endif

	/*每个node对应一个数组项，kmem_cache_node中包含partial slab链表*/
	struct kmem_cache_node *node[MAX_NUMNODES];
};
```

```c
struct kmem_cache_cpu {
    // 指向下一个可用的 object
	void **freelist;	
    // 全局唯一的 trasaction id？ 主要用来同步？
    // 这个被初始化为 cpu num，用于检测，如果在操作过程中执行了进程切换，将进程切换到另一个 CPU 上了，通不过 nid 的检查就需要进行重新操作
	unsigned long tid;	
    // slab 内存的 page 指针，当前的 page
	struct page *page;	
    // 本地slab partial链表。主要是一些部分使用object的slab?
	struct page *partial;	
};
```



```c
struct kmem_cache_node {
    // 保护数据的自旋锁
    spinlock_t list_lock;
    // slab 中 slab 的数量，slub 中 partial 好像就表示 slab？
    unsigned long nr_partial;
    // 链表节点
	struct list_head partial;
}
```



slub 对应的 API：

```c++
// 创建缓存结构
struct kmem_cache *kmem_cache_create(const char *name,
        size_t size,
        size_t align,
        unsigned long flags,
        void (*ctor)(void *));
// 销毁缓存结构
void kmem_cache_destroy(struct kmem_cache *);
// 从创建的缓存中申请内存，分配一个对象。
void *kmem_cache_alloc(struct kmem_cache *cachep, int flags);
// 与 kmem_cache_alloc 相对的。 
void kmem_cache_free(struct kmem_cache *cachep, void *objp);
```







```c++
void __init kmem_cache_init(void)
{
	static __initdata struct kmem_cache boot_kmem_cache,
		boot_kmem_cache_node;

	if (debug_guardpage_minorder())
		slub_max_order = 0;

	kmem_cache_node = &boot_kmem_cache_node;
	kmem_cache = &boot_kmem_cache;
	
    // 创建 kmem_cache_node 类型的缓存结构
	create_boot_cache(kmem_cache_node, "kmem_cache_node",
		sizeof(struct kmem_cache_node), SLAB_HWCACHE_ALIGN);

	register_hotmemory_notifier(&slab_memory_callback_nb);

	// slub 的初始化状态
	slab_state = PARTIAL;
	
    // 创建 kmem_cache 缓存结构
	create_boot_cache(kmem_cache, "kmem_cache",
			offsetof(struct kmem_cache, node) +
				nr_node_ids * sizeof(struct kmem_cache_node *),
		       SLAB_HWCACHE_ALIGN);

    // boot_kmem_cache 的自启动
	kmem_cache = bootstrap(&boot_kmem_cache);

	// boot_kmem_cache_node 的自启动
	kmem_cache_node = bootstrap(&boot_kmem_cache_node);

	// 设置和创建 kmalloc 相关的 kmem_cache.
	setup_kmalloc_cache_index_table();
	create_kmalloc_caches(0);

	init_freelist_randomization();

	cpuhp_setup_state_nocalls(CPUHP_SLUB_DEAD, "slub:dead", NULL,
				  slub_cpu_dead);

}
```



### kmalloc 的分配：

```c++
static __always_inline void *kmalloc(size_t size, gfp_t flags)
{
    // 给出的是一个编译时常亮，比如直接的常量表达式，或者 sizeof
	if (__builtin_constant_p(size)) {
        // kmalloc 申请的内存大于 KMALLOC_MAX_CACHE_SIZE 时，就会直接调用 alloc_pages 从 buddy 系统中申请内存
		if (size > KMALLOC_MAX_CACHE_SIZE)
			return kmalloc_large(size, flags);
		// 如果不是在 DMA zone 中申请，执行
		if (!(flags & GFP_DMA)) {
			int index = kmalloc_index(size);

			if (!index)
				return ZERO_SIZE_PTR;

			return kmem_cache_alloc_trace(kmalloc_caches[index],
					flags, size);
		}

	}
    // 否则就是调用 __kmalloc
	return __kmalloc(size, flags);
}

```



```c++
static __always_inline void *kmem_cache_alloc_trace(struct kmem_cache *s,
		gfp_t flags, size_t size)
{
	void *ret = kmem_cache_alloc(s, flags);

	kasan_kmalloc(s, ret, size, flags);
	return ret;
}
```















slub 相关文章：https://blog.csdn.net/lukuen/article/details/6935068

slub 相关文章：http://www.wowotech.net/memory_management/426.html