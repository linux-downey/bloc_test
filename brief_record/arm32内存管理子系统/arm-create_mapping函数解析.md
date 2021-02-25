# arm-create_mapping 函数解析



## struct map_desc

```c
struct map_desc {
	unsigned long virtual;        // 虚拟地址
	unsigned long pfn;            // 物理页帧号
	unsigned long length;         // 以页为单位的长度
	unsigned int type;            // 内存类型
};
```

```c

static void __init create_mapping(struct map_desc *md)
{
    // 判断是不是 user region，是则返回
    // 判断是不是超出 vmalloc 范围，是则警告，继续工作。
    __create_mapping(&init_mm, md, early_alloc, false);
}
```

__create_mapping 需要申请内存，对应的申请函数为 early_alloc，因为这个时候伙伴系统还没有完成初始化，因此还是得使用 memblock，而且申请的内存都是低端内存，高端内存还没准备好。 

同时，这个时候的 mapping 还没有完全建立完成，因此要注意虚拟地址的访问是否合法。 

```c
memblock.bottom_up 为 true 表示从低地址向高地址分配，而 false 则反之。 
static void __init *early_alloc_aligned(unsigned long sz, unsigned long align)
{
	void *ptr = __va(memblock_alloc(sz, align));
	memset(ptr, 0, sz);
	return ptr;
}

static void __init *early_alloc(unsigned long sz)
{
	return early_alloc_aligned(sz, sz);
}
```









