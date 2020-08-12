# systemd-6-bootup启动过程
systemd 替代了 sysvinit 和 Upstart 接管用户空间的启动，其一大特点在于优化了系统的启动速度，了解整个 systemd 的启动过程，才能更容易地对 linux 的整个用户空间的服务体系有一个整体的把握，在这一章节中，我们将讨论基于 systemd 的启动流程。  

## bootload 阶段
bootloader 是 linux 最开始执行的程序，在嵌入式领域，通常使用 U-boot 进行启动，实际上，在多数现代的嵌入式设备中，第一条执行的指令并不一定是从 U-boot 开始，比如对于 arm 处理器来说，系统上电后 CPU 默认读取的第一条指令在 0 地址处，在我们通常的认知中，程序运行在内存中，但是内存是掉电不保存的，所以在上电启动的时候需要将程序从 ROM 中拷贝到内存(即ram)中，所以需要一段程序执行拷贝功能，同时它不运行在内存中。  

通常厂商会在芯片的 0 地址处放置一块 boot-ROM，这是一片容量并不大的 Nor-flash，因为 Nor-flash 支持程序的片上执行，这一片 boot-ROM 中并不是放置 U-boot 的地方，而是放置厂商固化的程序，配合 CPU 内部的 SRAM，该程序中通常实现启动设备的选择、通过接口烧写 emmc，主要目的在于让开发者可以将打包好的系统镜像拷贝到片上存储器中或者使用外接的存储设备运行系统。  

开发者制作的镜像通常包含三个部分：U-boot、内核(包含dtb)、rootfs，U-boot 就是通用的系统启动代码，当镜像烧写到 emmc 中时，Boot-ROM 中的代码跳转到 emmc 中执行程序，emmc 上的 U-boot 负责初始化内存、将 linux 内核和 dtb 拷贝到内存中，然后跳转到内核执行，内核初始化硬件、建立接口，就开始着手启动用户空间。  

总所周知，用户空间的所有程序代码以及数据都是保存在 rootfs 中，启动用户空间需要挂载 rootfs，而 rootfs 可能保存在 SCSI 接口的硬盘上、也可能保存在 USB 接口的 U 盘上，甚至可能使用网络根文件系统。  

为了兼容各个硬件存储接口，一种方式就是将各种硬件驱动编译到内核中，通过开机时的硬件检测确定 rootfs 在哪种存储设备上，然后再初始化对应的驱动，挂载 rootfs，这种方式会导致内核中包含许多无用的驱动。  

另一种方式也是现在常用的方式，使用 initramfs，这是一种基于内存的 ramfs，initramfs 是一个非常小型的 rootfs，上面包含了系统启动必要的资源，于是，启动用户空间的流程变成了：内核挂载 initramfs，通过 initramfs 加载硬件驱动、并挂载真正的 rootfs，然后再卸载 initramfs，释放 initramfs 占用的所有资源，就像 initramfs 从来没有存在过，但是却轻松地解决了内核挂载 rootfs 需要支持多种硬件驱动的问题。   



## systemd 开始工作
当成功挂载了"root="内核引导选项指定的根文件系统之后，内核将启动由"init="内核引导选项指定的init程序， 从这个时间点开始，即进入了"常规启动流程"： 检测硬件设备并加载驱动、挂载必要的文件系统、启动所有必要的服务，等等。  

对于 systemd 系统来说，上述"init程序"就是 systemd 进程，而整个"常规启动流程"也以几个特殊的 target 单元作为节点，被划分为几个阶段性步骤。在每个阶段性步骤内部，任务是高度并行的， 所以无法准确预测同一阶段内单元的先后顺序， 但是不同阶段之间的先后顺序总是固定的。  

当启动系统时， systemd 将会以 default.target 为启动目标，借助单元之间环环相扣的依赖关系，即可完成"常规启动流程"。 default.target 通常只是一个指向 graphical.target(图形界面) 或 multi-user.target(文本控制台) 的软连接。 为了强制启动流程的规范性以及提高单元的并行性， 预先定义了一些具有特定含义的 target 单元。 

