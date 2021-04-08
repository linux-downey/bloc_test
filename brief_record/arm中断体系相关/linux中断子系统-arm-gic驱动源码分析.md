# GIC 驱动源码分析

GIC 通常是集成在处理器内部的一个硬件设备，既然是硬件设备，自然就可以在设备树中找到相关的信息：



```c++
intc: interrupt-controller@00a01000 {
		compatible = "arm，cortex-a7-gic";
		#interrupt-cells = <3>;
		interrupt-controller;
		reg = <0x00a01000 0x1000>，
		      <0x00a02000 0x100>;
	};
```



上述的示例是 cortex-A7 中的 GIC 的实现，其中 interrupt-controller 属性表示当前节点是一个中断控制器节点，而 reg 中断的两组内存描述分别对应 distributor 和 CPU interface 的外设地址，在多核的情况下，这两个外设地址可能是 bank 类型的，有些 gic 的实现并不支持 bank 寄存器，这时候就要提供另一个属性："cpu-offset"，为每个 CPU 指定一个 offset，对应不同的 distributor/CPU interface 寄存器地址，不过大部分的实现都是支持 bank 寄存器的。



按照惯例，通过 "arm，cortex-a7-gic" 找到对应的驱动程序，在 drivers/irqchip/irq-gic.c 中，可以找到对应的 driver 定义：

```c++
...
IRQCHIP_DECLARE(cortex_a9_gic， "arm，cortex-a9-gic"， gic_of_init);
IRQCHIP_DECLARE(cortex_a7_gic， "arm，cortex-a7-gic"， gic_of_init);
IRQCHIP_DECLARE(msm_8660_qgic， "qcom，msm-8660-qgic"， gic_of_init);
...
```

 

通过这些 driver 的定义可以看出，irq-gic.c 中实现的驱动针对 gicv2 标准，这一份驱动对应多个基于 gicv2 的中断控制器实现，包括 GIC400、cortex-A7/A9 的 GIC 实现，driver 与设备树的匹配流程就不啰嗦了，总之不难猜到，设备节点与 dirver 匹配成功之后，就会调用 gic_of_init，如果存在多个级联的 gic 设备描述节点，也就会多次调用到该初始化函数。  

gic_of_init 函数有两个参数，一个是设备树节点 device node，另一个是其 parent node，对于 root GIC，parent 为 NULL，而对于 secondary GIC，其 parent node 参数为 root node，对于不同层级的 GIC，在驱动处理上有很大的不同，目前没见过大于两级的 GIC 级联，两级的级联可以扩展的 GIC 数量已经非常多了，想来也没有必要再纵向扩展(还是存在多级 GIC 的需求，只是我没有考虑到？)。  

gic_of_init 的源码如下：

```c++
int __init gic_of_init(struct device_node *node， struct device_node *parent)
{
    struct gic_chip_data *gic;
	gic = &gic_data[gic_cnt];
	ret = gic_of_setup(gic， node);                .........................1
    
    if (gic_cnt == 0 && !gic_check_eoimode(node， &gic->raw_cpu_base))
        static_key_slow_dec(&supports_deactivate);   ......................2
    
    __gic_init_bases(gic， -1， &node->fwnode);      ........................3
    
    if (parent) {
		irq = irq_of_parse_and_map(node， 0);
		gic_cascade_irq(gic_cnt， irq);              .......................4
	}
    gic_cnt++;
    ...
}
```



注1：每一个 GIC 对应一个 gic_chip_data 结构，这个实例是全局静态数组 gic_data 中的一个成员，而 gic_cnt 随着每一个 GIC 的初始化会递增，也就是全局静态数组 gic_data 中保存着所有系统中的 GIC 描述结构体。 

既然是通过设备树传递参数，通常第一步都是解析设备树，在 gic_of_setup 中负责干这件事，对于 GIC 设备节点而言，标准的节点属性为 reg 和 cpu-offset 属性，reg 通常描述两段内存 distributor 相关的寄存器组和 CPU interface 相关的寄存器组，也可能是连续的或者多段，根据具体实现而定，cpu-offset 属性针对那些不提供 bank 寄存器的 GIC 实现，不过大部分 GIC 的实现都带有 bank 寄存器组，因此该属性通常并不需要。  

