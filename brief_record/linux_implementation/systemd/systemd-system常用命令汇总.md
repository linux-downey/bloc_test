# systemd 常用命令汇总
在前面的文章中，针对 systemd 的产生背景、单元文件的配置做了一些介绍，相对于系统管理员来说，了解原理固然重要，摆在面前更重要的问题是：如何使用 systemd。  

在这一章节中，我们专注于使用者的角度，看看我们可以使用 systemd 这个管理系统做些什么，这其中包括我们已经使用过的 systemctl 命令，自然也不仅仅是 systemctl 命令。  

几乎每个接触过 systemd 的开发者都使用过 systemctl 这个命令，其中绝大多数第一次接触到 systemd 的开发者可能仅仅是因为要将自己的服务设置成开机自启动，在尝试过 rc.local 开机自启动失败之后无奈地发现只能寻找新的开机启动方式，一番搜索自然就会找到 systemd，然后按照网上某个博客照葫芦画瓢地编写配置文件，再使用：systemctl enable foo.service 命令将自己的 foo 服务设置开机自启动，这也是我第一次接触到 systemctl 命令的情况。  
(注：运行 systemd 的系统是兼容 sysvinit 自启动方式的，只是某些系统上并不会默认使能 rc-local.service 服务，所以 rc.local 配置可能并不奏效)。  

实际上，systemctl 命令远远不止设置开机启动这个功能，systemd 也不仅仅只有 systemctl 这个命令。 

## systemctl 常用命令
systemctl 命令的格式如下：

```
systemctl [OPTIONS...] COMMAND [UNIT...]
```
systemctl 主要是两个部分，一个是选项部分，一个是命令部分，下列是一些常用的 systemctl 选项或者命令。  

### start PATTERN…、stop PATTERN…、restart PATTERN…、try-restart PATTERN…
start 和 stop 命令用于启动和停止服务，实际上是调用到 service 文件中的 ExecStart= 和 ExecStop= 对应的指令，如果没有指定 ExecStop=，停止服务的方式使用默认方式，即发送停止信号给对应服务进程。   

restart 则表示先停止在开始，如果服务原本处于停止状态，则直接将服务启动。  

try-restart 和 restart 类似，区别在于 try-restart 不会直接启动原本处于停止状态的服务。  

需要注意的是，尽管平常使用 start 和 stop 都是直接指定对应服务，实际上可以使用 PATTERN 进行匹配。  

### enable/disable
enable 负责启用指定的单元,大多数情况下,这被用于设置开机自启动.   

enable 指令是按照单元文件中的 [Install] 小节进行操作的,[Install]小节一般做这样的设置:
```
WantedBy=multi-user.target
```
WantedBy= 和 Want= 是一对反义的设置,在当前文件(foo.service)中设置 WantedBy=multi-user.target 的含义相当于在 multi-user.target 中设置 Wanted=foo.service.  

但是具体实现方式有区别,因为在启动 multi-user.target 只会去读 multi-user.target 对应的依赖项,而不会读取 foo.service 的配置项,所以这条配置实现依赖的方式是通过另一种方法:该 WantBy= 配置项将会在调用 enable 时在 /etc/systemd/system/multi-user.target.wants/ 目录中创建一个当前单元文件的软连接,实际上我们也可以通过在调用 systemctl enable foo.service 时看到打印消息.  

在之前的介绍 service 的文章中有提到, 在 multi-user.target 被启动时,会去读 multi-user.target.wants 中的指向其它服务的软连接,在 .wants 目录下的软连接等同于设置 Wants= 配置项,因此,foo.service 就会在 multi-user.target 之前启动,而 multi-user.target 是开机时启动的服务,所以也就实现了 foo.service 的开机自启动.  

上面的示例是 Wants 类型的依赖,对于 requires 依赖也是同样的效果.  

如果 [Install] 小节中指定了 Also= 单元,enable 同样作用与该 Also 单元.  

相对于 enable 而言,disable 表示撤销 enable 的操作,即删除软链接,同样作用与 Also 单元,在删除之后,还会重新加载 systemd 自身的配置,相当于执行 systemd daemon-reload,以确保所有变更都生效.   

### mask/unmask
屏蔽指定的单元或单元实例。 也就是在单元目录中创建指向 /dev/null 的同名符号连接，从而在根本上确保无法启动这些单元。 这比 disable 命令更彻底，可以通杀一切启动方法(包括手动启动)，所以应该谨慎使用该命令。 若与 --runtime 选项连用，则表示仅作临时性屏蔽(重启后屏蔽将失效)，否则默认为永久性屏蔽。 除非使用了 --now 选项(相当于同时执行 stop 命令)，否则仅屏蔽一个单元并不会导致该单元被停止。 此命令的参数仅能接受单元的名字， 而不能接受单元文件的路径。   

unmask 解除 mask 的屏蔽.  

