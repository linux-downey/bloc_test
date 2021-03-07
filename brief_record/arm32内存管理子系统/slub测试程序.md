## slub 测试程序

```c++
struct test_struct{
     unsigned long val[99];   // 长度为 396 字节
};
static unsigned long n;
static struct test_struct *objects[100];


static void slab_ctor(void *cachep){  
     n++; 
}

void __init my_slub_test(void)
{
	int i;
	struct kmem_cache *test_cachep;
	struct page *debug_page;
	struct page *debug_partial_page;
	void **debug_freelist;
	test_cachep = kmem_cache_create("slab_test_cachep", 
													sizeof(struct test_struct), 0, SLAB_HWCACHE_ALIGN, slab_ctor);
	debug_page = this_cpu_read(test_cachep->cpu_slab->page);
	debug_page = debug_page;
	debug_partial_page = this_cpu_read(test_cachep->cpu_slab->partial);
	debug_partial_page = debug_partial_page;
	debug_freelist = this_cpu_read(test_cachep->cpu_slab->freelist);
	debug_freelist = debug_freelist;

	if(!test_cachep){
			printk("alloc test_cachep failed!");
			return -ENOMEM; 
	}    
	for(i = 0;i<50;i++){
			objects[i] = kmem_cache_alloc(test_cachep, GFP_KERNEL);
	}
	kmem_cache_free(test_cachep,objects[48]);
	kmem_cache_free(test_cachep,objects[40]);
	kmem_cache_free(test_cachep,objects[12]);
	kmem_cache_free(test_cachep,objects[25]);
	kmem_cache_free(test_cachep,objects[36]);
	kmem_cache_free(test_cachep,objects[37]);
	kmem_cache_free(test_cachep,objects[5]);
	kmem_cache_free(test_cachep,objects[4]);

	for(i = 0;i<50;i++){
			objects[50+i] = kmem_cache_alloc(test_cachep, GFP_KERNEL);
	}

	kmem_cache_free(test_cachep,objects[60]);
	kmem_cache_free(test_cachep,objects[90]);
	kmem_cache_free(test_cachep,objects[62]);
	kmem_cache_free(test_cachep,objects[95]);
	kmem_cache_free(test_cachep,objects[86]);
	kmem_cache_free(test_cachep,objects[87]);
	kmem_cache_free(test_cachep,objects[58]);
	kmem_cache_free(test_cachep,objects[84]);
}


```



## qemu 调试指令

```
print test_cachep.node[0].nr_partial
print debug_partial_page
```



## 运行完 kmem_cache_create 之后

在运行完 kmem_cache_create 之后，状态如下：

```
对齐的参数依旧是 0x40
offset 参数为 396，当指定了 ctor 回调函数或者 flags & (SLAB_DESTROY_BY_RCU | SLAB_POISON) 时，size 会扩展一个指针的大小，ofsset 的值设置为 size，其它情况下，offset 通常被设置为 0。
cpu_partial 为 13
cache 的 inuse 为 396
name 为创建时传入的字符串
size 为 448，因为要向 64 字节对齐
object_size 为 396
min_partial 为 5
oo 为 9，说明一个 slab 占用一个 page，包含 9 个 object

c->freelist 没有正确的值。
```



## 运行了第一个 alloc 之后

```
c->freelist 的值为 448，也就是第二个 object 在 page 中的地址。而 page 中 448+396(offset)处保存的是第三个 object 的地址。
c->page 开始指向对应的 page 结构，注意是 page 结构的地址，而不是 page 的起始地址，page 结构是在系统启动的时候统一申请的，在 0x8ff7xxxx 地址上。
c->page->inuse 是 9，percpu 的 page 上，inuse 一直是 objects 的数量。
c->page->frozen = 1,percpu 的 page 和 partial 链表上， frozen 都是 1.
c->page->freelist 为 NULL
c->page->objects 的值为 9，也就是整个 page 中包含的 object 数量(是整个 page 还是整个 slab？)。
c->page->slab_cache 指向当前的 kmem_cache.
c->page = 0x8ff77080
```

## 运行了两个 alloc 之后

```
c->freelist 的值向后移动
```



## 运行了 9 个 alloc 之后

```
c->freelist 为 NULL,因为 slab 中不再存在空闲的 object
```

## 运行了 10 个 alloc 之后

```
c->page 的值变成了 0x8ff770a0，也就是新申请了一个 page
旧的 page 并没有挂在 c->partial 上，也没有挂在 node[0]->partial 上，因为已经用完的 page 不需要记录，在 free 时可以直接通过地址找到。 
其它的没什么区别
```

## 全部申请完之后