除了内核定义的标准的属性节点，还可以传入自定义的节点属性，自然在驱动程序中就需要自定义的解析方式。

注2：全局变量 supports_deactivate 表示是否使用 EOI mode，该值默认为 true，这个 EOI 模式在前文 [gic介绍](https://zhuanlan.zhihu.com/p/363129733)中有提到，EOI 模式使能与否决定了在 CPU 处理完中断之后通知 GIC 是一步操作还是两步操作，不使能 EOI 模式时，一次写寄存器操作表示GIC 同时执行 priority drop 和 deactivate，而使能 EOI 模式，需要分两步操作两个不同的寄存器来执行这两个操作。当处理器不处于 hyper 模式或者没有足够的外设寄存器空间时，不使能 EOI 模式，也就是  supports_deactivate 为 0.

注3：GIC 主要的初始化工作，下文详细讨论

注4：如果 parent 参数不为 NULL，也就说明当前 GIC 是被级联的 GIC，对于级联的 GIC 自然是需要进行特殊处理的，这部分在下文中详细讨论。

 



### __gic_init_bases

__gic_init_bases 是 GIC 初始化的核心函数，不过在讲解这个函数之前，有必要先了解两个数据结构：irq_chip_data 和 irq_desc。  

先来看看 irq_chip_data 结构：

```c++
struct gic_chip_data {
	struct irq_chip chip;
	union gic_base dist_base;
	union gic_base cpu_base;
	void __iomem *raw_dist_base;
	void __iomem *raw_cpu_base;
	u32 percpu_offset;
	struct irq_domain *domain;
	unsigned int gic_irqs;
    ...
};
```

通常内核中的命名都是相通的，比如 xxx_chip 用来对一个硬件控制器的抽象，比如 gpio_chip/pwm_chip，既然是硬件控制器，也就有硬件控制器相关的信息和对该硬件控制器相关的操作，所以 irq_chip 中包含大量针对 GIC 硬件操作的函数指针，作为回调函数在特定时间调用，比如 gic 驱动中的 gic_chip 的初始化:

```c++
static struct irq_chip gic_chip = {
	.irq_mask		= gic_mask_irq，
	.irq_unmask		= gic_unmask_irq，
	.irq_eoi		= gic_eoi_irq，
	.irq_set_type		= gic_set_type，
	...
};
```

这些函数都可以从名称看出它们所实现的功能，而这些函数都是对 GIC 中寄存器的控制.  

domain 这个单词在内核中出场率也不低，通常同一类组件中存在层级或者模块化的结构，为了方面管理，就需要为其划分域，通俗地说就是分组，比如不同层级的 GIC 划分为 irq_domain，不同层级的 CPU 之间划分为 sched_domain，或者针对功耗的 pm_domain

其它的一些数据成员在上文中已经介绍过了，比如 dist_base 和 cpu_base 是 distributor 和 CPU interface 对应寄存器组的地址，同时还需要兼容 GIC 没有实现 bank 寄存器的情况，这种情况下使用 percpu 的 \_\_iomem 地址，因此使用一个 union 描述. 



irq_chip_data 用于描述 gic 硬件相关的信息，其中包含的 irq_chip 主要描述 gic 的相关操作。

对于中断处理而言，比较重要的是 irq_desc 结构，用于描述单个的 irq:

```c++
struct irq_desc {
	...
    struct irq_data		irq_data;
    irq_flow_handler_t	handle_irq;
    struct irqaction	*action;
    wait_queue_head_t       wait_for_threads;
    ...
}
```

作为描述单个 irq 的数据结构，irq_desc 的数据成员有很多，上面只是列出了比较重要的几个，其中：

* irq_data : 从名称不难看出，这是 irq_desc 相关的数据成员，其中包含该中断的逻辑 irq，hwirq，以及该终端所属的 chip 和 domain，这个数据结构并不是指针而是实例结构，如果你经常阅读内核代码，就知道可以通过 irq_data 的指针反向获取到对应的 irq_desc 结构的地址。
  因此，在实际的内核处理中，在外面抛头露面的是这个 irq_data，毕竟 irq_data 包含了索引到该中断的关键信息(irq，hwirq，chip，domain)，找到了 irq_data 也就相当于找到了 irq_desc，而不用直接操作整个臃肿的 irq_desc，这大概也是内核中将 irq_data 从 irq_desc 中独立出来的原因吧。  

* handle_irq：按照命名的经验来看，这个函数应该就是中断的处理函数了，它也可以算是中断的处理函数，只不过是相对 high level 的，也就是在中断处理函数相对靠前的阶段被调用，为什么需要再添加一层 high level 的定义呢，自然是为了针对不同的处理情况进行抽象，比如上层的 PPI 中断和 SPI 中断需要分别处理，对于级联的 GIC 也需要做不同处理，不同的情况就对应不同的回调接口，而 high_level 的 irq handle 就是针对这一层面的抽象。 
* action：action 才是我们的正主，也就是内核或者驱动开发者在申请中断时传入的中断处理函数就被保存在这里，在中断处理的后一阶段被调用。
  由于共享中断的存在，同一个中断号对应的 action 并不一定只有一个，struct  irqaction 结构中包含 next 指针，也就是将共享该中断线的所有 action 组成一个链表，在处理的时候再执行，对于共享的中断，需要通过 dev_id 来对某一个中断进行唯一识别。
  同时，内核引入中断线程化的概念，也就是中断可以不在中断上下文中执行，而是在内核线程上下文中执行，action 中包含 thread_fn 以及 thread 等数据结构，以支持中断线程化。 
* wait_for_threads：wait_queue_head_t 是一个等待队列头，这是在中断同步的时候使用的，比如进程在 disable 或者 free 某个 irq 时，需要等待 irq 执行完成，如果该 irq 的例程正在执行，该进程就会睡眠并记录在该等待队列中，在该中断处理完成之后就会唤醒这些进程。 



了解完这两个数据结构，再回过头继续看 GIC 的驱动初始化代码：__gic_init_bases，按照一贯的风格，咱们还是讲逻辑为主，贴代码为辅。  

在 __gic_init_bases 中，分为两种情况：

* root gic 的处理
* 非 root gic 的处理

针对 root gic，中断输出引脚直接连接到 CPU 的 IRQ line，和 secondary gic 的区别在于 root gic 需要处理 SGI 和 PPI，而 secondary git 不需要，同时，在多核环境中，还需要配置相应的 CPU interface：

```c++
static int __init __gic_init_bases(struct gic_chip_data *gic，
				   int irq_start，
				   struct fwnode_handle *handle)
{	
    ...
    if (gic == &gic_data[0]) {
        set_smp_cross_call(gic_raise_softirq);                   ..................1
        cpuhp_setup_state_nocalls(CPUHP_AP_IRQ_GIC_STARTING，     ..................2
                          "AP_IRQ_GIC_STARTING"，
                          gic_starting_cpu， NULL);
        set_handle_irq(gic_handle_irq);                          ..................3
    }
    ...
}
```

注1：在内核中，SGI 中断基本上都被用于 CPU 核之间的通信，又被称为 IPI 中断，由某一个 CPU 发给单个或者多个 CPU，set_smp_cross_call(gic_raise_softirq) 这个函数就是设置发送 IPI 中断的回调函数为 gic_raise_softirq，这个函数实现很简单，就是写 GIC 的 SGI 中断寄存器，对应的参数就是目标 CPU 的 mask 和对应的中断号。

比如唤醒其它 CPU 时，  调用路径为 arch_send_wakeup_ipi_mask -> smp_cross_call -> gic_raise_softirq，对应的参数为目标 CPU 以及 0 (0号SGI中断为唤醒中断).

注2：cpuhp_setup 开头的函数都是和 CPU hotplug 相关的，可以理解为注册在 CPU 热插拔时执行的回调函数，

这里的调用注册了 gic_starting_cpu 回调接口，函数的 nocalls 后缀表示本次注册不会调用到该接口，只是注册，它会在 CPU 接入并 setup 时被调用，不难想到，当一个 CPU 启动并设置 GIC 时，需要对 GIC percpu 的部分进行初始化，一方面是 SGI 和 PPI 相关的，另一方面是和当前 CPU 对应的 CPU interface 相关的，因为这个回调接口将会被每个 CPU 调用，它只会设置本 CPU 对应的 CPU interface，包括 CPU 优先级的 mask 寄存器，以及 CPU interface 的 enable 设置等等。  

注3：set_handle_irq(gic_handle_irq)，该函数用于设置中断发生时的 higher level 回调函数，之所以用 higher，是因为前面用了一个 high level 的概念，自然，这个回调函数在更早的时候被调用，gic_handle_irq 是中断处理程序的汇编代码调用，这是一个很重要的函数，也是后续在分析中断处理流程时的主角。



root gic 相关的部分处理完了，接下来就处理非 root gic 部分，其实严格来说不能说是非 root gic 部分，因为这部分是共用的：

```c
static int __init __gic_init_bases(struct gic_chip_data *gic，
				   int irq_start，
				   struct fwnode_handle *handle)
{
    ...
    gic_init_chip(gic， NULL， name， false);            
    ret = gic_init_bases(gic， irq_start， handle);
    ...
}
```

gic_init_chip：从名字可以看出，这是对 gic_chip 结构的初始化，gic_chip 这个结构体中主要包含了很对针对 GIC 寄存器的回调函数，gic chip 中静态定义了一些公共的回调函数，比如 gic_mask_irq(屏蔽某个中断)、gic_set_type(设置中断线触发方式)等等，在 gic_init_chip 中主要设置了 name、parent 等识别参数，如果支持 EIO 模式，还会设置 EIO 相关的回调函数，不过一般情况下是不支持 EIO，即中断的 completion 回复直接一步到位。 

如果是多核系统，还可以设置中断的 affinity

gic_init_bases，这又是一个主要的函数，也是初始化的核心函数。 



#### gic_init_bases

gic_init_bases 函数也被分为两个部分：

* 不带 bank 寄存器的 GIC 特殊处理
* irq domain 与 irq 映射关系的建立
* GIC 寄存器的初始化设置

对于不带 bank 寄存器的 GIC 实现，GIC 中有部分寄存器是和 CPU 相关的，比如大部分 CPU interface 相关的寄存器，多个 CPU 核自然不能使用同一套寄存器，需要在内核中为每个 CPU 分配对应的存储空间，因此需要使用到 percpu 相关的数据结构和变量，gic 相关数据结构中的 percpu 变量大多都是和这种情况相关的.    

比如 struct gic_chip_data类型的 gic_data 数组中的每个成员，不带 bank 寄存器的 GIC 驱动中使用 gic->dist_base.percpu_base 记录 distributor 的基地址，否则使用 gic->dist_base.common_base.  

大多数的 GIC 实现都支持 bank 寄存器，因此说它是正常的 GIC 实现.  

至于 irq domain 和 irq 映射相关的，这是一个相对复杂的问题，暂时放到后面讲解.

先来看一下GIC 寄存器的初始化设置部分，这部分其实就是对 GIC 中的一部分寄存器做一些初始化工作，和 GIC 的硬件实现有关，软件上的关联并不大，要理解这一部分的源代码，最好的方式就是阅读 GICv2 的手册以及具体的 GIC 实现(比如 cortex-A7 手册包含 GIC 的实现，或者 GIC400 手册)，初始化分为两个函数:gic_dist_init 和 gic_cpu_init

，不难看出，这两个函数分别针对 GIC 的 distributor 和 CPU 相关的初始化函数，这两个函数对应的设置项为:

* 全局地使能中断从 distributor 到 CPU interface 的传递
* 为每个中断设置传递的目标 CPU mask，通常就是一个特定的 CPU
* 为每个中断设置默认的触发方式，默认为低电平触发(具体平台取决于 GIC 的驱动实现)
* 为每个中断设置优先级
* 初始化复位所有的中断线
* 为每个 CPU interface 记录对应的 CPU mask，当然，这个 mask 只对应一个 CPU
* 设置 CPU interface 的中断屏蔽 threshold
* 其它的 CPU interface 的一些初始化工作

做完这些初始化设置之后，GIC 就可以开始工作了，它所做的工作在前面的文章提到过多次:从各个中断源收集中断触发信息，然后根据配置将中断传递给预设的 CPU，仅此而已. 

 

## irq domain

domain 翻译过来就是 "域" 的意思，在内核中是一个比较常见的概念，也就是将某一类资源划分成不同的区域，相同的域下共享一些共同的属性， domain 实际上也是对模块化的一种体现.   

比如常见的 sched_domain，鉴于 CPU 之间互操作的成本取决于硬件上 CPU 之间的距离，就是将不同距离的 CPU 划分为不同的域，同域内的操作成本相对较低，以此来合理地安排进程迁移这项重要的工作.  

或者对于 power manage 这样的工作就体现得更为明显了，硬件上的电源提供通常分为多个独立的 block，每个 block 对应不同域，以此简化电源管理的逻辑.  

irq domain 的产生是因为 GIC 对于级联的支持. 实际上，虽然内核对每个 GIC 都设立了不同的域，但是它只做了一件事:负责 GIC 中 hwirq 和逻辑 irq 的映射.   

#### hwirq

hwirq 即 hardware irq，也就是 GIC 硬件上的 irq ID，这是 GIC 标准定义的，在 GIC 的介绍中有提到:

* 0~15 对应 SGI 中断，该中断是 percpu 的
* 16~31 对应 PPI 中断，该中断是 percpu 的
* 32~1020 对应 SPI 中断，即共享中断，是所有 CPU 共享的，这些中断会被分发到哪个 CPU 根据配置决定

实际上，中断数量的多少取决于 GIC 的具体实现，有可能是 320/480 个或者更多，这些中断的实现在 ID 上也并不一定是连续的，总归是符合 GIC 的标准定义之内的，而且每个 GIC 的 hwirq 都是相同的.

#### 逻辑irq   

再回过头来看软件，其实软件上的要求很简单:当某个中断源产生中断时，我只要能够根据中断号的映射找到该中断对应的中断资源就好了，这里的中断资源包括中断回调函数/中断执行对应的参数等.

但问题是，对于级联的 gic，不同中断对应的 hwirq 可能是相同的，因此，需要对硬件上的中断号做一层映射，也就是软件上维护一个全局且唯一的逻辑 irq 映射表，每一个 GIC 的每一个 ID 都有一个对应的唯一的逻辑 irq，然后大可以通过唯一的逻辑 irq 来匹配对应的中断资源.于是一个完整的映射表就完成了: GICn 的 hwirq -> 逻辑 irq -> 对应的中断 resource.  

理解了 irq domain 所完成的工作，接下来的问题就是:如何建立 hwirq 与逻辑 irq 之间的映射表?

#### 简单的映射

在最简单也是最常见的情况下，系统中只有一个  root GIC，且 GIC 中所支持的中断是连续的，比如 0~31 对应 SGI 和 PPI 中断，系统中所有外设中断占用 32~319 中断号. 

这种情况几乎不需要映射，直接使用 hwirq 作为 irq 就好，当然，考虑到软件上的兼容，还是需要一个 mapping 函数，只是 irq 的获取直接返回 hwirq 而已.  

当然，也有可能是简单的映射，比如内核中使用的 SGI 和 PPI 不会用到 16 个，hwirq 上不使用但是软件上的 irq 可以使用，比如 irq num 为 8 的中断对应 hwirq 为 32 的中断，当然，具体怎么映射是很灵活的.

#### 不连续的映射

对于简单的映射，通常就是一个数组就搞定了，中断号或者对中断号做一些线性的运算作为数组下标，简单而且效率高，但是如果中断 ID 的实现是非连续的，使用数组就不大合适了，这时候通常使用 radix tree 对 hwirq 和 irq 进行映射，这种数据结构查找效率也很高，只是会占用稍微多一点点的内存，对于使用者而言，使用内核提供的接口，也比较简单. 

#### 级联的 GIC

在单个 GIC 的系统中，因为只存在一个 GIC， hwirq 也是唯一的，当系统中存在多个 GIC 时， 对于每个 GIC，hwirq 都是一样的，因此 hwirq 不再是唯一的，这时候需要考虑的问题就是如何将不同 GIC 中相同的 hwirq 映射为系统中的逻辑 irq.  

最容易想到的办法就是增加一个 GIC ID 的参数，GIC ID 配合该 GIC 中对应的 hwirq 就可以唯一定位一个中断源了，而内核中并不是增加一个 GIC ID，而是引入 irq domain 的概念，每个 GIC 对应一个独立的 domain，从原理上来说，不管用什么办法，只要能识别到 GIC，再通过 hwirq 就可以唯一定位中断源. irq domain 负责建立和维护当前 domain 下的 hwirq 到逻辑 irq 的映射.  

比如，系统中存在一个 root GIC，一个 secondary GIC， root GIC domain 中将 0~479 GIC ID 映射为 0~479 的逻辑 irq，而在 secondary GIC domain 内，32~480 的 GIC ID 映射为  480~927 的逻辑 irq，每个 domain 保存各自的映射表，并提供相应的查询接口 irq_find_mapping，这个接口参数为 domain 和 hwirq，返回值为 hwirq 对应的逻辑 irq.  

还是来一个示例更加实在:假设系统中存在两个 GIC，secondary GIC 连接到 root GIC 的 40 号引脚，也就是 hwirq 为 40，现在在 secondary GIC 的 48 号中断触发了，会发生什么呢? 

* 首先，GIC 配置正常的情况下，中断会通过 root GIC 传递到 CPU，CPU 进入中断处理过程.
* CPU 通过读取 root gic 相关寄存器获取到触发中断的 hwirq 为 40，执行 irq_find_mapping 函数，传入 root domain 和 hwirq 号，获取逻辑 irq，因为 root domain 中保存了当前 GIC hwirq 与逻辑 irq 的映射表
* 获取到逻辑 irq 之后，通过 irq_to_desc 函数获取对应的 irq desc，irq desc 中包含了该中断相关的信息，包括对应的中断执行函数以及参数，执行 irq_desc 中的 high level 回调函数.
* 因为 root GIC 的 40 号中断对应 child GIC，因此该 high level 函数是进一步到 secondary gic 中进行处理.和 root gic 一样，先读取 secondary gic 中触发的中断源的 hwirq，然后通过 secondary domain 中保存的 hwirq 到逻辑 irq 的映射获取对应的逻辑 irq，只是个查表过程. 
* 获取到触发中断源的真实逻辑 irq，通过 irq_to_desc 获取对应的 irq desc，执行对应的中断处理函数.

需要注意的是，irq desc 并不属于 domain 单独维护，而是全局的. 

因此irq domain 目前只负责 hwirq-逻辑irq 之间的映射，irq desc 的保存是全局的，这样处理起来会更简单，和逻辑 irq 的映射类型，irq desc 与逻辑 irq 的映射也存在静态数组和 radix tree 两种方式. 

下图中描述了各个成员之间的关系(TODO):

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/IRQ-related/cpu-irqdomain-desc-flow-picture.jpg)

