# linux 网络驱动程序-驱动初始化以及发送数据
在这一章节中，我们要来分析网络驱动程序的编写。  

事实上，对于开发者而言，网卡驱动框架并不复杂，主要解决的问题是如何处理数据的接收和发送，我们以一个传统的网卡 dm9000 为例，来讲解一个网卡驱动程序的基本框架。   


## 驱动框架
在驱动分析开始的第一步，有必要先了解整个网卡驱动程序的框架，网卡驱动程序和我们之前了解了其它外设驱动程序流程并无二致：
* 驱动的 probe 依旧通过设备树的 compatible 属性实现，一旦匹配上，就执行驱动 driver 部分的 probe 函数。  
* 不论是虚拟网卡还是物理网卡，都会对应一个结构体，结构体类型为 struct net_device，需要向内核申请;  
* 填充该结构体中的必要字段，然后调用 register_netdev(net_dev) 将当前网卡注册到系统中，驱动就可以使用了，当然，这基于结构体的正确填充。  

## 结构体的申请
申请 struct net_device 结构体并不是简单的使用内存申请接口，而是需要使用特定的接口，因为在结构体的申请过程中还会进行一些通用的初始化，对于以太网而言，申请的接口有两种：
* alloc_netdev(sizeof_priv, name, name_assign_type, setup)
* alloc_etherdev(sizeof_priv)

### alloc_netdev
alloc_netdev 接口接收四个参数，返回一个 struct net_device 结构体指针：
* sizeof_priv:在编写一个驱动程序时，都会在驱动程序中维护一个私有数据，调用 dev_set_drvdata(dev, data) 将该私有数据挂在 device 的 drv_data 成员中以实现后续私有数据的使用，通常该私有数据结构体都是独立地调用 kmalloc 申请的内存。
   \
   但是在网络驱动程序中并没有沿用这种设计，而是在申请 netdev 的同时就多申请一块私有数据，然后调用 netdev_priv(ndev) 返回该私有数据指针，仅从应用而言，这两者的做法产生的结果是一致的。所以，在申请 netdev 的同时需要提供私有数据结构体的大小。  
* name:作为该网卡的名称，当我们在应用层调用 ifconfig 的时候，显示的名称就是这个字段所设置的，值得注意的是，这里的名称可以使用格式化的方式，比如 "eth%d" ,系统会自动分配一个整型数给网卡，该整型数是递增的，比如如果系统中已经使用 "%d" 注册过两次网卡，那么 eth0 eth1 就已经被使用，下一个就是 eth2.  

* name_assign_type：名称分配的类型，初始化时通常设置为 NET_NAME_UNKNOWN，一共有五个枚举值，各项的详细定义可以在 include/uapi/linux/netdevice.h 中查看。  
* setup：这是一个 void (\*)(struct net_device\*) 类型的函数指针，该函数主要负责 netdev 的初始化工作和一些注册前的设置，这些设置会根据网卡的不同而不同。  

### alloc_etherdev
网络子系统针对 alloc_netdev 函数，为不同种类的接口封装了许多函数，最常用的就是 alloc_etherdev，它是针对以太网的封装，其他类型的就使用其它的封装函数，比如FDDI设备使用 alloc_fddidev 函数。  

alloc_etherdev 只接收一个参数 sizeof_priv ，返回 struct net_device 类型的结构体，那么 name、setup 的设置在哪一部分呢？  

跟踪源码其实可以发现，对于注册以太网设备，将会使用统一的 "eth%d" 命名，setup 函数也封装成统一的，如果你对封装的细节感兴趣，可以查看源代码，在后续的文章中我们也会详细解析。  



## 结构体的填充
当结构体申请完，且完成默认的初始化工作之后，就需要驱动开发者进行相应的配置，如果是完成一个简单的带网络收发数据功能的网卡驱动程序，它是非常简单的，需要关注两个核心部分：数据的发送和接收
* 设置 netdev 中的 netdev_ops 结构，从 _ops 后缀可以看出，这是一系列操作接口，对应网卡的操作，最重要的部分就是数据的发送。  
* 初始化网卡中断，网卡在接收数据时将会触发中断，这时需要在中断中处理数据的接收。  

### netdev_ops
netdev_ops 中实现了一系列的回调函数，这些回调函数将会在特定的时候被调用以实现网卡的功能，其中常用的回调函数有：
#### ndo_open 
该函数为当设备被打开时调用的回调函数，进行一些初始化工作，通常对应硬件的启动、准备工作。  

中断的注册通常也是在这个函数中完成，对于某些实现不当的驱动程序，将中断的注册放在 probe 函数中，这会造成只要该驱动代码注册到系统中，即使不使用该网卡，该中断线路也会被该设备占用。尽管这样做大多数情况下不会有问题，但是对于某些共享的中断线路，这种无理的占用会造成资源的浪费。     

