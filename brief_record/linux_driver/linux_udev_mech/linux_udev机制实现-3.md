# udev builtin 接口
从内核 uevent 事件机制到 netlink 通信,再到用户空间的 udevd 守护进程对内核信息的规则匹配,以及如何使用官方工具调试 udev,讲到这里,udev 的整个流程基本上已经浮出水面了.   

我们可以通过阅读 /lib/udev/rules.d/50-udev-default.rules 规则文件来检验学习成果,这个规则文件为系统提供了大量的默认规则,其中包括各种基础设备的处理,完整地阅读这个文件可以大致了解到 /dev 下的各个设备文件,软链接,以及文件权限是如何设置的.   

通过前面章节的铺垫,尽管大部分的规则都可以了解,但是还有一部分没有讲到:udev 的 builtin(內建) 接口.很不幸的是,我在网上没有查到官方文档,因此,只好通过阅读相关版本的源代码来解决这个问题.  

udev 目前被集成到了 systemd 中,所以它的源代码地址为:TODO.   

下载程序时要注意下载对应版本的 systemd 源码，systemd 的版本之间是不兼容的，所以在安装使用 udev 的时候，如果使用的 udevadm 客户端程序和 udevd 守护进程的版本不匹配，并不能保证它能完全正常运行。前后版本互不兼容也是 systemd 快速迭代带来的一个弊端。在 systemd 逐渐步入成熟之后这个问题应该会逐步得到解决。    


## builtin 接口
首先来看看 /lib/udev/rules.d/50-udev-default.rules 中与 builtin 相关的规则：

```
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", IMPORT{builtin}="usb_id", IMPORT{builtin}="hwdb --subsystem=usb"
SUBSYSTEM=="input", ENV{ID_INPUT}=="", IMPORT{builtin}="input_id"
ENV{MODALIAS}!="", IMPORT{builtin}="hwdb --subsystem=$env{SUBSYSTEM}"
```
builtin 为内置程序，需要通过 IMPORT 关键字导入，也就是 IMPORT{builtin}="function-name" 表示执行內建的 function-name 函数，执行內建函数会输出一系列 "key=value" 值，这些键值对将会导入到 udev 的全局变量中，相当于在规则中使用 ENV 关键字创建全局变量。  


## builtin 源码
builtin 接口的源码实现位于 systemd 源码 src/udev/udev-builtin.c 中，所有 udev 支持的內建接口都定义在指针数组 builtins 中：

```c++
static const struct udev_builtin *builtins[] = {
#if HAVE_BLKID
        [UDEV_BUILTIN_BLKID] = &udev_builtin_blkid,
#endif
        [UDEV_BUILTIN_BTRFS] = &udev_builtin_btrfs,
        [UDEV_BUILTIN_HWDB] = &udev_builtin_hwdb,
        [UDEV_BUILTIN_INPUT_ID] = &udev_builtin_input_id,
        [UDEV_BUILTIN_KEYBOARD] = &udev_builtin_keyboard,
#if HAVE_KMOD
        [UDEV_BUILTIN_KMOD] = &udev_builtin_kmod,
#endif
        [UDEV_BUILTIN_NET_ID] = &udev_builtin_net_id,
        [UDEV_BUILTIN_NET_LINK] = &udev_builtin_net_setup_link,
        [UDEV_BUILTIN_PATH_ID] = &udev_builtin_path_id,
        [UDEV_BUILTIN_USB_ID] = &udev_builtin_usb_id,
#if HAVE_ACL
        [UDEV_BUILTIN_UACCESS] = &udev_builtin_uaccess,
#endif
};
```