在上图中，GIC0 是 root GIC，而 GIC1 是连接在 GIC0 的 34 号中断上的次级 GIC. 

各个 GIC domain 保存当前 GIC hwirq 对应的逻辑 irq 的映射表，而逻辑 irq 与 irq desc 的映射是全局的.

对于 root GIC 上发生的中断，irq desc 将会直接调用到用户注册的 irqaction 中的回调函数，而对于 GIC1 上发生的中断，root GIC 执行 34 号中断对应的 desc 中的 high level 中断处理程序，该中断处理程序会进入到 GIC1 domain 中进行进一步的中断处理. 

对于 root GIC，0~15 的 hwirq 不需要映射，PPI 中断和 SPI 中断一样进行映射，对于 secondary GIC 而言，SGI 和 PPI 都不使用，因此不需要映射. 

上图中 irq 以及 irq desc 的映射基于线性映射，如果是 radix 的映射方式，就没有这么紧凑和规整了，**在实际的平台实现中，具体的映射实现也可能是非常灵活的，GIC 标准或者内核并没有规定应该怎么去映射.**不过其原理总归是为每个 hwirq 找一个对应的逻辑 irq 或者 desc 罢了，把握这个核心思想就够了.   



### irq domain 的初始化

扯得有点远了... 再回到我们前面要讲的内容， irq domain 的初始化，这部分源代码在 gic_init_bases 中:

