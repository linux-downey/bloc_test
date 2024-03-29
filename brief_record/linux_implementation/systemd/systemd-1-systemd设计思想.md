# systemd-1-systemd 的设计思想

## PID 为 1 的进程
在每个 unix 系统上，都存在一个特殊的 PID 为 1 的进程，通常被称为 init 进程，这是内核在初始化阶段创建的，这个进程是系统中的第一个进程，也是所有后续进程的祖先，自然也不会是任何进程的子进程。   

正因为它是第一个进程，系统也赋予了它更多的权利，来做一些其它进程所不能做的事，其中最核心的部分在于创建其它进程以及对它们的管理，从而在系统引导过程中启动和维护用户空间，毕竟，一个完整可用的 linux 系统并不仅仅包含内核，还包含一系列用户进程。  

因为 1 号进程的特殊性，通常 1 号进程并不会是以单独的一个进程存在的，而是伴随着一套完善的 init 系统，早年的 1 号进程是 sysvinit 初始化系统的 init 进程，而且多年来占据主导地位，随着软硬件发展得越来越快，逐渐地不再能适应市场的需求。  

在众多的替代 init 方案中，Upstart 脱颖而出，被所有主要的发行版使用，作为系统中初始化方案管理用户进程。   

上文中提到，初始化系统负责用户空间的启动和维护，从市场需求来看，一个好的初始化系统的启动速度应该是足够快的，要实现快速的启动，两件事情至关重要：
* 减少启动工作
* 将启动工作更多地并行化

这两点是什么意思呢?  

减少启动工作的理念在于服务应该只有在需要的时候才会启动,有些服务是在系统一开始就需要的,比如 syslog,D-bus 系统等等,而有些服务并不是这样,比如蓝牙守护进程只有用户在需要使用时才会用到,没有必要在开机时就提供,打印机也是如此,只有在用户需要打印的时候,打印机的服务才应该启动,同时,如果系统没有连接到网络,依赖于网络的一些组件也可以不启动.  

将启动工作并行化指的是如果我们要运行某些服务,我们不必要串行地运行(sysvinit是这么做的),而是同时地将这些服务一并运行,这时候就可以将 CPU 和磁盘 IO 的利用率大大提升,以降低启动时间. 


## 动态化的硬件和软件
无论是从软件还是硬件来说,现代的操作系统是高度动态化的,在系统运行时,硬件重复地接入或者移除,与硬件相关的软件也必须重复这种过程,初始化系统负责监听这些动态的修改,维护这些服务.  

当前大多数尝试并行化启动的系统还是会存在一系列的序列化,比如 Avahi 依赖于 D-Bus,就需要 D-bus 先启动,然后 Avahi 再启动,其它的服务也是类似.实际上 linux 中存在很多的依赖关系,所以也就存在序列化的现象.  

**或许我们应该考虑,我们说的 Avahi 依赖于 D-Bus 到底依赖的是什么,很可能就是依赖于两者之间的通信接口,比如 socket,如果是这样,被依赖的服务可以先将通信用的 socket 启动,其它的依赖进程也就不需要等依赖进程完全启动,也可以同步启动,并不会影响实际的结果.**  

举个例子,大部分的进程都会依赖于 syslog,启动 syslog 之前先启动通信使用到的 scoket,然后再启动 syslog ,其它进程也可以同时启动,其它进程向 syslog 打印信息时,只是将信息放在 syslog 对应 socket 的缓冲区中,只要缓冲区没有满,其它写进程就不会阻塞,当 syslog 进程启动时,再统一处理缓冲区中的内容,这并不会有什么问题,但是却大大地加速了系统的启动.   

如果一个服务需要另一个服务提供支持，而对应的服务并没有被启动，这时候确实会发生阻塞，这种阻塞是不可避免的，但是明显这种"接口依赖"的并行方式比"服务依赖"的并行方式要好，做到了只有真正在需要时才会进行阻塞，而不是只要两个服务有依赖关系，就必须等到被依赖的服务先启动完成，比如很多服务依赖于网络功能，但是在启动阶段并不需要使用到网络，就没有必要先等网络服务完全可用再启动,只是这种激进的策略会带来复杂性,复杂也就意味着出错的可能性更大,这也是 systemd 在推广过程成遇到的信任问题,这种信任问题只能用时间来证明,目前 systemd 也算是做得不错.  
。  

套接字可以集中被一次性创建,然后在调用 exec() 时进行传递，这样，我们可以在init系统的第一步中为所有守护程序创建所有套接字，然后在第二步中一次运行所有守护程序。实际上，这种做法让系统更具健壮性，因为无论实际的守护进程是否可能暂时变得不可用(可能是由于崩溃)，套接字都保持可用状态。
 
