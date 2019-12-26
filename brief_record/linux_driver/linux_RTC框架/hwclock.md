# linux 时钟同步之 hwclock
在前两章中我们讲解了linux内核中的 RTC 驱动框架，在这一章节中，我们来到用户空间，看看 RTC 在系统中是如何发挥作用的。  

## linux中的时间
在 linux 中，时间的获取非常简单，直接在 shell 中键入 date 指令即可，命令行就会返回当前时间，格式通常是这样的:

```
Thu Dec 26 14:48:16 UTC 2019
```
也就是当前的时间信息，当然，根据 date 的各种参数，我们还可以可以获取到时区、unix 时间戳等时间相关的信息。  

可以了解到的是，linux 在上电启动时，是通过读取 RTC 的时间然后根据时区信息确定了当前的系统时间，也就是我们通过 date 指令获取的时间。  

在通常情况下，CPU 的内部 RTC 并非独立供电，当 CPU 断电时，RTC 也随之停止工作(电路板上的电容可能可以维持 ms 级别的供电)，通常当系统再次启动时，从内部 RTC 中读取到的时间是不准确的。   

所以，在很多应用场景下，都在电路板上外挂一个外部 RTC,且由电池独立供电，保证即使整个电路板断电的情况下，RTC 也可以正常运行很长一段时间。  

还有另外一种方式可以保持系统时间的准确性：NTP 服务，即网络时间的同步，通过标准的 NTP 服务器进行时间同步。不过，这依赖于网络，所以在某些情况下并不那么合适。   


## hwclock
既然是 RTC 系列的文章，这一章我们要讨论的是 RTC 时间与系统之间的同步，主要用到的指令是 hwclock 指令。  

hwclock 作为 busybox 的一员，我们可以通过 man 手册找到它的使用方法,通常，使用频率最高的几个参数如下：

* -r ，--show ：这两个参数都是现实当前 RTC 的时间
* -w，--systohc：将当前系统时间写入到 RTC 设备中
* -s，--hctosys：将当前 RTC 设备的时间设置为当前的系统时间
* -f ：指定需要读取的 RTC 设备节点，即 /dev/rtcn,当存在多个 RTC 设备时，需要指定操作的 RTC 设备节点。

hwclock 指令主要是访问 RTC 设备，同时调用系统时间接口，进行硬件 RTC 与系统时间的交互。   

既然了解了 RTC 在内核中的驱动实现，自然也是要看看 RTC 的设备接口是如何被使用的，这样才能对整个 RTC 在整个系统中的作用有一个比较清晰的认识，于是，博主就从 busybox 中找到了 hwclock 的源码，来一探究竟。


## hwclock 源码实现

hwclock 实现的功能并没有多复杂，因此源码实现也算简单，下面就是其源码实现部分：

```
int hwclock_main(int argc UNUSED_PARAM, char **argv)
{
	const char *rtcname = NULL;
	unsigned opt;
	int utc;

	/* Initialize "timezone" (libc global variable) */
	tzset();

	opt = getopt32long(argv,
		"^lurswtf:" "\0" "r--wst:w--rst:s--wrt:t--rsw:l--u:u--l",
		hwclock_longopts,
		&rtcname
	);

	/* If -u or -l wasn't given check if we are using utc */
	if (opt & (HWCLOCK_OPT_UTC | HWCLOCK_OPT_LOCALTIME))
		utc = (opt & HWCLOCK_OPT_UTC);
	else
		utc = rtc_adjtime_is_utc();

	if (opt & HWCLOCK_OPT_HCTOSYS)
		to_sys_clock(&rtcname, utc);
	else if (opt & HWCLOCK_OPT_SYSTOHC)
		from_sys_clock(&rtcname, utc);
	else if (opt & HWCLOCK_OPT_SYSTZ)
		set_system_clock_timezone(utc);
	else
		/* default HWCLOCK_OPT_SHOW */
		show_clock(&rtcname, utc);

	return 0;
}
```
该函数主要包含三个部分：
* 命令行参数的解析
* 时区的设置
* 时间的同步



这第一步就是设置时区，同时调用 getopt32long() 接口，从名字可以看出，这个接口主要是获取、解析命令行参数并返回给 opt，同时获取 rtc 的设备名。  

其 opt 是一个参数集，共 32 位，每一位都可以表示一个参数，其中定义如下：
```C
#define HWCLOCK_OPT_LOCALTIME   0x01
#define HWCLOCK_OPT_UTC         0x02
#define HWCLOCK_OPT_SHOW        0x04
#define HWCLOCK_OPT_HCTOSYS     0x08
#define HWCLOCK_OPT_SYSTOHC     0x10
#define HWCLOCK_OPT_SYSTZ       0x20
#define HWCLOCK_OPT_RTCFILE     0x40
```
而 rtc 的设备名被保存在 rtcname 中，默认情况下为 /dev/rtc0，使用 -f 参数时，为指定的设备名，比如 /dev/rtc1.

也就是当我们键入以下指令时:
```
hwclock -r -s -f /dev/rtc1
```
opt 的值为 HWCLOCK_OPT_SHOW | HWCLOCK_OPT_HCTOSYS。

而 rtcname 指向字符串 "/dev/rtc1"。




