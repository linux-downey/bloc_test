# systemd 启动速度的调试
在 i.mx8 平台上开发的时候，发现板子的启动需要2分钟以上，但是使用官方编译工程所编译出的最小化的镜像只需要十几秒，我所做的就是在官方镜像的基础上将 rootfs 替换成 ubuntu18.04 的rootfs，通俗地说就是将 Ubuntu 移植到 i.mx8 平台上。  

从十几秒到两分钟，仅仅是因为移植了 ubuntu，而且除了 docker 之外，没有在 ubuntu 上预装额外的软件 ，这是非常不正常的。   


## 复现并观察问题
有过调试经验的都知道，解决 bug 的第一步就是正确地了解这个 bug，这就需要通过不断地复现和观察问题。通过多次重启，启动时间稳定在2分钟左右，而且观察到两个问题：
* 被放置在 /lib/modules/`uname -r`/kernel 目录下的，需要开机被 udev 自动加载到内核中的模块加载非常慢，几乎是在启动流程的最后阶段才被加载到内核中。
* 在整个两分钟的加载过程中，在启动十几秒之后 ssh 和串口终端可以登录，但是 docker 无法使用，所以初步判定是 docker 的原因。  

## 查看 log 信息
调试 bug 最重要的一部分就是查看 log 信息，由于当前系统中的启动服务由 systemd 来负责，自然是从 systemd 下手，好在 systemd 提供比较完善的 dump 系统。  

### 查看启动时间

首先，查看启动服务的耗时，使用 systemd-analyze ：

```
systemd-analyze blame
```

这条指令将会根据时间的顺序，排列出所有启动的服务，输出如下：

```
2.506s docker.service
2min 57ms systemd-networkd-wait-online.service
1.802s dev-mmcblk0p2.device
543ms networkd-dispatcher.service
541ms NetworkManager.service
438ms systemd-journal-flush.service
....
....
```
可以看到，罪魁祸首是 systemd-networkd-wait-online.service，花了整整两分钟，从网上查找了一下这个模块的功能，它的作用是检查并等待网络可用。  

但是，我尝试了一下，在启动十几秒之后便可以通过 ssh 登录，而且可以 ping 通外网，说明网络这时候已经初始化完成了，为什么还卡在这里？  

接着在网上找，但是也没有找到原因，更多说法是这是一个版本的 bug，既然确定了这个服务是检查网络而且网络已经是可用的状态，那么就可以绕过这个服务的启动，具体做法是这样的：
* systemd-networkd-wait-online 这个服务的配置文件在 /lib/systemd/system/systemd-networkd-wait-online.service,于是编辑这个配置文件：

```
[Unit]
Description=Wait for Network to be Configured
Documentation=man:systemd-networkd-wait-online.service(8)
DefaultDependencies=no
Conflicts=shutdown.target
Requires=systemd-networkd.service
After=systemd-networkd.service
Before=network-online.target shutdown.target

[Service]
Type=oneshot
ExecStart=/lib/systemd/systemd-networkd-wait-online
TimeoutStartSec=5sec     //-->>添加 
RemainAfterExit=yes

[Install]
WantedBy=network-online.target
```
根据 systemd 服务的编写规则，给该服务添加一个超时时间：
```
TimeoutStartSec=5sec
```
这种情况下，超过设置的超时时间就放弃这个服务的启动。   


### 问题的转移
改了这个问题之后，立马重启查看效果，发现启动时间并没有减少，几乎也是要 2 分钟，继续使用 systemd-analyze blame 命令查看各服务启动时间：

```
2min 1.297s docker.service
5.302s systemd-networkd-wait-online.service
1.802s dev-mmcblk0p2.device
543ms networkd-dispatcher.service
541ms NetworkManager.service
438ms systemd-journal-flush.service
```
发现上次的修改是有用的，systemd-networkd-wait-online.service 这个服务的占用时长明显降低，但是 docker 服务所占用的时间变长，不难猜测到，docker本身的启动也是需要2分钟，但是 systemd-networkd-wait-online 的启动的那两分钟时间 docker 也已经启动好，于是掩盖了这个问题。  
 
