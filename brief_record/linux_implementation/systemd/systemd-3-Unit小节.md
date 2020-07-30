# systemd-2-Unit/Install section
在上一章中，介绍了单元文件的特性，在这一章中，我们来看一下单元文件的通用配置，单元文件的通用配置在配置文件中隶属于 [Unit] 小节。  

## Unit section

### Description=
用户易读的描述字符串，该字符串仅用作用户识别，没有其他特定的功能，主要被用在 systemctl status *.service 的显示信息或者日志中，所以命名方式需要可以描述服务相关信息，但是又不需要过分说明。   

### Documentation=
一组用空格分隔的文档URI列表， 这些文档是对此单元的详细说明。 可接受 "http://", "https://", "file:", "info:", "man:" 五种URI类型。这些URI应该按照相关性由高到低列出。 比如，将解释该单元作用的文档放在第一个， 最好再紧跟单元的配置说明，最后再附上其他文档。可以多次使用此选项，依次向文档列表尾部添加新文档。但是，如果为此选项设置一个空字符串，那么表示清空先前已存在的列表。  

### OnFailure=
当当前单元进入失败(failed)状态时，将会启动列表中列出的服务项，服务启动返回错误或者 restart 超过次数上限时，将会进入到 failed 状态。 

因为 OnFailure 可设置一个列表，OnFailureJobMode 配置项可以配置需要执行的服务以什么方式排队执行。  

### PropagatesReloadTo=, ReloadPropagatedFrom=
PropagatesReloadTo= 表示 在 reload 该单元时， 也同时 reload 所有列表中的单元。 ReloadPropagatedFrom= 表示 在 reload 列表中的某个单元时， 也同时 reload 该单元。

### RequiresMountsFor=
接受一个空格分隔的绝对路径列表，表示该单元将会使用到这些文件系统路径。 所有这些路径中涉及的挂载点所对应的 mount 单元，都会被隐式的添加到 Requires= 与 After= 选项中。也就是说，这些路径中所涉及的挂载点都会在启动该单元之前被自动挂载。

因为 systemd 是完全的并行启动机制，所以很可能在服务启动时对应的文件系统还没有挂载，尤其是在 /home 目录挂载到一个独立的文件系统情况下，建议使用该配置项

### IgnoreOnIsolate=
当执行 systemctl isolate *.service 时，设置 IgnoreOnIsolate=no 的 且 AllowIsolate=yes 的服务都会被停止(默认为no)，isolate 指令类似于传统的切换SysV运行级的概念，几乎是将系统切换到另一个完整的运行状态，该命令会立即停止所有在新目标单元中不需要的进程， 这其中可能包括 当前正在运行的图形环境以及正在使用的终端。  

### AllowIsolate=
如果设为 yes ， 那么此单元将允许被 systemctl isolate 命令操作， 否则将会被拒绝。 唯一一个将此选项设为 yes 的理由，大概是为了兼容SysV初始化系统。 此时应该仅考虑对 target 单元进行设置， 以防止系统进入不可用状态。 建议保持默认值 no


### StopWhenUnneeded=
如果设为 yes，那么当此单元不再被任何已启动的单元依赖时，将会被自动停止。默认值 no 的含义是，除非此单元与其他即将启动的单元冲突，否则即使此单元已不再被任何已启动的单元依赖，也不会自动停止它。  

### RefuseManualStart=, RefuseManualStop=
如果设为 yes，那么此单元将拒绝被手动启动(RefuseManualStart=) 或拒绝被手动停止(RefuseManualStop=)。也就是说，此单元只能作为其他单元的依赖条件而存在，只能因为依赖关系而被间接启动或者间接停止， 不能由用户以手动方式直接启动或者直接停止。 设置此选项是为了禁止用户意外的启动或者停止某些特定的单元。默认值是 no

### DefaultDependencies=
是否保持默认依赖，默认为 yes，no 表示不使用默认依赖。每种不用的单元文件都有不同的默认依赖。  

