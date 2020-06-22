# arm-linux 系统调用流程

在 linux 中,软硬件是有明显的分层的,出于安全或者是资源统筹考虑,硬件资源由内核进行统一管理,内核拥有绝对的权限,而用户空间无法直接访问硬件.在实际的应用中,用户进程总是无法避免需要操作到硬件,这个硬件可能是磁盘文件,USB接口等,这时候就需要向内核递交申请,让内核帮忙做硬件相关的事情,这个过程就由系统调用完成.  

无论从硬件还是从软件角度来说,用户空间与内核空间有一道无法轻易逾越的屏障,如果是简单地一分为二,事情并不会有多复杂,不幸的是,这两者不能简单地完全隔断,用户空间的大部分操作都需要通过内核来完成,就连简单的申请内存操作,用户空间都无法独立自主地做到,因为这涉及到物理内存的分配,而物理内存也是硬件的一种,所以在这道屏障上需要开一扇门,来进行内核与用户之间的交互,这道门也就是系统调用.  

用户空间的东西不能进入到内核中,内核中的东西自然也不能暴露到用户空间中,所有请求与回复的资源都只能通过传递的方式,而这种信息传递还需要通过严格的检查.这就像在监狱中,服刑人员只能向监管人员提出申请,让他帮忙代办一些事情,而不能提出申请自己亲自出去办某件事,监狱内和外部世界是有严格划分的.  

也就是说,用户程序执行系统调用时,由内核代替该进程去执行该用户原本没有权限执行的工作,比如磁盘操作,而用户程序等待内核的执行完成,再返回到该用户程序(实际情况会涉及到各进程的调度,为了理解方便这里简化了概念).该过程的另一种说法叫陷入内核.  

从形式上来说,系统调用看起来就跟普通的 API 一样,比如进程需要读磁盘上的文件,使用 read 接口,因为普通用户没有读磁盘的权限,所以需要通过系统调用让内核帮忙做这件事,在实际的编程中,每当我们使用 read 函数时,内核中的 sys_read 函数就会被调用,看起来 sys_read 是 read 函数中的一个子程序一样,但是实际上情况要复杂得多.  

read 函数处于用户空间,而 sys_read 处于内核空间,这两者明显不是调用与被调用的关系,而是一方请求另一方帮忙处理工作的过程,而 API 只是简单的程序调用而已,这两者的区别不言而喻.

在这一章中,我们将深入地讨论系统调用的执行过程,看看用户空间到底是如何通过系统调用这道门进入到内核,又是如何从内核返回的.   

本文是基于 armv7 平台架构的分析,自然需要对 arm 的体系架构有一定的了解,关于 armv7 的指令架构,可以参考我的这些博客 TODO.

## 预备知识
本文是基于 armv7 平台架构的分析,自然需要对 arm 的体系架构有一定的了解,关于 armv7 的指令架构,可以参考我的这些博客 TODO.

## EABI 和 OABI
ABI 是应用程序二进制接口,每个操作系统都会为运行在该系统下的应用程序提供应用程序二进制接口（Application Binary Interface，ABI）。ABI包含了应用程序在这个系统下运行时必须遵守的编程约定,对于 arm 的函数调用而言,它定义了函数调用约定,系统调用形式以及目标文件的格式等.  

在 arm 平台架构中,存在两种不同的 ABI 形式,OABI 和 EABI,OABI 中的 O 是 old 的意思,表示旧有的 ABI,而 EABI 是基于 OABI 上的改进,或者说它更适合目前大多数的硬件,OABI 和 EABI 的区别主要在于浮点的处理和系统调用,浮点的区别不做过多讨论,对于系统调用而言,OABI 和 EABI 最大的区别在于,OABI 的系统调用指令需要传递参数来指定系统调用号,而 EABI 中将系统调用号保存在 r7 中.  

所以在系统调用的源码实现中,尽管大多数情况下都是使用 EABI 的系统调用方式,也会保持对 OABI 的兼容,而 CONFIG_OABI_COMPAT 表示是否保持对 OABI 的兼容,大部分平台都没有定义,所以在本章的讨论中不包括 OABI 的部分.  

## 系统调用的初始化
了解了系统调用的基本概念之后,我们同样需要知道,系统调用有哪些?   

系统调用由内核中定义的一个静态数组描述的,这个数组名为 sys_call_table,系统调用的数量由 NR_syscalls 这个宏描述，默认为 400，也可以手动地修改,这些系统调用的定义在 entry-common.S 中:

```
syscall_table_start sys_call_table
...
#include <calls-eabi.S>
...
syscall_table_end sys_call_table
```

\#include 这个预编译指令将预编译阶段会将指定文件中的内容全部拷贝到当前地址，而 calls-eabi.S 文件中就是系统调用表列表的定义：

```
NATIVE(0, sys_restart_syscall)
NATIVE(1, sys_exit)
...
NATIVE(396, sys_pkey_free)
NATIVE(397, sys_statx)
```

于是整个系统调用表的定义变成了：

```
syscall_table_start sys_call_table
NATIVE(0, sys_restart_syscall)
NATIVE(1, sys_exit)
...
NATIVE(396, sys_pkey_free)
NATIVE(397, sys_statx)
syscall_table_end sys_call_table
```

这里有三个部分需要关注，syscall_table_start、syscall_table_end 和 NATIVE 这三个宏，接下来就一个个解析。  

```
	.macro	syscall_table_start, sym
	.equ	__sys_nr, 0
	.type	\sym, #object
ENTRY(\sym)
	.endm
```
syscall_table_start 宏接收一个参数 sym，然后定义一个 __sys_nr 值为零，.type 表示指定符号的类型为 object。需要注意的是，这里的 ENTRY 并不是链接脚本中的 ENTRY 关键字，实际上这也是一个宏定义：

```
#define ENTRY(name)				\
	.globl name;				\
	name:
```

上面的调用中传入的参数是 sys_call_table，syscall_table_start sys_call_table 这个宏的啥意思就是：
* 创建一个 sys_call_table 的符号并使用 .globl 导出到全局符号
* 定义一个内部符号 __sys_nr，初始化为 0，这个变量主要用于后续系统调用好的计算和判断。

紧接着 syscall_table_start sys_call_table 这个语句的就是以 NATIVE 描述的系统调用列表,NATIVE 带两个参数：系统调用号和对应的系统调用函数，它的定义同样是一个宏：

```
#define NATIVE(nr, func) syscall nr, func

.macro	syscall, nr, func
.ifgt	__sys_nr - \nr       //__sys_nr 从0开始，总是指向已初始化系统调用的下一个系统调用号，如果需要初始化的系统调用号小于  __sys_nr，表示和已初始化的系统调用号冲突，报错。  
.error	"Duplicated/unorded system call entry"
.endif
.rept	\nr - __sys_nr       // .rept 指令表示下一条 .endr 指令之前的指令执行次数
.long	sys_ni_syscall       // 放置 sys_ni_syscall 函数到当前地址，sys_ni_syscall 是一个空函数，直接返回 -ENOSYS，即如果定义的系统调用号之间有间隔，填充为该函数
.endr
.long	\func                //将系统调用函数放置在当前地址
.equ	__sys_nr, \nr + 1    //将 __sys_nr 更新为当前系统调用号 +1 。
.endm
```
其实使用 NATIVE 指令实现一个 sys_call_table 的数组，不断地在数组尾部放置函数指针，而系统调用号对应数组的下标，只是添加了一些异常处理。  

接下来就是系统调用的收尾部分：syscall_table_end,sys_call_table，传入的参数为 sys_call_table，它也是通过宏实现的：

```
.macro	syscall_table_end, sym
.ifgt	__sys_nr - __NR_syscalls       //__NR_syscalls 是当前系统下静态定义的最大系统调用号，当前初始化的系统调用号不能超出
.error	"System call table too big"
.endif
.rept	__NR_syscalls - __sys_nr       //
.long	sys_ni_syscall                 //当前已定义系统调用号到最大系统调用之间未使用的系统调用号使用 sys_ni_syscall 这个空函数填充 
.endr
.size	\sym, . - \sym                 //设置 sym 也就是 sys_call_table 的 size
.endm
```


## 系统调用的产生
系统调用尽管是由用户空间产生的，但是在日常的编程工作中我们并不会直接使用系统调用，只知道在使用诸如 read、write 函数时，对应的系统调用就会产生，实际上，发起系统调用的真正工作封装在 C 库中，要查看系统调用的产生细节，一方面可以查看 C 库,另一方面也可以查看编译时的汇编代码。    



## glibc 库
既然系统调用基本都是封装在 glibc 中,最直接的方法就是看看它们的源代码实现,因为只是探究系统调用的流程,找一个相对简单的函数作为示例即可,这里以 close 为例,下载的源代码版本为 glibc-2.30.