而正确的做法是在真正使用的时候申请资源，在停用的时候释放资源，这也符合 linux 的设计原则。  

除此之外，软件上还会打开该网卡设备的发送队列，启动网卡数据的检测与发送，对应的接口为 netif_start_queue。  

同时，如果网卡使用工作队列、额外的内存等资源，也可以在这里进行初始化。总之，open 函数负责网卡的准备工作，实现也是相对灵活的。    

#### ndo_stop 
该函数为设备关闭时的回调函数，其实现和 open 函数相反，在 open 中注册、申请的资源，将会在该函数中一一释放，同时对硬件进行关闭或者休眠设置。

#### ndo_tx_timeout
发送超时时调用的回调函数，在网卡发送一个数据包时，可以通过设置 watchdog_timeo 和 watchdog_timer 来确定一个数据包的发送是否超时，如果超时将会调用该回调函数，在该回调函数中可以根据实际应用情况对数据包进行一些针对性的处理。  

#### ndo_get_stats64、ndo_get_stats
获取当前网卡收发数据的统计信息，当我们在 linux 下使用 ifconfig 命令时，系统会列出当前系统中活跃网卡的信息，通常是这样的：

```
eth0      Link encap:Ethernet  HWaddr B2:19:F9:1E:EB:68
          inet addr:192.168.1.104  Bcast:192.168.1.255  Mask:255.255.255.0
          inet6 addr: fe80::b019:f9ff:fe1e:eb68/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:110 errors:0 dropped:0 overruns:0 frame:0
          TX packets:155 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000
          RX bytes:14872 (14.5 KiB)  TX bytes:17171 (16.7 KiB)
```
其中，第五行开始的统计信息就是通过调用这个内核回调函数获取的，对应的数据结构为：

这两个函数的原型分别为：

```
void (*ndo_get_stats64)(struct net_device *dev,struct rtnl_link_stats64 *storage);
struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
```
其中，存放统计信息的结构体为 struct net_device_stats：

```
struct net_device_stats {
	unsigned long	rx_packets;
	unsigned long	tx_packets;
	unsigned long	rx_bytes;
	unsigned long	tx_bytes;
	unsigned long	rx_errors;
	unsigned long	tx_errors;
	unsigned long	rx_dropped;
	unsigned long	tx_dropped;
    ...
};
```
其中包含所有的网卡统计信息，包括接收/发送包数量、接收数据长度、错误次数、包丢弃次数等等。  

ndo_get_stats64 接口使用的是 struct rtnl_link_stats64* 类型结构体，这个结构体类型与 struct net_device_stats 结构体几乎一致，只是有两个区别：
* 将类型定义由 unsigned long 换成了 _u64 类型，将数据类型统一成 64 位，而 unsigned long 在 32 位系统上为 32 位类型。
* 增加一个 __u64	rx_nohandler 成员。  

一般情况下，可以不关心这两者的区别。  

struct net_device_stats stats 结构体是 netdev 中的一个成员，专门用来存放网卡的统计信息，通常在驱动程序中，收发数据的时候，会根据实际情况更新 netdev->stats 中的成员，比如在发送一包数据之前，会增加以下代码：

```
netdev->stats.tx_packets++;
netdev->stats.tx_bytes += tx_byte_cnt;
```

如果这两个成员函数(ndo_get_stats64/ndo_get_stats)没有被赋值，在使用 ifconfig 等接口获取网卡的统计信息时，将使用 netdev->stats 中的统计数据返回给调用者。  

开发者完全可以基于效率或者其他因素使用自定义的统计函数，也就是自己实现 ndo_get_stats64/ndo_get_stats 函数，填充函数中的 storage 参数(ndo_get_stats64)或者返回 struct net_device_stats* 类型的参数(ndo_get_stats)即可。  
对于这三种统计方式的使用优先级，也就是这两个统计函数同时实现的情况下，谁会被系统使用。被调用的优先级为：ndo_get_stats64 > ndo_get_stats > netdev->stats 成员。毕竟在内核注释中也有提到，推荐使用 ndo_get_stats64 以代替 ndo_get_stats 和传统的实现方式。  

#### ndo_start_xmit
实现的回调函数中最重要的函数莫过于这个回调函数了，当系统将要从网卡发送一个数据包到网络上时，将会调用这个回调函数，这个回调函数中就负责数据发送的处理，也就是驱动程序的核心部分。  

发送数据必然要涉及到硬件的操作，对于网卡而言，开发者倒是不必处理底层的的物理协议，就像操作一个串口控制器一样，当我们发送数据的时候，将数据放到网卡的数据寄存器中，然后设置网卡启动传输。这些都是寄存器的操作，寄存器直接映射到内存中，所以可以直接像读写内存地址一样来控制数据的收发。  