```c++
static int gic_init_bases(struct gic_chip_data *gic， int irq_start，
			  struct fwnode_handle *handle)
{
    ...
	gic_irqs = readl_relaxed(gic_data_dist_base(gic) + GIC_DIST_CTR) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;
	gic->gic_irqs = gic_irqs;                     .............................1
    ...
    if (gic == &gic_data[0] && (irq_start & 31) > 0) {
        hwirq_base = 16;
        if (irq_start != -1)
            irq_start = (irq_start & ~31) + 16;
    } else {
        hwirq_base = 32;                         ..............................2
    }

    gic_irqs -= hwirq_base; 

    irq_base = irq_alloc_descs(irq_start， 16， gic_irqs，     ...................3
                               numa_node_id());
    if (irq_base < 0) {
        WARN(1， "Cannot allocate irq_descs @ IRQ%d， assuming pre-allocated\n"，
             irq_start);
        irq_base = irq_start;
    }

    gic->domain = irq_domain_add_legacy(NULL， gic_irqs， irq_base，    ..........4
                                        hwirq_base， &gic_irq_domain_ops， gic);
    ...
}
```

注1：建立 hwirq-逻辑irq 之间映射的第一步是确定 hwirq 的范围以及数量，GIC 硬件上支持多少中断是由硬件实现决定的，软件上通过读取 GIC_DIST_CTR 寄存器的低5位可以获取到 gic 的数量

