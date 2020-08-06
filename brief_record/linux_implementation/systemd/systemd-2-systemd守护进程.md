# systemd-2-systemd 守护进程
通过前面两章的铺垫,相信各位对 systemd 的基本概念已经有了一定的了解:systemd 是一个专用于 linux 操作系统的系统与服务管理器,在大部分支持 systemd 的机器上,它都是作为 PID 为 1 的 init 进程启动的,启动并维护各种用户空间的服务.  

作为验证,你可以通过 ps -ef | head 指令查看当前系统中的进程列表,在我的 ubuntu18 机器上,PID 为 1 的进程为 /sbin/init,而 /sbin/init 是指向 /lib/systemd/systemd 的一个软链接.   

因此,systemd 系统的守护进程就是 /lib/systemd/systemd,作为系统的 init 进程启动.因此 systmed 代表一个管理系统,同时也表示该管理系统对应的执行程序,所以为了避免混淆,systemd 依旧表示整个管理系统,而对应的守护进程统一使用 /lib/systemd/systemd 来表示.  


## systemd 的系统实例与用户实例
/lib/systemd/systemd 是一个静态的二进制程序被放在根文件系统中,当它被加载到内存中运行就对应一个进程,也可以说这个执行进程是对应 /lib/systemd/systemd 的一个执行实例,相对于其它服务进程来说,systemd 的一个特殊之处在于:它不仅仅只运行系统实例,同时在系统中还可能运行着多个用户实例.   

系统实例只有一个,用于管理整个系统的服务,同时以 root 权限启动,而用户实例是针对用户的,也就是每当有一个新的用户登录上系统时,系统会执行 
/lib/systemd/systemd --user 产生一个新的进程,这个新进程对应当前登录的新用户,这个新的用户实例对系统和其它用户来说都是完全独立的,也就是当用户使用 systemctl --user status foo.service 时,是与新产生的用户实例进行交互,而不是与系统实例进程进行交互.  

区分系统实例和用户实例需要注意以下几点:
* 只有当用户第一次登录到系统中时,才会产生新的实例,实际上是将 /lib/systemd/system/user@.service 模板实例化一个服务并启动.执行 ExecStart 命令:/lib/systemd/systemd --user.早期的版本中用户实例针对的是 session 而不是用户,只要存在登录系统的行为就会创建一个用户实例,现在更新为针对用户,从 systemd 226 版本之后改为针对用户.    
* 用户与系统,用户与用户之间是独立的,对系统实例使用 systemctl enable 时,是设置开机启动,而对用户实例使用同样的操作时,只有用户实例启动后才会按照 enable 的规则启动对应服务,这里有必要说说 systemctl enable 的原理,它实际是建立启动的依赖关系,只不过对于系统实例而言,enable 的操作将会设置为被一个开机启动的服务所依赖,所以所操作的服务也就会在开机的时候被启动.  
* 只要用户还有会话存在，这个进程就不会退出；用户所有会话退出时，进程将会被销毁。  
* 操作系统实例的修改操作通常需要 root 权限才能实现,而用户实例不需要,但是这并不意味着普通用户使用 systemctl status 操作的就是用户实例,实际上普通用户使用该指令操作的还是系统实例,看到的是系统状态,如果需要操作用户实例,需要指定 --user 选项.    
* 系统实例存在对应的系统目录,通常是 /lib/systemd/system 或者是 /usr/lib/systemd/system 等(位于system目录下),而所有用户实例的系统目录在 /lib/systemd/user 或者 /usr/lib/systemd/user 下.即使所有用户实例共用一份系统配置文件,因为运行的是不同的进程,因此也是可以做到独立的.例如:系统中存在两个用户 foo 和 bar,在系统开机时只会执行系统实例,当用户 foo 登录到系统时,会为 foo 启动一个系统实例,foo 用户开启了原本处于关闭状态的 test.service,此时登录 bar,在 bar 用户下查看 test.service 的状态时依旧处于关闭状态.  


使用用户实例主要是方便针对某个用户进行特定的配置,比如邮件服务,mpd 等等.  

### 随系统启动的 systemd 用户实例
参考:https://wiki.archlinux.org/index.php/Systemd_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87)/User_(%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87)#%E9%9A%8F%E7%B3%BB%E7%BB%9F%E8%87%AA%E5%8A%A8%E5%90%AF%E5%8A%A8_systemd_%E7%94%A8%E6%88%B7%E5%AE%9E%E4%BE%8B