通过上述贴出的源码定义可以发现，udev 支持的內建接口有：
* UDEV_BUILTIN_BLKID：块设备 ID 的处理，该功能取决于是否定义了 HAVE_BLKID
* UDEV_BUILTIN_BTRFS：btrfs 文件系统检查，Btrfs 是一种新型的写时复制 (CoW) Linux 文件系统，已经并入内核主线。
* UDEV_BUILTIN_HWDB：硬件信息数据库的查询与处理。软件上可以通过某些参数来匹配硬件信息数据库中的数据，比如通过 USB 设备的 UID/PID 可以匹配到硬件数据库中厂商提供的额外设备信息。    
* UDEV_BUILTIN_INPUT_ID：输入设备 ID 的处理
* UDEV_BUILTIN_KEYBOARD：键盘设备的处理
* UDEV_BUILTIN_KMOD：外部模块的处理，该功能取决于是否定义了 HAVE_KMOD
* UDEV_BUILTIN_NET_ID：网络 ID 的处理，比如网络设备的命名
* UDEV_BUILTIN_NET_LINK：网络底层链路的处理，比如网卡 mac 地址的设置
* UDEV_BUILTIN_PATH_ID：根据不同的子系统路径执行不同的程序逻辑，并导出变量
* UDEV_BUILTIN_USB_ID：USB ID 的处理
* UDEV_BUILTIN_UACCESS：uaccess 相关的安全处理，该功能取决于是否定义了 HAVE_ACL

內建接口的调用方式和在 udev 规则中调用 RUN 命令执行脚本是一样的，比如： 

```
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", IMPORT{builtin}="usb_id", IMPORT{builtin}="hwdb --subsystem=usb"
```

上述规则演示了当内核发生 usb 子系统相关事件，且 DEVTYPE 为 "usb_device" 时，调用內建函数 usb_id,随后再调用內建函数 hwdb，并传递命令行参数：--subsystem=usb，可以将內建接口看成是一个完整的程序。  

上文中列出的內建接口列表中每个成员都是一个结构体指针，指向一个 struct udev_builtin 类型的结构体

```
struct udev_builtin {
    // 名称
    const char *name;    
    // 真正执行的函数
    int (*cmd)(struct udev_device *dev, int argc, char *argv[], bool test);
    ...
};
```

该结构体中的 name 成员对应內建接口的名称，udev 规则中直接使用名称即可调用內建的接口，比如上述的 usb_id 正是 UDEV_BUILTIN_USB_ID 类型的內建接口。而真正执行的接口为 cmd 函数。  

接下来，我们简要地分析几个常用的內建接口所实现的功能，如果你对这部分的具体实现有兴趣，也可以仔细阅读相关源代码。  


### udev_builtin_hwdb

####　hwdb 简介
硬件数据库文件(hwdb)位于操作系统发行商维护的 /usr/lib/udev/hwdb.d 目录中，以及系统管理员维护的 /etc/udev/hwdb.d 目录中。所有的 hwdb 文件(无论位于哪个目录中)，统一按照文件名的字典顺序处理。对于不同目录下的同名 hwdb 文件，仅以 /etc/udev/hwdb.d 目录中的那一个为准。依据这个特性，系统管理员可以使用 /etc/udev/hwdb.d 目录中的自定义文件替代 /usr/lib/udev/hwdb.d 目录中的同名文件。 如果系统管理员想要屏蔽 /usr/lib/udev/hwdb.d 目录中的某个 hwdb 文件， 那么最佳做法是在 /etc/udev/hwdb.d 目录中创建一个指向 /dev/null 的同名符号链接， 即可彻底屏蔽 /usr/lib/udev/hwdb.d 目录中的同名文件。 注意，硬件数据库文件必须以 .hwdb 作为后缀名，否则将被忽略。  

每个硬件数据库文件(hwdb)都包含一系列由"matche"与关联的"key-value"组成的记录。 每条记录由一个或多个用于匹配的"matche"字符串(可包含shell风格的通配符)开头， 多个"matche"字符串之间使用换行符分隔，但必须是依次紧紧相连的行(也就是中间不能出现空行)， 每一行都必须是一个完整的"matche"字符串(也就是不能将一个"matche"字符串分为两行)， **多行之间是逻辑或(OR)的关系。** 每一个"matche"字符串都必须顶行书写(也就是行首不能是空白字符)。

"matche"行之后是一个或多个以空格开头的"key-value"行(必须以空格开头作为区分)， "key-value"行必须符合 "key=value" 格式。 一个空白行表示一条记录结束。 以 "#" 开头的行将被视为注释而被忽略。

