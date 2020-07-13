# linux-netlink

## netlink简介
说到内核与用户空间的交互，其本质上是通过系统调用，而具体的实现形式上各种各样，对于驱动工程师而言，使用的最多的还是 procfs,sysfs 这一类将内核数据导出到用户空间的做法，由用户向内核发起访问，陷入内核。对于一些有明显 IO 访问效率要求的，通常使用 mmap 映射的方式，以减少数据在用户空间和内核之间的拷贝次数。   

相同的是，这些常用的方式都是单向的，从用户空间到内核，只有当用户空间主动请求的时候，内核才会返回数据，通俗地说就是内核无法通知用户空间数据的到来，也无法主动地向用户空间发送数据。  

当然，使用轮询或者文件 poll 机制可以实现"伪双向"的访问机制，即内核一旦数据准备好，用户空间就能读到，但是这种方式毕竟还是靠不断的检查来达到"即时"通信的效果，依然存在一些诸如执行效率、通信效率上的问题。  

而 netlink 就是内核与用户空间的一种双向通信方式，这是一种 socket 通信，除了实现内核与用户的双向通信，netlink 还有一个比较大的优势就是支持数据的广播，这种特性使得在主从结构或者 C/S 结构中(需要数据分发的场景)有效地减少数据拷贝次数，从而提升通信效率。  

在开发应用中，netlink 受欢迎的程度并不如文件导出的方式，因此开发者对它的熟悉度也比较低。netlink 进入开发者的视线通常是因为它实现了内核与用户空间的双向通信，实际上它也可以作为一种 IPC 机制，实现多进程之间的通信，属于套接字编程的一种，相比于其他 IPC 机制的优势同样在于它的广播特性。  

在本章中，我们将详细讨论 netlink 的使用，包括内核交互与进程 IPC 两种使用方式，并给出示例。  



## netlink 创建
socket 通信最经典的使用场景是在网络通信中，这也是被大多数人所熟悉的，甚至有相当一部分开发者认为 socket 通信机制就是应用于网络通信，确实，socket 机制原本是为网络通信设计的，后来进行了扩展，socket 通信机制可以用在多种不同的通信方式，比如进程间通信、用户与内核交互。  

socket 原本为插座的意思，将插头插入插座就有电输送过来，而通信中的 socket 也是一样，表示一种数据接口，至于数据是通过网络、又或者通过本机的内存拷贝，都是可以的，总的来说，socket 实现的是通信的框架，比如数据结构与接口的定义，而具体的实现可以千变万化。  

socket 通信中，定义了三个概念，一个是域-domain，一个是 protocol-协议，另一个是 trans_type，即数据传输方式。  

可以理解为域划分了不同的实现方式或者分属不同领域的通信方式，比如网络和进程 IPC 。  

协议则是同一个域下的不同实现，比如网络中的 AF_INET 和 AF_INET6。  

trans_type 也分为多种，比如最常用的 SOCK_STREAM 和 SOCK_DGRAM，前者是面向连接的数据流，后者是数据包的格式，可能有些朋友会说，SOCK_STREAM 是稳定数据传输，SOCK_DGRAM 是不稳定的数据传输，这可能是某些教科书上的定义，针对网络通信而言，这是正确的，而针对所有基于 socket 机制的通信实现中，只能说并不一定。由于某些通信方式的特性，SOCK_DGRAM 也可以是稳定的数据传输，比如通过内存拷贝实现的 socket 不存在数据丢失的情况。 

SOCK_RAW 这种 trans_type 一般人基本使用不到，表示原始套接字，协议栈不会自动添加协议字段，在 tcp/ip 的 socket 实现中，网络调试或者黑客才会涉及，但是在其它的通信实现(域)中会使用到，比如我们今天要讨论的 netlink。  

使用 socket 通信的第一步就是调用 socket 函数获取一个套接字文件：

```c++
int socket(int domain, int type, int protocol);
```
三个参数：domain，type，protocol，也就是对应了上述我们讨论的：域、tran_type、协议，创建一个 socket 接口时需要告诉系统，当前 socket 通信使用哪种实现，该实现中的哪种协议，以及数据传输方式。    

对于 TCP、IP 协议而言，域为 AF_INET，protocol 为 0，使用系统定义好的协议非常方便，而对于 netlink 而言，要实现用户与内核的交互，通常需要在内核中自定义协议，即使是用户空间的进程 IPC 形式，也需要内核提供的支持。  