这种相对激进的进程启动方式，并不是什么崭新的想法,这种做法在苹果的 MacOS 上实现了,但是这种巧妙的设计并没有在苹果阵营之外的系统上应用过.   

现代的 linux 倾向于使用 D-Bus 而不是 AF_UNIX socket，所以在初始化阶段，，D-bus 是作为核心通信机制使用的，总而言之,socket 和 D-Bus 的组合使得我们可以并发地启动各个守护进程,不需要其它的同步手段.同时可以做一些 lazy-loading,如果某个进程使用得很少,我们大可以在第一次使用的时候启动,而不是在 boot 阶段就将其启动.   


## 文件系统的并行化
上文中我们介绍的是服务的并行化执行，在实际的启动过程中，如果查看当前发行版引导过程的启动顺序图，会发现同步点不仅仅是守护程序启动：最为显着的是文件系统相关的工作。  

服务启动的并行化思想也可以应用到文件系统中，在某些文件系统还没有初始化好时，服务就可以并行地启动，只有服务真正依赖于某个文件系统且该文件系统没有初始化完成时进入等待，由内核进行排队。  

其具体操作为：systemd 会针对文件系统设置一个 autofs 挂载点，这个挂载点并不能实际访问文件系统，当文件系统真正地初始化完成时，再将该挂载点替换为实际的文件系统安装点，在文件系统实际替换之前，所有文件系统的文件访问操作都会被阻塞。  

实际应用中，尝试根目录下的并行化启动优化是没有什么意义的，因为服务的二进制文件和配置文件都放在系统目录下，更多的是针对 /home 目录，而且是在 / 和 /home 分属于不同文件系统的情况下，/home 目录通常会比较大、或者可能是远程的，有些甚至是加密的，这时候 /home 目录对应的文件系统工作将会占用大量时间，采用上述的文件系统并行化机制可以避免 /home 目录的挂载、fscking 工作导致服务启动的延迟。  

当然，除了 /home 目录，其它的任何目录都可以挂载不同的文件系统，在 /home 目录下实现一个独立的文件系统是一个比较通用的做法，也比较具有代表性。   


## 尽量少地启用进程
在启动过程中，某些服务依赖于一些脚本程序，相对于单纯的 C 语言二进制程序而言，脚本的效率相对来说要低，尽管在经验丰富的工程师手上，脚本的执行效率并不会拖后腿，但是我们不能假设所有的启动脚本都是最优化的版本。   

在服务启动尤其是读取配置文件时，我们经常会用到 grep、awk、cut、sed 等文本处理命令，它们实在是太方便了，如果用 C 来做文本处理，在开发效率上必然要大打折扣，在目前快速迭代的软件环境下这种做法自然是不受欢迎的。  

但是，开发效率和程序执行效率是冲突的，至少在上文中讨论的文本处理上是这样的，使用一次 grep 意味着需要通过当前进程调用 fork 复制一个子进程，然后调用 execve 系统调用将子进程的程序数据替换为 /bin/grep,执行完成之后子进程退出，如果你对 linux 下的编程有一定了解，就可以意识到进程的创建开销是非常大的，你可能只是想打印一串字符，在 C 程序中只需要调用 printf 或者 write 函数，而在脚本中不得不创建一个新的进程来执行 echo 指令。   

对于启动时因为脚本命令而导致的执行效率低下的问题，systemd 是不是提出了非常奇妙的点子？实际上是没有的，最好的方法就是尽可能的用 C 语言重写一些没必要用脚本实现的功能，实际上这种做法非常无聊且没有新意，但是却行之有效，能解决问题的办法就是好办法，不是吗？  

实际上尽管 systemd 是整个系统的管理 daemon，它更像是一个管理框架而不是全权负责各个服务的开发，比如 systemd 的作者不可能重新去开发一个 busybox，也没这个必要，systemd 作者建议大家写出高效的启动程序，并采用 systemd 给出的一些能提高效率的方案(为适配 systemd 的设计框架而适当修改程序)，这需要一部分系统服务开发者的配合。所以，对于尽量少地启动进程这一点，需要所有开发者的努力，不过值得期待的是，systemd 的目前发展态势还是比较可观的。  


## 进程的追踪
除了致力于系统启动效率的提升，它同时负责进程的管理，涉及到进程管理就必须考虑到一个问题：进程的追踪，具体的事务为：监视并标识各个服务，如果它们关闭了，需要重新启动，如果它们崩溃了，应该收集相应的日志信息，将其留给系统管理员处理。  

同时，systemd 应该有能力完全关闭服务，这可能需要处理多个进程之间的交叉关系。  

