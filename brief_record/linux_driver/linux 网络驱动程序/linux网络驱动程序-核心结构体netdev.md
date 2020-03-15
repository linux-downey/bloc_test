# linux 网络驱动程序 -- 核心结构体0
linux 网络驱动程序的编写，需要先了解两个核心的结构体：struct net_device 和 struct sk_buff,这一章我们来讨论第一个：struct net_device.  

外设的主机驱动不论是 gpio 还是 i2c 又或者是 USB，都会存在一个核心的结构体来描述一个对应的硬件控制器，这个结构体中记录了该外设的硬件和软件特性。   

网卡也不例外，对于网卡而言，不论是虚拟网卡(loopback)还是真实存在的网卡，都对应一个核心结构体： struct net_device。  

实际上，软件的模型几乎都是基于硬件的结构来设计的，延伸到通信协议上也是一样的道理，整个网络协议栈的实现基本上对应网络的分层模型。所以，软硬件是相对的，同时，通过分析一个硬件的核心结构体，我们也可以大概知道这个硬件所实现的功能。   

接下来我们就来看看 struct net_device 结构体的具体实现。  

## struct net_device 实现
从 2.4 版本以来，网络协议栈经历了多次升级，struct net_device 也变得越来越庞大，庞大到想要搞清楚其中每一个成员的详细功能几乎不可能(对于博主而言)，所以，在基于 4.14 版本的基础上，博主将只会介绍其中主要的成员，对于一些不那么重要或者出场率低的成员就不作过多讨论。   


## 起点
网卡驱动子系统的开始于 net/core/dev.c 的 net_dev_init 初始化函数，这个函数被 subsys_initcall 修饰，所以将会在系统启动的时候被自动加载到内存中并运行，该函数主要负责一些初始化以及资源的提前申请，包括创建 /proc 下的用户接口、创建队列、初始化软中断等，实现流程这里不作详细讨论。   

这里主要关注在初始化过程以及子系统运行过程中常使用到的一个变量：init_net，这是 struct net 类型的变量，它被定义在 /net/core/net_namespace.c 中，其中有一个成员 

```
struct list  dev_base_head;
```

这个成员就是所有注册到系统中的 net_device 的链表头，也就是说，当开发者调用 register_netdevice(netdev) 将一个网卡注册到内核中，下面这条语句就会被间接调用：

```
list_add_tail(netdev->dev_list, &init_net.dev_base_head);
```

该 net_device 结构就会被链接到 dev_base_head 链表中，内核就是通过 init_net.dev_base_head 这个链表来管理所有的网卡，这不禁让我想起系统中如果存在多个 gpio 控制器、i2c 控制器时，也会由一个全局链表管理，当需要索引某个控制器时往往就是从轮询链表开始，网卡驱动和其它外设实现是一致的。  

## net_device 成员的分类
对于 net_device 这种复杂的数据结构，包含了描述一个网卡的所有信息，其中的信息主要可以分为下列的几种类型：

* 配置
* 统计数据
* 设备状态
* 列表管理
* 流量管理
* 功能专用
* 通用
* 函数指针

### 配置
以下是 net_device 中的配置字段：
* char	name[IFNAMSIZ] ：网卡的名称，以字符串的形式指定，同时也可以是格式化的形式，比如 "net%d"，系统会自动分配索引号，从 net0 开始递增。  
* unsigned long mem_end、mem_start：描述设备所用到的共享内存，用于设备与内核沟通，它的使用局限在网卡的设备驱动程序中，与上层协议无关。这两个成员为非必须初始化成员，只有在需要使用共享内存时会被用到。  
* unsigned long base_addr：外设基地址，通常由设备树中的 reg 属性指定，而且是必须的。 
* int irq：中断号，通常由设备树指定
* atomic_t carrier_changes：载波开/关次数的统计信息，"载波侦听"是一种通用的说法，表示通过检测线路的电平来确定当前总线上是否有主机在发送数据，事实上对于不同的物理层通信方式，载波侦听的方式也是不同的。  
  

* unsigned char	if_port：接口使用的端口类型，这里的端口类型指的是物理上的网络连接线端口，接口不同代表不同的接线方式和电气特性,比如双绞线、同轴电缆。该枚举类型的定义在 include/uapi/linux/netdevice.h。  
* unsigned char	dma：该驱动所使用的 dma 通道，通常由设备树指定。
* flag、gflag、priv_flags：flag 是一组掩码的集合，其中某些位代表网络设备的功能，某些位则代表状态的改变，可以在 include/linux/if.h 中找到 flag 所代表的所有状态集合，比如我们在用户空间使用 ifconfig 时，其中的 UP / LOOPBACK / RUNNING 等标志位就是由 flag 中的 IFF_UP、IFF_LOOPBACK、IFF_RUNNING 表示的。
    priv_flags 用于存储用户空间不可见的标识，所以前缀为 priv，由 VLAN 和 bridge 虚拟设备使用，gflags 目前基本上不再使用。  