## netlink 的注册
netlink 最大支持 32 种协议，被定义在 include/uapi/linux/netlink.h 中，对于不同的平台或者版本可能对应不同的文件，要找到这个文件，你可以在内核代码中通过全局搜索 MAX_LINKS 的定义，就可以找到，而 MAX_LINKS 这个宏正是定义了最大的 netlink 协议类型数量：

```c++
#define MAX_LINKS 32
```

系统默认占用了一些 netlink 的协议号，尽管在内核代码中有定义，但是并不一定它就已经被注册到了内核中，这和内核的模块实现有关，某些内核模块占用注册了 netlink 协议号，但是并没有编译到内核中。在 include/uapi/linux/netlink.h 文件中可以看到那些被注册的协议号，其中 NETLINK_USERSOCK 可以被用于用户空间的通信，NETLINK_KOBJECT_UEVENT 被用作报告内核设备的增删改，和 udev 相关。  

作为测试，可以使用已定义的 NETLINK_USERSOCK 来测试，也可以自定义一个协议号，这里我们自定义一个协议号：

```
#define MY_NETLINK 28
```

把它添加到 include/uapi/linux/netlink.h 中，也可以在驱动中定义，其实这不是重点，加不加不影响实现，include/uapi/linux/netlink.h 中的定义只是提示别的开发者这些协议号已经被占用了，最主要是需要向内核注册这个协议号：

```
static inline struct sock *netlink_kernel_create(struct net *net, int unit, struct netlink_kernel_cfg *cfg)
```

第一个参数 net 表示网络命名空间，直接使用内核提供的全名结构 init_net 即可。  

第二个参数 cfg 就是针对当前注册协议号的配置，struct netlink_kernel_cfg 结构的定义为：

```c++
struct netlink_kernel_cfg {
	unsigned int	groups;     // 当前协议号所支持的组数量
	unsigned int	flags;      // 标志位
	void		(*input)(struct sk_buff *skb);  // 接收回调函数
	...
};
```
内核中的 config 参数直接决定了用户空间的使用，比如 groups 的设置决定了用户空间和内核中可以使用的广播组数。   

其中 flags 有两个选项，分别是 NL_CFG_F_NONROOT_RECV 和 NL_CFG_F_NONROOT_SEND，分别表示非 root 用户是否可以监听广播组或者是发送到某广播组，如果未设置该标志位，用户空间中普通用户在调用 bind 时若指定监听广播组，将绑定失败，发送也是一样。    

input 表示接收回调函数，内核中 netlink 当前协议号一旦接收到数据，将进入到该函数，netlink_kernel_create 接口默认已经进行了绑定。  


## 内核中创建 netlink 协议号
无论是内核还是用户空间使用 netlink，都需要提前注册，因为本章的示例中使用自定义的协议号，所以需要先编译一个内核模块加载到内核中：

```c++
#define MY_NETLINK 28
struct sock *sock = NULL;
static void my_netlink_rcv(struct sk_buff *skb){

}
static int __init mnetlink_init(void)
{
	int ret = 0;
	struct netlink_kernel_cfg cfg = {
		.groups = 1,     //设置组数为1，实际上内核对该组号进行处理，当小于 32 时，会自动将 groups 设置为 32.
		.input	= my_netlink_rcv,  //设置回调函数
		.flags  = NL_CFG_F_NONROOT_RECV | NL_CFG_F_NONROOT_SEND,  //允许非 root 用户监听和发送到广播组。  
	};
	sock = netlink_kernel_create(&init_net, MY_NETLINK, &cfg);
	if(NULL == sock){
		pr_err("Failed to create socket\n");
	}
	return 0;
}
```

将该部分代码编译再加载进内核即可，注册的协议号为 28.  


## netlink 使用讲解
了解了 netlink 的基本概念，就可以进入到 netlink 的使用部分了，这才是开发者最关心的问题，netlink 的使用一共分为两部分：
* netlink 作为进程间的 IPC 机制，实现进程间基本通信和广播。
* netlink 作为内核与用户空间的通信，实现基本通信和广播。


### netlink IPC 机制
netlink 在用户空间和内核空间所使用的 API 接口并不一致，用户空间使用标准的 socket 通信接口，基于 glibc 的封装，而内核空间使用内核自定义的接口。   

现在进入标准的 socket API 复习阶段：