注2：对于 root gic，SGI 和 PPI 会被系统使用，而其它 secondary GIC 并不会使用到 SGI 和 PPI，因此对于 secondary GIC 而言，不用对前 32 个中断号进行映射，hwirq_base 为 32，而对于 root gic 而言，SGI 和 PPI 并不一定使用完整的 16 个，因此根据实际情况对其进行映射。 

注3：有些奇怪的是，irq desc 的申请在 irq domain 建立之前，毕竟 irq desc 是全局数据类型，理论上应该由 irq domain 建立之后，即 hwirq 和逻辑 irq 之间的映射建立完成之后再通过逻辑 irq 分配对应的 irq domain，这里并没有这么做，实际上是使用了一个 allocated_irqs 全局 bit map，每次需要申请新的 irq desc 时，会先从这个 bit map 中找到合适且连续的空间，该空间的起始位置为 start，实际上这个 start 就是逻辑 irq，也就是此时基本上已经将 hwirq 和逻辑 irq 的映射建立完成，再通过调用 alloc_descs 为连续的逻辑 irq 分配对应的 irq desc，所有的 irq desc 可以使用数组也可以使用 radix tree。

注4：irq_alloc_descs 的返回值是逻辑 irq 的起始值，此时分配的逻辑 irq 是连续的，因此，已经明确了逻辑 irq 的起始以及数量、hwirq 的起始以及数量，建立逻辑 irq 与 hwirq 之间的映射就很简单了。  