硬件操作比较复杂且繁琐，而且对于每一块网卡，它的操作方式都不一样，这里就不做过多讨论，大家在开发的时候需要根据硬件的编程手册来操作。在这里，我们就以 net_card_tx() 来指代发送数据的硬件操作。  

回到数据发送的回调函数中，我们来看看它的函数原型：

```
netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,struct net_device *dev);
```
接受两个参数：skb 和 dev，通常返回 NETDEV_TX_OK ，表示数据成功发送，也可以返回 NETDEV_TX_BUSY 表示网卡处于忙状态。  

dev 是代表网卡的结构体。    

skb 在整个网络子系统中是非常核心的结构体，它有哪些特殊之处呢？  

在嵌入式的开发中，免不了要接触各种通信协议，这就涉及到协议的封包和解析，尤其是在多层协议的处理，即应用协议的嵌套时，通常的做法是定义多个函数，每一个函数对应一层协议的封装和解析。比如在解析时，函数参数是待解析的数据包指针，返回解析完成后的数据 buf 地址，这样做的一个缺点就是，每解析一层协议就要将数据从一个 buf copy 到另一个 buf，效率比较低。  

同样的，网络包含多层协议，如果每一层协议的解析都单独进行，势必也会造成上述的问题，于是，linux 的网络子系统采取另一种做法：对于每一次协议的封装和解析，都申请一块的内存，因为数据的 size 在发送和接收时都是确定的，所以这片内存的 size 也是可以预知的。  

对于每一层协议，都使用一个或多个指针，指向该层的协议头或者协议数据，这样，数据发送的时候进行协议封装，将该层协议的控制信息放在预定义的地址，然后设置指针值。每一个数据包的发送和接收，会经过应用层、传输层、IP层、链路层和物理层，但是从始至终，各层都是在操作同一片内存，没有任何的数据 copy 和转发。  

该片内存就是由 struct sk_buff *skb 来描述，当 ndo_start_xmit 回调函数被调用的时候，skb 中已经包含了应用层、传输层、IP层以及链路层的协议信息，它的结构是这样的：

```
| 链路层头部帧 | IP层包头 | 传输层包头 | 应用层 | 应用数据 | 链路层校验帧尾 |
```
而 skb->data 表示指向数据的指针，该数据就是要从网卡发出的所有数据，skb->len 则是发送数据的长度，在驱动程序中，我们要将这些数据取出来然后操作硬件将其发送到网口,然后调用 dev_kfree_skb(skb) 或者其封装接口 dev_consume_skb_any(skb) 释放掉该 skb。  

##### 流量控制
由于网络的流量通常是非常大的，同时网卡的收发缓存是有限的，所有有必要在发送数据的时候进行适当的流控。  

当流量很大的时候，很可能出现上一份数据还没发送完，下一帧数据又被 push 到队列中，导致可能的数据丢失。

注意，这里的数据拥塞并不代表 ndo_start_xmit 回调函数被同时调用，如果需要防止这种情况，可以使用自旋锁的方式，但是这也避免不了数据的拥塞。因为数据在硬件上的发送通常是异步的，也就是说，ndo_start_xmit 回调函数的返回并不代表数据已经发送出去，只表示待发送数据已经放置到待发送队列中，所以顺序执行的 ndo_start_xmit 回调函数可能会将待发送队列占满。  
 
这时候就需要进行流量控制，流控的内核接口很简单，就是下列的两个接口：
```
netif_start_queue(dev);
netif_stop_queue(dev);
```
这两个函数是对立的，实现的功能是启动/停止发送队列，更直观地说就是 netif_stop_queue 阻止上层传送数据并调用 ndo_start_xmit 回调函数，netif_start_queue 表示继续接收上层传来的数据，然后在 ndo_start_xmit 回调函数中将数据从硬件发出。  

对于接口的实现细节，如果你有兴趣完全可以去查看源码，同时博主也将会在后续的文章中讨论。  

#### 数据发送小结
鉴于数据发送是整个网卡驱动的核心部分，所以需要在这里做一个小结，其实整个数据发送流程看下来是非常简单的，下面是 ndo_start_xmit 的伪代码实现：

```
//当数据发送完成后会产生发送中断
static irqreturn_t xxx_interrupt(int irq, void *dev_id)
{
    //如果产生的中断是发送中断
    if(irq_type == send_complete){
        //使能发送队列
        netif_start_queue(dev);
    }
    ...
}

static int xxx_xmit(struct sk_buff *skb, struct net_device *dev)
{
    //组织上层继续往下传递数据
    netif_stop_queue(dev);

    memcpy(send_buf,skb->data,skb->len);
    len = skb_len;
    
    //硬件发送数据
    net_card_tx(buf,len);
    
    //释放 skb
    dev_kfree_skb(skb);
}
```







工作队列
软中断