对于 server 端而言，主要的接口为 socket、bind、accept，netlink 不需要使用 accept 接口接受请求(accept主要用作 TCP)，而是直接使用 recvmsg 阻塞读取消息，使用 sendmsg 发送消息。  

对于 client 端而言，主要的接口为 socket 以及使用 sendmsg 发送消息，同时使用 recvmsg 读取消息。  

recvmsg 和 sendmsg 这两个接口在网络编程中并不常用，TCP/IP 通信通常使用的是 read/send，其实 read/send 这两个接口只是 socket 通信收发数据系列接口中最精简的接口，除此之外，还有 readv、sendv，以及 netlink 中使用到的 recvmsg、sendmsg。  


#### readv/sendv
readv/sendv 和 read/write 差不太多，区别在于带后缀 v 的接口接受多个 buffer 作为读写缓冲区，比如当你的应用程序读写的数据量达到 10K，但是没有一个连续的 10K 缓存，只有一个 5K 的一个 6K 的，可以使用 readv/sendv 接口，从逻辑上将这两个块连接起来(实际地址是分开的)，作为数据收发的 buffer，在收到数据时，数据总是会先将第一个 buffer 填满，再依次填充后续的 buffer，读者根据返回的接收数据长度依次读取各个不连续的缓存区，直到读取完毕。   

这种做法通常被称为 scatter/gather，译为分散聚合，这种方式在内核中也很常见，比如 dma 的使用时没有合适的内存块或者硬件设备地址本来就是分散的，就需要使用 scatter/gather 方式。  

readv/writev 主要的结构体为 ：

```c++
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
```
使用一个 struct iovec 类型的指针数组，每一段分散的内存用一个 iovec 描述，再放到数组中作为参数传递到 API。 

对应的 API 定义为：

```c++
/* iov 为指针数组，包含一个或以上的内存区域描述，iovcnt 表示指针数组长度 */
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
```

#### recvmsg/sendmsg
recvmsg/sendmsg 接口是功能实现最完整的接口，同时也比较复杂，其 API 实现为：

```c++
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
```

最主要的就是第二个参数：msg，struct msghdr 结构的定义为：

```c++
struct msghdr 
{
    void         *msg_name;       // 可选的地址，在发送时需要在这个成员指定目标地址
    socklen_t     msg_namelen;    // 地址长度
    struct iovec *msg_iov;        // 分散聚合向量，可以理解为 struct iovec 类型的数组指针，描述见上文，如果不使用分散聚合而使用连续的 buffer，还是使用这个成员指定，只添加一个内存描述成员即可 
    size_t        msg_iovlen;     // 分散聚合向量长度
    void         *msg_control;    // 辅助信息
    size_t        msg_controllen; // 辅助信息长度
    int           msg_flags;      // 标志位
};

```
在数据传输中，基本上都是围绕三个要素：源、目的、待发送数据，其中涉及到的逻辑为：发送数据需要知道目的地，同时确定发送需要发送东西的信息，同时，接收者需要通过接受到的信息判断这个数据的发送源，生活中发送快递、信件都是这个道理。   

在具体的实现中，源的处理要特殊一些，对于某些协议，比如 UDP 中，发送方不需要提供源的信息提供，而 netlink 中需要提供发送源地址，这并不代表 UDP 中接收方不需要知道发送源，而是协议中自动添加了发送源的信息，这是由协议实现的。   

在 struct msghdr 结构中，msg_name 描述发送的目的地址，msg_iov 描述发送的数据以及长度，但是源并没有被指定，netlink 的实现中，源由一个 struct nlmsghdr 类型的结构体进行描述，相对于 struct msghdr 的命名，多了一个 nl 前缀，可以知道这是专门针对 netlink 的结构.  


### struct nlmsghdr
struct nlmsghdr 这个结构在整个非常重要,需要拿出来单独讲一讲,首先你需要记住一句话:不论是内核与用户之间的交互,还是用户进程之间的 netlink 交互,所传递的数据都是遵循 struct nlmsghdr 的结构.  

它的结构是这样的:

```c++
struct nlmsghdr {
	__u32		nlmsg_len;	    //整个 nlmsg 的长度,包括头部
	__u16		nlmsg_type;	    //用于指定数据类型
	__u16		nlmsg_flags;	//额外的标志位
	__u32		nlmsg_seq;	    //序号
	__u32		nlmsg_pid;	    //发送方的 pid
};
```
TCP/IP 协议中用于标识发送方和接收方使用的是 ip+port，这是因为需要跨主机通信，而 netlink 只需要一个唯一 ID 来作为 netlink 端的识别，被称为 PID(port id)指端口 ID，这里的 PID 概念上不同于进程的 PID，同时也可以使用进程 PID 作为 port id，理论上只要是唯一识别就行，而内核对应的 PID 为 0。  