实际上 irq_domain_add_legacy 这个函数并不仅仅是建立关系，还会根据当前 gic 的信息新建一个 irq_domain 结构：申请一个新的 irq domain 结构，并初始化对应的 ops、revmap、revmap_tree 等成员，这些 map 用于保存映射关系，同时将该 irq domain 链接到全局链表 irq_domain_list 中，方面后续的遍历工作。 

 初始化 irq domain 完成之后，就会调用 irq_domain_associate_many 建立 hwirq 和逻辑 irq 的映射关系，在该函数中，最终会确定使用线性映射还是使用 radix tree 的映射方式。如果在创建 irq domain 时提供了 irq_domain 中的 map 接口，就调用该用户提供的 map 接口进行映射，这种映射可以是自定义的，默认情况下是建立简单的线性映射，如果内核配置了 CONFIG_SPARSE_IRQ，就使用 radix tree 的方式进行映射，如果是线性映射，保存在 domain->linear_revmap 中，该数组中保存逻辑 irq 的值，hwirq 为下标，如果是 radix tree 的方式映射，则保存在 &domain->revmap_tree 中，hwirq 作为 key，逻辑 irq 为对应的 value。 

到这里，irq domain 的建立与 irq 之间的映射关系就完成了，当发生中断时，内核只需要通过 domain 和 hwirq 两个参数调用 irq_find_mapping 函数就可以找到唯一的逻辑 irq num。  