如果有多条记录的"matche"串都匹配同一个给定的查找字符串，那么所有匹配记录中的"key-value"都将被融合在一起。如果某个"key"出现了多次，那么仅以最高优先级记录中的"value"为准(每个"key"仅允许拥有一个单独的"value")。 对于不同硬件数据库文件(hwdb)中的记录来说，文件名的字典顺序越靠后，优先级越高； 对于同一个硬件数据库文件(hwdb)中的记录来说， 记录自身的位置越靠后，优先级越高。  

所有 hwdb 文件都将被 systemd-hwdb 编译为二进制格式的数据库，并存放在 /etc/udev/hwdb.bin 文件中。 注意，操作系统发行商应该将预装的二进制格式的数据库存放在 /usr/lib/udev/hwdb.bin 文件中。 系统在运行时，仅会读取二进制格式的硬件数据库。  

下面是一个 hwdb 文件的片段,以帮助我们对 hwdb 文件建立一个基本概念:

```
...
usb:v1D6B*
 ID_VENDOR_FROM_DATABASE=Linux Foundation

usb:v1D6Bp0001*
 ID_MODEL_FROM_DATABASE=1.1 root hub

usb:v1D6Bp0002*
 ID_MODEL_FROM_DATABASE=2.0 root hub

usb:v1D6Bp0003*
 ID_MODEL_FROM_DATABASE=3.0 root hub
...
```

该片段取自 /lib/udev/hwdb.d/20-usb-vendor-model.hwdb,ubuntu 18.04 中使用这个文件保存 USB 的 UID:PID 相关信息.对于一个 USB 设备而言,UID:PID 是跟随硬件的,且对于某一类设备是唯一的,因此内核可以在驱动程序中获取 USB 设备的 UID:PID 值,并将它们通过 /sysfs 导出或者通过 netlink 发送到用户空间,udevd 则通过获取的 UID:PID 到数据库中查询,获取相应的数据库信息.   

比如上面贴出的 hwdb 文件片段中,VID 1D6B 是被 linux 占用的,PID 为 0001~0003 都被注册为 linux 的 root hub,当当前触发事件的 UID/PID 与数据库中的字段匹配时,ID_MODEL_FROM_DATABASE 这个变量就被赋值为 1.1/2.0/3.0 root hub,并导入到 udev 的全局变量中,udevd 通过这些信息就可以判断当前 root hub 的 USB 类型,并执行一些相应的程序逻辑.  


udev_builtin_hwdb 內建接口被定义在 src/udev/udev-builtin-hwdb.c 中：

```c++
const struct udev_builtin udev_builtin_hwdb = {
        .name = "hwdb",
        .cmd = builtin_hwdb,
        .init = builtin_hwdb_init,
        ...
};
```
builtin_hwdb_init 为 hwdb 初始化函数,在 systemd 初始化 hwdb 时调用一次,它调用 sd_hwdb_new 函数,该函数将会读取以下文件:
* /etc/systemd/hwdb/hwdb.bin
* /etc/udev/hwdb.bin
* /usr/lib/systemd/hwdb/hwdb.bin
* /lib/systemd/hwdb/hwdb.bin,是否支持该文件由宏定义 HAVE_SPLIT_USR 是否被定义决定

也就是说,hwdb 中的数据并不取决于 hwdb.d 中的各个以 .hwdb 结尾的硬件数据库文件,而是取决于这些硬件数据库文件最后编译生成的 bin 文件.  

在 hwdb 內建程序被调用时,主要是调用 cmd 回调函数,即 builtin_hwdb 函数:

```c++
static int builtin_hwdb(struct udev_device *dev, int argc, char *argv[], bool test) {
    
    // 命令行参数解析部分,主要是对搜索过程进行控制,支持四种命令行参数:
    //     --filter,-f : 指定搜索过滤规则
    //     --device,-d : 指定其它设备
    //     --subsystem,-s : 指定搜索子系统
    //     --lookup-prefix,-p : 指定搜索前缀

    // 搜索函数
    if (udev_builtin_hwdb_search(dev, srcdev, subsystem, prefix, filter, test) > 0)
            return EXIT_SUCCESS;
    return EXIT_FAILURE;
}
```