该结构的 nlmsg_pid 字段保存着发送方的 pid，在发送时需要将自己的 PID 填充到该字段

看起来整个 struct nlmsghdr 结构并不复杂，描述了发送消息的各种属性,而且压根就没看到哪里有数据部分,连个指针都没有，实际上这个结构并不简单，除了头部部分，该结构还隐含了数据部分，数据部分并没有在结构体中体现出来，它的实现是这样的：

在为 struct nlmsghdr 结构申请内存空间的时候需要多申请一部分空间，这部分空间用来保存数据，即数据部分的开始是该结构体最后一个成员 nlmsg_pid 的下一个字节，也就是这个结构最后还有一个隐形的 buffer 用来保存数据部分。   

它的结构是这样的:TODO

glibc 中提供一系列的宏用于操作 struct nlmsghdr 结构：

```c++
#define NLMSG_ALIGNTO	4U            //对齐字节
#define NLMSG_ALIGN(len) ( ((len)+NLMSG_ALIGNTO-1) & ~(NLMSG_ALIGNTO-1) )  // len 对齐到 4 字节之后的值
#define NLMSG_HDRLEN	 ((int) NLMSG_ALIGN(sizeof(struct nlmsghdr)))      // nlmsghdr 对齐到 4 字节后的长度
#define NLMSG_LENGTH(len) ((len) + NLMSG_HDRLEN)                           // 传入数据长度为参数，记录整个 nlmsghdr 的长度，为数据长度+nlmsghdr结构长度
#define NLMSG_SPACE(len) NLMSG_ALIGN(NLMSG_LENGTH(len))                    // 整个 msg 所占的空间，像 4 字节对齐后的 NLMSG_LENGTH 值
#define NLMSG_DATA(nlh)  ((void*)(((char*)nlh) + NLMSG_LENGTH(0)))         // msg 的数据部分，也就是 nlmsghdr 初始地址偏移 nlmsghdr 结构的长度
#define NLMSG_OK(nlh,len) ((len) >= (int)sizeof(struct nlmsghdr) && \      
			   (nlh)->nlmsg_len >= sizeof(struct nlmsghdr) && \
			   (nlh)->nlmsg_len <= (len))                                  //判断消息是否有 len 这么长

```
建议使用官方提供的这些宏进行操作，而不是按照既往的经验进行处理，毕竟相对其它结构而言，它的实现要相对特殊一些。  

struct msghdr 结构和 struct mlmsghdr 结构的关系为:TODO.

### server 端实现
server 端实现主机的绑定、接收与发送，为了简化代码，删除一些错误判断及一些不需要的说明，需要注意的是， server 端的实现依赖于上文中在内核中创建并注册的协议号，文章中只提供一些必要的代码片段，完整的源码实现可以在我的代码仓库中下载：TODO。  

#### 创建并绑定 socket 

```c++
// 已经注册到内核的 netlink 协议号
#define MY_NETLINK 28
#define SRC_PID    100      
int netlink_init(void)
{
	int nlfd = 0;
	struct sockaddr_nl src_addr;
    
    nlfd = socket(AF_NETLINK, SOCK_RAW, MY_NETLINK);  

    // 设置地址参数
    src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = SRC_PID;
	src_addr.nl_groups = 0;
    // 绑定
    bind(nlfd, (struct sockaddr *)&src_addr,sizeof(src_addr));
}

```

创建并返回一个 socket 接口，协议族(域)为 AF_NETLINK，数据包传输方式为 SOCK_RAW，也可以使用 SOCK_DGRAM，协议号为自定义并注册到内核中的 MY_NETLINK.   

bind 函数需要指定当前主机的相关参数，比如当前 netlink 协议的 pid，由 nl_pid 指定,可以是当前进程的 PID，也可以是其它自定义的唯一 ID 号。  
nl_groups 成员指定当前 netlink socket 监听的广播组，最多可以设置 32 个组，组号并不是以 int 形式指定，而是以掩码位的方式，nl_groups 一共 32 位，每一位代表一个组。  

bind 接口的标准定义为：

