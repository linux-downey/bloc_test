# systemd - service 文件
service 文件以 .service 结尾，这一类文件封装了一个被 systemd 监视与控制的进程，也就是负责管理服务对应具体程序的相关配置，比如启动、停止、重启该服务需要执行的指令，或者指定执行进程的运行方式，对于基本的系统管理行为而言，最核心的工作就是编写 service 文件。  

尽管对于各个系统服务的启动以及依赖关系的处理是非常复杂的，但是这项工作通常都由发行版厂商做好了，不需要我们从0设计，所以作为管理员来说，通常所面临的还是针对于自建的服务，包括设置启动策略、建立应用程序之间的简单依赖关系。   


## service 文件
针对所有的单元文件，都存在一些通用配置，这些配置由 [Unit] 小节和 [Install] 小节进行描述，Unit 小节通常配置服务的属性、依赖关系、执行条件检查、、垃圾回收策略等与执行环境相关的配置，而 Install 小节主要针对服务的 enable/disable 行为，通常是被用在开机自启动上，其本质是建立依赖关系从而完成自动启动，而这个自动启动也不一定是在系统启动阶段。   

service 文件中和服务具体执行行为相关的操作，就放在 [Service] 小节中，这也是本章重点介绍的内容。  

按照模块化的设计原则，也可以理解为：对于一个服务而言，与具体服务运行相关的配置，需要各个服务自行填写，被放在 [Service] 小节，而其它通用的配置分类放在其它小节，大部分都放在 [Unit] 小节，这本质上也是一种抽象，提取共性、分离差异。  


## 模板
抽象是无处不在的，service 提供的模板功能也是这一体现，对于某一类服务，比如登录服务、网络接口服务等，通常需要创建多个类似的服务进程(ssh1、ssh2，getty1、getty2等)，各个服务之间的差别可能仅仅是识别号或者是名称的区分，而其它的配置都是一样的，这种情况下就没有必要为每个可能产生的服务创建一个 service 文件，而是把它抽象出来，形成一个服务模板，通过传递参数的方式来区分不同的服务，以这种方式来简化配置。  

模板文件有特定的文件命名，后缀部分同样以 .service 结尾，而文件名主体部分需要以 @ 结尾，比如 getty@.service，ssh@.service。  

模板只是一个抽象化的通用描述，而不同作为实际的服务被使用，实际的工作需要交给模板的实例化，模板的实例化也是通过文件名来实现的，实例的命名就是在模板的基础上，在 @ 字符后添加对应实例 ID 或者是识别字段，比如 ssh@foo.service、getty@bar.service。  

模板的使用需要解决两个关键问题：
* 如何创建模板实例？
* 如何传递及使用参数？  

service 文件创建模板实例是非常简单的，方式看起来也有些奇怪，就是创建指向模板文件的软链接，这个软链接的命名符合当前实例命名即可，而这个实例命名就会被传递到模板文件中。    

在模板文件中，通常被抽象化的部分都是实例名，而实例名在 service 中以 %i 替代，因此，通过软链接提供实例名，再使用实例名将模板文件中的 %i 进行替代，也就完成了模板的实例化，下面是一个简单的示例：

1、创建模板文件：temp@.service

    ```
    [Unit]
    Description=%i
    [Service]
    ExecStart=/home/debian/systemd_test/%i.sh
    ```

2、将模板文件拷贝到 systemd 支持的系统目录，比如 /usr/lib/systemd/system/ 或者 /lib/systemd/system/ 。  
3、同在系统目录下创建软链接，ln -s temp@.service temp@foo.service,创建了一个对应的实例
4、实例生成的实例，对应模板文件中的 i% 参数为 foo，所以，在执行 systemctl start temp@foo.service 时，会执行对应的 /home/debian/systemd_test/foo.sh 脚本。  

除了 i% 参数，单元文件中还支持多种文本替换方式，替换符的列表可以参考[单元文件介绍](TODO)

## 服务状态
系统中的所有服务都会由 systemd 记录运行状态，用户运行 systemctl status foo.service 时可以查看进程状态，以 ssh 为例：