### CollectMode=
垃圾回收策略的设置，可设为 inactive(默认值) 或 inactive-or-failed 之一。默认值 inactive 表示如果该单元处于停止(inactive)状态，并且没有被其他客户端、任务、单元所引用，那么该单元将会被卸载。 注意，如果此单元处于失败(failed)状态，那么是不会被卸载的， 它会一直保持未卸载状态，直到用户调用 systemctl reset-failed (或类似的命令)重置了 failed 状态。设为 inactive-or-failed 表示无论此单元处于停止(inactive)状态还是失败(failed)状态， 都将被卸载(无需重置 failed 状态)。注意，在这种"垃圾回收"策略下， 此单元的所有结果(退出码、退出信号、资源消耗 …) 都会在此单元结束之后立即清除(只剩下此前已经记录在日志中的痕迹)。

### FailureAction=, SuccessAction=
当此单元停止并进入失败(failed)或停止(inactive)状态时，应当执行什么动作。 对于系统单元，可以设为 none, reboot, reboot-force, reboot-immediate, poweroff, poweroff-force, poweroff-immediate, exit, exit-force 之一。 对于用户单元，仅可设为 none, exit, exit-force 之一。两个选项的默认值都是 none

### JobTimeoutSec=, JobRunningTimeoutSec=
当该单元的一个任务(job)进入队列的时候， JobTimeoutSec= 用于设置从该任务进入队列开始计时、到该任务最终完成，最多可以使用多长时间，如果上述任意一个设置超时，那么超时的任务将被撤销，并且该单元将保持其现有状态不变(而不是进入 "failed" 状态)。 对于非 device 单元来说，DefaultTimeoutStartSec= 选项的默认值是 "infinity"(永不超时)， 而 JobRunningTimeoutSec= 的默认值等于 DefaultTimeoutStartSec= 的值。

### JobTimeoutAction=, JobTimeoutRebootArgument=
JobTimeoutAction= 用于指定当超时发生时(参见上文的 JobTimeoutSec= 与 JobRunningTimeoutSec= 选项)，将触发什么样的额外动作。

### ConditionArchitecture=, ConditionVirtualization=, ConditionHost=, ConditionKernelCommandLine=, ConditionKernelVersion=, ConditionSecurity=, ConditionCapability=, ConditionACPower=, ConditionNeedsUpdate=, ConditionFirstBoot=, ConditionPathExists=, ConditionPathExistsGlob=, ConditionPathIsDirectory=, ConditionPathIsSymbolicLink=, ConditionPathIsMountPoint=, ConditionPathIsReadWrite=, ConditionDirectoryNotEmpty=, ConditionFileNotEmpty=, ConditionFileIsExecutable=, ConditionUser=, ConditionGroup=, ConditionControlGroupController=
这组选项用于在启动单元之前，首先测试特定的条件是否为真。 若为假，则悄无声息地跳过此单元的启动(仅是跳过，而不是进入"failed"状态)。 注意，即使某单元由于测试条件为假而被跳过， 那些由于依赖关系而必须先于此单元启动的单元并不会受到影响(也就是会照常启动)。 可以使用条件表达式来跳过那些对于本机系统无用的单元， 比如那些对于本机内核或运行环境没有用处的功能。 如果想要单元在测试条件为假时， 除了跳过启动之外，还要在日志中留下痕迹(而不是悄无声息的跳过)， 可以使用对应的另一组 AssertArchitecture=, AssertVirtualization=, … 选项。  

ConditionArchitecture= 检测是否运行于 特定的硬件平台： x86, x86-64, ppc, ppc-le, ppc64, ppc64-le, ia64, parisc, parisc64, s390, s390x, sparc, sparc64, mips, mips-le, mips64, mips64-le, alpha, arm, arm-be, arm64, arm64-be, sh, sh64, m68k, tilegx, cris, arc, arc-be, native(编译 systemd 时的目标平台)。 可以在这些关键字前面加上感叹号(!)前缀 表示逻辑反转。注意， Personality= 的值 对此选项 没有任何影响。