## 级联 GIC 的中断处理

在 GIC 驱动中 gic_of_init 的最后初始化一部分为级联 GIC 的处理：

```c++
int __init
gic_of_init(struct device_node *node， struct device_node *parent)
{
	if (parent) {
		irq = irq_of_parse_and_map(node， 0);
		gic_cascade_irq(gic_cnt， irq);
	}    
}

```

irq_of_parse_and_map 负责通过设备树获取当前 GIC 是连接在父级 GIC 的哪一个中断 ID 上.通过这个中断 ID 才能通过父级 GIC 寻址到当前的 GIC. 

gic_cascade_irq(gic_cnt， irq) 这个函数的作用就是将 high level 的中断处理函数指针修改为 gic_handle_cascade_irq，专门处理级联的情况，默认情况下，对于 root gic 而言，High level 的中断处理函数负责获取用户设置的中断数据，然后执行用户传入的 low level 中断处理程序.

级联的情况不一样，假设 GIC1 连在 root GIC 的 40 号中断上，root GIC 的 40 号中断处理函数实际上是针对 GIC1 的处理，就需要向下多处理一层，可以来看看 gic_handle_cascade_irq 的源码实现:

```c++
static void gic_handle_cascade_irq(struct irq_desc *desc)
{
	struct gic_chip_data *chip_data = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	status = readl_relaxed(gic_data_cpu_base(chip_data) + GIC_CPU_INTACK);
	gic_irq = (status & GICC_IAR_INT_ID_MASK);                               
    
	cascade_irq = irq_find_mapping(chip_data->domain， gic_irq);
	if (unlikely(gic_irq < 32 || gic_irq > 1020))
		handle_bad_irq(desc);
	else
		generic_handle_irq(cascade_irq);
	...
}
```