```
root@beaglebone:/home/debian# systemctl status ssh
● ssh.service - OpenBSD Secure Shell server
   Loaded: loaded (/lib/systemd/system/ssh.service; enabled; vendor preset: enabled)
   Active: active (running) since Sat 2020-08-01 10:21:02 UTC; 1h 10min ago
   Process: 1156 ExecStartPre=/usr/sbin/sshd -t (code=exited, status=0/SUCCESS)
   Main PID: 1200 (sshd)
    Tasks: 1 (limit: 4915)
   CGroup: /system.slice/ssh.service
           └─1200 /usr/sbin/sshd -D
    ...
    
```
这是我的 beaglebone 系统中的 ssh 服务状态，状态显示主要分成两个部分：
* Loaded：指示当前服务的加载状态，加载状态分为几种：
    * loaded ：正常加载
    * error ：加载错误
    * not-found ：找不到指定的加载文件
    * bad-setting：指定的文件存在致命的配置问题
    * masked：该服务被 mask 屏蔽了。 
* Active：执行当前服务的运行状态
    Active 项表示服务的运行状态，进程运行状态涉及到 systemd 中依赖关系的管理、内存回收等，运行状态分为几种：
    * active：表示该服务正处于活动状态，活动状态是 systemd 的一种逻辑概念，并不一定表示服务正在运行，当然大多数处于活动状态的服务都是正在运行，也可能即使进程已经退出，因为设置了 RemainAfterExit=yes，服务仍然处于活动状态。  
    * inactive：非活动状态
    * activating：正在激活该服务
    * deactivating：正在取消激活该服务
    * failed:服务执行错误，通常是因为进程运行出错、返回了错误的进程结束状态或者执行超时

针对不同的 Active 状态，systemd 还会在后面的括号中给出附属提示信息，比如上文中的  active(running) 表示服务正在运行，active (exited) 表示尽管进程退出，服务仍处于 active 状态，inactive (dead) 表示进程退出，处于 inactive 状态。   


## 隐含依赖和默认依赖
每种类型的单元文件都存在隐含依赖和默认依赖，service 文件对应的依赖如下：

### 隐含依赖
* 如果设置了 Type=dbus 的服务会自动添加 Requires=dbus.socket 与 After=dbus.socket 依赖。  
* 基于套接字启动的服务会自动添加对关联的 .socket 单元的 After= 依赖。 服务单元还会为所有在 Sockets= 中列出的 .socket 单元自动添加 Wants= 与 After= 依赖。

### 默认依赖
除非明确设置了 DefaultDependencies=no ，否则 service 单元将会自动添加下列依赖关系：
* Requires=sysinit.target, After=sysinit.target, After=basic.target, Conflicts=shutdown.target, Before=shutdown.target 。 这样可以确保普通的服务单元： (1)在基础系统启动完毕之后才开始启动，(2)在关闭系统之前先被干净的停止。 只有那些需要在系统启动的早期就必须启动的服务， 以及那些必须在关机动作的结尾才能停止的服务才需要设置 DefaultDependencies=no 。
* 从同一个模版实例化出来的所有服务单元(单元名称中带有 "@" 字符)， 默认全部属于与模版同名的同一个 slice 单元。该同名 slice 一般在系统关机时，与所有模版实例一起停止。 如果你不希望像上面这样，那么可以在模版单元中明确设置 DefaultDependencies=no。  

## service 文件中的 \[Service\] 小节
service 文件中的 \[Service\] 小节中描述 service 文件的核心内容，它包含但不限于以下内容：

### Type=
设置服务进程的类型，配置服务的启动类型，必须设为 simple, forking, oneshot, dbus, notify, idle 之一(在 232 版本的 systemd 上测得新版本已经不再支持 exec)：
* simple，顾名思义，这是最简单也是最通用的配置，当没有配置 Type= 和 BusName= 时，默认选项就是 simple，**此时 ExecStart 指定运行的进程就是主进程，并且 systemd 会认为在创建(注意：是创建而不是完成)了该服务的主服务进程之后，该服务就已经启动完成**，这就意味着对于 simple 类型的服务来说， 即使不能成功调用主服务进程(例如 User= 不存在、或者二进制可执行文件不存在)， systemctl start 也仍然会执行成功。

