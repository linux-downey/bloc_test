trap命令：

在linux的应用编程中，经常使用到信号机制，通常在终端结束一个进程时会在键盘上按下 CTRL+C ，这个指令操作就是给进程发送了一个 SIGINT 信号，这个信号的默认行为就是终止进程。  

很多信号都会有一些默认的行为,比如
SIGHUP为终止进程,
SIGINT同为终止进程，大多数情况下由键盘CTRL+C 发出
SIGSTOP 停止进程，由键盘CTRL+Z发出

大多数信号都是可以被忽略或者捕捉(signal函数捕捉信号)，有两个信号是不能被操作的，就是SIGSTOP和SIGKILL

在bash脚本里面，也可以处理各种信号，就是用trap命令：

    trap [-lp] [arg] signals

-p表示打印当前的trap设置，p表示print
-l表示打印所有的信号(信号的值对应信号宏名)

arg表示对信号的处理方式，如果arg为"",则表示忽略指定的信号
signals则表示指定要处理的信号值


示例：
    trap "echo test" SIGINT
在接收到信号时，并不会忽略原来SIGINT的动作，而是在其基础上增加一个上述指定的动作，也就是打印test

忽略信号：
    trap ""  SIGINT
这样就表示忽略一个信号，在接收到SIGINT信号时不做任何动作

恢复：
没有特定的恢复手段，只能说是重新设置
    trap  SIGINT
在没有arg字段时，就恢复了指定信号的默认行为


除了信号之外，还能对脚本处理退出（EXIT）、调试（DEBUG）、错误（ERR）、返回（RETURN）这几种状态，在写脚本的时候是非常有用的。