* netdev_features_t	features：与 flag 作用差不多，这是另一组标志位，用于存储其他一些设备功能.
* mtu：Maximum Transmission Unit，最大传输单元，表示当前设备能处理帧的最大尺寸，在以太网中，常见的取值为 1500，这个值是一个折中的选择，对于同一个大小的包，太小的 mtu 设置会增加分包个数，从而增加传输次数，加大网络负担。而太大的 mtu 在发送大数据包是将会连续占用太多的总线时间，更容易产生数据包的碰撞，在 CSMA/CD 的冲突解决算法中，一旦产生碰撞，双方都会丢弃已经发送的部分数据重新发送，总的来说就是大的数据包更容易产生碰撞且产生碰撞所带来的带宽浪费更大。  

* unsigned short type：设备所属的类型，在 include/linux/if_arp.h 中声明该类型的列表。
* unsigned short hard_header_len、min_header_len：最大和最小的硬件头长度，比如 Ethernet 的报头长度是 14 字节。根据协议类型的不同而不同。 

* unsigned char	broadcast[MAX_ADDR_LEN]：广播地址，以太网是 0xFFFFFFFFFFFF。
* unsigned char	*dev_addr、unsigned char addr_len：设备的地址，地址的长度由 addr_len 决定，在以太网中为 mac 地址，在 eth_type_trans 函数中被使用。  

* unsigned int promiscuity:表示进入混杂模式的次数，在内核中，当需要进入到混杂模式时，需要调用 dev_set_promiscuity(dev,1),而取消混杂模式时，调用 dev_set_promiscuity(dev,-1)，两个函数的操作就是对 promiscuity 进行 +1 或者 -1 操作，设备是否处于混杂模式取决于 promiscuity 是否为 0，这种实现和引用计数机制类似，因为常常会出现多个用户设置网卡进入混杂模式。  

    混杂模式在不同的网络中有不同的表现，在非混杂模式下，网口接收到目的地址不是本机 mac 地址的包将会被丢弃，而在混杂模式下则接收并处理所有的包，在有限以太网中，如果是共享电缆或者 hub 连接多台主机，一个主机可以接收到当前共享电缆上所有主机的包，但实际的情况通常是主机之间通过路由器或者交换机相连，这种情况下想要接收目的地为其它主机的包变得困难。
    
    但是如果是 wifi 局域网，情况就完全不一样，wifi 能抓取到一定范围内空气中所有的包，甚至可以是不同的局域网。  


### 统计数据
* struct net_device_stats stats：网卡的统计数据，当我们使用 ifconfig 指令显示网卡信息时，同时会显示统计信息，包括发送/接收的包数量，字节数量，以及发送错误包数量、丢弃的数量等等，这些统计数据将会在驱动程序中使用，通过回调函数返回给用户程序，在新版本的内核中，通常可以使用 rtnl_link_stats64 回调函数的实现来替代这个结构成员，统计数据将会在后续的章节中详细介绍。  

### 设备状态

* unsigned long	state：由网络队列子系统使用的一组标识，队列是用于数据的传输，其索引值是 enum netdev_state_t,在 include/linux/netdevice.h 中定义， 最常见的使用在 net_if_stop_queue 和 net_if_start_queue 函数中，这两个函数就是通过设置该值实现的。

* enum {...}reg_state：设备的注册状态。  

* unsigned long	trans_start：最近的一个帧传输的时间，由 txq_trans_update 函数设置，单位是 jiffies。

* void 			*atalk_ptr;
* struct in_device __rcu	*ip_ptr;
* struct dn_dev __rcu     *dn_ptr;
* struct inet6_dev __rcu	*ip6_ptr;
* void	*ax25_ptr;
指向特定协议的专用数据结构，每个数据结构都包含一些针对该协议的私有参数。  



### 列表管理
* struct list_head dev_list：链表节点，用于将当前 netdevice 链接到全局链表中。
* struct list_head napi_list：链表节点，用于将当前节点链接到 NAPI 全局链表中，NAPI 是有别于中断的一种数据包接收模式，该模式将在后续的驱动中讲解。  
* struct hlist_node	name_hlist、index_hlist：网卡名称的 hash 值和 index 的 hash 值，作为节点链接到全局 hash 链表中。  
* struct list_head unreg_list,close_list:注销和关闭网卡时，将会把网卡添加到该全局链表节点中。  
* struct netdev_hw_addr_list mc：mc 意为 multicast，除了 IP 层有多播技术，链路层也有，该节点用于管理链路层的多播地址。  