* 如果设为 forking ，那么表示 ExecStart= 进程将会在启动过程中使用 fork() 系统调用。 也就是当所有通信渠道都已建好、启动也已经成功之后(包括子进程已经运行完成)，父进程将会退出，而子进程将作为主服务进程继续运行。 这是传统UNIX守护进程的经典做法。 在这种情况下，systemd 会认为在父进程退出之后，该服务就已经启动完成。 如果使用了此种类型，那么建议同时设置 PIDFile= 选项，以帮助 systemd 准确可靠的定位该服务的主进程。 systemd 将会在父进程退出之后立即开始启动后继单元。  

* oneshot 与 simple 类似，不同之处在于， 只有在该服务的主服务进程退出之后，systemd 才会认为该服务启动完成，才会开始启动后继单元。 此种类型的服务通常会设置 RemainAfterExit= 选项，该设置保证在主进程退出之后服务依旧处于 active 状态而不是 dead 状态。 当 Type= 与 ExecStart= 都没有设置时， Type=oneshot 就是默认值。这种类型的进程适用于不需要长期执行的服务，执行一次之后就会退出。  

* dbus 与 simple 类似，不同之处在于， 该服务只有获得了 BusName= 指定的 D-Bus 名称之后，systemd 才会认为该服务启动完成，才会开始启动后继单元。 设为此类型相当于隐含的依赖于 dbus.socket 单元。 当设置了 BusName= 时， 此类型就是默认值，通常需要显式指定 dbus 依赖的服务需要使用这种启动方式。  

* notify 与 exec 类似，不同之处在于， 该服务将会在启动完成之后通过 sd_notify 之类的接口发送一个通知消息。systemd 将会在启动后继单元之前， 首先确保该进程已经成功的发送了这个消息。如果设为此类型，那么下文的 NotifyAccess= 将只能设为非 none 值。如果未设置 NotifyAccess= 选项、或者已经被明确设为 none ，那么将会被自动强制修改为 main 。注意，目前 Type=notify 尚不能与 PrivateNetwork=yes 一起使用。sd_notify 用于主动通知 systemd 系统发生的事件，比如启动完成。这种模式相当于自定义启动状态消息，而不是由系统判定，对于 simple 类型而言，执行 fork 就会导致服务进入 active，而对于 forking 和 oneshot，fork 和调用 execve 完成之后就进入 active 状态。    

* idle 与 simple 类似，不同之处在于， 服务进程将会被延迟到所有活动任务都完成之后再执行。 这样可以避免控制台上的状态信息与shell脚本的输出混杂在一起。 注意：(1)仅可用于改善控制台输出，切勿将其用于不同单元之间的排序工具； (2)延迟最多不超过5秒， 超时后将无条件的启动服务进程。

在所有的进程类型中，为什么要强调启动完成这个概念呢？因为各个服务之间的依赖关系需要依据服务的启动状态来执行后续的动作，比如执行状态为 failed，就会导致所有设置为 require 项的后续服务无法启动。同时，启动状态也会反映到使用 systemctl status foo.service 指令查看到对应的启动状态。    

建议对长时间持续运行的服务尽可能使用 Type=simple (这是最简单和速度最快的选择)。因为使用任何 simple 之外的类型都需要等待服务完成初始化，所以可能会减慢系统启动速度。因此，应该尽可能避免使用 simple 之外的类型(除非必须)。另外，也不建议对长时间持续运行的服务使用 idle 或 oneshot 类型(oneshot 适用于一次执行即退出的，而 idle 主要针对控制台输出)。 

注意，因为 simple 类型的服务 无法报告启动失败、也无法在服务完成初始化后对其他单元进行排序， 所以，当客户端需要通过仅由该服务本身创建的IPC通道(而非由 systemd 创建的套接字或 D-bus 之类)连接到该服务的时候， simple 类型并不是最佳选择。在这种情况下， notify 或 dbus(该服务必须提供 D-Bus 接口) 才是最佳选择， 因为这两种类型都允许服务进程精确的安排何时算是服务启动成功、何时可以继续启动后继单元。 notify 类型需要服务进程明确使用 sd_notify() 函数或类似的API，否则，可以使用 forking 作为替代(它支持传统的UNIX服务启动协议)。  

