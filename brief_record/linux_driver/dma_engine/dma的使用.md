# linux DMA 简介及设备树配置
在众多的外设中，DMA 算是一个比较特殊的存在，它算是一个 CPU 的辅助设备，帮助 CPU 解决一些纯数据搬运的工作，比如外设接收的数据到内存，又或者是内存中的数据需要copy到外设，这两种情况通常对应于外设数据的接收和发送，这些部分可以交由 dma 来完成，而不用额外地占用 CPU 的时间。  

比如：串口发送数据，一般的做法是通过程序将数据一个个放到硬件串口控制器的发送寄存器中，通过查询串口控制器的标志位来确定发送是否完成以发送下一个字节，这个过程对于 CPU 来说是非常慢的，正如上文所说，我们可以交给 dma 来完成这项工作，指定数据原地址(内存)，指定数据目标地址(外设发送寄存器)，就可以让dma自动地搬运这些数据，直到数据全部收发完成。  

使用 dma ，可以大大提高数据处理时的工作效率。

## linux 中的 dma
在简单的系统比如单片机中，dma 仅仅作为一个外设，在使用 dma 时无非是一些寄存器的配置，设置内存，并不复杂。  

但是在 linux 中，情况就有所不同，linux 中由内存引入的复杂性导致 dma 的操作也变得复杂。  

虚拟内存的存在对 dma 并不友好，虚拟地址、物理地址和外设地址的概念以及之间的相互转换并不是一个简单的事。  

同时，dma 在操作时通常要求物理地址的连续，这给系统的内存分配带来一定的复杂性，因为在linux中几乎所有的内存分配都是基于虚拟地址的，某些接口分配的内存并非物理连续，从而无法被dma使用。  

再者，dma 和 CPU 一样，都有对系统总线的控制权，相对于外设一样，它担任的是 master 的角色，但是在一些特殊的情况下，dma 控制器的寻址能力并不能达到理想中的状态，可能只有24 位的地址线，这就导致它无法访问到全部的内存，从而在 dma 的使用上会有一些限制。  

最后在于内存一致性的问题，由于现代处理器引入的缓存机制，通常一个CPU配备多级缓存，在大多数情况下都可以提高程序的运行效率，而 dma 对应的是直接的内存操作，很可能造成缓存与内存中数据不一致从而带来内存一致性问题导致效率的降低。当然，在内核中也可以使用特定的接口比如：dma_alloc_coherent() 来申请 uncache 的内存，但是这又损失了性能。在内存的操作中，缓存命中和内存一致性问题从来都是最大的麻烦。

