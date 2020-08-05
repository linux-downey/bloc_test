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
几乎所有开发人员对日志都有一种复杂的情感，一方面日志为我们提供了非常详细的系统信息，以便我们查看以及分析日志对象的详细信息，另一方面，查看日志往往意味着出现了异常情况，需要去 debug，因为如果一切正常，往往通过其它简单的手段就可以获知到对象状态。  

正因为开发人员的日常工作就是 写bug、测bug、调bug 这几种，而日志系统又是 bug 的最大克星，尽管分析日志通常是一件非常枯燥的事，却往往是解决 bug 最快最有效的方式。所以，一个完善的日志系统对一个系统来说是非常必要的。  

systemd 对应的日志系统 journald，它对应一个独立的守护进程：systemd-journald，通常被放在 /lib/systemd/ 目录下，同样是作为 systemd 的一个服务被启动，所有 systemd 产生的日志都由 journald 进行记录。  

实际上，systemd-journald 记录的不仅仅只有 systemd 产生的日志，它主要记录以下日志：
* 通过 kmsg 收集内核日志
* 通过 glibc 的 syslog 接口导出的系统日志，也就是原 syslog 系统。 
* 通过 本地日志接口 sd_journal_print 收集结构化的系统日志
* 捕获服务单元的标准输出(STDOUT)与标准错误(STDERR)，比如：在程序中使用 printf 或者脚本中 echo 到终端的数据。
* 通过内核审计子系统收集审计记录

从这里可以再次看到 systemd 的野心，不仅仅是应用空间，连内核空间以及审计系统产生的日志都提供管理，当然，对于内核的日志，systemd 也只是通过内核日志接口收集再进行简单处理以方便用户检索，毕竟 systemd 只是一个应用空间的程序。   

journald 收集到的大部分日志数据都是文本数据，但同样也可以收集二进制数据。 组成日志记录的各个字段大小上限为16ZB(16*1024*1024TB)，日志服务要么将日志持久存储在 /var/log/journal 目录中、 要么将日志临时存储在 /run/log/journal/ 目录中(关机即丢失)。默认情况下， 如果在启动期间存在 /var/log/journal/ 目录，那么日志将会被持久存储在此目录中， 否则将会临时存储在 /run/log/journal/ 目录中。  

存储位置可以通过 journald 对应的配置文件 Storage= 选项进行设置，配置文件通常为 /etc/systemd/journald.conf，同样也可以使用同名的 .d 目录来保存。   

和 linux 中其它软件的运行模式一样，一个服务通常分成服务端和客户端，journald 作为服务端负责收集各种日志并格式化，而用户通过 journalctl 命令作为客户端来访问这些日志。  


## journalctl
正如上文中所说的，journalctl 可用于检索 systemd 的日志，如果不带任何参数直接调用此命令， 那么将显示所有日志内容，即从最早一条日志记录开始，因为日志的记录通常是持久化的，所以不带参数的 journalctl 命令看到的往往是多天前的日志。

默认情况下，结果会通过 less 工具进行分页输出， 并且超长行会在屏幕边缘被截断。 不过，被截掉的部分可以通过左右箭头按键查看。 如果不想分页输出，那么可以使用 --no-pager 选项。  

如果是输出到 tty 的话， 行的颜色还会根据日志的级别变化： ERROR 或更高级别为红色，NOTICE 或更高级别为高亮， DEBUG 级别为淡灰，其他级别则正常显示。

journalctl 通过选项对日志进行过滤，支持多种选项以灵活的方式对日志进行检索，下列是 journalctl 的常用选项：

### -f, --follow，-r，--reverse
相对于默认情况下从头显示日志，-f 选项表示只显示最新的日志项，并且阻塞当前进程，不断显示新生成的日志项。  

-r，--reverse 同样是反序显示日志，只是 -r 并不阻塞。  

### -o, --output=
-o 选项用于控制日志的输出格式。可以使用如下选项：

### -b [ID][±offset], --boot=[ID][±offset]
显示特定于某次启动的日志，可以通过 offset 指定是哪一次启动。  

如果未指定 ID 与 ±offset 参数， 则表示仅显示本次启动的日志。

如果省略了 ID ， 那么当 ±offset 是正数的时候， 将从日志头开始正向查找， 否则(也就是为负数或零)将从日志尾开始反响查找。 举例来说， "-b 1"表示按时间顺序排列最早的那次启动， "-b 2"则表示在时间上第二早的那次启动； "-b -0"表示最后一次启动， "-b -1"表示在时间上第二近的那次启动， 以此类推。 如果 ±offset 也省略了， 那么相当于"-b -0"， 除非本次启动不是最后一次启动(例如用 --directory 指定了 另外一台主机上的日志目录)。  

如果指定了32字符的 ID ， 那么表示以此 ID 所代表的那次启动为基准 计算偏移量(±offset)， 计算方法同上。 换句话说， 省略 ID 表示以本次启动为基准 计算偏移量(±offset)。

这个选项对于嵌入式工程师非常有用。  

### -u, --unit=UNIT|PATTERN
仅显示 属于特定单元的日志。 也就是单元名称正好等于 UNIT 或者符合 PATTERN 模式的单元。 这相当于添加了一个 "_SYSTEMD_UNIT=UNIT" 匹配项(对于 UNIT 来说)， 或一组匹配项(对于 PATTERN 来说)。