对于需要操作的服务而言，系统服务通常是二进制文件，这也是 systemd 建议的做法，将服务需要执行的程序以二进制的形式提供，但在实际开发过程中，很可能服务程序是以脚本的形式提供的，而脚本程序意味着服务程序中还存在多级子进程的调用，对于 simple 的形式而言，脚本执行的结果通常是无法预测的，因为脚本刚开始执行之初，systemd 认为该服务已经成功启动，接着开始启动后续的单元，而并不保证整个脚本会被执行完，同时，即使脚本中的命令执行报错，只要主进程没有退出，也不会直接反映在启动结果上，所以，在这种情况下，可以使用 forking，或者尽量地提供二进制程序。   


### RemainAfterExit=
当该服务的所有进程全部退出之后， 是否依然将此服务视为活动(active)状态。 默认值为 no。  

### GuessMainPID=
在无法明确定位 该服务主进程的情况下， systemd 是否应该猜测主进程的PID(可能不正确)。 该选项仅在设置了 Type=forking 但未设置 PIDFile= 的情况下有意义。 如果PID猜测错误， 那么该服务的失败检测与自动重启功能将失效。 默认值为 yes。  

**也就是说，如果使用 forking 类型的进程执行方式，如果不指定 PID file，可能导致 systemd 无法确定主进程的 PID 从而导致失败检测和重启功能失效。**

### PIDFile=
指定该服务的 PID 文件(一般位于 /run/ 目录下)，在设置 forking 类型的进程启动时推荐使用，以指定主进程的 PID，因为 forking 方式的进程启动在主进程启动之后，父进程会退出，系统可能会因为没有记录到正确的主进程 PID 从而导致一些控制问题，PID 文件就是一个纯文本的文件，里面放置指定进程的 PID 值。  

### BusName=
设置与此服务通信 所使用的 D-Bus 名称。 在 Type=dbus 的情况下 必须明确设置此选项。  

### ExecStart=
在启动该服务时需要执行的 命令行(命令+参数)。 

除非 Type=oneshot ，否则必须且只能设置一个命令行。 仅在 Type=oneshot 的情况下，才可以设置任意个命令行(包括零个)， 多个命令行既可以在同一个 ExecStart= 中设置，也可以通过设置多个 ExecStart= 来达到相同的效果。 如果设为一个空字符串，那么先前设置的所有命令行都将被清空。 如果不设置任何 ExecStart= 指令， 那么必须确保设置了 RemainAfterExit=yes 指令，并且至少设置一个 ExecStop= 指令。 同时缺少 ExecStart= 与 ExecStop= 的服务单元是非法的(也就是必须至少明确设置其中之一)。

如果没有指定 ExecStop，sysytemd 对服务默认的停止方式是发送停止信号。 

可以在执行命令之前添加前缀表示不同的含义：TODO

"@", "-" 以及 "+"/"!"/"!!" 之一，可以按任意顺序同时混合使用。 注意，对于 "+", "!", "!!" 前缀来说，仅能单独使用三者之一，不可混合使用多个。 注意，这些前缀同样也可以用于 ExecStartPre=, ExecStartPost=, ExecReload=, ExecStop=, ExecStopPost= 这些接受命令行的选项。

如果设置了多个命令行， 那么这些命令行将以其在单元文件中出现的顺序依次执行。 如果某个无 "-" 前缀的命令行执行失败， 那么剩余的命令行将不会被继续执行， 同时该单元将变为失败(failed)状态。

当未设置 Type=forking 时， 这里设置的命令行所启动的进程 将被视为该服务的主守护进程。

### ExecStartPre=, ExecStartPost=
设置在执行 ExecStart= 之前/后执行的命令行。 语法规则与 ExecStart= 完全相同。 如果设置了多个命令行， 那么这些命令行将以其在单元文件中出现的顺序 依次执行。

