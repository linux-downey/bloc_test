# linux调度子系统11 - cpu 的初始化
毫无疑问,CPU也是硬件设备，理论上它应该是由设备树来描述的，目前在 arm/RISC-V 上是这么做的，在 arm 平台的设备树文件中可以找到 cpu 相关的信息，而其它平台则并不强制使用设备树，毕竟 CPU 的相关信息通常并不会需要用户去配置，可以由硬件固化或者由 CPU 自动识别并且配置以支持热插拔，对于不同的架构有不同的实现方式，在 arm 中，同时存在设备树描述和读取内部寄存器来识别 cpu 的两种方式.  

## 设备树节点
CPU 信息相关的设备树节点描述可以参考 [linux 官方 document](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/cpu/cpu-topology.txt)，这篇文章主要是针对 cpu topology 的描述，而 cpu 相关的其它信息比较少，可能是因为 cpu 这种器件的平台特殊性，各厂商采用不同的配置，比如 cpu 的 capacity 属性，则是放在分属的 arch 下，比如 Documentation/devicetree/bindings/arm/cpu-capacity.txt,要找相应的设备树描述需要去对应的架构下，很有可能随着硬件的发展，对于 CPU 的软件抽象会有些修改，下面我们以 arm64 为示例进行讲解.  

### 拓扑结构
CPU 的 topology 基本上符合硬件的组织结构，目前分为四个层次:



* socket:socket 可以理解为物理的 cpu 插槽或者物理 CPU,一个 socket 通常包含多个核，单核心的 cpu 可以不定义 socket 节点.  
* cluster: cluster 类似于 arm 中的 cluster 概念，也就是一个 socket 同质 CPU 的集合，一个 socket 包含一个或者多个 cluster,同时,cluster 节点还可以嵌套. 
* core: cpu 核，一个 cluster 可以包含一个或多个 core
* thread:超线程架构中的硬件线程，如果支持超线程,thread 就作为系统中的逻辑CPU,如果不支持，软件层面的逻辑CPU就是 core.

所有的 cpu 描述都比如位于设备树节点 cpus 下，对于 CPU topology 的组织由 cpu-map 节点进行描述，上述的 socket/cluster/core/thread 作为子节点.  

从上面的 cpu 层次可以看到，设备树对于 CPU 的描述并不是 ALL NUMA/socket/core/thread,刚开始研究这份资料的时候，我以为的对应关系是 socket-ALL NUMA,cluster-socket,这样刚好一一对应，但是后来发现不能做这种假设(从文档中的描述或者cluster 的嵌套可以看出)，随后又找到了[关于 numa 的官方文档](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/numa.txt)，实际上 numa 是由额外的节点指定的，并不直接包含在设备的层级中.  

因此，设备树节点中所多出来的 cluster 应该是为了兼容 arm 架构中的 cluster 概念，将一个 socket(或者物理CPU)中的多个不同质的核再次分组，同时支持嵌套.  

下面是一个 arm64 架构下，单 socket ，嵌套 cluster,16 核的 CPU 设备树描述的示例:
```
cpus {
	#size-cells = <0>;
	#address-cells = <2>;

	cpu-map {
		socket0 {
			cluster0 {
				cluster0 {
					core0 {
						thread0 {
							cpu = <&CPU0>;
						};
						thread1 {
							cpu = <&CPU1>;
						};
					};

					core1 {
						...
					};
				};

				cluster1 {
					core0 {
						thread0 {
							cpu = <&CPU4>;
						};
						thread1 {
							cpu = <&CPU5>;
						};
					};

					core1 {
						...
					};
				};
			};

			cluster1 {
				cluster0 {
					core0 {
						thread0 {
							cpu = <&CPU8>;
						};
						thread1 {
							cpu = <&CPU9>;
						};
					};
					core1 {
						...
					};
				};

				cluster1 {
					core0 {
						thread0 {
							cpu = <&CPU12>;
						};
						thread1 {
							cpu = <&CPU13>;
						};
					};
					core1 {
						...
					};
				};
			};
		};
	};
    CPU0: cpu@0 {
		device_type = "cpu";
		compatible = "arm,cortex-a57";
		...
	};
}
```

