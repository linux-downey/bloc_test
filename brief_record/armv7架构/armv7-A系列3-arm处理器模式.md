# armv7-A系列3-arm处理器模式
对于大型操作系统而言，系统安全是一个至关重要的问题，对于系统的硬件资源，应该限制应用程序直接访问，而由系统更高级别的程序进行管理，linux 和 windows 都是基于此原则实现的，这里更高级别的程序指的就是操作系统的内核。  

内核相对于应用程序自然需要拥有更高的操作权限，这种权限的区分完全由软件实现是非常困难的，需要芯片架构层面的支持。对于所有现代的处理器，几乎都会在处理器架构层面设计不同的特权等级，以适应不同的软件隔离要求。  

arm 当然也不例外，armv7-A 架构提供了 9 种工作模式：



* User：用户进程运行在 User 模式下，拥有受限的系统访问权限
* FIQ ： 快中断异常处理模式，相对于中断而言，快中断拥有更高的响应等级、更低延迟。
* IRQ ： 中断异常处理模式
* Supervisor ： 内核通常运行在该模式下，在系统复位的时候或者应用程序调用 svc 指令的时候将会进入到当前模式下，系统调用就是通过 svc 指令完成
* Abort ： 内存访问异常处理模式，常见的 MMU fault 就会跳转到该模式下进行相应的处理
* Undefined：当执行未定义的指令时，触发硬件异常，硬件上自动跳转到该模式下
* System ： 系统模式，这个模式下将与用户模式共享寄存器视图
* Monitor： 针对安全扩展，在该模式下执行 secure 和 non-secure 处理器状态的切换
* Hypervisor ： 针对虚拟化扩展

其中， User 模式处于 PL0 特权级，又被称为非特权级模式，可以访问的硬件资源十分受限，其它 8 种模式除了 hypervisor 都处于 PL1 特权级模式。

普通的应用程序就是运行在user模式(PL0)下，没有对硬件的直接访问权，所有的硬件操作都需要通过系统调用向内核进行申请，内核运行在特权级模式(PL1)下，对系统调用、中断、异常等系统事件进行响应、处理并返回，以这种隔离的方式保证了内核的安全。   

armv6 架构只有前七种工作模式，armv7 新增了安全扩展和虚拟化扩展，在系统模式上新增了两种：monitor mode 和 hypervisor mode，同时也增加了更高的特权级：PL2，hypervisor mode 就运行在 PL2 等级中，hyp mode 与虚拟化扩展相关，只有在使用虚拟化扩展功能时才进入到 hyp mode ，关于安全扩展和虚拟化扩展这里不做讨论。

值得一提的是，在 linux 内核的实现中，arm 处理器尽管实现了 IRQ、ABORT 等模式，当中断和异常发生时，硬件上会直接跳转到对应的模式下，但是其真正的处理却统一在 svc 模式下，比如发生中断时，只会在 IRQ 模式下作短暂停留，随后就跳转到 svc 模式下，由内核进行统一处理。



## 处理器模式的切换
在上文中的工作模式简介中可以看到，除了 user、monitor 和 hypervisor 模式之外，其它的 6 中处理模式都属于 PL1 特权级模式，可以访问处理器的大部分寄存器资源(其它模式的bank寄存器除外)，user 模式运行在 PL0 特权级，也被叫做非特权级模式，很多处理器资源都无法访问，而 monitor 和 hypervisor 模式属于安全扩展和虚拟化扩展，暂且不作讨论。  

系统 reset 时，系统处于 svc 模式下。  

事实上，大多数的特权级模式都是因为系统触发异常而自动进入的，比如 IRQ、FIQ 是因为产生了硬件中断，处理器强制性地修改 CPSR 模式位并跳转到相应的模式下执行程序，而 Undefined、Abort 是因为产生系统错误，同样也是系统强制地修改 CPSR，对于 svc ，在处理中断时，内核大多数情况下都运行在这个模式，在用户程序需要使用系统调用时，使用 svc 指令进入到 supervisor 模式，进入内核。  

