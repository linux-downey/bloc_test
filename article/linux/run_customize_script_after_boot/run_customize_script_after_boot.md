# linux下添加简单的开机自启动脚本
在linux的使用过程中，我们经常会碰到需要将某个自定义的应用程序设置为开机自启动以节省操作时间，这里提供两个设置开机自启动的方法。  

###注：博主使用的ubuntu-16.04进行实验，其它版本可能有偏差，但实现原理类似。  

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
上面提到的两种方式适用于经典的system V控制系统启动和关闭的情况，但是目前(2018年10月)在大多数发行版上都开始使用了systemd的系统软件控制方式，包括Ubuntu16，centos.systemd系统管理着linux下的进程运行，属于应用程序，不属于linux内核的范畴。   

在systemd系统上设置开机自启动的方式也是非常简单的(尽管systemd这套软件管理工具并不简单)。 

## 确定系统是否应用了systemd工具来管理
这里要注意的是，systemd是linux发行版上的预装工具，用来管理系统软件的启动运行和结束，所以通常来说，这套东西是依赖于发行版的，如果系统使用了这一套工具，那么就可以使用它来管理进程，如果不是，即使你安装了它，它也不会被默认配置为系统管理工具。  

查看系统是否使用systemd工具我们可以使用如下的指令：

    systemd --version
如果系统返回如下类似的信息，表明该系统是由systemd工具来管理软件：

    systemd 232
    +PAM +AUDIT +SELINUX +IMA +APPARMOR +SMACK +SYSVINIT +UTMP +LIBCRYPTSETUP +GCRYPT +GNUTLS +ACL +XZ +LZ4 +SECCOMP +BLKID +ELFUTILS +KMOD +IDN

## systemctl的使用

对软件的管理主要是同通过systemd工具中的systemctl命令，相比于之前的system V的控制方式，systemd显得更加简洁明了，对用户更加友好，拿httpd来举例：

    开启httpd服务：
    sudo systemctl start httpd
    设置httpd服务自启动：
    sudo systemctl enable httpd

至于关闭和取消自启动，大家心里应该有数了吧。  

## 设置开机自启动
我们再回到重点，设置开机自启动。  

我们要为目标设置一个配置文件，其实这是可以预想到的，linux作为一个复杂的系统，开机自启动涉及到的依赖、运行级别、运行环境等等问题肯定需要用户去指定，在启动的时候系统才知道怎么正确地去运行软件。这个配置文件固定以.service作为后缀，比如我们如果要运行/home/downey目录下的test.sh脚本，我们可以添加一个配置文件***test.service***:

    [Unit]
    Description=
    Documentation=
    After=network.target
    Wants=
    Requires=

    [Service]
    ExecStart=/home/downey/test.sh
    ExecStop=
    ExecReload=/home/downey/test.sh
    Type=simple

    [Install]
    WantedBy=multi-user.target
将文件放在/usr/lib/systemd/system 或者 /etc/systemd/system目录下，然后可以测试一下：  

    sudo systemctl start test.service  

然后你可以查看你的/home/downey/test.sh脚本是否已经运行，如果已经运行表示配置文件没有问题。然后可以键入：

    sudo systemctl enable test.service  

设置test脚本开机启动。如果上一步没有出问题，这一步基本上也不会有什么问题，系统会打印出如下信息：

    Created symlink /etc/systemd/system/multi-user.target.wants/test.service → /usr/lib/systemd/system/test.service.  

可以看到，这里在/etc/systemd/system/multi-user.target.wants/目录下创建了一个/usr/lib/systemd/system/test.service文件的软链接，到这里设置开机自启动就完成了。  

## 配置文件的简单解析
在上面的配置文件中，为了演示起见，我将一些本测试脚本不需要但是比较重要的配置项也写了出来，其实如果不需要可以删除，但是[Unit]/[Service]/[Install]这三个标签需要保留。  
我们来一个个简单介绍一下配置项：

    Description：运行软件描述
    Documentation：软件的文档
    After：因为软件的启动通常依赖于其他软件，这里是指定在哪个服务被启动之后再启动，设置优先级
    Wants：弱依赖于某个服务，目标服务的运行状态可以影响到本软件但不会决定本软件运行状态
    Requires：强依赖某个服务，目标服务的状态可以决定本软件运行。
    ExecStart：执行命令
    ExecStop：停止执行命令
    ExecReload：重启时的命令
    Type：软件运行方式，默认为simple
    WantedBy：这里相当于设置软件，选择运行在linux的哪个运行级别，只是在systemd中不在有运行级别概念，但是这里权当这么理解。  

如果有多项，用逗号作为分隔。  


好了，关于linux开机自启动脚本就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.