# 浅谈操作系统与内存
对于计算机的发明，相信大家都有耳闻那个占地面积按平米算的第一台计算机。在那个时候，CPU的资源是极其珍贵的，随着这些年突飞猛进的发展，一片指甲盖大小的民用级CPU一秒钟能执行的指令数可以达到上亿级别。  

随着计算能力的增长，芯片外围的硬件和配套的软件也是一路高歌，发生了天翻地覆的变化，今天我们简单回顾历史，来看一看操作系统和内存机制的演变，不仅要了解它们是怎样，同时也看看它们为什么会是这样。  

***

## CPU的运行
一说到CPU(Center processing unit)，大家都觉得这是非常了不起的东西，我们的手机电脑都是由它进行核心控制，拥有掌管一切的能力，但是，它真的有传说中那么聪明么？  

事实并非如此，CPU唯一的能力其实就是处理二进制数据，CPU的组成是这样的：  

* CPU有三种总线：控制总线，地址总线，数据总线，这些总线统称为系统总线，主要用来与外设交换数据。    
* 内部寄存器若干，一般只有几十个字节，普通寄存器负责传输数据，特殊寄存器如PC,SP,LD则控制程序流程。  
* ALU-算数逻辑运算单元，既然是处理数据，当然也就依赖运算单元，这些运算单元只能处理加减乘法，其实严格来说，就是处理加法，减和乘是在加法基础上实现的。  

这三个就是早期CPU的主要部件了，那它是怎么处理数据的呢？  

系统总线负责与外部的数据交换，将交换的数据暂时放在寄存器中，然后CPU再从寄存器中获取数据使用算数逻辑运算单元进行运算，必要时将数据写回。  

是的，仅此而已，CPU不能直接播放音乐，它也不能生成游戏界面，只是孜孜不倦地拿数据，处理数据，写回数据，像个一刻也不停的流水线工人。    

***
### 系统的存储

上文中我们提到CPU的内部寄存器，因为是集成在CPU内部，所以速度非常快，但是同时也因为集成度的问题，CPU中一般只有几十字节的寄存器，这对于数据处理是远远不够的，所以当运行时存储空间不够的时候我们必须有另一个地方进行存储，这就是系统内存。  

即使都是数据，也分别有不同的属性，比如是否需要掉电保存，数据处理的速率要求。    

在程序执行时，需要在存储设备中保存一些运行参数，这部分存储数据的速率直接决定了程序运行速率，所以对这部分存储的要求是速度快，同时因为是存储运行时参数，所以不需要掉电保存。  

在程序执行之外，需要保存大量的静态数据，比如用户数据，文件，这部分数据需要掉电能够保存，且要求可存储数据量大，由于访问并不频繁，所以对速率要求可以降低以节省成本。

由此，存储设备分化出了ram和rom两大类，ram有速率快，掉电不保存的特性，而rom是速率慢，存储量大，掉电保存。  

ram和rom只是分别对应易失性存储器和非易失性存储器的统称，事实上ram有sram，sdram等等，而rom有eeprom，flash，硬盘等等。 

***

## 微控制器和单板机

上面说到CPU的作用，可能你开始有疑问了，既然CPU只能处理简单的数据，那么它是怎么处理复杂软件的运行的呢？  

这就是很多人的一个常见误区，将CPU和MCU，还有单板机搞混了。  

MCU(Microcontroller unit):微控制器，又被称为单片微型计算机，常被直接称为单片机。它通常集成了CPU，rom，ram以及硬件控制器、外围电路等等，ram和rom的容量有限，接口电路也有限。   