下面的图表解释了 这些具有特定含义的 target 单元之间的依赖关系 以及各自在启动流程中的位置。 图中的箭头表示了单元之间的依赖关系与先后顺序， 整个图表按照自上而下的时间顺序执行。


正如上文所说，默认启动的服务是 default.target，通常 default.target 是指向 graphical.target 或者 multi-user.target 的软链接，从上图中可以得知，实际上这两个 target 反而是在最后被启动的，这是 systemd 的依赖机制在发挥作用，对于 graphical.target 你可能没什么印象，但是 multi-user.target 应该是经常见到，因为在编写 service 文件时，我们通常会添加这样的 [Install] 小节：

```
...
[Install]
WantedBy=multi-user.target
```

这样的语句表明当前 service 会在 multi-user.target 之前被启动，以实现开机启动，而整个系统中所有服务的启动基本上都是通过这种环环相扣的依赖关系实现的(参见 Unit 部分：wants、requires、After、before 等依赖配置项)。  


通过单元文件中 wants、requires、After 等依赖配置项的追溯，最早被启动的单元为 local-fs-pre.target，这是本地文件系统相关的预设置。  

紧接着，就是一些前期的初始化服务的建立：
*  local-fs.target：本地文件系统的初始化，要启动用户空间，本地文件系统的挂载和检查工作是必不可少的，linux 的目录结构是非常灵活的，例如很可能 /usr 目录下挂载了另外的磁盘，由于 /usr 是系统启动必须的目录，所以需要检查并处理类似的情况。
* swap.target：处理交换分区
* cryptsetup.target：处理加密设备，加解密工作都是在非常早期进行处理。  
* 底层服务: 底层服务比如 udevd, tmpfiles,random seed,sysctl，通常这些底层服务都与其他服务相关，甚至需要为其它服务提供接口，所以在初始化时就需要将这些服务启动。 
* 底层虚拟文件系统：虚拟文件系统并不是真实存在在磁盘上的，比如 configfs，debugfs，这些文件系统由内核建立，为开发者或者其它服务提供接口，所以同样需要在初始化阶段将这些服务启动。  

上述这些服务都是并行启动的，这些服务有一个共同的特点，就是被 sysinit.target 单元依赖，可以通过下面两条指令查看到 sysinit.target 之前启动了哪些服务：

```
cat /lib/systemd/system/sysinit.target           //查看 sysinit.target 文件中静态设置的依赖
ls /lib/systemd/system/sysinit.target.wants      //查看该目录下的动态依赖，该目录中每一个软链接都是一个依赖项
```

当 sysinit.target 中依赖项启动过后(不一定启动成功，wants 依赖项启动失败不会影响 sysinit.target 进程，同时，只要没有设置 After= 项，并行启动就一直在发生)，就会进入到下一个阶段，启动 basic.target 以及其依赖项：
* timers：定时事件，可能是系统自带的，也可以是系统管理员为了满足某些功能新增的 timer
* paths：系统检测的路径
* socket：创建 socket，systemd 中一大特点在于，当服务创建 socket 时，建议让 systemd 统一创建所有 socket，这样可以缓解因接口依赖而导致的进程阻塞。 
* rescue.service：进入救援模式，这是一个独立的分支，通常不会启动该服务。  

如果你直接查看对应的单元文件，比如：

```
cat /lib/systemd/system/timers.target
```

在服务文件中基本上不会发现有任何静态定义的 timer 服务，通常，这些对应的单元文件都是动态使能的，如果要查看系统都有哪些 timers，需要查看 timers.target 对应的依赖目录：

```
ls /lib/systemd/system/timers.target.wants
```
当然，/lib/systemd/systemd 只是所有系统目录中的一个，这里只是举个例子，说明 timers.target 是如何影响这些动态定义的 target 的。  

对于 paths.target 和 sockets.target 也是同样的道理。 

在上述这些都启动完成之后，就进入到多用户模式的启动和图形界面的启动了，需要注意的是，在启动图形界面之前，需要先启动各个图形界面的组件，同样是通过依赖关系的设置来实现，对于图形界面的启动，这里就不过多赘述了。  