理论上，linux 创建进程的方式是通过父进程复制的方式进行的，通过父子进程的关联是很容易做到进程跟踪，但实际的实现比想象中要难，一个子进程如果想要摆脱当前父进程只需要调用两次 fork，这并不是说系统服务启动的进程会刻意地使用这种方式以脱离父进程，而是在多进程环境的服务中，这种多级子进程的情况并不少见，当你终止服务的主进程时，并不能完全确定其所属的所有子进程是否已经正常关闭。  

所以，仅仅依靠进程或者进程组之间的关系进行进程管理是不够的，当然，只要进程之间能够实现关联，也可以使用各种 IPC 机制来实现进程之间关系的记录，只是涉及到效率和开销的问题，比如使用曾经尝试过但实际效果并不佳的 netlink，systemd 所采用的方案是通过 cgroup 的机制来实现进程跟踪.    

cgroups 是Linux内核提供的一种可以限制单个进程或者多个进程所使用资源的机制，可以对 cpu，内存等资源实现精细化的控制，目前越来越火的轻量级容器 Docker 就使用了 cgroups 提供的资源限制能力来完成cpu、内存等部分的资源控制。  

cgroups 的资源控制不像进程关系那样可以脱离继承关系，如果某个进程属于特定的cgroup fork()，则其子进程将成为同一组的成员。除非它具有特权并有权访问cgroup文件系统，否则它无法转出其组。cgroup 原本是内核为虚拟化设计的，除了资源控制，它在进程跟踪方面也有独特的优势。有一个可用的通知系统，以便当 cgroup 空运行时可以通知主进程。通过查看 /proc/$PID/cgroup 可以找到进程的 cgroup。因此，cgroups 作为进程跟踪一个很好的选择。   