搜索函数是 builtin hwdb 的核心部分,它将设备关联属性转换成 hwdb 数据中的 match 格式,再使用这些属性逐个匹配数据库中的数据.  

比如,对于 USB 而言,它先读取设备属性中的 idVendor(vid) 和 idProduct(pid),并对属性进行拼接,代码如下:

```c++
v = udev_device_get_sysattr_value(dev, "idVendor");
p = udev_device_get_sysattr_value(dev, "idProduct");
vn = strtol(v, NULL, 16);
pn = strtol(p, NULL, 16);
snprintf(s, size, "usb:v%04Xp%04X*", vn, pn);
return s;
```
最后将返回结果字符串作为匹配关键字去匹配数据库,如果匹配成功,就将数据库中的数据部分(键值对)导入到 udev 全局变量中.  


### udev_builtin_net_id , udev_builtin_net_setup_link
在平常的使用中,网络算是比较重要的部分了,通常跟它打交道的时间最多,同时,因为使用得多,网络接口问题也是非常频繁的.   

同样的,udev 负责网络接口的生成以及配置问题,比如网络接口名,比如:为什么 ubuntu 上默认网络接口名为 ens33,而嵌入式设备上通常是 eth0,它们是如何被生成的,又比如为什么我的两台嵌入式设备的 MAC 地址是相同的,理论上 mac 地址不应该是全球唯一的吗?除了最常见的这两个问题,网络还有很多设置问题也是通过 udev 来解决.  

网络相关的两个內建函数,一个是 udev_builtin_net_id,另一个是 udev_builtin_net_setup_link,前者负责硬件接口的变量导出,而后者则是真正负责网络相关参数设置的接口. 


udev_builtin_net_id 的定义在 src/udev/udev-builtin-net_id.c 中:

```c++
const struct udev_builtin udev_builtin_net_id = {
        .name = "net_id",
        .cmd = builtin_net_id,
};
```

执行 IMPORT{builtin}="net_id" 时调用 cmd 回调函数,也就是 builtin_net_id 接口:

```c++
static int builtin_net_id(struct udev_device *dev, int argc, char *argv[], bool test) {
    // 读取网卡对应 sysfs 路径下(下文中的文件读取都是基于设备路径)的 type 文件,确定当前链路层是以太网还是 slip 网络
    // 读取 ifindex 和 iflink 文件,确定两者的值一致,以确定目标设备是符合处理条件的
    // 获取设备类型,设备类型可能是 wlan/wwan,以此确定命名前缀,wlan 前缀为 wl,wwan 对应 ww,以太网对应 en,而 slip 网络对应 sl.
    // 获取设备 mac 地址,这个 mac 地址是通过读取 address 文件获取的,但同时要满足一个条件:addr_assign_type 文件中的值不为 0,addr_assign_type 表示 mac 地址的确定方式,这个值后文中详解.
    // 将上述确定的 前缀+x+mac地址 进行字符串连接,并赋值给全局变量 ID_NET_NAME_MAC,比如:ID_NET_NAME_MAC=enx00049f0593e6
    // 处理硬件接口:网络设备的接口可能是多种,比如 PCI,USB,根据硬件接口的不同导出 ID_NET_NAME_PATH 和 ID_NET_NAME_SLOT 两个变量. 
}
```

builtin_net_id 主要工作还是导出全局变量给后续的规则使用.  


接下来看看核心的部分:udev_builtin_net_setup_link,通过这个接口可以直接对网络设备接口进行设置,udev_builtin_net_setup_link 接口的源代码在 src/udev/udev-builtin-net_setup_link.c 中:

```c++
const struct udev_builtin udev_builtin_net_setup_link = {
    .name = "net_setup_link",
    .cmd = builtin_net_setup_link,
    .init = builtin_net_setup_link_init,
    .validate = builtin_net_setup_link_validate,
};

```
init 回调函数在初始化的时候调用,它的实现是这样的:
```c++
static int builtin_net_setup_link_init(struct udev *udev) {
    // 初始化上下文结构,准备执行环境
    ...
    link_config_load(ctx);
}
```