ConditionVirtualization= 检测是否运行于(特定的)虚拟环境中： yes(某种虚拟环境), no(物理机), vm(某种虚拟机), container(某种容器), qemu, kvm, zvm, vmware, microsoft, oracle, xen, bochs, uml, bhyve, qnx, openvz, lxc, lxc-libvirt, systemd-nspawn, docker, rkt, private-users(用户名字空间)。参见 systemd-detect-virt(1) 手册以了解所有已知的虚拟化技术及其标识符。 如果嵌套在多个虚拟化环境内， 那么以最内层的那个为准。 可以在这些关键字前面加上感叹号(!)前缀表示逻辑反转。

ConditionHost= 检测系统的 hostname 或者 "machine ID" 。 参数可以是：(1)一个主机名字符串(可以使用 shell 风格的通配符)， 该字符串将会与 本机的主机名(也就是 gethostname(2) 的返回值)进行匹配；(2)或者是一个 "machine ID" 格式的字符串(参见 machine-id(5) 手册)。 可以在字符串前面加上感叹号(!)前缀表示逻辑反转。

ConditionKernelCommandLine= 检测是否设置了某个特定的内核引导选项。 参数可以是一个单独的单词，也可以是一个 "var=val" 格式的赋值字符串。 如果参数是一个单独的单词，那么以下两种情况都算是检测成功： (1)恰好存在一个完全匹配的单词选项； (2)在某个 "var=val" 格式的内核引导选项中等号前的 "var" 恰好与该单词完全匹配。 如果参数是一个 "var=val" 格式的赋值字符串， 那么必须恰好存在一个完全匹配的 "var=val" 格式的内核引导选项，才算检测成功。 可以在字符串前面加上感叹号(!)前缀表示逻辑反转。

ConditionKernelVersion= 检测内核版本(uname -r) 是否匹配给定的表达式(或在字符串前面加上感叹号(!)前缀表示"不匹配")。 表达式必须是一个单独的字符串。如果表达式以 "<", "<=", "=", ">=", ">" 之一开头， 那么表示对内核版本号进行比较，否则表示按照 shell 风格的通配符表达式进行匹配。

注意，企图根据内核版本来判断内核支持哪些特性是不可靠的。 因为发行版厂商经常将高版本内核的新驱动和新功能移植到当前发行版使用的低版本内核中， 所以，对内核版本的检查是不能在不同发行版之间随意移植的， 不应该用于需要跨发行版部署的单元。

ConditionSecurity= 检测是否启用了 特定的安全技术： selinux, apparmor, tomoyo, ima, smack, audit, uefi-secureboot 。 可以在这些关键字前面加上感叹号(!)前缀表示逻辑反转。

ConditionCapability= 检测 systemd 的 capability 集合中 是否存在 特定的 capabilities(7) 。 参数应设为例如 "CAP_MKNOD" 这样的 capability 名称。 注意，此选项不是检测特定的 capability 是否实际可用，而是仅检测特定的 capability 在绑定集合中是否存在。 可以在名称前面加上感叹号(!)前缀表示逻辑反转。

ConditionACPower= 检测系统是否 正在使用交流电源。 yes 表示至少在使用一个交流电源， 或者更本不存在任何交流电源。 no 表示存在交流电源， 但是 没有使用其中的任何一个。

ConditionNeedsUpdate= 可设为 /var 或 /etc 之一， 用于检测指定的目录是否需要更新。 设为 /var 表示 检测 /usr 目录的最后更新时间(mtime) 是否比 /var/.updated 文件的最后更新时间(mtime)更晚。 设为 /etc 表示 检测 /usr 目录的最后更新时间(mtime) 是否比 /etc/.updated 文件的最后更新时间(mtime)更晚。 可以在值前面加上感叹号(!)前缀表示逻辑反转。 当更新了 /usr 中的资源之后，可以通过使用此选项， 实现在下一次启动时更新 /etc 或 /var 目录的目的。 使用此选项的单元必须设置 ConditionFirstBoot=systemd-update-done.service ， 以确保在 .updated 文件被更新之前启动完毕。 参见 systemd-update-done.service(8) 手册。

ConditionFirstBoot= 可设为 yes 或 no 。 用于检测 /etc 目录是否处于未初始化的原始状态(重点是 /etc/machine-id 文件是否存在)。 此选项可用于系统出厂后(或者恢复出厂设置之后)， 首次开机时执行必要的初始化操作。

