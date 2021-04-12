# linux 网络驱动程序 - sk_buff 结构
相较于 net_device 结构而言，linux 网络子系统中还有一个更为重要的结构体：struct sk_buff，通常被简称为 skb，net_device 针对一个物理网卡，而 skbuf 的作用域则贯穿整个网络协议栈。  

在嵌入式的开发中，免不了要接触各种通信协议，这就涉及到协议的封包和解析，尤其是在多层协议的处理，即应用协议的嵌套时，通常的做法是定义多个函数，每一个函数对应一层协议的封装和解析。比如在解析时，函数参数是待解析的数据包指针，返回解析完成后的数据 buf 地址，这样做的一个缺点就是，每解析一层协议就要将数据从一个 buf copy 到另一个 buf，效率比较低。  

同样的，网络包含多层协议，如果每一层协议的解析都单独进行，势必也会造成上述的问题，于是，linux 的网络子系统采取另一种做法：对于每一次协议的封装和解析，都使用同一片内存，因为数据的 size 在发送和接收时都是确定的，所以这片内存的 size 也是可以预知的，描述这片内存的结构就是 skbuf。  


## skb 的内存结构
严格地说，skb 并不仅仅是一片内存，最简单的 skb 是由两部分组成的，一部分保存着 skb 结构体，另一部分用来保存数据，将数据与结构本身分开以降低耦合性，在某些情况下可以只对一部分进行单独处理。同时，对于某些更大的数据包，会将数据包分成多段存储，这也会导致一个 skb 占用多个不连续的内存。  

skb 是一片内存集合，这片内存中保存着网络数据，这是我们目前所知的。按照上文中的描述，这片内存理论上应该是被格式化的，也就是内存的哪一部分存被分配为存放哪一类数据，只有这样，当接收到一个完整的网络数据包放入到这片内存时，接收者才知道如何去解析这包数据。  

skb 的结构是这样的：

可以从图中看到，skb struct 本身和数据并不属于同一块内存，skb 中存在 head、data、tail、end 指针分别作为数据部分的索引节点：

head 表示整个数据内存的起始地址。
data 表示真实数据部分的起始地址。
tail 表示真实数据部分的终止地址。
end 表示整个数据内存的终止地址。

需要注意的是，这里所说数据部分不仅包含用户的应用数据，同时包含了各协议层的头部信息，所以发送数据时构建 skb 的时候，会将数据放到数据内存的中间位置而不是从头部开始存放，因为头部还需要留出空间用来存储传输层头、IP层头等信息。  