```c++
int bind (int fd, struct sockaddr* addr, socklen_t len)
```

第二个参数为当前端口的地址，为 struct sockaddr 类型，标准的结构是 16 字节。实际上，这里的 struct sockaddr 类型只是泛指,对于不同的协议有不同的格式，再强制转换成 struct sockaddr 结构，网络套接字里面通常使用 sockaddr_in,netlink 中使用 sockaddr_nl,再强制转换成 sockaddr 类型.

```c++
struct sockaddr_nl {
	__kernel_sa_family_t	nl_family;	 //设置为 AF_NETLINK
	unsigned short	nl_pad;		         //恒为 0 
	__u32		nl_pid;		             // pid(port ID)唯一识别号
       	__u32		nl_groups;	         //组掩码,最多支持 32 个组
};
```

#### 数据接收
netlink 的 server 端并没有 accept 的过程，直接使用 readmsg 阻塞式地读取 socket 数据：

```c++
// 数据部分的最大长度
#define  MSGLEN 512 

void netlink_echo()
{
    int state;
    // nlhdr 保存 接收到的数据，包括发送源的信息和需要交互的数据。  
    // 申请的 nlhdr 的长度为 struct nlmsghdr 结构的长度加上数据部分的长度 MSGLEN
    struct nlmsghdr *nlhdr = malloc(NLMSG_SPACE(MSGLEN));  
    
    // iov 负责描述缓冲区信息
    struct iovec iov;
    iov.iov_base = (void *)nlhdr;
	iov.iov_len = NLMSG_SPACE(MSGLEN);
	
    // msg 是收发数据的数据结构
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    // 设置接收数据的内存区
    msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

    state = recvmsg(nlfd, &msg, 0);
    if(state < 0){
			printf("recv error!\n");
		}
		else{
			printf("recieve message:%s\n",(char *) NLMSG_DATA(nlhdr));
			printf("recv pid :%d\n",nlhdr->nlmsg_pid);
			printf("recv pid :%d\n",nlhdr->nlmsg_len);
			printf("recv pid :%d\n",nlhdr->nlmsg_type);
		}
}
```
netlink 一旦接收到数据，这些数据将被保存在 msg.msg_iov 处，也就是 nlhdr，nlhdr 中包含了数据长度、数据类型、序列号，发送方的pid(struct nlmsghdr 类型可参考上文)，同时，使用 NLMSG_DATA(nlhdr) 可以获取到数据的实体部分，再进行进一步的处理。  


### client 端实现
client 端的实现相对比较简单，我们就实现一个发送然后等待回复的示例。同样的，该示例依赖于内核中相应 netlink 协议号已经注册的情况。  

#### 创建 socket 

```c++
int init_netlink(void)
{
    int nlfd = -1;
    nlfd = socket(AF_NETLINK, SOCK_RAW, MY_NETLINK);
}

```
创建 socket 和 server 端没有区别，不做赘述。  

#### 发送数据

```c++
#define SRC_PID 101
void netlink_send(void)
{
    struct sockaddr_nl dest_addr;
    
    dest_addr.nl_family = AF_NETLINK;
	// 指定接收方的 pid，0 表示内核
    dest_addr.nl_pid = dst_pid; 
    // 指定需要广播的组号，0 表示不广播
	dest_addr.nl_groups = 0;

    // 填充需要发送的数据信息以及发送方 PID
    struct nlmsghdr *nlhdr = malloc(NLMSG_SPACE(MSGLEN));
    nlhdr->nlmsg_len = NLMSG_SPACE(MSGLEN);
    // 指定发送方 PID
	nlhdr->nlmsg_pid = SRC_PID; 
	nlhdr->nlmsg_flags = 0;
    // 填充需要发送的数据：Hello World
	strcpy(NLMSG_DATA(nlhdr),"Hello World!\n"); 
	iov.iov_base = (void *)nlhdr;

    // 构建一个 msg 结构，包括目标地址，发送数据
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

    while(1){
        // 发送数据并判断是否发送成功
        state_smg = sendmsg(nlfd,&msg,0);
        if(state_smg < 0) {
            printf("send failed\n");
        }
        sleep(1);
    }
}

```

client 发送数据主要是构建一个 struct msghdr msg 结构，分别为：
* msg.msg_name 指定需要发送目标地址，可以设置具体的 PID，也可以是广播组。
* msg.msg_iov 指定需要发送的数据，该数据为 struct nlmsghdr nlhdr 结构，其中包含发送源的 PID，因此接收方在接收到该 nlhdr 时可以通过该信息判断发送源的 PID。  