### status
status 命令主要用于查看服务的状态，如果不指定目标服务，即：systemctl status，将会显示整个系统的状态信息，这些状态信息是针对所有服务的信息概览。  

如果需要查看指定服务更详细的信息，需要指定具体服务,具体服务可以通过服务名或者 PID 来指定，将会显示服务对应的状态信息，状态信息中包含服务的加载状态、运行状态、Cgroups、运行日志等，通过 status 给出的信息基本上可以判断一个服务所处的状态，下面是 ssh 服务的 status 信息：

systemctl status ssh
```
● ssh.service - OpenBSD Secure Shell server
   Loaded: loaded (/lib/systemd/system/ssh.service; enabled; vendor preset: enabled)
   Active: active (running) since Mon 2020-08-03 12:15:24 UTC; 3h 13min ago
  Process: 1160 ExecStartPre=/usr/sbin/sshd -t (code=exited, status=0/SUCCESS)
 Main PID: 1207 (sshd)
    Tasks: 1 (limit: 4915)
   CGroup: /system.slice/ssh.service
           └─1207 /usr/sbin/sshd -D

Aug 03 12:15:22 beaglebone systemd[1]: Starting OpenBSD Secure Shell server...
Aug 03 12:15:24 beaglebone sshd[1207]: Server listening on 0.0.0.0 port 22.
Aug 03 12:15:24 beaglebone sshd[1207]: Server listening on :: port 22.
Aug 03 12:15:24 beaglebone systemd[1]: Started OpenBSD Secure Shell server.
Aug 03 14:28:33 beaglebone sshd[3099]: Accepted password for debian from 192.168.1.104 port 51835 ssh2
Aug 03 14:28:33 beaglebone sshd[3099]: pam_unix(sshd:session): session opened for user debian by (uid=0)
```

### show [PATTERN…|JOB…]
使用 status 命令可以查看对应服务的基本状态，但是如果需要进行 debug 时，status 给出的信息是不够的，这时候就需要用到 show 命令，show 显示指定单元或任务的所有属性。单元用其名称表示，而任务则用其id表示。 如果没有指定任何单元或任务，那么显示管理器(systemd)自身的属性。

该命令显示的许多属性都直接对应着系统、服务管理器、单元的相应配置。 注意，该命令显示的属性，一般都是比原始配置更加底层与通用的版本， 此外，还包含了运行时的状态信息。 例如，对于服务单元来说，所显示的属性中就包含了运行时才有的主进程的标识符("MainPID")信息； 同时时间属性的值总是以 "…USec" 作为时间单位(即使在单元文件中是以 "…Sec" 作为单位)， 因为对于系统与服务管理器来说， 毫秒是更加通用的时间单位。  


### list-units [PATTERN…]
在 PATTERN 参数为空时，list-units 列出 systemd 当前已加载到内存中的单元，也就是：活动的单元、失败的单元、正处于任务队列中的单元。  

如果指定了 PATTERN…，则会列出符合 PATTERN 的单元，比如：systemctl list-units *.service 会列出所有已加载的 service 文件。  

使用 list-units 可以非常方便地查看系统中各个服务的状态，对整个系统状态有一个整体的把握。  

### cat PATTERN…
显示指定单元的配置文件,在显示每个单元文件的内容之前,会额外显示一行单元文件的绝对路径。注意，该命令显示的是当前文件系统上的单元文件的内容。 如果在更改了单元文件的内容之后， 没有使用 daemon-reload 命令， 那么该命令所显示的内容 有可能与已经加载到内存中的单元文件的内容不一致。  

同时,对于 service 文件,支持使用同名的 .d 目录来保存附属的配置,目录中的附属配置也会被显示.比如对于 foo.service,存在 foo.service.d/foo.conf,foo.conf 也会被显示出来.  

这个指令可以非常方便地查看单元文件对应的配置,尤其是在目标单元存在多个配置文件的情况下.  

### show-environment 
显示所有 systemd 环境变量及其值。这些环境变量会被传递给所有由 systemd 派生的进程。显示格式遵守shell脚本语法，可以直接用于shell脚本中。如果环境变量的值中不包含任何特殊字符以及空白字符，那么将直接按照 "VARIABLE=value" 格式显示，并且不使用任何转义序列。如果环境变量的值中包含任何特殊字符或及空白字符，那么将按照 "VARIABLE=$'value'" 格式(以美元符号开头的单引号转义序列)显示.  

### daemon-reload
重新加载 systemd 守护进程的配置,在修改了任何 service 文件之后都需要手动地执行 systemctr daemon-reload 命令重新加载配置,除了加载配置文件之外,还会重新运行所有的生成器(生成器用于动态地生成单元文件,环境变量等),重新加载所有的单元文件,重建整个依赖关系树.
在重新加载过程中， 所有由 systemd 代为监听的用户套接字 都始终保持可访问状态。  


## 日志操作









  