## struct sk_buff 结构
先来看看看结构体的主要成员，从成员中建立一个对 skbuf 的基本印象。  
```
struct sk_buff {

    //这两个成员主要用于 skb 的管理，多个 skb 组成一个双向链表，链表头是一个 struct sk_buff_head 类型的节点。 
    struct sk_buff		*next;     
	struct sk_buff		*prev;      

    //当前 skb 所属的套接字，struct sock 是套接字在网络层的表示。只有当数据被确认属于当前主机时，该值被赋值，否则为 NULL。
    struct sock		*sk;            

    //缓冲区中数据区块的大小，主要包括缓冲区的数据以及一些片段的数据(当数据量很大时，应用数据会分段存储)，当缓冲区从一个网络分层移往下一个网络分层的时候，其值就会变化，因为在协议栈中往上移动的时候报头就会被丢弃，向下移动会增加报头。len也会随之变化。
    unsigned len；

    //data_len 只计算数据的长度，这里的数据包括头部和应用数据。
    unsigned int data_len;
    
    //Mac报头的长度。
    mac_len  

    atomic_t users；
    //用户引用计数,一个 skb 可能由多个用户使用，当一个用户使用当前的 skb 时，将此值增一，不用了就减一操作，到 0 就释放该 skb，这是内核中典型的引用计数回收机制。

    truesize
    //当前缓冲区的总大小，包含 sk_buff 的本身。

    ktime_t	tstamp; 
    //通常只对一个已接受的封包才有意义，用于表示封包何时被接收，或者有时用于表示封包预定传输时间。netif_rx 会调用 net_timestamp 设置。

    __u8 cloned;
    //当一个 skb 结构由多个消费者处理时，就需要将该 skb 分发，但是某些时候多数消费者仅仅只需要对 skb 的 struct 部分进行操作，而并不需要对 data 部分进行写操作，这时候就没有必要赋值整个 skb(struct 和 data)，只需要复制 struct 部分，然后共用 data，并给 data 添加一个引用计数，在这种情况下，cloned 就会被设置为 1。

    其对应的接口为：skb_clone.
    如果是要复制整个 skb，对应的接口为：skb_copy.

    u8 pkt_type;
    //包的类型，这个类型的定义列表在 include/uapi/linux/if_packet.h 中,分别有以下几种：
    PACKET_HOST(当前主机的包)
	PACKET_BROADCAST(广播包)
	PACKET_MULTICAST(多播包)
	PACKET_OTHERHOST(其他主机的包)
    上层程序根据这个包类型来决定对包的处理。    

    __u32 priority:数据包入队列的优先级，也就是包处理的优先级，这是 linux 中流量控制的实现，与 QOS 相关。  

    __be16 protocol：这是上层协议类型，需要在驱动程序中确定，表示上层协议需要使用哪种协议来处理这个数据包，通常是 ETHE_P_IP(IPv4)。

    tc_* ：skb 中有一类以 tc 开头的成员，意为 traffic control，这一类属于流量控制相关,这一部分需要在内核中进行相应的配置。  

    *csum*:skb 中所有带有 csum 字段的成员都是和协议的 checksum 相关的，校验和是用于保证数据传输的正确性的，在网络协议定义的标准中，校验的工作分别在 L2、L3、L4都存在，但是在实现中并不一定需要严格地使用校验位。  

    L2 层是在 L3/L4 层数据的基础上增加一个链路层帧头、帧尾，这一层所做的校验可以保证在网络传输的过程中整个 L2/L3/L4 层的数据正确性，但是并不能保证在数据分层处理时 L3/L4 的数据正确性，所以，多数情况下，也会对 L3/L4 层进行校验以及验证，但是这样看起来这种层层校验似乎有些冗余。    



    sk_buff_data_t		tail;
	sk_buff_data_t		end;
	unsigned char		*head,
				*data;
    //分别表示数据区的起始、结束、数据开始、数据结尾。
	unsigned int		truesize;
    //包含 skb struct 和 data 部分在内的所有长度。
	refcount_t		users;
    //引用计数，表示有多少个消费者正在使用这个 skb。


    __u16			transport_header;
	__u16			network_header;
	__u16			mac_header;

    //分别表示传输层、网络层和 mac 层的头部地址，这个地址并非是系统内的地址，而是基于 skb->head 的偏移地址。  
};
```



## netdev_alloc_skb 的实现
要详细了解 skb 结构的实现，最好是去了解它是如何被创建的，这就涉及到 netdev_alloc_skb 的实现。  

alloc_skb 的调用通常是在网卡接收到数据包时，申请一个空的 skb 结构，然后将数据包放到该 skb 中，而 netdev_alloc_skb 则是在申请 skb 的基础上进行了一些网络相关的初始化操作，我们来看它的源码实现：

