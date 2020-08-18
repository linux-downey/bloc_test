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
hwdb 相对于其它內建接口来说比较特殊，

udev_builtin_hwdb 內建接口被定义在 src/udev/udev-builtin-hwdb.c 中：

```c++
const struct udev_builtin udev_builtin_hwdb = {
        .name = "hwdb",
        .cmd = builtin_hwdb,
        ...
};
```






















udevd 的启动以及使用. 