至于 NUMA 部分的设备树描述暂不讨论,arm 目前并没有 NUMA 的实现.  



### CPU节点信息描述
上面讲解了 CPU 的拓扑结构是如何进行组织的，同时，针对每个 CPU 节点，也需要通过设备树将相关信息传递到内核中.  

不幸的是，暂时没有找到统一的文档，而是散落在各个 arch 目录下，同时，对于设备树中 CPU 相关信息的解析也存在于 arch/ 目录下，因此，这部分是几乎是完全硬件相关的，下面就以 arm64 的 CPU 描述进行分析:

```
...
A53_0: cpu@0 {
        device_type = "cpu";
        compatible = "arm,cortex-a53";
        reg = <0x0 0x0>;
        enable-method = "psci";
        next-level-cache = <&A53_L2>;
        cpu-idle-states = <&CPU_SLEEP>;
        clocks = <0x4 0x5a 0x4 0x58 0x4 0xa 0x4 0xc 0x4 0x4e>;
        clock-names = "a53"， "arm_a53_src"， "arm_pll"， "arm_pll_out"， "sys1_pll_800m";
        clock-latency = <0xee6c>;
        ...
};
...
```
clock 相关属性,compatible 和 reg 几乎是每个组件都会指定的，指定了时钟，驱动匹配字段以及 reg 属性，对于普通的外设,reg 通常指定了外设地址，而在 CPU 描述中作为标识.  

CPU 的休眠配置由 cpu-idle-states 决定，该节点的值通常是对另一个节点的引用，睡眠部分也是硬件相关的，不同的架构会有不同的操作或者是不同的睡眠等级，以及唤醒机制，对于 arm64 而言，可配置进入以及退出的 latency,是否关闭通用定时器等.  

enable-method = "psci" 这个节点表示使用 arm 定义的电源管理框架,psci 通常由 firmware 来实现，说到电源管理，自然是和 CPU 的功耗相关.  

next-level-cache 指定了下一级的 cache,也是一个节点引用，在 cortex-A53 4核 SMP 处理器中,L1 cache 是 percpu 的，而 L2 cache 是 4 核共享的，因此需要将该信息传递给内核.  

可以看到，其实 CPU 节点相关的属性基本上都是体系架构特定的.  



## 设备节点的解析
说完设备树的配置项，自然就要说到设备树的节点解析，毕竟这两者是相对的，对于 arm64 架构而言，解析代码在 arch/arm64/kernel/topology.c 中，尽管代码位于各自的 arch/ 目录下，但主要的还是解析 topology 相关的，这一部分是通用的，同时也会解析一些架构自定义的属性.     

关于 SMP 多核的前期初始化部分，调用路径为:start_kernel->rest_init->kernel_init->kernel_init_freeable->smp_prepare_cpus ，在该函数中将完成 CPU 初始化的前期工作.  

对于设备树的解析在 smp_prepare_cpus->init_cpu_topology->parse_dt_topology 中.  

在解析之前，先需要看一看内核中描述 CPU topology 的数据结构:

```c++
struct cpu_topology {
	int thread_id;        				// SMT 相关，arm 目前不支持
	int core_id;          				// core，对应逻辑节点
	int cluster_id;                     // cluster id
	cpumask_t thread_sibling;           // SMT 相关
	cpumask_t core_sibling;				// core 同一个 cluster 下的兄弟节点。  
};
```
这个数据结构同样定义在 arch/ 下，也就是说，对于不同的 CPU,这个数据结构是不同的，内核中通过统一的接口来操作相关数据，类似于面向对象的覆盖，在 arm64 中，没有针对 SMT(超线程) 和 NUMA 的实现，当前平台也是单 socket,因此对 topology 的描述主要是两个部分:core_id 和 cluster_id,而 thread_id 其实是 SMT 的属性，可能是处于软件兼容性的考虑，而另外的成员则记录的是当前的 core 在同一个 cluster 中的兄弟节点.   

内核中静态定义了一个 cpu_topology 数组:

```c++
struct cpu_topology cpu_topology[NR_CPUS];
```
NR_CPUS 可以通过内核配置项进行配置，可以直接配置成静态的数量，在支持热插拔的系统上需要预留空间，每个 CPU 对应数组的一个 struct cpu_topology 成员.  

当然，这针对的是一个简单的单 socket 单 cluster 4 核处理器的配置，如果硬件上的拓扑结构越复杂，就需要增加相应的字段来记录. 

再回到设备树的解析，在 parse_dt_topology 中，主要是以下几点:



* 在设备树中找到 cpus 节点以及 cpu-map 节点，如果不存在 cpu-map,则退出设备树解析，表示不通过设备树来获取 CPU 的 topology 信息.
* 调用 parse_cluster 函数，寻找设备树节点中的 cluster 节点，节点的命名需要以 cluster%d 的形式，才会被解析代码所识别，因为 cluster 可以递归包含，因此 parse_cluster 也可能会被递归调用.
* 在 cluster 中寻找 core%d 节点，针对 core 节点调用 parse_core 函数
* 在 parse_core 函数中完成真正的解析工作，一方面，继续寻找 core 中的 thread%d 节点,arm64 并不支持超线程，因此没有 thread%d 节点，在该函数中，对 cpu_topology[cpu] 中的 thread_id/core_id/cluster_id 进行赋值，由此记录了 cpu 的各类 id,保存在 cpu_topology[NR_CPUS] 数组中.   

CPU 的拓扑结构描述的是各个 CPU 之间的联系，上面的解析只是记录了逻辑 CPU 对应的各类 id,而 sibling 的生成在 smp_prepare_cpus->store_cpu_topology 中.   

在上面的介绍中提到,CPU 拓扑结构的建立并不一定使用设备树，也可以使用其它方式，如果不支持设备树的方式，对应的处理也是在 store_cpu_topology 中，它完成以下的工作:



* 判断 cpu_topology[cpu] 是否被赋值，被赋值意味着使用了设备树的解析方式，直接跳转到 sibling 成员的赋值.  
* 如果没有使用设备树来描述 CPU 结构，则需要使用硬件相关的方式，比如 arm64 中是保存在 cp15 协处理器的 MPIDR 寄存器中的，通过该寄存器可以直接读取 cluster/core/thread id.并赋值给 cpu_topology[cpu].  
* cpu_topology 数组中保存的 id 是一一对应的，因此可以通过扫描整个数组来建立 CPU 的拓扑结构.

需要再次申明的是，设备树节点的设置以及解析几乎都是体系架构相关的，包括目前通用的 topology 设置也仅仅是部分体系架构通用，随着硬件的更新，这部分的实现细节很可能会变化，因此上述的设备树以及解析仅供参考.  


## CPU mask 的初始化
在内核中，涉及到 CPU 之间的操作时，会经常见到这几个宏:

```c++
#define for_each_possible_cpu(cpu) for_each_cpu((cpu)， cpu_possible_mask)
#define for_each_online_cpu(cpu)   for_each_cpu((cpu)， cpu_online_mask)
#define for_each_present_cpu(cpu)  for_each_cpu((cpu)， cpu_present_mask)
```
从 for_each 可以看出，这是对 cpu 的轮询操作，在内核中，通过 cpu_possible_mask/cpu_online_mask/cpu_present_mask 以及 cpu_active_mask 来表示内核中不同状态下的 CPU 数量，各个 mask 的类型是 struct cpumask,这其实就是一个长度为 NR_CPUS 的 bitmap,每一个 CPU 占用一个 bit,如果系统中给出的 possible cpu 为 cpu0~cpu3,那么该 mask 的值为 0b00001111(实际上前面还有多个0).从字面意思来看，它们分别代表 可能存在的CPU/已上线的CPU/被提供的CPU/被激活的CPU,当然，从字面上并不能看出精确的信息，还是需要深入到内核中.    

这些变量定义在 include/linux/cpumask.h 中，在这些 mask 定义的上方可以看到比较详细的注释，翻译如下:



* cpu_possible_mask:可被分配资源的 CPU(has bit 'cpu' set iff cpu is populatable).
* cpu_present_mask:已经被分配资源的 CPU(has bit 'cpu' set iff cpu is populated).
* cpu_online_mask:可以被调度器操作的 CPU
* cpu_active_mask:可以进行迁移操作的 CPU