```C
struct sk_buff *netdev_alloc_skb(struct net_device *dev,unsigned int length)
{
	return __netdev_alloc_skb(dev, length, GFP_ATOMIC);
}

struct sk_buff *__netdev_alloc_skb(struct net_device *dev, unsigned int len,
				   gfp_t gfp_mask)
{
	struct page_frag_cache *nc;
	unsigned long flags;
	struct sk_buff *skb;
	bool pfmemalloc;
	void *data;

	len += NET_SKB_PAD;

    //SKB_WITH_OVERHEAD(PAGE_SIZE) 为 PAGE_SIZE - sizeof(struct skb_shared_info),，在 PAGE_SIZE 为 4K 的情况下，该值为 3900 左右，通常数据包的大小并不会大于这个数值。
    //同时，传入的 gfp_mask 为 GFP_ATOMIC ,所以该 if 判断为 false.
	if ((len > SKB_WITH_OVERHEAD(PAGE_SIZE)) ||
	    (gfp_mask & (__GFP_DIRECT_RECLAIM | GFP_DMA))) {
		skb = __alloc_skb(len, gfp_mask, SKB_ALLOC_RX, NUMA_NO_NODE);
		if (!skb)
			goto skb_fail;
		goto skb_success;
	}

    //设置数据对齐
	len += SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
	len = SKB_DATA_ALIGN(len);

    ...
	nc = this_cpu_ptr(&netdev_alloc_cache);
    
    //从高速缓存中申请内存 page
	data = page_frag_alloc(nc, len, gfp_mask);
	pfmemalloc = nc->pfmemalloc;

    //构建一个 skb
	skb = __build_skb(data, len);
    ...

skb_success:
	skb_reserve(skb, NET_SKB_PAD);
	skb->dev = dev;

skb_fail:
	return skb;
}

```
从上面的源码可以看到，当申请大容量的 skb 时，会使用 \_\_alloc_skb 接口，否则使用 \_\_build_skb 接口，\_\_build_skb 传入一个 data 指针和 len。  

该 data 就是 skb 的数据部分所指代的内存，由 page_frag_alloc 接口申请，len 是对齐之后的数据长度,接下来我们继续看 __build_skb 的实现：

```C
struct sk_buff *__build_skb(void *data, unsigned int frag_size)
{
	struct skb_shared_info *shinfo;
	struct sk_buff *skb;
	unsigned int size = frag_size ? : ksize(data);

    //申请一个 skb 结构
	skb = kmem_cache_alloc(skbuff_head_cache, GFP_ATOMIC);
	if (!skb)
		return NULL;

    //对齐 skb 结构
	size -= SKB_DATA_ALIGN(sizeof(struct skb_shared_info));

	memset(skb, 0, offsetof(struct sk_buff, tail));
	skb->truesize = SKB_TRUESIZE(size);
    
    //增加一个引用计数
	refcount_set(&skb->users, 1);

    //设置 skb 的指针
	skb->head = data;
	skb->data = data;
	skb_reset_tail_pointer(skb);
	skb->end = skb->tail + size;

    //初始化协议头
	skb->mac_header = (typeof(skb->mac_header))~0U;
	skb->transport_header = (typeof(skb->transport_header))~0U;

	初始化 shareinfo
	shinfo = skb_shinfo(skb);
	memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
	atomic_set(&shinfo->dataref, 1);

	return skb;
}
```

对照上述的源码，build_skb 做了这些事：

* 使用 kmem_cache_alloc 申请一个 skb，这一片 skb 的内存是从高速缓存中分配的，高速缓存机制是针对特定申请/释放非常频繁的设备，产生一片专用的内存区域，在释放的过程中并不是真正地释放，而是缓存下来等待下一次的申请，使用高速缓存机制，内存不会被真正地回收，当再次申请时就不需要重新从系统中分配，对效率有明显的提升。  

* 设置 skb 成员，其中包括 truesize，truesize 包含 skb struct + data 的总长度，实际上还包含一个 shareinfo 结构，该结构将在后文中详解。 

    增加引用计数，同时设置 head、end、data、tial 四个指针，其中因为目前 skb 中还没有存放任何数据，所以 head、data、tail 同时指向内存的起始位置，而 end 指向内存的终止位置。  

    初始化 mac_header(mac 头) 和 transport_header(传输层头)为 ~0U，也就是将其中所有字节设置为 0xFF，这两个字段指示 mac 头和传输层头相对于 head 的起始地址，并不是对应的协议内容。

* 初始化 shareinfo：shareinfo 是一片特殊的内存，用来存放网络中分段的数据，我们将在下文中详细讨论该结构。  


