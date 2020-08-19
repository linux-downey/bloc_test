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


### udev_builtin_input_id

用户通过执行命令: ls /dev/input 可以查看到系统中存在的输入设备,结果通常是这样的:

```
by-path  event0  event1  event2
```

最常见的鼠标,键盘,摇杆之类的输入设备都会显示在该设备目录下,一般情况下,直接显示为 event+id,而这个 id 就是由 udev 中的內建接口 udev_builtin_input_id 来管理.  

udev_builtin_input_id 的源码位于 systemd 的 src/udev/udev-builtin-hwdb.c 中:

```c++
const struct udev_builtin udev_builtin_input_id = {
    .name = "input_id",
    .cmd = builtin_input_id,
    .help = "Input device properties",
};
```

udev 规则中的 input_id 命令直接调用到上面的 cmd 回调函数,也就是 builtin_input_id:

```
static int builtin_input_id(struct udev_device *dev, int argc, char *argv[], bool test) {
    
}
```


参考:http://www.jinbuguo.com/systemd/hwdb.html


















udevd 的启动以及使用. 
