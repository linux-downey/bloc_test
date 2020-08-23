# linux udev 机制-2-实现原理
经过前面章节的铺垫，对于 udev 以及 udev 的 rules 编写已经有了一个基本的概念，足够应付一些简单的应用场景，但是，这是不够的，作为一名合格的软件工程师，尤其是嵌入式软件工程师，必须深入到实现原理，了解 udev 的来龙去脉。  

## 内核对设备的处理
在 udev 系列的第一章中，我们就提到：对于支持热拔插的设备，先需要在设备树中进行注册，当设备硬件上连接到系统上时，通常硬件上就可以检测到设备的插入，通常是产生硬件中断事件，内核的中断处理中就会匹配到设备相应的驱动程序并执行，这个过程就属于内核中对设备的处理。  

内核中的驱动程序负责初始化设备，同时提供设备操作接口并导出到用户空间，实际上，内核是完全有权限直接在用户空间创建设备接口的，而且 devfs 也是这么做的，但是后续证明这一部分工作完全是可以在用户空间做的，也可以更灵活地实现，同时 devfs 本身也有各种各样的问题，也就推进了 udev 的发展，因此，2.6 之后的版本中，设备接口由 udevd 守护进程在用户空间接管。  

之前是直接通过内核创建，现在改为由用户空间管理，然而用户空间自然是无法接触到内核信息的，因此，需要实现内核与用户空间之间的通信，linux 中采用的是 netlink 套接字机制，这是一种异步通信机制，对于 netlink 套接字的介绍和使用，可以参考我的另一篇博客：TODO,详细地说明了 netlink 如何实现用户与内核、用户与用户之间的通信。  

另一个必要重要的点是，内核与用户之间传递了什么数据？对于设备的变动，在内核中为描述为设备事件，而 kuevent 事件机制专门用来处理内核事件，kuevent 是建立在 kobject/kset 结构之上的，也就是和 sysfs 强相关的。  

对于某些行为，kuevent 会自动通过 netlink 将数据发送到用户空间，比如当驱动中直接或间接地调用了 kset_create_and_add 接口时，最终会调用到 kobject_uevent 函数，该函数负责设备信息的发送。同时，驱动程序中也可以显式地调用 kobject_uevent 相关的接口，直接发送设备消息到用户空间。关于内核的 kuevent 机制以及 kuevent 的实现，可以操作这一篇博客，在这篇博客中，详细讲解了数据内容的组织以及数据的发送。  

netlink 属于套接字的一种，用户空间的 udevd 守护进程一直在监听该套接字，也就能收到对应的内核消息。内核消息的格式通常是这样的:TODO.  

同时内核发送消息是广播的形式，你甚至可以自己实现一个应用程序来监听内核消息。如果你想在用户空间监听内核消息，可以试试我提供的这个 netlink server 端程序，用它来监听内核的 netlink 设备信息,TODO。    

在内核发送的消息中，有三个关键字是固定存在的：ACTION=,DEVPATH=,SUBSYSTEM=,同时内核中也可以以键值对的方式自定义地添加关键字，比如 driver=，这些关键字将会自动地添加到 udev 的关键字列表中，而在用户空间的 udev rules 文件中，匹配字段和关键字远远不止这几个，这是因为 udev 本身支持一些默认的、自定义的关键字，并不是所有匹配关键字都是由内核提供的，比如 KERNEL=，这是 udev 通过判断设备内核名称所创建的关键字，在内核信息中是不包含 KERNEL= 这个关键字的,所以如果要完全了解 udev 的配置项,除了了解内核发送的 netlink 消息,同时需要了解 udev 本身对设备的处理。  

## udevd 处理设备数据
udev 软件上的实现分为两部分,一个是 udevd 守护进程,负责监听内核套接字,接收内核信息并处理,另一部分是客户端程序,客户端程序通过和守护进程交互获取设备信息或者对设备进行操作.   

在实际的实现中,存在一个问题:在开机启动时,内核会首先对所有设备进行枚举并逐个初始化,而这个时候用户空间还没有启动,udevd 自然也是没有启动的,所以在 udevd 真正被启动时,大部分的系统设备已经初始化完成,udevd 也错过了所有内核发送的设备事件信息.对于这个问题,内核是通过 sysfs 解决的,内核将设备相关的信息导出到 sysfs 中,在 udevd 启动时,通过读取 sysfs 文件系统就可以获取到设备信息,同时,使用客户端程序 udevadm trigger 命令重新生成设备事件,使用 udevadm 工具模拟内核发送设备事件的行为,让 udevd 重新处理错过的那些设备事件.   

## udevadm 
udevadm 是官方提供的一个用于调试管理 udevd 的一个客户端程序,集成了数据库查询,监视,触发等关键功能,下面是 udevadm 的相关使用方法.  

### udevadm info
对于 udevd 接收到的设备信息,会把它们存放到 udev 数据库中,使用客户端程序可以实现对数据库的操作,udevadm info 指令用于查询数据库内容.udevadm info 的指令格式如下:

```
udevadm info [options] [devpath|file|unit...]
```
udevadm info 带有一个选项和查询对象,这个对象可以是 /dev 目录下的某个设备,也可以是 /sys 目录下的某个路径,同时也支持 systemd 中的设备描述文件(必须以.device)结尾.    

udevadm info 支持以下选项:
* -q,--query=TYPE:查询的指定类型的设备,TYPE 可以是下列值之一： name, symlink, path, property, all(默认值)
* -p,--path=DEVPATH:指定目标设备在 /sys 目录下的路径,因此可以省略 /sys 目录前缀,udevadm info --path=/class/block/sda 和 udevadm info /sys/class/block/sda 是等价的. 
* -n,--name=FILE:设备在 /dev 目录下的名称,也可以是一个软链接,同样的 /dev 目录前缀可以省略,udevadm info --name=sda 和 udevadm info /dev/sda 是相等的.  
* -r,--root:以绝对路径显示 --query=name 与 --query=symlink 的查询结果
* -a,--attribute-walk:按照udev规则的格式，显示所有可用于匹配该设备的sysfs属性： 从该设备自身开始，沿着设备树向上回溯(一直到树根)， 显示沿途每个设备的sysfs属性。
* -x,--export:以 键='值' 的格式输出此设备的属性(注意，值两边有单引号界定)。 仅在指定了 --query=property 或 --device-id-of-file=FILE 的情况下才有效。
* -d, --device-id-of-file=FILE:显示设备文件所在底层设备的主/次设备号,如果使用了此选项，那么将忽略所有位置参数。
* -e, --export-db:导出数据库的全部内容
* -c,--cleanup-db:清除 udev 数据库. 

### udevadm trigger
udev 的另一个调试利器就是 udevadm trigger，在系统启动时 udevd 会在内核驱动都初始化完成之后再启动，也就监听不到系统启动时内核发送的设备信息，正是由 udevadm trigger 重新生成设备事件，对于 udevd 来说，将这些设备信息当成由内核发送的来处理。  

同时，在系统正常运行时，基于调试的目的，也完全可以触发某个设备事件来模拟热插拔，下面是 udevadm trigger 的使用方法：
* -v, --verbose：显示将会被触发的设备列表，该设备列表对应 /sys/devices 目录下的大部分设备文件。 
* -n, --dry-run：执行触发的动作，但是最后并不真正触发设备事件，也就是在执行 udevadm trigger 的基础上，最后不将数据发出，其它操作照样执行。   
* -t, --type=TYPE：仅触发特定类型的设备，TYPE 可以是下列值之一： devices(默认值), subsystems
* -s, --subsystem-match=SUBSYSTEM：仅触发属于 SUBSYSTEM 子系统的设备事件。 可以在 SUBSYSTEM 中使用shell风格的通配符。 如果多次使用此选项，那么表示以 OR 逻辑连接每个匹配规则， 也就是说，所有匹配的子系统中的设备都会被触发。SUBSYSTEM 的值可以参考 udev 中的 rules 文件。 
* -S, --subsystem-nomatch=SUBSYSTEM：不触发属于 SUBSYSTEM 子系统的设备事件。 可以在 SUBSYSTEM 中使用shell风格的通配符。 如果多次使用此选项，那么表示以 AND 逻辑连接每个匹配规则， 也就是说，只有不匹配所有指定子系统的设备才会被触发。
* -a, --attr-match=ATTRIBUTE=VALUE：仅触发那些在设备的sysfs目录中存在 ATTRIBUTE 文件的设备事件。 如果同时还指定了"=VALUE"，那么表示仅触发那些 ATTRIBUTE 文件的内容匹配 VALUE 的设备事件。 注意，可以在 VALUE 中使用shell风格的通配符。 如果多次使用此选项，那么表示以 AND 逻辑连接每个匹配规则， 也就是说，只有匹配所有指定属性的设备才会被触发。
* -A, --attr-nomatch=ATTRIBUTE=VALUE：不触发那些在设备的sysfs目录中存在 ATTRIBUTE 文件的设备事件。 如果同时还指定了"=VALUE"，那么表示不触发那些 ATTRIBUTE 文件的内容匹配 VALUE 的设备事件。 注意，可以在 VALUE 中使用shell风格的通配符。 如果多次使用此选项，那么表示以 AND 逻辑连接每个匹配规则， 也就是说，只有不匹配所有指定属性的设备才会被触发。
* -p, --property-match=PROPERTY=VALUE：仅触发那些设备的 PROPERTY 属性值匹配 VALUE 的设备事件。注意，可以在 VALUE 中使用shell风格的通配符。 如果多次使用此选项，那么表示以 OR 逻辑连接每个匹配规则， 也就是说，匹配任意一个属性值的设备都会被触发。
* -g, --tag-match=PROPERTY：仅触发匹配 PROPERTY 标签的设备事件。如果多次使用此选项， 那么表示以 AND 逻辑连接每个匹配规则，也就是说，只有匹配所有指定标签的设备才会被触发。
* -y, --sysname-match=SYSNAME：仅触发设备sys名称(也就是该设备在 /sys 路径下最末端的文件名)匹配 SYSNAME 的设备事件。 注意，可以在 SYSNAME 中使用shell风格的通配符。 如果多次使用此选项，那么表示以 OR 逻辑连接每个匹配规则， 也就是说，匹配任意一个sys名称的设备都会被触发。
* --name-match=DEVPATH：触发给定设备及其所有子设备的事件。DEVPATH 是该设备在 /dev 目录下的路径。 如果多次使用此选项，那么仅以最后一个为准。
* -b, --parent-match=SYSPATH：触发给定设备及其所有子设备的事件。SYSPATH 是该设备在 /sys 目录下的路径。 如果多次使用此选项，那么仅以最后一个为准。
* -w, --settle：除了触发设备事件之外，还要等待这些事件完成。 注意，此选项仅等待该命令自身触发的事件完成， 而 udevadm settle 则要一直等到 所有设备事件全部完成。
* --wait-daemon[=SECONDS]：在触发设备事件之前，等待 systemd-udevd 守护进程完成初始化。 默认等待 5 秒之后超时(可以使用 SECONDS 参数修改)。 此选项等价于在 udevadm trigger 命令之前先使用 udevadm control --ping 命令。