处理器当前的模式是由状态寄存器 CPSR 的 bits[4:0] 来控制的，arm cpsr 可以参考[arm状态寄存器](https://zhuanlan.zhihu.com/p/362687525)，对于 user 模式而言，并没有权限操作 CPSR mode 位，只能通过 svc 汇编指令进入到 svc(Supervisor) 模式，对于其它 PL1 及以上的特权级而言，可以通过给相应的模式位赋值来切换到目标模式，比较常见的是 svc 切换到 system 模式，而对于其它自动进入的模式，并没有太多手动切换的需求。   

CPSR 中模式位对应的模式可以见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/cpsr%E6%A8%A1%E5%BC%8F%E4%BD%8D%E5%AF%B9%E5%BA%94%E6%A8%A1%E5%BC%8F.jpg)

## 处理器模式的不同
既然分出了这么多中处理器模式，自然是存在相应的应用需求，对于不同的异常情况使用不同的工作模式进行处理，一方面有效地对系统资源进行隔离，保证安全，另一方面对于不同的异常需要定制不同的处理方式。  

这种区别体现在每种模式对应的寄存器上：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/bank%E5%AF%84%E5%AD%98%E5%99%A8.jpg)

表格中，蓝色背景的寄存器属于 bank 寄存器，即相同的寄存器名对应不同的寄存器实体，关于 bank 寄存器的概念可以参考[arm核心寄存器](https://zhuanlan.zhihu.com/p/362668607)。  

不难看到，R0～R7、PC 是所有工作模式通用的，除了 system 模式，大多数模式都有自己独立的 sp 和 lr，对于 sp 而言，这些模式会初始化并使用自己的栈，如果公用一个栈，一方面模式的切换时 sp 指针需要频繁地进行 long jump，另一方面也涉及到安全问题。  

而对于 lr 而言，模式的跳转同时代表着程序执行流的跳转，通常从一个模式跳转到另一个模式是为了处理一些当前模式处理不了或者不方便处理的事，执行完之后再返回到当前模式，所以需要用一个寄存器来保存返回地址，这个寄存器就是 lr，如果共用一个 lr 寄存器，同时又涉及到多种模式切换，lr 将会被覆盖或者需要要转存在栈上，而栈并不是共享的，这会导致非常多的麻烦。

另一个明显的特点是：FIQ 的 R8 ～ R12 是 bank 类型的，这是因为：FIQ 作为快中断，需要更快地处理中断，这种快体现在：



* 更多的寄存器，更多的寄存器意味着很多数据处理可以在寄存器中进行，而不用使用栈，寄存器和栈(内存)的访问速度不在同一个量级，尽管缓存让这种差距缩小，不过这种访问速度差距依旧明显，所以 FIQ 模式会有更多的 bank 寄存器。  
* 更高的优先级，FIQ 和 IRQ 同时到达时，FIQ 将会先响应并处理，在操作系统的软件实现中，应该支持 FIQ 可以抢占 IRQ 运行，同时 FIQ 的处理不应该被任何其它异常所抢占。  
* 中断向量表的位置在最后，对于 IRQ 的中断向量，只有四个字节，所以 IRQ 的向量必然是一条跳转指令，而 FIQ 在中断向量表的最后，FIQ 的中断处理代码可以直接放在中断向量表之后，避免跳转指令，当然这也由实现来决定。  
* 更低的时延，这里的时延指的是中断信号产生到执行中断的第一条指令的延时时间，这是由硬件决定的。  

实际上，在 linux 中，并不使用 FIQ，所有的中断都只会被路由到 IRQ 引脚上。

另外，除了核心寄存器，还有 bank 类型的状态保存寄存器 SPSR，在上文中提到，模式切换时需要保存断点状态以处理完成之后恢复到断点继续运行，除了核心寄存器，CPSR 的断点状态也必须保存，所以在模式切换的时候，处理器会将 CPSR 复制到 SPSR 中，这一步是由处理器自动完成的，同样的，大部分模式的 CPSR 状态信息都需要独立保存，所以存在多个 SPSR，而不是共用一个 SPSR。  




## 向量表
在前面的小节中，一直在讨论模式切换、跳转等概念，其中包含一个很关键的问题：为什么模式切换会伴随着跳转？程序跳转又是跳转到哪里？这之中的操作到底是硬件完成还是由软件完成？只有理解了这些问题，才能真正理解处理器的异常处理过程。  

问题的答案要从向量表说起，做嵌入式的都知道这个东西，总的来说，向量表中保存了一系列的跳转指令，当系统发生异常时，由处理器负责将程序执行流转到向量表中的跳转指令，最常见的就是中断向量，应用工程师只需要使用固定的函数名编写中断处理程序或者注册中断处理子程序，在中断发生时该中断处理程序就会被自动调用，这背后的实现就是中断向量表的功劳，简单的系统比如单片机中是向量表直接跳转到用户级的中断子程序，而 linux 中向量表对应的 handler 只是一个中间过程，处理过程相对复杂很多。  

在 armv7 中，中断向量表可以设置在两个地址：0x00000000 和 0xffff0000，由协处理器 cp15 的 SCTLR 的 bit13 来控制，默认情况下，中断向量表的位置在 0x00000000，实际上，对于操作系统而言，比如 linux，会更倾向于将中断向量表放在 0xffff0000 处，因为 0x0 处在用户空间下，需要额外做一些限制，而 0xffff0000 处在内核空间，另一方面，0 地址通常是 NULL 指针访问的地址，这需要 MMU 做相应的访问限制规则，总之，将向量表放在高地址处会更方便。  

armv7 架构中向量表地址内容见下表：

![](https://gitee.com/linux-downey/bloc_test/raw/master/zhihu_picture/armv7/armv7%E5%90%91%E9%87%8F%E8%A1%A8.jpg)

当对应的 exception event 发生时，系统会自动地修改 CPSR 状态寄存器，并跳转到上表中的地址执行指令，而软件上要做的，就是在该地址上放置对应的代码，除了 FIQ 之外，其它模式对应的都是一条跳转指令，如果存放多于 4 字节的指令，将会覆盖调用后续的异常向量，因为 FIQ 是最后一条异常向量，根据其特殊性，FIQ 的处理指令可以直接放置在以 oxffff001c 处，当然，具体怎么做由软件的实现来决定。  

比如，在 U-boot 中，armv7 对应的 vector.S 中(启动代码)可以看到这样的代码：

```
_start:
    b	reset
    ldr	pc, _undefined_instruction
    ldr	pc, _software_interrupt
    ldr	pc, _prefetch_abort
    ldr	pc, _data_abort
    ldr	pc, _not_used
    ldr	pc, _irq
    ldr	pc, _fiq
```
因为在链接脚本中定义了 ENTRY(_start)，所以程序的第一条指令就是 b reset，尽管后续的指令在启动时得不到执行，但是它们在编译链接之后就被放置在以 0x4 开始的地址处，发生异常时会自动跳转到这些地址处，每一条指令占4字节，从指令内容可以看出这些指令都是跳转指令。   

在汇编中标号表示标号之后的第一条指令的地址，比如，当发生 irq 中断时，处理器会强制跳转执行 ldr pc, _irq 这条指令，它的内容是这样的：

```
_irq:			.word irq

irq:
	get_irq_stack
	irq_save_user_regs      //保存断点
	bl	do_irq
	irq_restore_user_regs  //恢复断点
```

要在中断中实现软件上的功能，我们可以在 do_irq 中编写程序，当然，一般来说这一部分系统已经帮你做好了，你只需要在特定的位置实现特定的函数即可。同样的情况完全适用于其它的异常。   


到这里，对于异常的处理和模式的跳转最底层的部分基本上算是清晰了。  





### 参考

[armv7-A-R 参考手册](https://gitee.com/linux-downey/bloc_test/blob/master/%E6%96%87%E6%A1%A3%E8%B5%84%E6%96%99/armv7-A-R%E6%89%8B%E5%86%8C.pdf)



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。