关于 Cgrous 的概念，可以参考这两篇博客：
[美团技术团队：cgroups 简介](https://tech.meituan.com/2015/03/31/cgroups.html)
[知乎：浅谈cgroups机制](https://zhuanlan.zhihu.com/p/81668069)


## 进程的环境控制
进程管理系统如果仅仅是实现了监视和控制守护程序的启动、结束或崩溃这些基本功能，那么它不能算是优秀，一个好的进程管理系统更应该考虑系统层面的优化，比如更高的执行效率、更少的资源占用以及安全性，或者更易用。    

针对进程的优化可以通过设计更好的框架来实现，但是更多的情况在于，普适性的优化的效果总归是有限的，而针对性的优化才能带来更大的提升，要实现针对性的优化，自然就需要引用一些复杂的优化选项，而且这些选项必须由系统管理员进行设置，比如针对某些进程特性设置 CPU 和 IO 调度程序控件，设置进程的资源、功能边界，CPU 亲和力等等。

在 systemd 中，有专门的 resource control 配置选项，可以根据进程特性，针对 CPU、内存、IO 这三大块主要资源进行灵活的配置，以获得更好的执行效果。  

同时，systemd 还提供完善的日志系统，日志记录是执行服务的重要部分：理想情况下，服务生成的每一位输出都应记录下来。因此，一个init系统应该从一开始就为它生成的守护进程提供日志记录，并将stdout和stderr连接到syslog，甚至在某些情况下甚至将 /dev/kmsg连接到 syslog，这无疑对嵌入式领域的工作者非常友好，收集到的日志可以通过 journalctl 来查看。  


## 小结
对于早期的 sysvinit 和 Upstart 或者是更早的 init 系统来说,它们的重心都是放在 init 上,也就是 linux 用户空间的启动,而对于用户空间的维护做得并不多,这种做法有好有坏,好处在于其高度的模块化,系统每一部分的主要功能都是独立的,用户可以自由地进行选择,因为 linux 的开源特性,也有很多开发者乐衷于贡献自己的代码,缺点在于没有各个主要功能之间没有统一的接口,且学习成本较高.   

相对 Upstart 和 sysvinit 而言,systemd 明显具有非常大的野心,除了系统启动,还将很大一部分精力放在了统一用户空间服务上,是的,这里用的词是"统一",虽然目前并没有完全做到统一,但是 systemd 一直在朝这个方向努力,比如一直作为独立服务的 crontab,udev 被纳入 systemd,甚至是日志系统 syslog 都逐渐被 systemd 的 journalctl 取代,这种统一到底是好是坏,也只能让时间去见证了.   


## systemd 的基本特性
在上文中大致介绍了 systemd 背后的设计思想，接下来我们大概来看看 systemd 的相关功能特性，以建立一个基本的概念。  

如果不涉及到复杂的应用，系统管理员基本上只会接触到 service 配置文件，当我们要对某个服务程序进行管理时，编写一个 service 文件，指定服务开启、关闭时对应的指令，就可以通过 systemd 对该服务进行控制了("服务"这个概念本来是针对系统提供的 daemon 程序而言，比如 ssh、login，在本章中这个概念被引用到所有实现某个具体功能的进程或进程组，比如一个 QT 交互界面程序，尽管它可能是一个客户端程序).  

实际上，systemd 的配置文件分为多种，分别针对多种服务的配置，这些不同后缀的配置文件统称为单元文件(unit)，systemd 从系统目录下读取这些单元文件对服务进行相应的配置，这些文件通常被放置在这些目录下：
* /lib/systemd/system/
* /etc/systemd/system/
* /usr/lib/systemd/system/


以下是这些文件的简短介绍：
* service：.service 为后缀的文件，这是最常见的配置文件，针对服务相关的设置，包括服务的启停、开机启动所对应的指令，同时兼容 sysvinit 系统，能够读取 sysvinit 脚本，比如 /etc/init.d/ 目录下的文件，rc-local 等，sysvinit 系统的开机自启动一般是两种方式：
    * 将启动脚本放到 /etc/init.d/ 目录下，再使用 update-rc.d 指令更新开机启动设置
    * 直接在 /etc/rc.local 文件中添加启动指令，开机就会自动执行。 
    这两种方式都可以被 systemd 直接支持。
* socket：.socket 为后缀的文件，该单元将套接字封装在文件系统或网络上。当前支持流、数据报和顺序包类型的AF_INET，AF_INET6，AF_UNIX套接字。同时还支持经典FIFO作为传输方式。套接字文件中通常包含了其在套接字文件在系统中的目录、文件权限、buffer 大小等等。套接字文件不会被当成某个服务启动，都是通过依赖关系被系统自动启动。    
* device：.device 为后缀的文件，针对 linux 中的设备文件的配置，实际上这种类型的配置文件用得不多，所有硬件设备都由内核进行管理，当设备初始化、添加或者删除时，内核通过 netlink 发送消息到用户空间，用户空间统一通过 udev 接收并处理，执行用户指定的脚本或者是创建一些设备节点，需要使用 .device 文件来描述的情况并不多。
* mount: .mount 文件用于指定文件系统的加载点，这个文件系统可能是磁盘文件系统，也可能是基于 RAM 的文件系统，原本的 /etc/fstab 依旧可用，.mount 文件通常需要提供文件系统类型、挂载点等信息。  
* target：.target 文件是单元文件的逻辑分组，也就是说，单个服务配置文件通常是为了完成某个特定的功能，比如启动 ssh、login，当系统需要达到某个状态或者实现某个相对复杂的功能需求时，需要多个服务的组合，比如需要完成 sysinit，即系统初始化工作，需要先完成文件系统、交换分区、硬件设备的初始化，target 文件就是负责管理多个服务的分组，主要用于服务间依赖关系。  
* ...  

本章节只是对这些常见的单元文件做一些简短的介绍，后续还会对这些主要的配置文件做详细的解析。  


对应 systemd 的实现，主要有下列的特性：
* 对于每个产生的进程，用户可配置项为：执行环境、资源控制、工作目录和根目录，umask，IO class 和优先级，CPU亲和力、策略以及优先级，用户ID，组ID，执行组ID，可读/可写/不可访问目录，共享/私有/slave 挂载标志，安全控制位，各种子系统的cgroup控制等。这些配置项使得 systemd 拥有较大的资源配置灵活性，自然同时也会带来一定的复杂性。  

* 每个执行的进程都有自己的 cgroup(当前默认在 debug 子系统中)
* 配置文件使用的语法非常简单，采用 linux 中的 .desktop 文件格式，许多软件架构中都有对应的解析器，主要是两部分组成：section 描述配置组，每一个 section 下存在多项配置，以键值对的形式表示，右值可以是列表，section 名和配置属性都是在 systemd 命令手册中定义的。  
* 如果在多个配置源中配置了同一单元文件，例如，存在/etc/systemd/system/foo.service，也存在/etc/init.d/foo.service），则 systemd 系统配置将始终优先，而旧格式的配置(init.d)将被忽略，从而可以轻松地将服务从 sysvinit 升级到 systemd ，并且软件包可以同时承载SysV初始化脚本和systemd服务文件。  
* 服务配置文件支持模板的形式，比如：当系统中存在多个 getty 实例时，并不需要分别编写多个 getty 配置文件，而是实现一个 getty 的模板，需要使用到 getty 的时候在模板的基本上进行实例化。本质上，模板的作用就在于对多个同类服务抽象化，简化配置，模板的语法为：getty@.service