如果某个无 "-" 前缀的命令行执行失败， 那么剩余的命令行将不会被继续执行， 同时该单元将变为失败(failed)状态。

仅在所有无 "-" 前缀的 ExecStartPre= 命令全部执行成功的前提下， 才会继续执行 ExecStart= 命令。

ExecStartPost= 命令仅在 ExecStart= 中的命令已经全部执行成功之后才会运行， 判断的标准基于 Type= 选项。 具体说来，对于 Type=simple 或 Type=idle 就是主进程已经成功启动； 对于 Type=oneshot 来说就是最后一个 ExecStart= 进程已经成功退出； 对于 Type=forking 来说就是初始进程已经成功退出； 对于 Type=notify 来说就是已经发送了 "READY=1" ； 对于 Type=dbus 来说就是已经取得了 BusName= 中设置的总线名称。

所以，不可将 ExecStartPre= 用于 需要长时间执行的进程。 因为所有由 ExecStartPre= 派生的子进程 都会在启动 ExecStart= 服务进程之前被杀死。

### ExecReload=
这是一个可选的指令， 用于设置当该服务 被要求重新载入配置时 所执行的命令行。 语法规则与 ExecStart= 完全相同。  

### ExecStop=
这是一个可选的指令， 用于设置当该服务被要求停止时所执行的命令行。 语法规则与 ExecStart= 完全相同。 执行完此处设置的所有命令行之后，该服务将被视为已经停止， 此时，该服务所有剩余的进程将会根据 KillMode= 的设置被杀死。 如果未设置此选项，那么当此服务被停止时， 该服务的所有进程都将会根据 KillSignal= 的设置被立即全部杀死。 与 ExecReload= 一样， 也有一个特殊的环境变量 $MAINPID 可用于表示主进程的PID 。

一般来说，不应该仅仅设置一个结束服务的命令而不等待其完成。 因为当此处设置的命令执行完之后， 剩余的进程会被按照 KillMode= 与 KillSignal= 的设置立即杀死， 这可能会导致数据丢失。 因此，这里设置的命令必须是同步操作，而不能是异步操作。

注意，仅在服务确实启动成功的前提下，才会执行 ExecStop= 中设置的命令。 如果服务从未启动或启动失败(例如，任意一个 ExecStart=, ExecStartPre=, ExecStartPost= 中无 "-" 前缀的命令执行失败或超时)， 那么 ExecStop= 将会被跳过。 如果想要无条件的在服务停止后执行特定的动作，那么应该使用 ExecStopPost= 选项。 如果服务启动成功，那么即使主服务进程已经终止(无论是主动退出还是被杀死)，也会继续执行停止操作。 因此停止命令必须正确处理这种场景，如果 systemd 发现在调用停止命令时主服务进程已经终止，那么将会撤销 $MAINPID 变量。

重启服务的动作被实现为"先停止、再启动"。所以在重启期间，将会执行 ExecStop= 与 ExecStopPost= 命令。 推荐将此选项用于那些必须在服务干净退出之前执行的命令(例如还需要继续与主服务进程通信)。当此选项设置的命令被执行的时候，应该假定服务正处于完全正常的运行状态，可以正常的与其通信。 如果想要无条件的在服务停止后"清理尸体"，那么应该使用 ExecStopPost= 选项。 


### ExecStopPost=
这是一个可选的指令， 用于设置在该服务停止之后所执行的命令行。 语法规则与 ExecStart= 完全相同。 注意，与 ExecStop= 不同，无论服务是否启动成功， 此选项中设置的命令都会在服务停止后被无条件的执行。

应该将此选项用于设置那些无论服务是否启动成功， 都必须在服务停止后无条件执行的清理操作。 此选项设置的命令必须能够正确处理由于服务启动失败而造成的各种残缺不全以及数据不一致的场景。 由于此选项设置的命令在执行时，整个服务的所有进程都已经全部结束， 所以无法与服务进行任何通信。

### RestartSec=
设置在重启服务(Restart=)前暂停多长时间。 默认值是100毫秒(100ms)。 如果未指定时间单位，那么将视为以秒为单位。 例如设为"20"等价于设为"20s"。

