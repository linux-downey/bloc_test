# initramfs 实现
在大部分分析 linux 系统启动的资料中，往往都是着重描写 U-boot 和内核的启动，毕竟这两部分是重点也是难点，在启动的最后一部分，通常的描述是：在内核启动的最后阶段，挂载上根文件系统，并启动用户空间的第一个进程 init，正式进入到用户空间的启动阶段。  

实际上，从内核到用户空间的启动通常并没有这么简单和直接，这里面涉及到几个问题：
1、在 FHS 的根文件系统规定中，/usr 目录是可以单独挂载到其它介质中，但是，通常 /usr 目录中包含启动文件，内核默认只是挂载根文件系统，此时无法正常启动系统。  
2、如果根文件系统被加密，除非为这种行为定制内核解密程序，否则 Linux 内核将无法找到 init 程序，导致系统无法启动。  
3、linux 支持的启动介质越来越多，比如 U 盘，SCSI 硬盘甚至网络根文件系统，如果需要兼容各种启动方式，linux 就需要将各种硬件驱动编译进内核中，包括各种存储介质的硬件驱动以及各种文件系统驱动，这种全部编译进内核的方式无疑会让内核变得臃肿。 
4、如果因为意外断电或者其它问题导致根文件系统出了问题，系统将直接无法启动，或者运行过程中出错，当然，在内核中对根文件系统进行检查和修复理论上是可以的，但是实现起来并不现实。  
5、对于某些厂商,如果不愿意开源某些硬件驱动,只想以 ko 的形式提供驱动,比如比较复杂的 nand 驱动程序,这种情况下就不方面直接集成到内核中.  

再次引用计算机界的那一句名言："计算机科学领域的任何问题都可以通过增加一个间接的中间层来解决",上述问题的解决方案是使用 initrd 程序来解决，initrd 就是这样的一个中间层，它是一个基于内存的磁盘结构(ramdisk)，内核在初始化完成之后并不是直接挂载根文件系统，而是先挂载 initrd，可以把 initrd 看成一个真正的磁盘设备，该设备上包含各种工具和脚本，而 initrd 的优势在于：作为一个中间层，它就是一个微缩版的 rootfs，可以直接执行用户空间的程序，比如结合 udev 检查系统上的各类硬件设备，再加上内核传递的命令行信息，可以针对性地加载硬件驱动程序，mount 对应的文件系统，或者对真实的根文件系统进行检查和修复，在所有的一切就准备妥当之后，再挂载真正的根文件系统，开启用户空间的启动工作。

当然，早期的 initrd 并不是完全具备上述的所有功能，一方面，initrd 随着软硬件的发展不断地更新迭代，通常是针对性地做一些适配。另一方面，也并不是所有的系统都需要 initrd 这么一个中间层，在上面列出的四个问题中，有些是可以通过定制化内核来解决，而有些问题对于某些应用场景并不存在，比如在嵌入式领域中，内核在不同平台上的移植性并不是那么重要，通常每一类平台都是定制化的内核，而且启动介质也比较单一，所以某些嵌入式平台也就不需要使用 initrd。 

## initramfs
在 2.6 内核之后,initrd 逐渐被 initramfs 代替了,这是因为传统的 initrd 尽管基于 ram,实际上它被实现为一个磁盘设备,格式化为 ext2 文件系统,因此,内核在挂载 initrd 时需要准备好对应的驱动程序,同时,作为一个完整的块设备,他需要整个文件系统的开销，消耗 Linux 内核中的缓存内存和易于使用的文件（如分页），这使得 initrd 有更大的内存消耗。  

和 initrd 不一样的是，initramfs 是基于 tmpfs 的文件系统，这是个大小灵活、且轻量级的文件系统，同时也并不是一个单独的块设备，(所以没有缓存和所有额外的开销),和 initrd 一样，initramfs 也是一个最小化的根文件系统，它被内核启动，然后执行该文件系统中的工具和脚本，支持硬件驱动程序的加载以及解密等功能。  