这个 high level 的中断实现看起来并没有那么负责，就是读取当前 GIC(当前是级联的子 GIC)哪个中断号被触发，然后通过 irq_find_mapping 找到对应的当前触发中断的 hwirq，接着也就是调用通用的中断处理函数 generic_handle_irq.

和 root GIC 的 high level 处理程序不同的地方在于这里有一个递归进入的过程.



GIC 实际上是一个相对复杂的部分，一方面体现在它本身的复杂性，而另一方面，GIC 只是作为中断处理的一部分，如果需要完整理解 GIC，就需要对 linux 内核的整个中断处理流程有一个大概的掌握，当然，linux 内核的中断处理流程在后续的文章中会一一进行分析，中断处理流程需要反复研究，看一遍肯定是不够的。

当然，博客只是抛砖引玉，所有的秘密都藏在代码中，如果真的想深入研究 linux 内核中断子系统，还是需要看看源代码，内核源代码不是洪水猛兽，而是促成技术进步的最好工具，共勉。



### 参考

[蜗窝科技：gic代码分析](http://www.wowotech.net/irq_subsystem/gic_driver.html)

[gic v2 手册](https://gitee.com/linux-downey/bloc_test/blob/master/%E6%96%87%E6%A1%A3%E8%B5%84%E6%96%99/GIC-V2%E6%89%8B%E5%86%8C.pdf)

4.14 内核源码



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。













