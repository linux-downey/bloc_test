udev 可以实现的内容:
* 重新为设备节点命名
* 通过创建链接的方式针对同一个设备提供一个持久化的命名
* 根据程序输出命令设备节点
* 设置设备节点的权限
* 当设备修改的时候,执行指定的脚本程序
* 为网络接口重新命名

rules 文件被保存在 /etc/udev/rules.d 目录下
/lib/udev/rules.d/
/usr/lib/udev/rules.d/ , /etc 有更高优先级

rules 文件以词法顺序进行解析,


基本规则:
KERNEL-匹配内核名称
subsystem-匹配子系统名称
driver-与支持设备的驱动程序名称匹配

分配系统资源:
* name:创建的设备名称
* symlink:创建的软链接列表

创建的文件都在 /dev 目录下.  

一个比较重要的规则:当设备找到对应的 rule 时,并不会停止该节点的处理,而是继续搜寻匹配所有的 rules 文件,
所以一个变动的设备可以有多次匹配.  


PROGRAM 关键字是执行脚本用于取名
RUN     关键字是执行脚本


https://www.freedesktop.org/software/systemd/man/udev.html
https://wiki.archlinux.org/index.php/Udev


如果手动地发送了 add,系统会自动发送 remove.  

/etc/udev/rules.d 中的会覆盖掉 /lib/udev/rules.d/ 中的同名 rules
所有的 rules 文件都会按照字典顺序被加载执行.  

使用 netlink 应用层程序可以监听udev 的所有事件并打印出来. 

udevadm info $PATH       // 可以查看udev数据库的信息
udevadm trigger $PATH    // 可以触发一个 change 类型的 udev 事件,这个事件使用了udev数据库中的信息
事件的字符串格式为:
add@/module/kset_create_delAC
TION=addDEVPATH=/module/kset_create_delSUBSYSTEM=moduleSEQNUM=3676

首先,所有发送过来的数据都是这种类型,有的还会在自定义 uevent 中添加一些字段.  

参考:https://www.freedesktop.org/software/systemd/man/udev.html

KERNEL 对应名称,一般来说,就是 $PATH 的最后一个分量.  
KERNELS:沿路径向上查找. 


SUBSYSTEM 对应 subsystem 字段.
SUBSYSTEMS:沿路径向上查找 

DEVPATH:匹配消息中的 DEVPATH.   

NAME:针对网络设备,匹配消息中的 NAME 字段. 

ENV 表示全局变量,可以直接设置全局变量,也可以直接使用全局变量,这个全局变量不是系统级别的,而是 udev rules 的全局变量. 
注意:通过内核发送过来的键值对中的 key 会被自动设定为全局变量,比如 SUBSYSTEM,DEVPATH 等. 


GOTO:与 LABEL 相对应,跳转到 LABEL 处.  

ATTR:对应目录下的文件,ATTR{$FILE_NAME} 做判断,但是需要注意权限问题
如果没有权限操作的话,可以通过 ENV 来实现操作.

ATTR{foo}=="foo"  该语句表示判断当前文件夹下是否有 foo 文件,且其值为 foo.  

ATTRS:和 ATTR 差不多,只是当找不到对应的文件的时候,会根据 DEVPATH 向上找,如果父级目录找到对应的属性文件也可以匹配.  


DRIVER:只有在消息中设置了 DRIVER 字段的才可以进行匹配.  
DRIVERS:沿路径向上查找.  

OWNER:
GROUP:
MODE:  设置权限

TAG:

IMPORT:导入外部的item.

IMPORT{TYPE}:
TYPE:
"program":外部程序
"builtin":内部的程序
"file":导入文件,格式必须是全局变量的形式
"cmdline":

OPTIONS:
link_priority=value : 设置 link 的优先级,高优先级可以覆盖低优先级的同名link

watch:追踪该文件,使用 inotify 机制,如果该文件在打开修改关闭之后,会产生一个 change event.  
nowatch