initramfs 通常被实现为一个 cpio 的归档文件，再在归档文件的基础上使用 gzip 进行压缩以节省空间，在启动时由内核进行解压，在内核挂载 initramfs 时，会在该文件系统中寻找 /init 文件并执行，如果 initramfs 中不存在 init 可执行文件，内核将会尝试老式的启动方式，需要注意的是，这个 init 可执行文件并不是我们常说的 init 进程，它更可能是一个执行脚本，这个脚本将会完成上文中提到的 initramfs 的所有工作，在工作的最后，一旦安装了根文件系统和其他重要文件系统，initramfs中的 init 脚本将把根切换到真实的根文件系统，并最终在该系统上调用 /sbin/init 二进制文件以继续启动过程。在目前常用的发行版中，这个 /sbin/init 是指向 /lib/systemd/systemd,而用户空间的启动则正式开始。  


## initramfs 的解压
光说不练假把式，仅仅是介绍概念并不能对 initramfs 有一个深入的理解，以 beaglebone 平台为例来看看一个 initramfs 是长什么样的。  

因为 initramfs 是启动相关的文件，所以通常被放置在 /boot 目录中，其命名通常是 initrd.img-version，比如 beaglebone 上的 initramfs 为：

```
/boot/initrd.img-4.14.108-ti-r124
```

通过 file 来查看该文件的类型：

```
debian@beaglebone:~$ file /boot/initrd.img-4.14.108-ti-r124

/boot/initrd.img-4.14.108-ti-r124: gzip compressed data, was "initramfs.cpio", last modified: Tue Aug 25 15:43:20 2020, from Unix
```

可以看到这确实是一个 gzip 的压缩文件，接下来要做的工作就是对其进行解压，执行下面的命令即可：
* mv initrd.img-4.14.108-ti-r124 initrd.img.tar.gz         // 修改为 .tar.gz 后缀
* gzip -d initrd.img.tar.gz                                //解压 gzip 文件，获取 initrd.img.tar 归档文件
* cpio -i < initrd.img.tar                                 // cpio 命令解包，并将文件包内所有文件拷贝到当前目录

执行完之后，键入 ls 查看当前目录下的文件，就可以看到 initramfs 中的全部内容了：

```
debian@beaglebone:~/initrd$ ls .

bin  conf  etc  init  lib  run  sbin  scripts
```

其中，init 脚本就是整个 initramfs 的主角，默认情况下，内核在挂载 initramfs 之后将会(也只会)调用该可执行程序，在这里它是个 shell 脚本程序，主要实现功能如下：
* 读取 /proc/cmdline 文件，通过该文件可以获取系统相关信息，比如真实根文件系统所属的文件系统类型以及所属的磁盘设备、挂载参数、调试终端串口号、相关硬件信息等等。   
* 加载文件系统对应的驱动，磁盘设备的驱动
* 如果是 nfs 文件系统，需要准备好相应的网络驱动
* 调用磁盘检测程序对目标磁盘、文件系统进行检测
* 挂载真正的 rootfs，并将根目录切换到新的根文件系统，开始真正的用户进程启动。 

而其它的目录，就是作为 init 脚本的支持，比如 bin/sbin 中包含脚本中需要的各种命令，lib 中包含所必须的库文件，以及 conf 和 etc 中的各类配置文件等。  

需要注意的是，initramfs 并不是由内核加载，而是是在 uboot 阶段被加载到指定内存中，通过 kernel cmdline 传递给内核(在设备树系统中，kernel cmdline 由设备树的 chosen->bootargs 替代),原因也很简单，initramfs 放在 /boot 目录下，而 /boot 目录位于根文件系统下，这时候还完全没有挂载真正的根文件系统，自然不能访问到 initramfs 镜像。  

该示例只是各类 initramfs 中的一种，不同的平台可能存在不同的实现，有兴趣的朋友可以将其它平台的 initramfs 文件解压出来并查看。  

## initramfs 自定义功能
用户可以在 initramfs 中添加自定义的功能，将系统的 initramfs 解压出来，添加自己想要的功能，然后再将其打包。


mv initrd.img-4.14.108-ti-r124 initrd.img.tar.gz
tar -xvf initrd.img.tar.gz

file initrd.img.tar.gz
gzip -d initrd.img.tar.gz

cpio -i < initrd.img.tar

就获取到 initramfs 的解压缩包。


find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
gzip initramfs.cpio



为什么存在 initramfs:
1,不能把所有的驱动编译进内核
2,文件系统检查,万一文件系统所处的物理磁盘有问题呢
3,boot 被加密怎么办
4,/usr 目录处于不同的存储介质上怎么办




https://wiki.gentoo.org/wiki/Initramfs/Guide/zh-cn