link_config_load 是初始化阶段的核心函数,这个函数主要是加载以及解析配置文件,对应的配置文件都是以 .link 结尾,分布在指定的目录中,这些目录有: /etc/system/network,/run/systemd/network,/usr/lib/systemd/network,lib/systemd/network, 这些 .link 配置文件用于控制网络接口的各项参数,link 文件的相关信息可以参考文档:
* [systemd.link](https://www.freedesktop.org/software/systemd/man/systemd.link.html)
* [systemd.link 中文手册](http://www.jinbuguo.com/systemd/systemd.link.html)

如果你没有更改过你的系统,大概率在 /lib/systemd/network 下有一个 99-default.link 文件,这是系统对网络接口默认的配置,和 systemd 的配置语法一样,配置文件支持多个小节,每个小节有多个选项,而 link_config_load 对 .link 文件的解析结果将保存在 link_config 的结构体中,以备后续使用.   

初始化只有一次,当每次执行 IMPORT{builtin}="net_setup_link" 时,将会调用到 cmd 回调函数,即 builtin_net_setup_link:

```c++
static int builtin_net_setup_link(struct udev_device *dev, int argc, char **argv, bool test) {
    // 获取设备的 driver 属性,即该设备在内核中的 driver 名称.并赋值给全局变量 ID_NET_DRIVER,需要注意的是,内核并不会对所有的网络接口都导出 driver 属性. 需要通过内核到用户空间的 nelink 信息确定.  
    // 上文中初始化过程中解析 .link 文件会生成 link_config 结构体.使用当前网络设备的信息与 .link 文件中进行匹配,如果匹配成功,就获取对应的 link_config 结构体,作为配置参数.(具体的匹配需要参考 .link 文件中的 [match] 小节中的配置项).  

    // 应用所有配置.这个函数主要操作网络接口有:speed,wol,mac,name 等接口配置,需要注意的是,在此之前当前网络接口的命名和 mac 地址都存在默认的值,mac 的默认值为内核导出的 address 文件值,接口名为内核中导出的网络接口名,比如 eth0 .link 配置文件只是在默认值的基础上进行修改,如果没有匹配成功的 .link 文件,网络接口也是可以以默认参数存在并正常使用.  

    link_config_apply(ctx, link, dev, &name);
        /*
        根据 .link 文件的解析结果设置的 speed 设置当前网络接口的 speed.   
        设置 wol,wol 为 wake on line,远程唤醒
        获取并验证 ifindex 文件内容并验证其值

        获取 .link 文件中指定的 NamePolicy,翻译过来就是命名策略,命名策略一共有以下多种:
            kernel:由内核设置固定的名称,如果设置了该值,且 name_assign_type 文件中的值为 2,3,4,其它所有的策略设置都会失效. 
            database:基于网卡的 ID_NET_NAME_FROM_DATABASE 属性值(来自于udev硬件数据库)设置网卡的名称。
            onboard:基于网卡的 ID_NET_NAME_ONBOARD 属性值(来自于板载网卡固件)设置网卡的名称。
            slot:基于网卡的 ID_NET_NAME_SLOT 属性值(来自于可插拔网卡固件)设置网卡的名称。
            path:基于网卡的 ID_NET_NAME_PATH 属性值(来自于网卡的总线位置)设置网卡的名称。
            mac:基于网卡的 ID_NET_NAME_MAC 属性值(来自于网卡的固定MAC地址)设置网卡的名称。
            keep:如果网卡已经被空户空间命名(创建新设备时命名或对已有设备重命名)， 那么就保留它(不进行重命名操作)。
        在 .link 文件中,可以设置多种命名策略的组合,优先级从先到后,如果前面的策略被成功实施,后续的策略就会被省略,默认的 /lib/systemd/network/99-default.link 中就指定了 NamePolicy=kernel database onboard slot path.  

        同时,前面的策略并不一定被成功实施,比如,设置为 kernel 时,需要在 .link 配置文件中同时设置 Name= 选项,又比如设置为 database 时,ID_NET_NAME_FROM_DATABASE 不能为空,为空表示策略实施失败,尝试下一个,直到 new_name 不为空.  

        对于 mac 的设置,同样依赖于 .link 文件中的 MACAddressPolicy= 选项,mac 策略支持两种:
            persistent:内核确定的固定 mac 地址.
            random:随机生成的 mac 地址. 
            none:无条件使用内核提供的 mac 地址
        同时需要结合 addr_assign_type 文件中的值,addr_assign_type 是内核导出的文件,用户空间的设置需要和内核适配.比如配置文件中设置为 random,但是内核导出的  addr_assign_type 文件中值为 0,代表 persistent,则不会设置.  
        同时,当没有指定 mac policy 时,直接在 .links 文件中指定 MACAddress= 也可以指定 mac 地址,不够要符合 mac 地址的格式规范.  
        */
}
```


### udev_builtin_path_id
UDEV_BUILTIN_PATH_ID 这个內建接口主要作用是找到并导出当前设备在 /sys/devices 中的存在路径,它的源码位于:src/udev/udev-builtin-path_id.c 中:

```c++
const struct udev_builtin udev_builtin_path_id = {
        .name = "path_id",
        .cmd = builtin_path_id,
        ...
};
```

同样的,在执行 IMPORT{builtin}="path_id" 时会调用到 builtin_path_id 函数,这个函数实现比较简单,从当前节点向上索引,一级一级地找到 subsystem 所属的父级目录,并赋值给 ID_PATH,比如:ID_PATH=platform-4a100000.ethernet,其中 - 表示目录分隔符 / 的替换。   


### udev_builtin_usb_id
udev_builtin_usb_id 这个內建接口负责导出 usb 设备的相关信息，具体的定义在 src/udev/udev-builtin-usb_id.c 中：

```c++
const struct udev_builtin udev_builtin_usb_id = {
        .name = "usb_id",
        .cmd = builtin_usb_id,
        ...
};
```

当执行 IMPORT{builtin}="usb_id" 时会调用到 cmd 回调函数，即 builtin_usb_id,builtin_usb_id 这个函数的实现比较繁琐，但是总结起来又比较简单：就是通过读取设备目录下的各个属性文件，然后将这些属性文件中的数据复制给全局变量，然后导入到 udev 中。这些全局变量以及对应的属性文件如下：
* ID_VENDOR 的值源于 vendor 或 manufacturer
* ID_VENDOR_ENC 的值源于 vendor 或 manufacturer
* ID_VENDOR_ID 的值源于 idVendor
* ID_MODEL 的值源于 model 或 product 
* ID_MODEL_ENC 的值源于 model 或 product 
* ID_MODEL_ID 的值源于 idProduct
* ID_REVISION 的值源于 bcdDevice
* ID_SERIAL 的值源于 serial
* .... 更多全局变量的导出可以参考源码。 

## 小结
內建接口通常完成两项工作:
* 完成特定的工作,比如载入模块,比如设置网络接口名称,mac地址.
* 导出一些全局变量,这些全局变量可以在后续的规则中进行匹配以及使用,本章中并不对所有导出的变量进行持续跟踪,如果你想了解这些变量最后是如何使用的,可以参考我前面章节中介绍的 udev 机制以及 .rules 文件编写规则,对系统中的 udev 行为进行分析.  


需要注意的是，这篇文章并没有仔细地去分析每个內建接口的实现细节，而只是将內建接口的概念、源代码地址以及主要的內建接口进行了概括，如果各位有兴趣，可以下载源码进行研究。   


/sys 下的文件是否可以通过规则修改. 
是不是同时设置 policy=kernel 和 name= 才会指定对应的网卡名称.  
如果设置了该值,且 name_assign_type 文件中的值为 2,3,4,其它所有的策略设置都会失效. 
kernel:由内核设置固定的名称,如果设置了该值,且 name_assign_type 文件中的值为 2,3,4,其它所有的策略设置都会失效. 

参考:http://www.jinbuguo.com/systemd/hwdb.html


udevadm test 使用. 