可以多次使用此选项以添加多个并列的匹配条件(相当于用"OR"逻辑连接)。

### -p, --priority=
根据 日志等级(包括等级范围)过滤输出结果。 日志等级数字与其名称之间的 对应关系如下 ： 
"emerg" (0), 
"alert" (1),
"crit" (2), 
"err" (3), 
"warning" (4), 
"notice" (5), 
"info" (6), 
"debug" (7)
若设为一个单独的数字或日志等级名称， 则表示仅显示小于或等于此等级的日志(也就是重要程度等于或高于此等级的日志)。 若使用 FROM..TO.. 设置一个范围， 则表示仅显示指定的等级范围内(含两端)的日志。 此选项相当于添加了 "PRIORITY=" 匹配条件。

### -S, --since=, -U, --until=
显示晚于指定时间(--since=)的日志、显示早于指定时间(--until=)的日志。   

参数的格式类似 "2012-10-30 18:17:16" 这样。 如果省略了"时:分:秒"部分，则相当于设为 "00:00:00" 。 如果仅省略了"秒"的部分则相当于设为 ":00" 。 如果省略了"年-月-日"部分，则相当于设为当前日期。 除了"年-月-日 时:分:秒"格式，参数还可以进行如下设置： 
(1)设为 "yesterday", "today", "tomorrow" 以表示那一天的零点(00:00:00)。 
(2)设为 "now" 以表示当前时间。 
(3)可以在"年-月-日 时:分:秒"前加上 "-"(前移) 或 "+"(后移) 前缀以表示相对于当前时间的偏移。  

### --sync
要求日志守护进程 将所有未写入磁盘的日志数据刷写到磁盘上， 并且一直阻塞到刷写操作实际完成之后才返回。 因此该命令可以保证当它返回的时候， 所有在调用此命令的时间点之前的日志， 已经全部安全的刷写到了磁盘中。以此防止意外断电导致的日志丢失。  


## 系统分析
尽管日志记录着系统中所有的运行信息,系统管理员通常也不会通过日志来掌握整个系统的信息,因为日志记录太过于琐碎和庞杂,通常情况下日志只是针对某个或者某部分特定的服务.  

如果需要宏观上地针对整个系统进行调试和分析,需要用到 systemd 提供的另一个程序:systemd-analyze,这个命令是基于 systemd 守护进程的一个客户端程序,负责收集并展示系统的性能统计数据,获取 systemd 系统管理器的状态和跟踪信息等.此外，还可以用于调试 systemd 系统管理器。    

systemd-analyze 针对系统管理和优化,最常用到以下功能.  

### systemd-analyze time
显示启动/运行时间相关的信息,包含以下几个部分:
* 在启动第一个用户态进程(init)之前，内核运行了多长时间；
* 在切换进入实际的根文件系统之前，initrd(initial RAM disk)运行了多长时间；
* 进入实际的根文件系统之后，用户空间启动完成花了多长时间。

在针对系统启动做优化的时候,了解各部分运行时间是非常有必要的,这有利于我们非常直观地看到哪些部分拖慢系统的启动进程,从而针对性地进行排查.  

注意， 上述时间只是简单的计算了系统启动过程中到达不同标记点的时间， 并没有计入各单元实际启动完成所花费的时间以及磁盘空闲的时间。  


### systemd-analyze blame
统计系统启动是每个用户服务所花费的时间,并按照每个单元花费的启动时间从多到少的顺序，列出所有当前正处于活动(active)状态的单元。 这些信息有助于用户优化系统启动速度。 不过需要注意的是，这些信息也可能具有误导性， 因为花费较长时间启动的单元，有可能只是在等待另一个依赖单元完成启动。 注意， systemd-analyze blame Type=simple 服务的结果， 因为 systemd 认为这种服务是立即启动的， 所以无法测量初始化延迟。  

在优化启动的时候经常要用到该指令对用户空间的服务进行分析.  

systemd 只是一个用户空间程序,它并没有权限直接参与内核的启动,所以也就无法统计内核各组件的启动时间,如果需要进行内核组件的启动优化,只能用内核中的统计工具.  

### systemd-analyze dump
按照人类易读的格式输出全部单元的状态,这些状态包括单元的配置信息(除了用户的配置,还有大量的默认配置),运行信息等,所有服务的信息通常有几千甚至数万行,这也是所有 "dump文件" 的共性,就是一股脑地将所有细节全部输出.  

注意该指令和日志的区别,日志中保存的是本该输出到终端或者指定输出到日志系统中的数据,而 systemd-analyze dump 则是输出所有服务的状态信息.   

### systemd-analyze verify
校验指定的单元文件以及被指定的单元文件引用的其他单元文件的正确性，并显示发现的错误。 

指定的单元文件可以是单元文件的精确路径(带有上级目录)，也可以仅仅是单元文件的名称(没有目录前缀)。  

对于那些仅给出了文件名(没有目录前缀)的单元， 将会优先在其他已经给出精确路径的单元文件的所在目录中搜索， 如果没有找到，将会继续在常规的单元目录中搜索。  

可以使用 $SYSTEMD_UNIT_PATH 环境变量来更改默认的单元搜索目录。






参考地址:
http://www.jinbuguo.com/systemd/journalctl.html
http://www.jinbuguo.com/systemd/systemd-analyze.html#