这就是整个 skb 的构造实现，当 

```
if ((len > SKB_WITH_OVERHEAD(PAGE_SIZE)) || (gfp_mask & (__GFP_DIRECT_RECLAIM | GFP_DMA)))
```

这个判断语句为 true 时，会调用 alloc_skb 接口来构造 skb，其实现和 build_skb 接口几乎一致，也是分别申请一个 data 部分和一个 skb struct ，然后初始化 skb struct 中的各项成员，这两者的主要区别在于针对不同的数据包大小，data 部分的内存处理方式不一样，有兴趣的朋友可以自行阅读源码。  


### shinfo 结构
首先需要明确的一个问题是，shinfo 是干什么的？通常，以太网的 MTU 被设置成 1500 bytes, 但是 IP 层的封包的最大尺寸为 64K，当大量的数据从 IP 层发送过来时，数据链路层需要将这些数据进行分段处理，，这些被分段的数据到最后还是需要被还原成一个完整的包，程序需要对这些数据段建立内在联系，shinfo 结构就是用于管理这些分段的数据包的。  

在 skb 结构的成员中，并没有 struct skb_shared_info 这一项，那么，一个 skb 的 shareinfo 是如何找到的呢？  

内核提供 skb_shinfo(skb) 这个接口访问 skb 的 shareinfo,查看源代码： 

```
#define skb_shinfo(SKB)	((struct skb_shared_info *)(skb_end_pointer(SKB)))
static inline unsigned char *skb_end_pointer(const struct sk_buff *skb)
{
	return skb->end;
}
```
从源码实现可以清晰地看到，shareinfo 内存的起始地址被安排在 skb->end 处，也就是说，在申请 data 部分的内存时，除了 data 本身的 size，还会加上 sizeof(struct skb_shared_info), 这也不难解释为什么 skb 申请函数 __netdev_alloc_skb 中的 if 判断是：

```
if ((len > SKB_WITH_OVERHEAD(PAGE_SIZE)) ||
	    (gfp_mask & (__GFP_DIRECT_RECLAIM | GFP_DMA)))
```
在第一次阅读源码的时候就感到疑惑，为什么不是 len > PAGE_SIZE 而是 len > SKB_WITH_OVERHEAD(PAGE_SIZE)，其中 SKB_WITH_OVERHEAD(PAGE_SIZE) 的值为 PAGE_SIZE-sizeof(struct skb_shared_info) ,这是因为需要在一页中保留 sizeof(struct skb_shared_info) 的空间。  

同时，如果不进入到 if 的 true 分支，首先执行的语句就是：

```
len += SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
...
data = page_frag_alloc(nc, len, gfp_mask);
```
明显可以看出，在申请 data 内存空间 size 的时候，就已经包含了 sizeof(struct skb_shared_info) 的内存。  

包含 shareinfo 的 skb 接口见下图：TODO


### shareinfo 的实现

我们来看看这个结构的定义：
```
struct skb_shared_info {
    ...
    //分段的数目
	unsigned char	nr_frags;

	struct sk_buff	*frag_list;

	atomic_t	dataref;

	/* must be last field, see pskb_expand_head() */
	skb_frag_t	frags[MAX_SKB_FRAGS];
};
```

在 skb_shared_info 列出了几个主要使用到的成员:
nr_frags 表示分段的数量，主要是协助 IP 层记住该分包挂了多少个子分段(主要是将 IP 包分段).

frag_list： 指向分段的子 skb 结构。对于数据的分段而言，每一个分段都是一个 skb 结构，通过该字段可以找到下一个数据段的 skb 结构。  
dataref：引用计数。 

frags：这个成员是一个数组， MAX_SKB_FRAGS 的定义是 #define MAX_SKB_FRAGS (65536/PAGE_SIZE + 1)，通常 PAGE_SIZE 为 4K，所以该值为 17，也就是 IP 层最多一次传输 64K 数据，最多可以存放到 17 个页面中。这个数组就是用来管理 IP 层传来的这些应用数据，当然