### TimeoutStartSec=
设置该服务允许的最大启动时长。 如果守护进程未能在限定的时长内发出"启动完毕"的信号，那么该服务将被视为启动失败，并会被关闭。  

### TimeoutStopSec=
此选项有两个用途： 
(1)设置每个 ExecStop= 的超时时长。如果其中之一超时， 那么所有后继的 ExecStop= 都会被取消，并且该服务也会被 SIGTERM 信号强制关闭。 如果该服务没有设置 ExecStop= ，那么该服务将会立即被 SIGTERM 信号强制关闭。 

(2)设置该服务自身停止的超时时长。如果超时，那么该服务将会立即被 SIGTERM 信号强制关闭，如果未指定时间单位，那么将视为以秒为单位。 例如设为"20"等价于设为"20s"。 设为 "infinity" 则表示永不超时。 默认值等于 DefaultTimeoutStopSec= 的值。  

如果一个 Type=notify 服务发送了 "EXTEND_TIMEOUT_USEC=…" 信号， 那么允许的停止时长将会在 TimeoutStopSec= 基础上继续延长指定的时长。 注意，必须在 TimeoutStopSec= 用完之前发出第一个延时信号。当停止时间超出 TimeoutStopSec= 之后，该服务可以在维持始终不超时的前提下，不断重复发送 "EXTEND_TIMEOUT_USEC=…" 信号，直到完成停止。

### RuntimeMaxSec=
允许服务持续运行的最大时长。 如果服务持续运行超过了此处限制的时长，那么该服务将会被强制终止，同时将该服务变为失败(failed)状态。 注意，此选项对 Type=oneshot 类型的服务无效，因为它们会在启动完成后立即终止。 默认值为 "infinity" (不限时长)。  

### Restart=
当服务进程 正常退出、异常退出、被杀死、超时的时候， 是否重新启动该服务。 所谓"服务进程" 是指 ExecStartPre=, ExecStartPost=, ExecStop=, ExecStopPost=, ExecReload= 中设置的进程。 当进程是由于 systemd 的正常操作(例如 systemctl stop|restart)而被停止时， 该服务不会被重新启动。 所谓"超时"可以是看门狗的"keep-alive ping"超时， 也可以是 systemctl start|reload|stop 操作超时。

该选项的值可以取 no, on-success, on-failure, on-abnormal, on-watchdog, on-abort, always 之一。 no(默认值) 表示不会被重启。 always 表示会被无条件的重启。 on-success 表示仅在服务进程正常退出时重启， 所谓"正常退出"是指：退出码为"0"， 或者进程收到 SIGHUP, SIGINT, SIGTERM, SIGPIPE 信号之一， 并且 退出码符合 SuccessExitStatus= 的设置。 on-failure 表示 仅在服务进程异常退出时重启， 所谓"异常退出" 是指： 退出码不为"0"， 或者 进程被强制杀死(包括 "core dump"以及收到 SIGHUP, SIGINT, SIGTERM, SIGPIPE 之外的其他信号)， 或者进程由于 看门狗超时 或者 systemd 的操作超时 而被杀死。  

是否重启与退出状态对应可以参照下表：TODO

注意如下例外情况： 
(1) RestartPreventExitStatus= 中列出的退出码或信号永远不会导致该服务被重启。 
(2) 被 systemctl stop 命令或等价的操作停止的服务永远不会被重启。 
(3) RestartForceExitStatus= 中列出的退出码或信号将会 无条件的导致该服务被重启。

注意，服务的重启频率仍然会受到由 StartLimitIntervalSec= 与 StartLimitBurst= 定义的启动频率的制约。详见 systemd.unit 部分介绍。只有在达到启动频率限制之后， 重新启动的服务才会进入失败状态。

对于需要长期持续运行的守护进程， 推荐设为 on-failure 以增强可用性。 对于自身可以自主选择何时退出的服务， 推荐设为 on-abnormal。  

### SuccessExitStatus=
可以设为一系列以空格分隔的数字退出码或信号名称，当进程的退出码或收到的信号与此处的设置匹配时， 无论 Restart= 选项是如何设置的， 该服务都将无条件的禁止重新启动。 例如：