db_persist:将消息一直保存在 udev 数据库中,即使使用 udevadm info --cleanup-db 也不能删除.

static_node:创建静态的设备节点.  





主要是通过 udev_device_new_from_syspath  获取对应的 device 


properties 表示属性,也是全局变量 ENV{},这个可以是从内核发出的键值对中的 key,也可以是 udevd 中设置的.  

所有 properties 通过 list 的形式挂在内核上.  

udevadm 源码:

/etc/udev/udev.conf  在 udev_new 中设置 log 等级.  

udevadm 相关的 command 被保存在全局数组 udevadm_cmds 中. 

udevadm info -c 清除所有的 udev db,由此源代码可以找到数据库存放位置.
    **/run/udev/data:保存数据库位置,保存的是在解析中生成的 properties,内核发过来的 propertie 保存在对应的 uvent 文件中.**  
    /run/udev/links:链接
    /run/udev/tags:tags
    /run/udev/static_node-tags:
    /run/udev/watch:文件监测

udevadm info -e 将数据库全部导出,数据库为内核发过来的信息以及 /run/udev/data 中的数据.

udevamd info -a 将所有匹配项打印出来,用户可以根据这些匹配项去编写 rules 文件.  


udevadm trigger 通过往对应的 uevent 文件中写入 add/change.. 来实现 trigger 的操作. 



udevadm test,测试一个 uevent 事件的执行过程,可以看到哪些 rules 文件被解析.  




*******************************builtin function*******************************
src/udev/udev-builtin.c :

usb_id:
通过读取一系列的文件,添加一些环境变量:
ID_VENDOR  ID_VENDOR_ENC  ID_VENDOR_ID  ID_MODEL
ID_MODEL_ENC  ID_MODEL_ID  ID_REVISION  ID_SERIAL
ID_SERIAL_SHORT  ID_TYPE  ID_INSTANCE  ID_BUS
ID_USB_INTERFACES  ID_USB_INTERFACE_NUM  ID_USB_DRIVER

hwdb:
hwdb:搜索 /etc/udev/hwdb.d 和 /lib/udev/hwdb.d 中的文件,匹配其中的 hwdb 文件,将匹配上的键值对设置为全局变量.   

path_id:
根据不同的 subsystem 执行不同的程序生成 ID_PATH 和 ID_PATH_TAG 两个全局变量

net_id:
读取 type
读取 ifindex
读取 iflink

可以设置 DEVTYPE 为 wlan,wwan,如果没有设置,DEVTYPE 就是 eth.   
如果 addr_assign_type 被设置为 0,网口的 mac 地址就会从 address 文件中读.  
并且将 mac 地址赋值给全局变量 ID_NET_NAME_MAC  
并根据 mac 地址的前三个字节找到 hwdb:20-OUI.hwdb 中对应的厂商,当前 imx8 的 mac 为 00:04:9f:05:93:e6,对应的 hwdb 匹配为:
ID_OUI_FROM_DATABASE=Freescale Semiconductor

针对不同的通信协议做不同处理,ccw(ccwgroup device)/pci/usb,并导出一些全局变量.   

网卡路径:/sys/devices/platform/30be0000.ethernet/net/eth0


input_id:

读取 
capabilities/ev
capabilities/abs
capabilities/rel
capabilities/key
properties   //父级


设置 ID_INPUT_KEY 全局变量
设置 ID_INPUT 全局变量


net_setup_link:

根据 DRIVER 的值设置 ID_NET_DRIVER.  

设置 ID_NET_LINK_FILE ID_NET_NAME



设置名称,设置 mac.



kmod:
    直接调用 load_module,加载模块. 














文章思路:
udev 简介,机制实现,以及简单的 rules 文件编写,
udev 从内核到用户空间的传递流程
udevd 分析以及內建函数分析
udev 相关工具分析以及使用