ConditionPathExists= 检测 指定的路径 是否存在， 必须使用绝对路径。 可以在路径前面 加上感叹号(!)前缀 表示逻辑反转。

ConditionPathExistsGlob= 与 ConditionPathExists= 类似， 唯一的不同是支持 shell 通配符。

ConditionPathIsDirectory= 检测指定的路径是否存在并且是一个目录，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀表示逻辑反转。

ConditionPathIsSymbolicLink= 检测指定的路径是否存在并且是一个软连接，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀 表示逻辑反转。

ConditionPathIsMountPoint= 检测指定的路径是否存在并且是一个挂载点，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀表示逻辑反转。

ConditionPathIsReadWrite= 检测指定的路径是否存在并且可读写(rw)，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀 表示逻辑反转。

ConditionDirectoryNotEmpty= 检测指定的路径是否存在并且是一个非空的目录，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀 表示逻辑反转。

ConditionFileNotEmpty= 检测指定的路径是否存在并且是一个非空的普通文件，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀 表示逻辑反转。

ConditionFileIsExecutable= 检测指定的路径是否存在并且是一个可执行文件，必须使用绝对路径。 可以在路径前面加上感叹号(!)前缀 表示逻辑反转。

ConditionUser= 检测 systemd 是否以给定的用户身份运行。 参数可以是数字形式的 "UID" 、 或者字符串形式的UNIX用户名、 或者特殊值 "@system"(表示属于系统用户范围内) 。 此选项对于系统服务无效， 因为管理系统服务的 systemd 进程 总是以 root 用户身份运行。

ConditionGroup= 检测 systemd 是否以给定的用户组身份运行。 参数可以是数字形式的 "GID" 或者字符串形式的UNIX组名。 注意：(1)这里所说的"组"可以是"主组"(Primary Group)、"有效组"(Effective Group)、"辅助组"(Auxiliary Group)； (2)此选项不存在特殊值 "@system"

ConditionControlGroupController= 检测给定的一组 cgroup 控制器(例如 cpu)是否全部可用。 通过例如 cgroup_disable=controller 这样的内核命令行可以禁用名为"controller"的 cgroup 控制器。 列表中的多个 cgroup 控制器之间可以使用空格分隔。 不能识别的 cgroup 控制器将被忽略。 能够识别的全部 cgroup 控制器如下： cpu, cpuacct, io, blkio, memory, devices, pids。  

### AssertArchitecture=, AssertVirtualization=, AssertHost=, AssertKernelCommandLine=, AssertKernelVersion=, AssertSecurity=, AssertCapability=, AssertACPower=, AssertNeedsUpdate=, AssertFirstBoot=, AssertPathExists=, AssertPathExistsGlob=, AssertPathIsDirectory=, AssertPathIsSymbolicLink=, AssertPathIsMountPoint=, AssertPathIsReadWrite=, AssertDirectoryNotEmpty=, AssertFileNotEmpty=, AssertFileIsExecutable=, AssertUser=, AssertGroup=, AssertControlGroupController=
与前一组 ConditionArchitecture=, ConditionVirtualization=, … 测试选项类似，这一组选项用于在单元启动之前， 首先进行相应的断言检查。不同之处在于，任意一个断言的失败， 都会导致跳过该单元的启动并将失败的断言突出记录在日志中。 注意，断言失败并不导致该单元进入失败("failed")状态(实际上，该单元的状态不会发生任何变化)， 它仅影响该单元的启动任务队列。 如果用户希望清晰的看到某些单元因为未能满足特定的条件而导致没有正常启动， 那么可以使用断言表达式。

注意，无论是前一组条件表达式、还是这一组断言表达式，都不会改变单元的状态。 因为这两组表达式都是在启动任务队列开始执行时进行检查(也就是，位于依赖任务队列之后、该单元自身启动任务队列之前)， 所以，条件表达式和断言表达式 都不适合用于对单元的依赖条件进行检查。


### Requires=
### Requisite=
### Wants
### BindsTo=
### PartOf=
### Conflicts=
### Before=, After=
处理单元文件依赖相关的配置选项，参考[上一章](TODO)中的介绍。  