```
全部申请完之后的情况是这样：
一共申请了 5 个 slab，最后一个 slab 中申请了 5 个 cache，还有 4 个 cache 的余量。 

```



## free 48 号 cache 之后

```
第一个释放的是 48，这个比较简单，直接放回到当前的 slab 中即可，更新一下 freelist 和保存的下一个空闲 object 的值。 
c->freelist 的值变成了 48 号 object 对应的地址，而 48 号 object 上保存的下一个就是释放之前 c->freelist 指向的值。 
```



## free 40 号  cache 之后

```
c->partial 指针不再为空，而是 40 号对应 slab 的 page。
c->partial 对应 page 的 inuse 为 8
c->partial-> object 依旧为 9
c->partial-> frozen 为 1
c->page->freelist 不再是 NULL，而是变成了当前 slab 中的第一个空闲 object
```



## free 12 号  cache 之后

```
partial 链表上就存在两个 slab page了，第一个 page 为 partial 指针指向的，第二个 page 为第一个 page 的 next 指针指向。

```

## 重新开始 alloc

```
在 free 了多个 cache 之后，partial 页面变成了 4 个
重新开始 alloc 时，首先会填充当前的 c->page 页面，然后再重新申请新的 slab。
因此 partial 页面会变为空。
```



## cpu_partial

通过测试方式，当 cpu_partial 上链表个数超过这个值的时候，就会把 partial 链表上的 slab 全部转移到 kmem_cache_node 上，一次性转移的，不是逐个转移的。 

通过 page->lru 链接到 kmem_cache_node 的 partial 链表上。 



## min_partial

kmem_cache_node 上存在的 slab 数量的下限值。

当程序释放 cache 时，如果挂在 kmem_cache_node 上的 slab 中全部 object 都是 free 的，这时候会有两种情况。

如果 kmem_cache_node 的 partial 上数量大于等于 min_partial 时，就会直接释放掉这个 slab，让它回到 buddy 系统。

如果 kmem_cache_node 的 partial 上数量小于 min_partial 时，就会把这个空的 slab 保留下来。



# 源码运行分支解析

## 第一次 alloc

```c++
kmem_cache_alloc
    slab_alloc
    	slab_alloc_node
    		__slab_alloc
    			___slab_alloc
    				goto new_slab
    				new_slab_objects
    				goto load_freelist
    				return freelist;
```



## 耗尽一个 slab 时再 alloc

```c++
kmem_cache_alloc
    slab_alloc
    	slab_alloc_node
    		__slab_alloc
    			___slab_alloc
    				freelist = get_freelist(s, page);
    				if (!freelist) {
                        c->page = NULL;
                        stat(s, DEACTIVATE_BYPASS);
                        goto new_slab;
                    }
					new_slab_objects
                    goto load_freelist
                    return freelist;
```

在 get_freelist 中判断如果 c->freelist 为空，就会把 frozen 设置为 0，然后直接放掉这个 page，放掉不是释放掉，释放掉指的是释放会 buddy。



## 从一个 full slab 中释放一个

```c++
kmem_cache_free
    slab_free
    	do_slab_free
    		__slab_free
    			设置 new.frozen = 1;
				put_cpu_partial      
```

将 page 添加到 cpu_partial 中，同时第一个 partial 的 pobjects 保存 cpu_partial 列表中 slab 数量，当这个数量大于 s->cpu_partial 时，就将所有 cpu partial page 转移到 cache node 中。 



## 从非满非1的 cpu_partial 或者 node partial 中释放一个

```c++
kmem_cache_free
    slab_free
    	do_slab_free
    		__slab_free
    		do {
                if (unlikely(n)) {
                    spin_unlock_irqrestore(&n->list_lock, flags);
                    n = NULL;
                }
                prior = page->freelist;
                counters = page->counters;
                set_freepointer(s, tail, prior);
                new.counters = counters;
                was_frozen = new.frozen;
                new.inuse -= cnt;
               }while (!cmpxchg_double_slab(s, page,
                        prior, counters,
                        head, new.counters,
                        "__slab_free"));
```

在 cmpxchg_double_slab 中，将 page freelist 指针、page-> inuse 更新完成。



## 从 1 成员的 slab 中释放一个，释放之后 slab 为空

跟上面的一样，即使是 slab 为空，也照样保存在 partial 链表中。

只有等到 partial 链表中 page 数量大于 cpu_partial 时，才会全部转移到 node parital 链表中，在转移的同时也会判断 slab 是否为空，如果 slab 已经为空，而且转移的 page 数量大于 min_partial，才会将多余的页面释放给 buddy。

从 full 到添加到  partial 链表的接口为：put_cpu_partial

转移的接口为：unfreeze_partials

释放的接口为：discard_slab