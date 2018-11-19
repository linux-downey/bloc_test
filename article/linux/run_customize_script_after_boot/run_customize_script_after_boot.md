# linux下添加简单的开机自启动脚本
在linux的使用过程中，我们经常会碰到需要将某个自定义的应用程序设置为开机自启动以节省操作时间，这里提供两个设置开机自启动的方法。
####注：博主使用的ubuntu-16.04进行实验，其它版本可能有偏差，但实现原理类似。
## rc.local
### 在rc.local脚本中添加开机自启动程序
ubuntu在开机过程之后，会执行/etc/rc.local(注意/etc/init.d中也有个rc.local，不要弄混了)文件中的脚本程序，初始情况下，这个文件内容是这样的：

    downey@ubuntu:~$ cat /etc/rc.local
    #!/bin/sh -e
    #
    # rc.local
    #
    # This script is executed at the end of each multiuser runlevel.
    # Make sure that the script will "exit 0" on success or any other
    # value on error.
    #
    # In order to enable or disable this script just change the execution
    # bits.
    #
    # By default this script does nothing.
并不包含其它内容，用户可以在里面添加需要开机执行的脚本命令，这里以diodon粘贴板工具为例，如果我要开机运行diodon进程，在文本中添加：

    downey@ubuntu:~$ cat /etc/rc.local
    #!/bin/sh -e
    #
    # rc.local
    #
    # This script is executed at the end of each multiuser runlevel.
    # Make sure that the script will "exit 0" on success or any other
    # value on error.
    #
    # In order to enable or disable this script just change the execution
    # bits.
    #
    # By default this script does nothing.
    ./usr/bin/diodon &
    exit 0
看到这里有些盆友就要问了，为什么要在执行命令后面加&？
在shell执行命令后加&是为了让应用程序在后台运行，rc.local也是一个脚本，主进程在运行这个脚本时必须能够返回，如果在这个脚本里面执行了一些死循环或者其他无法返回的任务，整个系统就很可能卡死在这里，无法启动，所以在这里运行的用户程序必须是能够返回或者本身就使用一些后台运行的进程。
经过上面的添加，在下次重启的时候，使用命令：

    downey@ubuntu:~$ ps -ef |grep "diodon"
    downey     2097   1880  0 22:53 ?        00:00:04 diodon
    downey     2937   2842  0 23:27 pts/2    00:00:00 grep --color=auto diodon
就可以看到diodon进程已经在后台运行。
### 删除
既然有添加，就必须得有删除，其实以rc.local的删除方式很简单，直接删除rc.local中用户添加的部分即可。
***需要提醒的是，在操作系统文件时，做备份是非常必要的***

## 将用户脚本添加到/etc/init.d中
### 添加用户进程
第二种方式就是将自己的用户脚本添加到/etc/init.d并链接到自启动程序当中。
还是以diodon软件来举例，我编辑一个运行diodon的脚本：

    #!/bin/bash
    ./usr/bin/diodon
将其命名为diodon.sh,并用指令：

    chmod +x diodon.sh
    sudo cp diodon.sh /etc/init.d/
将文件放到/etc/init.d目录中，然后将diodon,sh脚本链接到开机运行序列中：

    cd /etc/init.d
    sudo update-rc.d diodon.sh defaults 96
    insserv: warning: script 'diodon' missing LSB tags and overrides
这样重新启动时，就可以看到diodon.sh正在运行了。
```sudo update-rc.d diodon.sh defaults 96```
在这条指令中，**update-rc.d**是一个系统的链接工具。
而**defaults 96**则是指定了脚本的开机顺序，数字为0-99，数字越大执行优先级越低，用户添加的程序最好选择低优先级的执行顺序，因为很可能我们的用户程序会依赖一些系统的应用进程，例如如果应用程序要使用到网络部分，就先得让网络后台程序先初始化完毕。
看到这里，细心的朋友已经发现了，在链接脚本时有一个警告：

    insserv: warning: script 'diodon' missing LSB tags and overrides
作为一个菜鸟而言，是不敢忽视任何警告的，所以只好求助google，解决办法是在自己的脚本中的#!/bin/bash下添加：

    ### BEGIN INIT INFO
    # Provides:          downey
    # Required-Start:    $local_fs $network
    # Required-Stop:     $local_fs
    # Default-Start:     2 3 4 5
    # Default-Stop:      0 1 6
    # Short-Description: tomcat service
    # Description:       tomcat service daemon
    ### END INIT INFO
添加这些的目的是告诉系统一些关于这个启动脚本的具体信息,其中比较重要的有这几项：

    # Required-Start:   运行这个脚本需要的环境
    # Required-Start:   停止这个脚本需要的环境
    # Default-Start:    提供运行的运行级别
    # Default-Stop:     不运行的运行级别
    # Description:      描述
关于linux下的运行级别参考:[linux运行级别](https://zh.wikipedia.org/wiki/%E8%BF%90%E8%A1%8C%E7%BA%A7%E5%88%AB)  

### 删除用户进程
既然有添加就必然有删除，如果需要删除自定义开机运行脚本，输入：

    sudo update-rc.d -f diodon remove


## systemd的开机自启动
上面提到的两种方式适用于经典的system V控制系统启动和关闭的情况，但是目前在大多数发行版上都开始使用了systemd的系统软件控制方式，systemd系统管理着linux下的进程运行，属于应用程序，不属于linux内核的范畴。  
linux内核加载启动之后，用户空间的第一个进程就是初始化进程，不同发行版采用了不同的启动程序，但是发展到目前，几乎大多数发行版已经开始使用systemd来管理软件的启动运行和结束。


好了，关于linux开机自启动脚本就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.