### 内核中收发数据
内核中使用 netlink 并不遵循 socket 的标准接口，而是使用内核的自定义接口，相对于用户空间的 socket 接口来说，内核专门为 netlink 实现的接口更纯粹，也更简洁，这大概是不用做一些兼容的原因。  

内核中注册一种协议号在上文中已经介绍过了，由 netlink_kernel_create 完成，该接口实现了一个接收回调函数，同时会返回一个 struct sock 结构体指针，这个结构体指针和 socket() 函数返回的 fd 一样，是整个数据交互过程的描述符。  

内核接收到数据会自动执行传入的回调函数，在回调函数中进行数据的处理,和网络驱动程序一样，内核中 netlink 收发数据的基础数据结构是 skb，和网络驱动不一样的是，netlink 协议没有逐层地添加协议头尾的过程，使用 skb 大概是因为兼容的原因。   

尽管内核驱动层使用的是 skb 结构，但是真实的数据处理依旧是使用 struct nlmsghdr 结构，这也很好理解，内核在跟用户交互的时候，用户发送过来或或者用户需要接受的就是按照 struct nlmsghdr 结构来组织的，内核中自然也使用同样的结构才更好处理。  

上文中注册内核协议号传入的是 my_netlink_rcv，我们就实现一个简单的接收处理函数：

```c++
static void my_netlink_rcv(struct sk_buff *skb)
{
    char *data = NULL;
    struct nlmsghdr *nlh;
    nlh = nlmsg_hdr(skb);
    data = NLMSG_DATA(nlh);
    // 接收到的数据指针为 data,发送方的 pid 为 nlh->nlmsg_pid
    pr_info("Recv from = %d\n",nlh->nlmsg_pid);
    pr_info("Recv data = %s\n",data);

    // 实现 handle data 接口，处理接收到的数据。
    handle_data(data);
}
```

发送数据可以直接发送或者使用广播，广播给那些正在监听指定广播组的进程，下面是一个数据发送的例子：

```c++
void send_data(void)
{
    struct sk_buff *send_skb;
    // 创建并返回一个新的 skb 结构，nlmsg_new 是基于 alloc_skb 的针对 netlink 的封装。  
    send_skb = nlmsg_new(len,GFP_KERNEL);
    char *back_str = "How are you,downey!";
    int len = strlen(back_str);	

    if(send_skb){
		char *scratch;
		struct nlmsghdr *send_nlh;
        // 为将要发送的数据扩展空间，len 为需要发送字符串的长度，第二个参数 100 是目标 PID，表示接收者的 PID 为 100。
		send_nlh = nlmsg_put(send_skb,100,0,NLMSG_DONE,len,0);
        // netlink 收发的数据是基于 struct nlmsghdr 结构的，其中包括头部和数据，这里获取指向数据的指针
		scratch = nlmsg_data(send_nlh);
        // 将需要发送的数据 copy 到待发送数据缓冲区
		memcpy(scratch,back_str,len);
        // 设置需要广播的组，0 表示不广播
		NETLINK_CB(send_skb).dst_group = 0;

        //设置发送源的 pid 为0.
        send_nlh->nlmsg_pid = 0;

        // 如果设定了需要发送的广播组，就发送广播数据
        //ret = netlink_broadcast(sock,send_skb,0,1,MSG_DONTWAIT);
        
        // 单播，第二个参数指定发送目标 PID。  
		ret = netlink_unicast(sock,send_skb,100,MSG_DONTWAIT);
		if(ret == -ENOBUFS || ret == -ESRCH){
			pr_err("Failed to broadcast,ret = %d\n",ret);
		}
	}
}
```

发送数据主要的步骤为：
* 申请并返回一个 skb 结构
* 将需要发送的数据填充到 skb 指定地址处
* 通过 netlink_unicast 或者 netlink_broadcast 发送单播数据或者广播到用户空间。    

需要注意的是，这只是一个简单的示例，实际的内核编程中需要做更多，比如使用 nlmsg_ok 判断接收的数据长度是否正确，每个接口都要判断返回值的正确性，毕竟在内核中编程需要特别谨慎。  

PS：内核中编程最实用的一个技巧就是看看别人怎么写的，毕竟能添加到内核中的代码都是经过严格检验的。  