当没有配置 CONFIG_HOTPLUG_CPU 也就是不支持 CPU 热插拔功能的系统中,cpu_possible_mask=cpu_present_mask,cpu_online_mask=cpu_active_mask.  

cpu_possible_mask 是在初始化阶段就已经定义好的，可能存在的 CPU 掩码数量，也就是说，如果你觉得你的系统尽管当前只有 4 个 CPU,但是可能后续会插进来 100 个插槽，就需要为这些可能提供的 CPU 预备相应的掩码.  

而 cpu_present_mask 则是确确实实硬件上已经连接的 CPU 掩码.  

至于 cpu_online_mask 和 cpu_active_mask 则描述得有一些模糊，尤其是 cpu_active_mask 很容易被理解为有除了 idle 之外的 active 进程运行的 CPU,实际上这并不对，要了解这些，还得看内核代码.  

### cpu_possible_mask
在内核的启动阶段，只需要一个 CPU 进行初始化工作，通常是 cpu0,对应 boot cpu 而言，会直接设置其对应的四种掩码位，下面讲解的都是其它 CPU 的掩码设置.   

在 arm64 中,kernel 使用一个整型的数组 cpu_logical_map 来保存物理 CPU 和逻辑 CPU 之间的映射，数组的长度由 NR_CPUS 来指定.  

这个数组 map 的赋值是通过解析设备树的节点完成的,dts 中的 reg 字段标示了 cpu 的 id,当解析到完整的 CPU 节点时，将其保存到 cpu_logical_map 中，并调用 set_cpu_possible() 来设置当前 cpu 在 cpu_possible_mask 中对应的掩码位为 1.  

因此，对于 arm 架构而言,cpu_possible_mask 中保存的掩码是针对在设备树中给出的 CPU 节点.  

### cpu_present_mask
在解析完 CPU 节点并建立了 CPU 拓扑结构之后，同样在 smp_prepare_cpus 中，执行一个 for_each_possible_cpu 循环，对所有 possible 的 CPU 进行轮询处理.  
该 CPU 需要满足的条件是:



* 操作的 CPU 不等于当前处理的 CPU,因为当前处理的 CPU 是 boot CPU,已经设置过了.
* cpu_ops[cpu] 不为空，这个 cpu_ops 是静态定义的一个 ops 数组，数组长度为 NR_CPUS,在 arm64 中，通过读取设备树节点的 enable_method 来确定对应 cpu_ops 的赋值.  
* 执行 cpu_ops[cpu]->cpu_prepare(cpu) 函数，通过调用这个函数确定该 cpu 是否已经准备好，如果一个 cpu 只是在设备树中给出而没有物理接入或者没有 power on,自然是返回异常.
* 满足上述三个条件，调用 set_cpu_present 函数设置 CPU 对应的 cpu_present_mask 掩码.

### cpu_online_mask
CPU 被添加到 cpu_present_mask 掩码中时，表示该 CPU 是具有运行条件的，但是从具有条件到真正 bootup 还有一段距离，因为在内核初始化的早期，是 boot cpu 在运行，其它 CPU 并没有运行，因此需要让其它 CPU 也运行起来，也就是让它们开始取指执行代码，这时候将会设置对应 CPU 的 online 状态.  

### cpu_active_mask
CPU 在内核中的工作就是服从调度器的调配来执行工作，在支持 CPU hotplug 的系统中,CPU 热插拔的控制并不是由调度器来管理的，而是由其它的模块，这也就意味着,CPU 的插入和拔出并不会立刻反映到调度器上，而是经过额外的通知机制，因此，一个 CPU 是否支持调度需要调度器的确认，因此 set_cpu_active() 函数的调用在 kernel/sched/core.c 中，在 sched_cpu_activate 中完成，这是一个 CPU 热插拔的回调函数.  



### 参考

4.9.88 内核源码

[cpu 拓扑结构官方文档](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/cpu/cpu-topology.txt)

[蜗窝科技：cpu热插拔](http://www.wowotech.net/pm_subsystem/cpu_hotplug.html)

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