close 的定义在 close.c 中:

```
int __close (int fd)
{
  return SYSCALL_CANCEL (close, fd);
}
```

SYSCALL_CANCEL 是一个宏,被定义在 sysdeps/unix/sysdep.h 中,由于该宏的嵌套有些复杂,全部贴上来进行解析并没有太多必要,就只贴上它的调用路径:
```
SYSCALL_CANCEL
	->INLINE_SYSCALL_CALL
		->__INLINE_SYSCALL_DISP
			->__INLINE_SYSCALLn(n=1~7)
				->INLINE_SYSCALL   
```
对于不同的架构,INLINE_SYSCALL 有不同的实现,毕竟系统调用指令完全是硬件相关指令,可以想到其最后的定义肯定是架构相关的,而 arm 平台的 INLINE_SYSCALL 实现在 sysdeps/unix/sysv/linux/arm/sysdep.h:

```
INLINE_SYSCALL
	->INTERNAL_SYSCALL
		->INTERNAL_SYSCALL_RAW
```
整个实现流程几乎全部由宏实现,在最后的 INTERNAL_SYSCALL_RAW 中,执行了以下的指令:

```
...
asm volatile ("swi	0x0	@ syscall " #name	\
		     : "=r" (_a1)				\
		     : "r" (_nr) ASM_ARGS_##nr			\
		     : "memory");				\
       _a1; })
...
```
其中的 swi 指令正是执行系统调用的软中断指令,在新版的 arm 架构中,使用 svc 指令代替 swi,这两者是别名的关系,没有什么区别.  

在 OABI 规范中,系统调用号由 swi(svc) 后的参数指定,在 EABI 规范中,系统调用号则由 r7 进行传递,系统调用的参数由寄存器进行传递.  

这里需要区分系统调用和普通函数调用,对于普通函数调用而言,前四个参数被保存在 r0~r3 中,其它的参数被保存在栈上进行传递.  

但是在系统调用中,swi(svc)指令将会引起处理器模式的切换,user->svc,而 svc 模式下的 sp 和 user 模式下的 sp 并不是同一个,因此无法使用栈直接进行传递,从而需要将所有的参数保存在寄存器中进行传递,在内核文件 include/linux/syscall.h 中定义了系统调用相关的函数和宏,其中 SYSCALL_DEFINE_MAXARGS 表示系统调用支持的最多参数值,在 arm 下为 6,也就是 arm 中系统调用最多支持 6 个参数,分别保存在 r0~r5 中.  

## 反汇编代码
除了直接查看源代码,也可以通过反汇编代码来查看系统调用细节,下面是 open 系统调用的片段,对应的反汇编代码为:

```
int fd = open("test",O_CREAT|O_RDWR,0666);

89d6:       f24d 30c4       movw    r0, #54212      ; 0xd3c4
89da:       f2c0 0004       movt    r0, #4            //第一个参数保存在 r0 中,赋值为 0x4d3c4,这是 "test" 的地址,保存在 .rodata 段中.
89de:       2142            movs    r1, #66 ; 0x42    //第二个参数保存在 r1 中,0x42 正是 O_CREAT|O_RDWR 的编码
89e0:       f44f 72db       mov.w   r2, #438          //对应第三个参数 
89e4:       f010 fcc4       bl      19370 <__libc_open> //跳转到 __libc_open 函数中

...
...

000193b0 <__libc_open>:
...
193c2:       4f0f            ldr     r7, [pc, #60]   ; 从 __libc_open+0x50 处取得系统调用号保存在 r7 中.
193c4:       df00            svc     0
...
19400:       00000005        .word   0x00000005      //系统调用号的保存地址,read 对应 05

```

从反汇编代码可以看到,系统调用的过程正是将参数一一保存到寄存器中,再将系统调用号保存在 r7 中,然后调用 svc 指令执行系统调用.  

因此，可以猜想到，在内核响应系统调用时，执行相反的过程，将参数从 r0~r5 中取出，然后根据 r7 中的系统调用号来决定执行哪个系统调用，最后返回到用户空间。具体实现是不是这样呢？这需要参照内核代码。  


## 内核中系统调用的处理
系统调用的处理完全不像是想象中那么简单，从用户空间到内核需要经历处理器模式的切换，svc 指令实际上是一条软件中断指令，和硬件中断不同的是软件中断是主动发起的，



