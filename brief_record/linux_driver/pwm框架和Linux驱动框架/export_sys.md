# 驱动接口导出到 /sys 用户空间
上一章我们讨论了 pwm controller 在系统中的注册，本章我们主要讨论 pwm controller 导出到 /sys 用户接口。  

## /sys 简介
sysfs文件系统是内核版本2.6引入的，它挂载在 /sys 目录下，在老版本系统中，内核信息和驱动信息都是放置在 /proc 目录下，这是一个基于 ram 的文件系统，也就是由内核启动时挂载，掉电或者重启将丢失所有数据，随着内核和驱动的逐渐庞大，内核维护者将运行时的驱动信息和内核信息分离，驱动专门由 /sys 目录下的文件描述。   

对 sysfs 有兴趣的朋友可以参考我的另一篇博客[linux sysfs文件系统简介](http://www.downeyboy.com/2019/03/03/linux_sysfs/)  
对于如何在驱动程序中创建 /sys 目录下文件接口可以参考我的另一篇博客[]()


## 通过 sysfs 操作 pwm
这一章我们主要讲解 pwmchip_sysfs_export() 这个接口，将 pwm 的操作导出到 sysfs 中，在了解该函数的实现之前，我们可以先看看怎么使用它导出的接口。  

事实上，pwmchip_sysfs_export() 是 linux 中的标准接口，所以，在大多数的平台上都有直接沿用了这个接口以将设备的操作导出到 sysfs 中，类似的还有 gpio_export() 函数，通过 sysfs 操作 pwm 在大部分平台都是同样的操作。  

### pwm controller目录
要操作某一路 pwm ，先要在 sysfs 中找到其对应的 pwm controller，通过其 controller 进行操作。  

一种常用的方法就是使用 find 指令，pwm controller 在 sysfs 中的命名通常是 pwmchipn,我们可以使用下面的指令：

```
find /sys -name "pwmchip*"
```

当然，通常一个系统中可能不止一个 pwm controller，会出现 pwmchip0、pwmchip1 等多个设备，这个时候就需要通过芯片的数据手册来确定，通常这些 pwm chip 的上级目录命名为 48302200.pwm 这一类带寄存器基地址的目录名，根据数据手册就可以确定你要操作的 pwm controller 的基地址，从而找到对应的 pwm controller.

### export
如果是标准的内核接口实现,在 sysfs 中对应一个 pwm_chip 的目录结构是这样的：

```
device  export  npwm  power  subsystem  uevent  unexport
```

其中，我们需要特别关注的文件就是 export、npwm、unexport，直接操作 pwm 就是通过这三个文件。  

其中，npwm 表示当前 pwm controller 包含多少路有效的 pwm device，具体操作就是将对应的 pwm device 从当前 pwm controller 中导出，可以用下面的指令：

```
echo 1 > export
```

这个写入到文件的数字表示当前 pwm device 在 pwm controller 中的编号，当然，硬件上的 pwm 输出引脚和软件上的 pwm device 的对应还得你自己去查数据手册。   

操作成功后将会在当前目录下生成一个表示 pwm device 的目录，如果当前目录是 pwmchip1 ，要操作的 pwm device number 为1，那么这个目录的命名为 pwm-1:1,，这个目录下的文件结构一般是这样的：

```

```