单板机：单板机是把微型计算机的整个功能体系电路(CPU、ROM、RAM、输入/输出接口电路以及其他辅助电路）全部组装在一块印制电板上，再用印制电路将各个功能芯片连接起来，一般来说资源比如主频、ram&rom容量要比MCU高出一大截。    

事实上，在我们常见的手机电脑中，用户常说的CPU，事实上指的是单板机。而在嵌入式设备上，经常会使用到单片机。  

常见的第二个误区就是：认为整个单板机(或者MCU)上只有CPU是可编程元件，其他部分都是硬件搭建起来的。  

事实上，除了CPU，像各种硬件控制器比如gpio、i2c、dma或者硬盘控制器，这些都是可编程器件，只是这些控制器所扮演的都是一个从机角色，而主控是CPU。  

CPU通过系统总线与各个控制器之间进行数据交互，通过特定的预定义的指令来控制控制器的行为：  

比如控制gpio的操作，将某个引脚的电平从0设置成1，这样再配合上继电器等硬件上的电路，就可以实现220V电路的控制。  

再者是通过控制gpio以通过预定义的控制协议与连接在另一端的设备进行通信。  

又或者是告诉硬盘控制器，CPU需要访问某些数据，硬盘控制器将数据传递给CPU。  

CPU就是这样通过将指令一级级地将数据传递给外设，通过在外设处理器中预定义了一些指令字段，外设接收CPU的数据相当于接收到控制指令，以此实现外设的控制。  

而CPU本身则只是孜孜不倦地处理数据，反馈数据。  

***

## 单片机到操作系统
在早期的CPU上，CPU都是顺序执行，一个CPU只运行一个程序，这样造成的问题就是：当程序在等待某个资源或者读写磁盘的时候，CPU就处于空闲状态，这对CPU来说是非常浪费的。由于CPU的资源非常珍贵，人们不得不想办法解决这个问题，所以就有人编写多任务程序：  

一个CPU中可以存在多个任务，一个时刻只允许一个任务运行，当检测程序检测到某个任务处于空闲状态，就切换下一个任务运行。或者是当前任务主动放弃运行权，切换下一个任务运行。  

这就是操作系统的原型，检测程序一般被称为调度器，遵循某种调度算法。  


***

### 任务调度算法  

调度算法又常被称为调度策略，目前的操作系统常用的调度策略有两种： 

* 基于优先级的调度，优先级高的任务总是有权利抢到CPU的使用权，高优先级的任务通过主动睡眠和被动睡眠让出CPU使用权(如等待信号量，等待锁)
* 时间片机制，每个任务分配时间片，一般是ms级别的时间，当时间片用完就切换到下一个任务，营造一种所有任务同时运行的假象。  

事实上，操作系统往往在基于上述调度算法的基础上，运行着更复杂的调度算法，比如以优先级为权重分配时间片，比如任务分组，不同的分组有不同的调度策略等等....

***

### 任务调度的执行原理
上文说到，操作系统的的最早雏形就是允许CPU中存在多个任务，通过某种调度算法来使每个任务交替运行。  

那么，CPU到底是怎么做到这件事的呢？  

首先我们要了解以下概念：时钟、中断，程序执行流的切换。  

* 时钟：是CPU上最重要的部分之一，任何一个CPU都必须有时钟部分，它提供了CPU上时间的概念，系统不同部分之间的交互和同步都要靠时钟信号来进行同步。通俗来说，就像人类定义了时间的概念，我们才可以统一做到准时上下班，相约同一时间去做某事。CPU也是一样，在进行数据传输时，通信双方必须统一传输单元数据的时间，才能进行同步和数据解析。  
* 中断：即使是目前非常简单的单片机，也提供中断功能。我们可以将中断理解为CPU中的突发事件需要紧急处理，这时CPU暂停处理原本任务转去处理中断，处理完中断之后再回头来处理原先的任务。  
* 程序执行流的切换：CPU中一般会有多个寄存器，分为通用寄存器和特殊寄存器，通用寄存器用来存取CPU处理的数据，而特殊寄存器中需要提及的就是PC指针(程序计数器)，SP(栈指针)，PC指针中存储着下一条将要执行的指令地址，而SP指针则指向栈地址。  
    在正常情况下，程序顺序运行，PC指针也是按顺序装载需要执行程序的地址，CPU每次执行一条指令都会从PC指针读取程序执行地址，然后从执行地址上取到数据，执行相应动作，而栈则保存着函数调用过程中的信息(因为调用完需要返回到调用前状态)。  

在发生中断的时候，因为需要跳转去执行中断服务函数，肯定需要更改程序执行流，在单片机上，CPU将会去查中断向量表，找到需要执行的函数地址装载到PC指针处，然后保存一些当前运行参数，比如寄存器数据，栈数据，下一个指令周期将会跳转执行中断服务函数。  

既然程序的执行流可以由程序员自由控制，只要控制PC指针指向就可以，那么我们也可以预先定义两个任务，当一个任务空闲时，直接跳转执行到另一个任务，每个任务的运行参数比如寄存器数据，栈数据，使用的资源等等进行保存，在切换到某个任务时，只要将保存的信息恢复到原来的状态就可以继续运行任务，这种操作方式当然可以从两个延伸到多个，这就是操作系统的模型。   

通常，保存信息的部分一般使用结构体链表，一般被称为任务控制块(每个操作系统的叫法不一样，但实现的思想是一样的)，每个线程都有独立的栈空间以保存每个线程的运行时状态。  

那么，我们怎么知道什么时候执行另一个任务呢？这就需要借助时钟中断提供一个时间的概念，任务的运行时间就以这个时钟中断作为度量,具体的做法就是每隔一个周期(通常非常短，1ms、10ms等)产生一次中断，在中断程序中检查是否需要切换任务运行，而切换的原因就比较多了，比如运行时间到了、等待某项资源自主休眠等等。  

事实上，到这里，你已经了解了嵌入式实时系统的运行原理了，比如ucos，freeRTOS之类的，建议去看看相关源代码，尤其是ucos，代码量少且易懂，麻雀虽小五脏俱全。  

***

### 嵌入式实时操作系统的内存限制

在早期的单片机上，程序运行在物理内存中，也就是说，程序在运行时直接访问到物理地址，在程序运行开始，将全部程序加载到内存中，所有的数据地址和程序地址就此固定。 

在运行多任务系统时，比较直接的办法也是直接为每个任务分配各自需要的内存空间，比如总内存为100M，task1需要40M，task2需要50M，task3需要20M，那么最简单的办法是给task1分配40M，给task2分配50M，而task3，不好意思，内存不够了，不允许运行，这样简单的分配方式有以下问题： 

* 地址空间不隔离 试想一下，所有程序都访问物理地址，那么就存在task1可以直接访问到task2任务内存空间的情况，这就给了一些恶意程序机会来进行一些非法操作，即使是非恶意但是有bug的程序，也可能会导致其他任务无法运行，这对于需要安全稳定的的计算机环境的用户是不能容忍的。
* 内存使用效率极低  由于没有有效的内存管理机制，通常在一个程序执行时，将整个程序装载进内存中运行，如果我们要运行task3，内存已经不够了，其实系统完全可以将暂时没有运行的task2先装入磁盘中，将task3装载到内存，当需要运行task2时，再将task2换回，虽然牺牲了一些执行效率，但总归是可以支持更多程序运行。
* 程序地址空间不确定  需要了解的是程序在编译阶段生成的可执行文件中符号地址是连续且确定的，即使实现了内存数据与磁盘的交换，将暂时不需要的任务放入磁盘而运行其他任务，程序每次装入内存时，都需要从内存中分配一段连续内存，这个空间是不确定的，但是在程序实现过程中，有些数据往往要求其地址空间是确定的，这就会给编写程序带来很大的麻烦。   


***

## 虚拟内存机制
那么怎么解决这个问题呢？曾经有人说过一句名言：

    计算机科学领域的任何问题都可以通过增加一个间接的中间层来解决。  

在这里这个道理同样适用，后来的操作系统设计者设计了一种新的模型，在内存中增加一层虚拟地址。  

面对开发者而言，程序中使用的地址都是虚拟地址，这样，只要我们能妥善处理好虚拟地址到物理地址的映射过程，而这个映射过程被当作独立的一部分存在，就可以解决上述提到的三个问题：  
* 首先，对于地址隔离，因为程序员操作的是虚拟地址，虚拟地址在最后会转化为物理地址，可能程序A访问的0x8000,转换到物理地址是0x3000,而程序B同样访问0x8000，但是会被转换到物理地址0x4000，这样程序中并不知道自己操作的物理地址，也操作不到实际的物理地址，因为虚拟地址的转换总会将地址映射到不冲突的物理地址上，同时进行检测，也就无从影响到别的任务执行。  
* 对于内存使用率来说  建立完善的内存管理机制，可以更方便地进行内存与磁盘数据的交换，将空置的内存利用起来而不增加编程者的负担。      
* 对于程序空间不确定  虚拟内存机制完美地解决了这个问题，在直接操作物理地址时，因为内存与磁盘的交换，可能导致函数或者变量地址的变化，原来程序中的数据必须进行更新。  
    而引入虚拟内存机制，程序员只操作虚拟地址，函数、变量虚拟地址不变，物理地址可能变化，但是程序员只需要关注虚拟地址就行了，虚拟地址到物理地址的转换就由内存管理部分负责。  

既然增加了一个中间层，那么这个中间层最好是由独立的部分进行管理，实现这个功能的器件就是MMU，它接管了程序中虚拟地址和物理地址的转换，MMU一般直接集成在CPU中，不会以独立的器件存在。  


***

### 内存分段分页机制
在经典32位桌面操作系统中，有32条地址线(特殊情况下可能36条)，那么CPU可直接寻址到的内存空间为2^32字节，也就是4GB,虽说内存寻址可以到4G，但是常常在单板机上 并不会有这么大的物理内存。  

根据实际情况而不同，可能是512M或者更少，但是由于虚拟内存机制的存在，程序看起来可操作的内存就是4GB，因为MMU总会找到与程序中的虚拟地址相对应的物理地址，在内存不够用时，它就会征用硬盘中的空间，在linux下安装系统时会让你分出一片swap分区，顾名思义，swap分区就是用来内存交换的。  

那4GB这么大的内存，如果不进行组织，在CPU读写数据时将会是一场灾难，因为要找到一个数据在最坏的情况下需要遍历整个内存。  

就像一个部队，总要按照军、师、旅、团、营、连、排、班来划分，如果全由总司令管理所有人，那也会是一场灾难。  

因此，对于内存而言，衍生了分段和分页机制，根据功能划分段，然后再细分成页，一般一页是4K，当然，这其中会有根据不同业务的差别做一些特别的定制。 

它的问题就是即使是只存储一个字节，也要用掉一页的内存，造成一定的浪费。 

但是如果将分页粒度定得过细，将会造成访问成本的增加，因为在很多时候，进行访问都是直接使用轮询机制。而且，就像每本书都有目录和前言，段和页的信息都要在系统中进行记录，分页更细则代表页数更多，这部分的开销也就更多。这也是一种浪费。 

就像程序中时间与空间的拉锯战，计算机中充满了妥协。    

***

### 单片机与单板机程序执行的不同
处理器是否携带MMU几乎完全成了划分单片机和单板机的分界线。  

带MMU的处理器直接运行桌面系统，如linux、windows之类的，与不带MMU的单片机相比，体现在用户眼前的最大区别就是进程的概念，一个进程就是一个程序的运行实体，使得桌面系统中可以同时运行多个程序，而每个程序由于虚拟机制的存在，看起来是独占了整个内存空间。  

而不带MMU的处理器，一般是嵌入式设备，程序直接在物理地址上运行，支持多线程。 

从程序的加载执行来说，桌面系统中程序编译完成之后存储在文件系统中，在程序被调用执行时由加载器加载到内存中执行。而在单片机中，程序编译生成的可执行文件一般是直接下载到片上flash(rom的一种)中。  

32位单片机的寻址范围为4G，由于不支持虚拟内存技术，一般在总线上连接的ram不会太大，所以可以直接将rom也挂载在总线上，CPU可以直接通过总线访问flash上的数据，所以程序可以直接在片上flash中执行，在运行时只是将数据部分加载到ram中运行。  


***

### 无系统单线程，嵌入式实时操作系统和桌面操作系统
可以在单板机上运行桌面操作系统，是不是运行单线程的单片机和嵌入式实时操作系统就可以抛弃了呢？  

从功能实现上来说，单板机确实比单片机强很多，但是资源的增加同时带来成本的增加，所以在一些成本敏感的应用场合下反而是单片机的天下。 

而对于操作系统而言，嵌入式实时操作系统由于没有一些臃肿的系统服务，有时反而有着比桌面系统更好的实时性，在实时性要求高的场合更是风生水起，比如无人机、车载系统。而且运行在单片机上，成本上更有优势。  

对于嵌入式开发而言，很有必要了解这三种程序运行方式：无系统单线程，嵌入式实时操作系统，桌面操作系统。  
* 无系统单线程很好理解，程序顺序执行，除了发生中断之后跳转执行中断服务函数，一般不会再发生程序执行流的切换，程序直接操作物理地址，常用于嵌入式小型设备，如灯、插座开关等等。  
* 嵌入式实时操作系统：支持多线程的并发执行，多任务循环执行，遵循一定的调度算法，程序直接操作物理地址。例如ucos、free rtos，通常也在移植在资源相对较多的单片机上。  
* 桌面操作系统：CPU自带MMU，所以支持虚拟内存机制，支持多进程和多线程编程，程序操作虚拟地址，在人机交互上有很好的表现。例如windows，linux，常用的手机、ipad、服务器、个人电脑。  

在单片机上，无系统单线程模式可以很简单地切换到嵌入式实时操作系统，进行相应的移植即可。  

但是是否能在其上运行桌面操作系统，是否集成MMU是一个决定性的因素。  


***






好了，关于操作系统和内存的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
（完）








 