### 流量管理
* struct netdev_queue *_tx:网卡的传输队列，通常一个网卡使用一个队列进行数据传输，而某些网卡支持多队列，多队列通常是伴随着 SMP 的支持，多个队列就相当于并发地处理网卡数据。  

    * struct Qdisc *qdisc：qdisc 是Linux 流量控制系统的核心，也被称为排队规则，在数据发送的过程中，并不是直接调用硬件发送函数，而是先将待发送数据包 push 到队列中，该数据包会在合适的时候被调度发送，以实现流量的控制。至于qdisc 的实现细节暂不做讨论。    
    * unsigned long	trans_timeout：发送的超时时间，
    * spinlock_t _xmit_lock:数据发送提供的锁机制，控制数据包在一个队列中的发送串行化。  
    * int xmit_lock_owner：持有 _xmit_lock 的 CPU ID，对于网卡中多队列的情况，通常是一个 CPU 负责一个队列的管理。  
    * unsigned long	trans_start：该队列中上一个被发送的数据包的时间(jiffies)值。  
    * unsigned long	state:队列的发送状态，常用的值有 XOFF/XON，用于管理上层接口数据包的 push 和 pop。   
* unsigned int num_tx_queues、real_num_tx_queues：发送队列的个数，num_tx_queues 指向内核申请的队列个数，real_num_tx_queues 指系统中处于激活状态的队列。  
* struct Qdisc *qdisc：root qdisc 节点，用于流量控制。  


### 功能专用
* struct vlan_info __rcu	*vlan_info;
* netdev_features_t	vlan_features;
VLAN 专用，也就是虚拟局域网。

### 通用
* int __percpu *pcpu_refcnt：linux 中经典的引用计数管理机制，注意这个 __percpu 前缀，它表示在 SMP 系统中，每个 CPU 都拥有一个 refcnt，当引用计数为都为 0 时，该结构会被释放。    
* int watchdog_timeo，
* struct timer_list	watchdog_timer：看门狗，也是计数器，负责管理数据包的超时判断，在数据发送时可以设置 watchdog ，超时将会调用 netdev_ops->ndo_tx_timeout 回调函数。   

napi 中，调用该接口将 NAPI 加入。
void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
		    int (*poll)(struct napi_struct *, int), int weight)


### 函数指针
const struct net_device_ops *netdev_ops：网卡操作函数的集合。
* ndo_init/ndo_uninit:分别在注册和注销的时候被调用的回调函数。   
* ndo_open/ndo_close:分别在打开和关闭时被调用的回调函数。
* ndo_start_xmit(skb，dev)：这是网卡驱动的核心回调函数，当上层需要调用网卡发送数据时，将会调用该回调函数，驱动接口需要实现这个接口将数据从网卡发出。该成员将在后续文章中详解。
* ndo_select_queue：当网卡支持多个队列传输时，需要实现这个接口以确定在传输时使用哪个队列。
* ndo_set_mac_address：设置 mac 地址，实现这个接口将会改变网卡的 mac 地址。
* ndo_validate_addr：测试 mac 地址是否可用,这个回调函数将会在打开网口(dev_open)的时候使用。
* ndo_do_ioctl:当用户的 ioctrl 指令不能被处理时将会调用该函数，如果该函数也没实现，就会返回错误。
* ndo_change_mtu：当用户想要修改 mtu 值时将会被调用，在内核函数 dev_set_mtu 中被调用。  
* ndo_tx_timeout：在数据时可以使能 watchdog 计时，超时时将会调用该函数。
* ndo_get_stats64/ndo_get_stats：当用户获取统计信息时会被调用，最常见的使用就是用户使用 ifconfig 时。
* ndo_poll_controller：用于 NAPI 模式的 poll。
* ndo_set_features：设置或者更新 features ，features 通常是一些硬件上的功能特性和设置。
* ndo_set_rx_mode:设置链路层的接收模式。
* 


const struct ethtool_ops *ethtool_ops：网卡配置操作的集合。
这是一组网卡的管理接口，专门针对网卡的配置，将会在后续的文章中详细讲解。  

const struct header_ops *header_ops：网卡的链路层头部的操作集合，对于以太网而言，被赋值为 eth_header_ops，具体实现可以查看 net/ethernet/eth.c 文件。  
* create：这是在发送数据包时调用，创建链路层头部，事实上的操作是在 skb 中的特定位置添加链路层头部。  
* parse：这通常在接收数据包时调用，解析头部。
* cache、cache_update：缓存头部和缓存刷新，使用缓存的意义在于直接使用缓存总是比从原始数据中取出头部效率要高。
* validate：验证头部，但是以太网头部的验证并不会用到这个函数。  

本文主要参考 《深入理解Linux网络技术内幕》，由于该书是基于 2.6 内核版本，博主是基于 4.14 内核版本进行讨论，其中有很多细节上的差异，但是总体框架没变。在参考书籍的基础上博主仔细比对了源码，作出一份 4.14 版本的结构体解析，如果有理解偏差或错误的地方，望不吝赐教。  

