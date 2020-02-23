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
一旦 dma controller 配置完成，驱动开发者就可以使用 dma 的功能了，在使用前需要指定当前设备所使用的 dma controller 和 dma 通道，我们以 spi 为例，看看是其他外设是如何使用 dma controller 的服务的：

```
spi@48030000 {
    ...
    dmas = <0x2d 0x10 0x0 0x2d 0x11 0x0 0x2d 0x12 0x0 0x2d 0x13 0x0>;
    dma-names = "tx0", "rx0", "tx1", "rx1";
    ...
}
```
其他设备引用 dma controller 节点主要是通过 dmas 和 dma-names 两个参数，在上面的 dma controller 节点中，#dma-cells 的值为 2(参数个数)，所以 dmas 对应的参数中以三个数值(目标节点 + 两个参数)为一组，描述一个 dma 服务。  

以第一组为例：0x2d 0x10 0x0，第一个为 dma controller 的 phandle 属性，表示引用的目标 dma controller，后面两个参数则取决于硬件特性，通常有一个参数为 dma 通道编号，而另一个有可能是 dma 模式设置、传输特性设置等，平台相关。  

设备树提供了硬件参数，具体的解析还需要参考具体的实现代码，在驱动实现的代码中，开发者可以通过解析该设备树节点获取 dma 设备并进行相应操作。  