RestartPreventExitStatus=1 6 SIGABRT 

可以确保退出码 1, 6 与 SIGABRT 信号 不会导致该服务被自动重启。 默认值为空，表示完全遵守 Restart= 的设置。 如果多次使用此选项，那么最终的结果将是多个列表的合并。 如果将此选项设为空，那么先前设置的列表将被清空。

### RestartPreventExitStatus=
可以设为一系列 以空格分隔的数字退出码或信号名称，当进程的退出码或收到的信号与此处的设置匹配时，无论 Restart= 选项 是如何设置的，该服务都将无条件的 禁止重新启动。

### RestartForceExitStatus=
接受一个布尔值。设为 yes 表示根目录 RootDirectory= 选项仅对 ExecStart= 中的程序有效， 而对 ExecStartPre=, ExecStartPost=, ExecReload=, ExecStop=, ExecStopPost= 中的程序无效。 默认值 no 表示根目录对所有 Exec*= 系列选项中的程序都有效。  

### NonBlocking=
是否为所有基于套接字启动传递的文件描述符设置非阻塞标记(O_NONBLOCK)。 设为 yes 表示除了通过 FileDescriptorStoreMax= 引入的文件描述符之外， 所有 ≥3 的文件描述符(非 stdin, stdout, stderr 文件描述符)都将被设为非阻塞模式。 该选项仅在与 socket 单元 (systemd.socket(5)) 联用的时候才有意义。 对于那些先前已经通过 FileDescriptorStoreMax= 引入的文件描述符则毫无影响。 默认值为 no


## 执行命令格式
ExecStart、ExecStartPre 等配置项可以指定执行特定的程序或脚本，命令指定的方式类似于命令行，比如 ssh.service 文件中指定执行指令：
```
ExecStart=/usr/sbin/sshd -D $SSHD_OPTS
```
等号右边的命令和命令行非常相似，同时也有一些小的区别，下面我们来看看 ExecStart=, ExecStartPre=, ExecStartPost=, ExecReload=, ExecStop=, ExecStopPost= 选项的命令行解析规则。  

如果要一次设置多个命令，那么可以使用分号(;)将多个命令行连接起来。 注意，仅在设置了 Type=oneshot 的前提下，才可以一次设置多个命令。 命令中自带的分号必须用 "\;" 进行转义。

每个命令行的内部都以空格分隔， 第一项是要运行的命令， 随后的各项则是命令的参数。 每一项的边界都可以用单引号或双引号界定， 但引号自身最终将会被剥离。 还可以使用C语言风格的转义序列，但仅可使用下文表格中的转义序列。最后，行尾的反斜杠("\") 将被视作续行符(借鉴了bash续行语法)。

命令行的语法刻意借鉴了shell中的转义字符与变量展开语法， 但两者并不完全相同。 特别的，重定向("<", "<<", ">", ">>")、 管道("|")、 后台运行("&") 都不被支持。

要运行的命令(第一项)可以包含空格，但是不能包含控制字符。  

支持 "${FOO}" 与 "$FOO" 两种不同的环境变量替换方式。 具体说来就是： "${FOO}" 的内容将原封不动的转化为一个单独的命令行参数，无论其中是否包含空格与引号，也无论它是否为空。 "$FOO" 的内容将被原封不动的插入命令行中， 但对插入内容的解释却遵守一般的命令行解析规则。 后文的两个例子， 将能清晰的体现两者的差别。

如果要运行的命令(第一项)不是一个绝对路径， 那么将会在编译时设定的可执行文件搜索目录中查找。 因为默认包括 /usr/local/bin/, /usr/bin/, /bin/, /usr/local/sbin/, /usr/sbin/, /sbin/ 目录， 所以可以安全的直接使用"标准目录"中的可执行程序名称(没必要再使用绝对路径)， 而对于非标准目录中的可执行程序，则必须使用绝对路径。建议始终使用绝对路径以避免歧义。


## 小结
实际上 service 文件中并不仅仅包含上述选项，上文主要列出了 service 文件中与服务执行相关的参数，其他配置参数将在后续文章中继续介绍。  