###　查看 log
继续在网上查找相关资料，发现 docker 的启动并不需要这么久，这是一个异常的情况，所以继续查 docker 的启动，从 docker 的启动日志下手。  

在 systemd 中，日志由 journalctl 进行管理，我们可以使用该命令来查看启动日志，比如：

journalctl -b              //使用该命令可以查看到当前的启动日志。  

journalctl -f              // 查看当前服务的log信息

journalctl -r             //以倒序的方式显示log，从最新的开始显示，这个非常有用

journalctl -u service     //针对某一个服务查看其log信息

journalctl --since=DATE    //显示某个时间段的消息，DATE 表示日期，通常设置为 today

于是，在使用 journalctl -u docker --since=today 指令查看 log 的时候发现两个问题：

* 第一个是一个报错信息：

    ```
    modprobe: FATAL: Module ip_tables not found in directory /lib/modules/4.14.98+
    Dec 20 12:19:15 localhost.localdomain dockerd[5142]: iptables v1.6.1: can't initialize iptables table `nat': Table does not exist (do you need
    Dec 20 12:19:15 localhost.localdomain dockerd[5142]: Perhaps iptables or your kernel needs to be upgraded.
    ...
    ```
    该日志的意思就是没有找到某个依赖的外部模块被加载，需要重新到 /lib/modules/ 目录下扫描并加载，也就是说启动到这里的时候，docker 所依赖的模块还没有加载。

* 第二个问题，是一条提示信息：

    ```
    crypto/rand: blocked for 60 seconds waiting to read random data from the kernel.
    ```

### 内核模块的加载
对于第一个问题，解决方案是放弃 udev 通过对模块依赖的分析并加载，而通过手动指定的方式加载。  

直接编辑 /etc/modules-load.d/modules.conf 文件，在该文件中写入需要开机加载的模块，每一个模块占用一行，保存、重启，查看修改结果。  

重启之后，继续查看启动 log ：

```
journalctl -u docker --since=today
```

发现针对模块的报错信息不存在了，在 ssh 登录之后使用 lsmod ，发现指定的模块已经被加载上，但是实际上的启动时间还是没有改善。  

那就接着看下一个问题。  

### 获取随机数
第二个问题实际上非常隐秘，仅仅是info而不是报错(甚至不是警告)，报错信息会由红色字体标出，所以info信息容易被忽略，整个 log 信息传达的意思非常明确：系统随机数不足，需要阻塞系统并获取足够的随机数。  

在加密系统中，随机数的专业名词叫 "熵"，简而言之就是加密系统的输入需要真随机数，主要是一些不可预测的数据，问题找到了：就是随机数不足。  

脑子里并没有清晰的解决方案，那就上 google 搜索，解决的方式也非常简单，安装一个软件即可：

```
sudo apt-get install rng-tools
```

这个软件负责收集系统中的随机事件，足可以解决随机数不足的问题，安装完成之后，重启。  


## 结果
在这次的重启中，发现整个的启动过程就只用 20 来秒时间，然后继续使用 systemd-analyze blame 指令查看各服务启动时间，结果为：

```
5.253s systemd-networkd-wait-online.service
2.622s docker.service
1.946s dev-mmcblk0p2.device
579ms networkd-dispatcher.service
505ms NetworkManager.service
491ms systemd-journal-flush.service
466ms systemd-udev-trigger.service

```

问题得到解决。 

## 总结
systemd 启动速度的优化是一个相对复杂的过程，并不一定由单一的原因导致，如果你也有系统启动的调试需求，需要注意以下几点：
* 复现问题是调试的第一步，对于有些偶发性的bug，可以尝试创建极端的软件运行环境以复现bug。
* 观察，仔细地观察，收集每次bug复现时的软硬件信息，与正常状态相对比。查找调试方法，通常是调试工具、日志等，这些信息是整个调试的重点。
* 每找到一个问题，并修正一个问题后要验证，必须控制单一的变量。重复地 还原-修改-还原-修改 过程以确定你的问题确实通过本次修改得到解决。
* 善于借助于网络，利用前人的经验。