整个结构可以参考下图:TODO




## skb 的函数接口
在上文中讲解了 skb 的结构，，接下来我们就来讲讲 skb 的操作函数，最常用的操作就是收发数据和 skb data 部分的交互，当上层往下层走时，发送数据，需要往 skb 中加入数据，当收到数据时，从下层往上层走，需要验证或者取出数据进行处理，这时候并不销毁数据，仅仅是移动 skb->data 和 skb->tail 指针，或者设置其他定位指针，比如：skb->mac_header 等，其中，skb->head 和 skb->end 总是固定的。  

在 netdev_alloc_skb 函数中，skb->data 、skb->tail、skb->head 在初始化时是同一个值，在下列的函数讨论中，都基于这样的模型：
TODO


### skb_push
```
void *skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data -= len;
	skb->len  += len;
	if (unlikely(skb->data<skb->head))
		skb_under_panic(skb, len, __builtin_return_address(0));
	return skb->data;
}
```
skb_push 函数就是将 skb->data 指针往前移 len 个单位，并返回当前的 skb->data，也就是将 data 上移,通常这是为了操作 header，先给 header 留出 len 个单位的空间，然后可以使用 memcpy 将数据拷贝到 data 处，就完成了 header 的赋值。  



### skb_pull

```
void *skb_pull(struct sk_buff *skb, unsigned int len)
{
	return skb_pull_inline(skb, len);
}
static inline void *__skb_pull(struct sk_buff *skb, unsigned int len)
{
	skb->len -= len;
	BUG_ON(skb->len < skb->data_len);
	return skb->data += len;
}
static inline void *skb_pull_inline(struct sk_buff *skb, unsigned int len)
{
	return unlikely(len > skb->len) ? NULL : __skb_pull(skb, len);
}
```

从上述实现可以看出，skb_pull 函数仅仅是将 skb->data 往下移，skb->len 相应地减少，这个接口和 skb_push 执行相反的操作，通常会丢弃头部。  

### skb_reserve

```
static inline void skb_reserve(struct sk_buff *skb, int len)
{
	skb->data += len;
	skb->tail += len;
}
```
将 skb->data 和 skb->tail 同时下移，在 skb data 部分的头部预留空间，这个接口通常会在分配的时候被调用，强迫对齐数据边界。因为只是预留空间，所以 skb->len 并不会改变。  

比如以太网帧头只有 14 个字节，但是需要对齐到 16 字节，使用该函数：skb_reserve(skb,2);将会在头部预留出两个字节，这样 IP 报头就可以从缓冲区开始按照 16 字节进行对齐，并紧接在以太网报头之后，对齐可以提高访问效率。   

### skb_put

```
void *skb_put(struct sk_buff *skb, unsigned int len)
{
	void *tmp = skb_tail_pointer(skb);
	SKB_LINEAR_ASSERT(skb);
	skb->tail += len;
	skb->len  += len;
	if (unlikely(skb->tail > skb->end))
		skb_over_panic(skb, len, __builtin_return_address(0));
	return tmp;
}
```

skb_put 接口将 tail 部分下移，返回原 tail 指针，通常是在需要将应用数据插入时使用，调用完 skb_put，就可以使用 memcpy 将应用数据拷贝到原 tail 处，需要注意的是，如果插入的数据超过了 end 指针，也就是溢出了，将会引发内核 panic。  



## skb 在网络协议栈中的应用

在前文中有提到过，skbuf 的作用域则贯穿整个网络协议栈，它是如何起作用的呢？

参考下面的图：
TODO

从上层往下走，先根据协议预留各层的头部空间，通常还需要预留对齐字节，具体做法就是将 data 下移，然后在每一层将头部控制字段数据拷贝到 skb 中。  

从下层往上走，就是一个完整的网络数据包放到 skb 中，然后层层解析的过程。  


参考：<深入理解linux网络技术内幕>