由于本系列文章暂时只讲解 dma 的框架使用，对于dma与内存的讨论不过多深入。关于 linux 中 dma 使用的问题可以参考这一篇博客：[Dynamic DMA mapping Guide](http://www.wowotech.net/memory_management/DMA-Mapping-api.html)  


## linux dma 框架简介
正如上文所说，dma 本身的配置并没有多么复杂，但是由于 dma 直接面对的是复杂的内存系统，加上可能存在的一些硬件寻址限制，从而也成为了一个让人头疼的部分。  

在此之前，我们已经分析过 pwm、i2c、spi 等常见外设的驱动框架，根据这些经验，不难猜测，dma 框架可能是这样的：
整个 dma 驱动框架分为 controller 和 consumer：
controller 对应 dma 控制器，是整个dma的控制核心，根据配置提供 dma 的传输功能。

而 consumer 部分则是dma框架提供给外设驱动开发者使用dma的一系列接口，开发者通过这些接口对dma进行配置以及使用，对于开发者而言，并不需要知道 dma 实现的具体细节，从而大大降低了开发难度。  

真实的情况也正是如此，dma 驱动核心部分在内核中被命名为 dmaengine,对应的代码文件为：
drivers/dma/dmaengine.c/h。 

接下来我们就看看 dmaengine 的具体实现。  


## 设备树配置

## controller 节点
自从设备树的面世之后，所有的资源描述和管理几乎都离不开设备树的配置，当然 dma 也不例外。  

在设备树中，每一个 dma 控制器都对应一个设备树节点,下面以 beaglebone 平台为例，看看一个 dma 控制器节点的内容：

```
edma@49000000 {
    compatible = "ti,edma3-tpcc";
    ti,hwmods = "tpcc";
    reg = <0x49000000 0x10000>;
    reg-names = "edma3_cc";
    interrupts = <0xc 0xd 0xe>;
    interrupt-names = "edma3_ccint", "edma3_mperr", "edma3_ccerrint";
    dma-requests = <0x40>;
    #dma-cells = <0x2>;
    ti,tptcs = <0x2e 0x7 0x2f 0x5 0x30 0x0>;
    ti,edma-memcpy-channels = <0x14 0x15>;
    phandle = <0x2d>;
};
```
compatible 属性用作驱动匹配。
reg 属性标识该控制器的外设地址，第一个参数表示基地址，第二个参数表示 size。
interrupts 属性标识该 dma 控制器支持的中断属性，在后续的三个参数中是如何描述中断属性的，需要根据中断控制器中的 #interrupt-cells 来确定：
```
interrupt-controller@48200000 {
			compatible = "ti,am33xx-intc";
			interrupt-controller;
			#interrupt-cells = <0x1>;
            ...
		};
```
在该平台中，#interrupt-cells 为 1，则表示中断的指定不带其他参数，该 dma 控制器享有 0xc 0xd 0xe 三个中断号，这个参数同样可以通过查找编程手册来求证：
![]()
TODO

interrupt-names 属性指定了中断号对应的名称，这个属性并不常见。  

与其他通用外设比如 i2c spi 不一样的是，dma 是服务于其他外设的期间，需要经常被其他外设节点引用，所以需要指定 #dma-cells 属性，该属性决定了其他外设在引用 dma 时需要指定的参数个数。  

phandle 属性相当于该节点的 UID，用于识别。

其他的属性例如 ti,hwmods、ti,tptcs 等都是一些自定义的平台相关数据，用于传递一些平台相关数据，并非 dmaengine 的必要参数。  

## consumer 设备树节点
一旦 dma controller 配置完成，驱动开发者就可以使用 dma 的功能了，在使用前需要指定当前设备所使用的 dma controller 和 dma 通道，每一个 dma 传输都将分配一个 dma 通道，这个通道可能是硬件决定的，也可能是可以灵活配置的，这取决于不同的硬件平台的实现。  

我们以 spi 为例，看看是其他外设是如何使用 dma controller 的服务的：

```
spi@48030000 {
    ...
    dmas = <&dma1 0x10 0x0 &dma1 0x11 0x0 &dma1 0x12 0x0 &dma1 0x13 0x0>;
    dma-names = "tx0", "rx0", "tx1", "rx1";
    ...
}
```
其他设备引用 dma controller 节点主要是通过 dmas 和 dma-names 两个参数，在上面的 dma controller 节点中，#dma-cells 的值为 2(参数个数)，所以 dmas 对应的参数中以三个数值(目标节点 + 两个参数)为一组，描述一个 dma 服务。  

以第一组为例：0x2d 0x10 0x0，第一个为 dma controller 的 phandle 属性，表示引用的目标 dma controller，后面两个参数则取决于硬件特性，通常有一个参数为 dma 通道编号，而另一个有可能是 dma 模式设置、传输特性设置等，平台相关。  

设备树提供了硬件参数，具体的解析还需要参考具体的实现代码，在驱动实现的代码中，开发者可以通过解析该设备树节点获取 dma 设备并进行相应操作。  


## dma 的使用

### dma 的相关数据结构
作为一个开发者，迫切需要了解的是我们该如何使用它，在此之前，我们需要先了解几个dmaengine 相关的结构体：

struct dma_device : 这个结构体用来描述一个 dma controller，每一个 dma device 对应一个 dma 硬件控制器，这个结构体我们在后文中讲解。  

struct dma_chan： 真正 dma 的传输是通过一个个的物理通道来完成的，struct dma_chan 则是控制 dma 传输的核心结构体，保存一个 dma 通道传输的核心数据。它的内容是这样的：

```C
struct dma_chan {
	struct dma_device *device;   //该 dma_chan 对应的 controller
	dma_cookie_t cookie;         //上一次传输的 cookie 值，s32 类型
	dma_cookie_t completed_cookie;  //上一次完成传输的 cookie 值，cookie 值用于标识传输过程，用于后续传输过程的跟踪和监控

	int chan_id;                 //传输通道 ID，针对 sysfs
	struct dma_chan_dev *dev;    //class device，针对 sysfs

	struct list_head device_node; //链表节点，用于将当前结构连接到 dma_device 的 channels 链表中，便于 controller 对通道的记录和管理
	struct dma_chan_percpu __percpu *local; 
	int client_count;             //记录使用该通道的客户端数量
	int table_count;              //记录内存到内存分配表中出现的次数

	/* DMA router */
	struct dma_router *router;    //dma 路由结构
	void *route_data;             //dma 路由结构的私有数据
 
 	void *private;                // dma_chan 的私有数据  
};
```


struct dma_slave_config：dma 在使用前需要进行一系列的配置，而该结构体就负责当前 dma 通道的配置，结构体内容如下：

```
struct dma_slave_config {
	enum dma_transfer_direction direction;  //dma 的传输方向，通常是 DMA_MEM_TO_DEV,DMA_DEV_TO_MEM。
	phys_addr_t src_addr;                   //dma传输的源物理地址，如果该地址是内存，该参数将会被忽略
	phys_addr_t dst_addr;                   //dma传输的目标物理地址，如果该地址是内存，该参数将会被忽略
	enum dma_slave_buswidth src_addr_width; //源地址传输带宽，通俗地说就是一次读取数据的宽度
	enum dma_slave_buswidth dst_addr_width; //目标地址传输带宽
	u32 src_maxburst;                       //源地址 burst 模式下的最大数据量
	u32 dst_maxburst;                       //目标地址 burst 模式下的最大数据量
	....
	bool device_fc;                         // 流控制设置
	unsigned int slave_id;                  //保存 slave id
};
```
熟悉或者使用过 dma 的朋友都了解这些参数，要使用一个 dma 传输数据，或者对于通用的数据传输，都离不开三个要素：源、目的、大小。  

在 dma 中，还需要设置一次读取、写入的数据宽度， burst 传输的数据量，流控等。当然，最必要的还是设置源、目的、大小。  


struct dma_async_tx_descriptor：dma 传输的描述符，用于标识一次 dma 的传输，它与 dma_chan 的不同的地方在于：dma_chan 是用于描述 dma 控制器的物理通道，通道是硬件上固定存在且唯一的。  描述符则是基于某通道上的一次 dma 传输，基于同一个通道的描述符可以是多个，即一个通道可以同时存在多个传输请求，而每个传输请求对应一个描述符，只是这些请求将逐一地被 dma 控制器串行执行。  

dma_cookie_t：dma 某一次传输的标识符，这个标识符的类型为 s32 类型，可以利用这个标识符对传输过程进行控制、查询等操作。  


### dma 接口的使用
了解了 dma 各数据结构，我们再来看看 dmaengine 的使用流程。  

#### 获取一个 dma_chan
既然 struct dma_chan 用于描述一个 dma 通道，首先我们就需要获取当前设备使用的 dma 通道对应的 dma_chan 结构体：

```
struct dma_chan *dma_request_chan(struct device *dev,const char *name)
```

该函数传入 dev 和 name 做参数，返回当前 dev 对应的 struct dma_chan 结构，对于 4.0 以上的内核，基本上都是使用设备树的方式来指定，而设备树节点一一对应 struct device，所以 dma_request_chan 函数根据设备树节点中的 dmas 和 dma-names 属性定位到当前设备所使用的 dma 控制器以及通道。  

#### 配置 dma
获取了 dma_chan 结构之后，需要对当前 dma 通道进行配置，配置使用以下的接口：

```
int dmaengine_slave_config(struct dma_chan *chan , struct dma_slave_config *config)
```

第一个参数表示需要配置的目标通道，第二个参数表示配置数据，参考上文中对 struct dma_slave_config 结构的介绍，它将配置源、目的、size 以及数据宽度等参数。  

#### 获取传输描述符
获取传输描述符，也就是准备进行数据传输可以使用下面的接口：

```
struct dma_async_tx_descriptor *dmaengine_prep_slave_single(struct dma_chan *chan, dma_addr_t buf, size_t len,enum dma_transfer_direction dir, unsigned long flags)
```
传入 dma_chan, dma_buf 和 buf_size,并设置传输方向，就可以获取一个 dma 传输描述符。  

值得注意的是，在上文中 struct dma_slave_config 的介绍中有提到，如果 dma 配置中的 src_addr/dst_addr 是内存地址，则在调用 dmaengine_slave_config 配置时该参数会被忽略，在 dmaengine_slave_config 阶段忽略 src_addr/dst_addr 地址并不是 dma 传输不需要指定内存地址，而是将内存地址的指定放到了当前阶段，也就是在获取描述符接口的第二个参数传入内存地址。   

除了 dmaengine_prep_slave_single 还有一个常用的获取描述符的接口是 dmaengine_prep_slave_sg：

```
struct dma_async_tx_descriptor *dmaengine_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl,	unsigned int sg_len,enum dma_transfer_direction dir, unsigned long flags)
```

与第一个接口不同的是，接口名后缀由 single 变成了 sg，sg 的全称为 scatter gather，即分散集合模式，这是 dma 一个特殊的传输模式。在一些特定的情况下，它允许 dma 将多个源/目标地址的多段传输以链表的形式组合成一次传输，准确地说是一次触发多次传输，而不用CPU逐次地进行设置。  

所以这个接口多了一个参数：struct scatterlist *sgl，表示一个传输的数据集合。  


另外两个并不太常用的同类型接口是：dmaengine_prep_dma_cyclic 和 dmaengine_prep_interleaved_dma，一个标识 dma 的循环传输模式，而另一个标识内部内存之间的 dma 传输，即传输方向为 MEMTOMEM.


值得注意的是，在获取了 dma 通道描述符之后，我们同时可以对该描述符进行配置，通常是设置 callback 成员，即：

```
struct dma_async_tx_descriptor *desc = dmaengine_prep_slave_sg(chan,sgl,sg_len,MEMTODEV,0);
desc->callback = dma_comlete_callback;
desc->callback_param = param;
```
该 callback 就是在该次 dma 传输完成之后将要调用的回调函数。  

#### 提交当前 dma 传输请求
设置妥当之后，需要提交当前 dma 请求，这时候就需要使用 dma_submit 接口：

```
dma_cookie_t dmaengine_submit(struct dma_async_tx_descriptor *desc)
```
这个接口比较简单，传入上文中获取的描述符即可，返回一个 dma_cookie,这个 cookie 是本次传输的唯一 id ，可以通过这个 cookie 进行查询等操作。  

比如通过 dma_async_is_complete 接口查询是否传输完成：

```
static inline enum dma_status dma_async_is_complete(dma_cookie_t cookie,
			dma_cookie_t last_complete, dma_cookie_t last_used)
```

比如查询当前传输状态：

```
enum dma_status (*device_tx_status)(struct dma_chan *chan,dma_cookie_t cookie,struct dma_tx_state *txstate);
```

提交请求并不代表启动了 dma 的传输，尽管看起来 dma 控制器支持多个物理通道甚至是虚拟通道的同时传输请求，但是其本质上还是由 dma 控制器对这些传输请求进行仲裁，以一定的顺序对这些请求进行串行传输，所以我们只能提交传输请求给dma 控制器。  

#### 传输就绪并等待结束
软件提交之后还需要最后一步：设置传输就绪，将当前提交添加到就绪列表，由 dma 控制器在合适的时间进行传输：

```
void dma_async_issue_pending(struct dma_chan *chan)
```

调用这个接口即表示设置了就绪状态，接下来要做的就是等待传输的完成，完成之后会自动进入到回调函数。  


### dma 设置流程总结
dma 配置以及使用的整个流程就是上文中所介绍的，需要注意的是，上述接口的调用并非是在同一时刻顺序调用的，获取 dma_chan 通常是在初始化的时候就进行了调用，而配置、获取描述符、提交dma请求等操作则是在需要进行数据交互的时候才会调用。   
对于 rx 的 dma 经常(大概率)会由接收中断(或者工作队列)来配置，对于 tx 的 dma 配置经常是当用户调用发送接口的时候进行配置。   


### dma 其他接口
除了上述提到的使用 dma 必要的接口之外，还有一些其他的辅助接口可以使用：

#### 停止通道上的 dma 传输

```
int dmaengine_terminate_sync(struct dma_chan *chan)
int dmaengine_terminate_async(struct dma_chan *chan)
```
sync 和 async 的区别在于：async 只是做了一个停止的动作，并不会等待当前 dma 传输停止之后再返回，适合在原子上下文中使用，而 sync 则会等待当前的 dma 传输完全停下来再返回。  

#### 暂停和恢复 dma 传输

```
int dmaengine_pause(struct dma_chan *chan)
int dmaengine_resume(struct dma_chan *chan)
```

#### 检查传输结果

```
enum dma_status dma_async_is_tx_complete(struct dma_chan *chan,
          dma_cookie_t cookie, dma_cookie_t *last, dma_cookie_t *used)
```
利用 dmaengine_submit 返回的 cookie 对传输过程进行追踪以及检查，查看当前传输状态，最常用的就是检查传输是否完成。  



## dmaengine 底层实现
知其然并知其所以然，知道 dma 如何使用，进一步的，自然是要了解 dmaengine 框架的实现原理，在 dma 框架的简介中我们就提到，整个 dma 框架由两部分组成：dma controller 和 dma consumer。   

dma controller 是 dmaengine 的核心部分，向开发者提供 dma 配置及使用接口，负责 dma 请求的接收和执行。  

dma consumer 可以看成是提供给驱动开发者的一系列操作接口，开发者可以使用这些接口配置并使用 dma 进行数据传输，正如我们前面章节所说的 dma_request_chan、dmaengine_submit 等接口。  

要了解整个 dma 框架的实现，我们就需要进入其实现代码，来看看它们是如何被注册到内核中并如何发挥作用的。  

在本次的框架分析中，我们采取由外到内、由表及里的方式对整个 dma 框架进行分析，方向依次是：

dma consumer 接口的使用 --> dma consumer 接口源码实现  --> dma controller 的源码实现

在上一章节部分我们已经介绍了 dma consumer 接口的使用，这一章节我们来讨论 dma consumer 接口的源码实现。  


## dma consumer 源码实现

### dma_request_chan
dma_request_chan 接口是一切的开始，表示当前设备申请一个 dma 物理接口，通常对于一个 CPU 而言，在硬件设计阶段就已经决定了 dma 通道与外设之间的对应关系，这是一个固定的一对一或者一对多的映射关系，所以哪个外设使用哪个通道通常是固定的并在设备树中进行设置。   

```
从技术上来说，dma 控制器的任意通道配置成任意外设可使用这是可以做到的。
但是这种做法将带来硬件上的复杂性从而增加成本，在大多数场景下并没有什么必要。  
```

正如前文所言，在设备树中，某个外设如果需要用到 dma 通道，可以直接对 dma 控制器进行节点引用：

```
spi@48030000 {
    ...
    dmas = <&dma1 0x10 0x0 &dma1 0x11 0x0 &dma1 0x12 0x0 &dma1 0x13 0x0>;
    dma-names = "tx0", "rx0", "tx1", "rx1";
    ...
}
```

dma_request_chan 的实现源码如下：

```
struct dma_chan *dma_request_chan(struct device *dev, const char *name)
{
	struct dma_chan *chan = NULL;
	...
	if (dev->of_node)
		chan = of_dma_request_slave_channel(dev->of_node, name);
	...
	return chan ? chan : ERR_PTR(-EPROBE_DEFER);
}


```

由于当前(2019年)使用的内核基本上都是以设备树的形式管理硬件，我们在这里就只关注设备树的方式，对于 dma controller 静态注册的方式就不再过多讨论。  

从源码可以看到，dma_request_chan 以同样的参数调用了 of_dma_request_slave_channel，接着跟踪源代码：

```C
struct dma_chan *of_dma_request_slave_channel(struct device_node *np,
					      const char *name)
{
	struct of_phandle_args	dma_spec;
	struct of_dma		*ofdma;
	struct dma_chan		*chan;
	int			count, i, start;
	int			ret_no_channel = -ENODEV;
	static atomic_t		last_index;

	//判断传入参数
	if (!np || !name) {
		return ERR_PTR(-ENODEV);
	}
	//检查设备树节点成员 dmas
	if (!of_find_property(np, "dmas", NULL))
		return ERR_PTR(-ENODEV);

	//检查设备树节点成员 dma-names
	count = of_property_count_strings(np, "dma-names");
	if (count < 0) {
		return ERR_PTR(-ENODEV);
	}

	start = atomic_inc_return(&last_index);
	for (i = 0; i < count; i++) {
		//根据 dmas 和 dma-names 获取目标 dma 控制器节点
		if (of_dma_match_channel(np, name,(i + start) % count,&dma_spec))
			continue;
		// 获取 ofdma 结构
		ofdma = of_dma_find_controller(&dma_spec);
		//调用 ofdma 的 of_dma_xlate 成员函数，获取最后的 dma_chan 结构。
		if (ofdma) {
			chan = ofdma->of_dma_xlate(&dma_spec, ofdma);
		} else {
			chan = NULL;
		}

		of_node_put(dma_spec.np);

		if (chan)
			return chan;
	}
	return ERR_PTR(ret_no_channel);
}
```
从源码可以看出：
* 在使用设备树的方式获取 dma_chan 时，dma_request_chan 接口的两个参数都不能为 NULL，一方面 struct device 结构用于定位当前设备的设备树节点从而找到对应的 dmas 和 dma-names 结构，另一方面，每一个设备并不一定只使用一个 dma 通道，所以需要使用 name 属性来指定使用的是当前设备关联的哪一个 dma 通道。  
* 判断当前设备节点中是否存在 dmas 和 dma-names 成员，如果不存在，返回解析错误，如果存在，获取 dma-names 的数量。
* 根据 dma 通道的数量，依次进行匹配，调用 of_dma_match_channel 获取目标 dma controller，存放在 dma_spec 结构中，dma_spec 中包含了目标 dma controller 的设备树节点指针，以及引用参数(通道、模式等)。  
	of_dma_match_channel 的实现其实就是调用了 of_parse_phandle_with_args 函数对节点引用进行解析，这个接口在之前的博客中有详细的介绍。TODO

* dma controller 根据引用参数就可以找到注册到内核中对应的 ofdma 结构，再调用 ofdma->of_dma_xlate，就可以解析出对应的 dma_chan 并返回。  

由此就在内核中申请到了我们想要的 dma_chan 结构。  

dma 的引用和我们之前提到的 gpio、pwm 的引用是一致的，第一个元素指定目标 controller 节点，后面的参数用于指定使用该 controller 的哪一项资源。  

在这一部分我们有两个细节部分需要注意：
* of_dma_find_controller 接口是如何通过 dma_spec 为参数获取到 ofdma 结构的？
* ofdma 的函数成员 of_dma_xlate 是在哪里被注册的？且它做了什么？

要在这里解答这两个问题并不容易，因为这涉及到 dmaengine 的核心部分，我们将这两个问题留到后面，在剖析 dma controller 的时候再行解答。  


### dma 其他配置接口
除了 dma_request_chan 之外，其他的配置接口比如 dmaengine_slave_config、dmaengine_prep_slave_sg、dmaengine_submit、dma_async_issue_pending 的源码实现都非常简洁：

**dmaengine_slave_config**：
```
static inline int dmaengine_slave_config(struct dma_chan *chan,
					  struct dma_slave_config *config)
{
	if (chan->device->device_config)
		return chan->device->device_config(chan, config);

	return -ENOSYS;
}
```

**dmaengine_prep_slave_sg**
```
static inline struct dma_async_tx_descriptor *dmaengine_prep_slave_sg(
	struct dma_chan *chan, struct scatterlist *sgl,	unsigned int sg_len,
	enum dma_transfer_direction dir, unsigned long flags)
{
	if (!chan || !chan->device || !chan->device->device_prep_slave_sg)
		return NULL;

	return chan->device->device_prep_slave_sg(chan, sgl, sg_len,
						  dir, flags, NULL);
}
```

**dmaengine_submit**
```
static inline dma_cookie_t dmaengine_submit(struct dma_async_tx_descriptor *desc)
{
	return desc->tx_submit(desc);
}
```

**dma_async_issue_pending**
```
static inline void dma_async_issue_pending(struct dma_chan *chan)
{
	chan->device->device_issue_pending(chan);
}
```

从这些源码可以看出，其实这些接口的实现就是调用了 dma controller 在内核中注册的回调接口，从这个角度来看，整个 dma 框架的层次其实并不复杂。   

话说回来，内核中的 dma 涉及到内存的部分确实比较麻烦，但是封装只有这么薄薄的一层，只能说明 dma 并不是很好封装，这跟 dma 的强硬件相关性有很大的关系。而在内核中为了减少开发人员的负担，所以将大量的复杂工作转移到了 controller ，对于 consumer 而言只需要调用一些回调接口即可。   



## dma controller 的实现
针对不同的平台，dma controller 的实现都有所差异，这里我们专注于框架核心部分的理解，也就是 dmaengine 抽象出来的公共部分。    

对于 dma controller 实现的其它平台相关部分，自然也是难以避免地要涉及到一些，我们还是以 beaglebone 平台(am335x系列，内核版本 4.14)为例，来讲解 dma controller 的实现。  

在前面的章节中有提到，struct dma_device 是一个 dma controller 的抽象，我们先来看看一个 dma_device 的结构：

```C
struct dma_device {

	unsigned int chancnt;                // dma 控制器支持的通道数量
	unsigned int privatecnt;             // 记录有多少个 dma 通道是通过 dma_request_channel 申请的，就目前而言，申请 dma 通道一般是使用 dma_request_chan
	struct list_head channels;           // 链表头，dma 的通道由 dma_chan 进行抽象，而所有属于该 controller 的 dma_chan 都将链接到当前链表中，方便管理 
	struct list_head global_node;        // 链表节点，当前的 dma_device 都通过该链表节点被链接到全局链表 dma_device_list 中，而 dma_device_list 链表中记录了所有的 dma_device，即记录所有 dma controller。
	...
	#define DMA_HAS_PQ_CONTINUE (1 << 15)

	int dev_id;                          //当前 dma device 的 UID,作为识别
	struct device *dev;                  //对应的设备节点

	u32 src_addr_widths;                 //当前 dma controller 支持的源地址数据宽度
	u32 dst_addr_widths;                 //当前 dma controller 支持的目标地址数据宽度
	u32 directions;                      //当前 dma controller 支持的传输方向
	u32 max_burst;                       //当前 dma controller 支持的最大 burst size
	bool descriptor_reuse;               //描述符是否可重用

	int (*device_alloc_chan_resources)(struct dma_chan *chan);   //回调函数，申请通道资源
	void (*device_free_chan_resources)(struct dma_chan *chan);   //回调函数，释放通道资源

	struct dma_async_tx_descriptor *(*device_prep_dma_memcpy)(
		struct dma_chan *chan, dma_addr_t dst, dma_addr_t src,
		size_t len, unsigned long flags);                        //回调函数，针对 MEMTOMEM 的传输

	struct dma_async_tx_descriptor *(*device_prep_slave_sg)(     
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context);                     //回调函数，被dmaengine_prep_slave_sg 调用
	struct dma_async_tx_descriptor *(*device_prep_dma_cyclic)(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags);                                    //回调函数，被 dmaengine_prep_slave_cyclic 调用
	...

	int (*device_config)(struct dma_chan *chan,
			     struct dma_slave_config *config);              //回调函数，被dmaengine_slave_config 调用
	int (*device_pause)(struct dma_chan *chan);                 //回调函数，被 dmaengine_pause 函数调用
	int (*device_resume)(struct dma_chan *chan);                //回调函数，被 dmaengine_resume 函数调用

	enum dma_status (*device_tx_status)(struct dma_chan *chan,
					    dma_cookie_t cookie,
					    struct dma_tx_state *txstate);         //回调函数，被 dmaengine_tx_status 函数调用,用于查询当前的传输状态，返回dma_status 枚举值，分别有：DMA_COMPLETE(完成)、DMA_IN_PROGRESS(正在执行)、DMA_PAUSED(暂停)、DMA_ERROR(错误)
	...
	void (*device_issue_pending)(struct dma_chan *chan);        //回调函数，被 dma_async_issue_pending 函数调用
};
```
对于 dma_device 中的部分成员进行省略，说实话，对于 dma 框架提供的一些稍微复杂的模式和功能，我也没有搞懂，在这里只是斗胆讨论一些最为常见的使用模式。    

可以直接从结构体的内容看出，其中记录了通道信息以及各种回调函数，这些回调函数将会被 consumer 部分的函数接口直接调用，所以在 dma controller 的初始化阶段，需要初始化这些回调函数。  

通常需要初始化的部分有：
* cap_mask：表示当前 dma controller 支持的功能与一些标志位
* directions/src_addr_widths/dst_addr_widths：当前 dma controller 支持的传输方向以及数据传输的宽度类型
* 各种回调函数，正如上文所示，这些回调函数将会被 consumer 的接口直接调用
* struct device 结构继承 platform_device 的 device 结构，同时初始化 channels 链表用于管理各通道的 dma_chan。


初始化这些必要的成员之后，就需要将当前的 dma_device 注册到内核中，注册接口为：

```
int dma_async_device_register(struct dma_device *device)
{
	//一系列参数检查
	list_for_each_entry(chan, &device->channels, device_node) {
		rc = -ENOMEM;
		chan->local = alloc_percpu(typeof(*chan->local));
		if (chan->local == NULL)
			goto err_out;
		chan->dev = kzalloc(sizeof(*chan->dev), GFP_KERNEL);
		if (chan->dev == NULL) {
			free_percpu(chan->local);
			chan->local = NULL;
			goto err_out;
		}

		chan->chan_id = chancnt++;
		chan->dev->device.class = &dma_devclass;
		chan->dev->device.parent = device->dev;
		chan->dev->chan = chan;
		chan->dev->idr_ref = idr_ref;
		chan->dev->dev_id = device->dev_id;
		atomic_inc(idr_ref);
		dev_set_name(&chan->dev->device, "dma%dchan%d",
			     device->dev_id, chan->chan_id);

		rc = device_register(&chan->dev->device);
		if (rc) {
			free_percpu(chan->local);
			chan->local = NULL;
			kfree(chan->dev);
			atomic_dec(idr_ref);
			goto err_out;
		}
		chan->client_count = 0;
	}
	...
	list_add_tail_rcu(&device->global_node, &dma_device_list);
}
```

dma_async_device_register 负责将当前的 dma_device 注册到内核中，其主要的工作就是初始化并注册当前 dma controller 支持的物理通道，软件上就是 dma_chan， 熟悉内核驱动的对 device_register 这个接口一定不会陌生。   

同时，将当前的 dma_device 链接到全局链表中，方便内核对所有的 dma controller 进行统一管理。  


除了 dma_device 的注册，设备树节点注册到内核也非常重要，它是开发者通过设备树节点引用获取 dma_chan 的关键：

```
int of_dma_controller_register(struct device_node *np,
				struct dma_chan *(*of_dma_xlate)
				(struct of_phandle_args *, struct of_dma *),
				void *data)
{
	struct of_dma	*ofdma;

	if (!np || !of_dma_xlate) {
		pr_err("%s: not enough information provided\n", __func__);
		return -EINVAL;
	}

	ofdma = kzalloc(sizeof(*ofdma), GFP_KERNEL);
	if (!ofdma)
		return -ENOMEM;

	ofdma->of_node = np;
	ofdma->of_dma_xlate = of_dma_xlate;
	ofdma->of_dma_data = data;

	list_add_tail(&ofdma->of_dma_controllers, &of_dma_list);

	return 0;
}
```

从源码可以看出，of_dma_controller_register 接口接受三个参数，第一个是当前设备即 dma controller 的设备树节点，第二个是设备树引用解析的回调函数，第三个参数是数据部分。  

在该注册函数中，构建了一个 ofdma 结构，该 ofdma 的定义如下：

```
struct of_dma {
	struct list_head	of_dma_controllers;
	struct device_node	*of_node;
	struct dma_chan		*(*of_dma_xlate)
				(struct of_phandle_args *, struct of_dma *);
	...
	void			*of_dma_data;
};
```
每一个 dma controller 的设备树节点都对应一个 ofdma 结构。  

其中 of_dma_controllers 是一个链表节点，所有的 ofdma 都会通过该节点链接到全局链表 of_dma_list 中，同时保存了 of_node 结构，以及 of_dma_xlate 引用解析函数。   

of_dma_controller_register 所实现的内容也非常清晰，将传入的参数进行赋值并将当前的 ofdma 结构链接到全局链表 of_dma_list 中。

#### 问题解答
不知道你还记不记得我们在 dma_request_chan 接口的源码解析部分提出的两个问题：

* of_dma_find_controller 接口是如何通过 dma_spec 为参数获取到 ofdma 结构的？
* ofdma 的函数成员 of_dma_xlate 是在哪里被注册的？且它做了什么？

在这里我们就可以做出解答：

为了讲解清楚，博主就在这里讲得啰嗦一点。  

首先，假设需要使用 dma 的节点是 spi，它在设备树中对 dma controller 节点进行引用：

```
spi@48030000 {
    ...
    dmas = <&dma1 0x10 0x0 &dma1 0x11 0x0 &dma1 0x12 0x0 &dma1 0x13 0x0>;
    dma-names = "tx0", "rx0", "tx1", "rx1";
    ...
}
```

dma controller 的节点是这样的：

```
dma1 : edma@49000000 {
    ...
    #dma-cells = <0x2>;
    ...
};
```


在设备树中因为是节点引用，dmas 的第一个数据就是 &dma1(dma controller 的 phandle) ,所以很容易就可以找到对应的 dma_spec 结构，of_dma_find_controller 传入的参数为 dma_spec。  

该 dma_spec 为 struct of_phandle_args类型，专门用于描述设备树中的引用，它的定义如下：

```
struct of_phandle_args {
	struct device_node *np;
	int args_count;
	uint32_t args[MAX_PHANDLE_ARGS];
};
```
成员为被引用的目标节点即 dma controller 的 device_node.
参数的个数
参数数组


而每一个 dma controller 对应的 of_dma 都被链接到全局链表 of_dma_list 中，我们只需要遍历整个 of_dma_list 链表,对比 of_dma->of_node 和 dma_spec->np 是否是同一个 device_node 即可。如果两个成员指向同一个 struct device_node 节点，说明该 of_dma 就是我们要找的。  

ofdma 的函数成员 of_dma_xlate 是在哪里被注册的？在上文中已经得到解答，至于 of_dma_xlate 做了什么，我们继续往下分析。  

of_dma_xlate 回调函数是被 dma_request_chan 间接调用的，它接收两个参数，struct of_phandle_args(mda_spec) 中包含目标节点的信息以及引用的参数信息，第二个参数为 of_dma,同时,该函数返回一个 dma_chan 的结构。  

不难想到，dma controller 为每个物理的 channel 分配了一个 dma_chan 结构，然后以链表的形式进行组织，或者以其它的形式存放起来,其它设备想要获取它对应的 dma_chan 结构时，通过设备树指定请求某个 dma controller 的某个通道，dma controller 根据这个参数找到对应的 dma_chan 结构并返回，就像在仓库中根据清单找货物一样，这并不难。   


### dma 框架其它细节
对于 dma 框架的讲解，大家应该有注意到，博主除了在讲解 dma_async_device_register 和 of_dma_controller_register 这一类内核通用函数时进行了源码讲解，对于其它的部分比如各回调函数的实现没有仔细去深挖。   

了解内核的朋友都清楚，回调函数的存在本身就是针对平台差异性，根据某个平台去深挖一个回调函数的实现并没有什么代表性、也不具备通用性，可能反而会让人疑惑。回调函数中大部分的操作都是涉及到平台的寄存器操作，这得根据芯片手册进行进一步分析。  

而且，dma 由于涉及到内存操作，以及各种模式的设置，相对来说比较复杂，博主也只是搞懂了框架中最常用到的这部分，对于其它复杂的部分也是一知半解，可能在以后的工作中有机会深入了解 dma 的使用，只是目前水平有限，请谅解。