udevadm 的大部分指令都是为了实现灵活地触发设备事件而设计，有个印象就好，需要用到的时候使用 udevadm trigger -h 或者查找[官方手册](https://www.freedesktop.org/software/systemd/man/udevadm.html)就好。  



### udevadm info 示例
使用 udevadm info 的目的在于查看设备保存在数据库中的信息，同样以 rtc 为例，看看 udevadm info 所展示的信息是否和内核设备事件发送的信息一致：

```
udevadm info -n /dev/rtc

P: /devices/platform/ocp/44e3e000.rtc/rtc/rtc0
N: rtc0
L: -100
S: rtc
E: DEVLINKS=/dev/rtc
E: DEVNAME=/dev/rtc0
E: DEVPATH=/devices/platform/ocp/44e3e000.rtc/rtc/rtc0
E: MAJOR=252
E: MINOR=0
E: SUBSYSTEM=rtc
E: USEC_INITIALIZED=2550426
E: net.ifnames=0
```

其中：
* P 表示 PATH，即设备路径
* N 表示 Name，对应设备内核名称
* L 表示 udev 中 OPTION 字段的 link_priority 属性，因为 rtc 是一个软链接，如果为多个不同的设备指定了相同的软连接， 那么实际的软连接将指向 link_priority 值最高的设备。  
* S 表示 SUBSYSTEM，对应子系统的名称。 
* E 表示 ENV，表示全局变量。  

同时，为了验证该设备触发时内核发送的消息，使用我自己编写的客户端程序(github地址:TODO)接收内核消息，然后使用 udevadm trigger --action=add  /dev/rtc 触发 rtc 设备，得到的消息为：  

```
add@/devices/platform/ocp/44e3e000.rtc/rtc/rtc0/omap_rtc_scratch0 ACTION=add DEVPATH=/devices/platform/ocp/44e3e000.rtc/rtc/rtc0/omap_rtc_scratch0 SUBSYSTEM=nvmem SYNTH_UUID=0 SEQNUM=4270
```

在内核发送的消息中,包含 ACTION=,DEVPATH=,SUBSYSTEM=,SYNTH_UUID=,SEQNUM= 等字段.其中,ACTION=,SEQNUM= 等字段是针对解析过程的,不需要持久地保存在数据库中.  

同时,在介绍 uevent 的文章中(TODO)有提到:内核在发送设备信息到用户空间时,默认存在三个字段:ACTION=,DEVPATH=,SUBSYSTEM=,除此之外,用户还可以提供自定义的 uevent 函数添加字段,比如内核中大部分硬件设备都会添加 MAJOR=,MINOR=,DEVNAME= 等字段,这些字段除了会被一起发送到用户空间,同时也会通过对应 /sys 目录下的 uevent 文件导出,你可以通过查看 /sys 下的各个 uevent 文件进行对比.  

经过对比发现,正如前文中所说的,udev 设备信息数据并非完全来自于内核消息,另外的部分可能来自于 /sys 对应目录下设备文件,还有可能是由 udev 程序产生的,比如上文中的 USEC_INITIALIZED 环境变量,link_priority 的值. 




### udevadm test
如果要了解 udev 对内核事件的处理过程，可以通过 udevadm test 命令来实现，该命令模拟一个设备事件，并输出调试信息。

下面是命令的格式：

```
udevadm test [options] [devpath] 
```

需要注意的是，这个命令只支持 /sys 下的设备文件，而不支持 /dev 目录下的设备节点，udevadm test 命令的输出为：
* 哪些配置目录、文件的时间戳被更新，哪个配置文件将会使用
* 依次输出 udev 扫描的 .rules 文件，尽管该 .rules 文件没有匹配成功。 
* 依次输出与当前设备匹配的规则，并打印该条规则属于哪个 .rules(规则文件) 的哪一行。  
* 输出当前设备匹配的配置文件
* 输出当次设备事件的所有全局变量，这些全局变量以键值对的方式输出



