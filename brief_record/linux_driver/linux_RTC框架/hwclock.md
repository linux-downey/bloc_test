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


## 时区的设置
在高中地理就学习过时区相关的知识，以为世界各地经度的不同导致时差的出现，而在 linux 系统中，默认设置的 UTC 为格林威治时间，但是中国的时区在东八区，需要在标准时间上加上 8 小时，这就需要进行时区的设置。  

在没有正确设置时区的系统上是这样的：
```
~$ date
Sat Dec 28 04:56:43 UTC 2019
~$ date -R
Sat, 28 Dec 2019 04:56:46 +0000
```

在使用 NTP 网络时间同步服务时，如果时区设置不正确，是得不到正确的时间的，如果你在中国，往往会发现当前时间比正常时间慢了 8 小时。  

如果你只使用 RTC ,自然是可以手动地在当前时间上加上 8 小时以得到正确的时间，但是这种违规的操作并不提倡。   

中国的标准时间为 CST ，我们需要将 UTC 设置为 CST ，也就是设置时区，直接执行以下的指令即可：

```
echo "TZ='Asia/Shanghai'; export TZ" >> ~/.profile
```

然后注销，重新登录就可以发现时区信息和时间都已经修改为上海的时区了：

```
~$ date
Sat Dec 28 12:57:37 CST 2019
~$ date -R
Sat, 28 Dec 2019 12:57:40 +0800
```
可以发现时区信息已经由 UTC 修改为 CST，时间偏移由原来的 +0000 调整为 +0800，意为在标准世界时上加 8 小时。  


除了针对用户设置时区之外，还可以针对系统级的设置时区，在用户登录的时候，系统会读取 /etc/localtime 文件中的时区信息，实际上 /etc/localtime 是一个链接文件，默认情况下，该文件的链接为：

```
~$ ls -l /etc/localtime
lrwxrwxrwx 1 root root 33 Dec 28 13:11 /etc/localtime -> /usr/share/zoneinfo/Etc/UTC
```

可以直接将链接地址修改为 /usr/share/zoneinfo/Asia/Shanghai。

```
:~$ sudo rm /etc/localtime 
:~$ sudo ln -s /usr/share/zoneinfo/Asia/Shanghai /etc/localtime
```

重新登录之后，也可以看到时区被设置为 CST 中国时区。


值得注意的是，在我的 debian 9 系统上，传统的 tzselect 命令不再能有效地设置时区，有兴趣的朋友可以在其他平台上试试。

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

	...

	if (opt & HWCLOCK_OPT_HCTOSYS)
		to_sys_clock(&rtcname, utc);
	else if (opt & HWCLOCK_OPT_SYSTOHC)
		from_sys_clock(&rtcname, utc);
	...

	return 0;
}
```
该函数主要包含三个部分：
* 命令行参数的解析
* 时间的同步



这第一步就是命令行参数的解析，调用 getopt32long() 接口，从名字可以看出，这个接口主要是获取、解析命令行参数并返回给 32 位变量 opt，同时获取 rtc 的设备名。  

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


## 时间的同步
在 hwclock 的使用中，时间的同步分两种：将 RTC 时间设置为系统时间，或者将系统时间写入 RTC ，分别是：

```
hwclock -s
hwclock -w
```

这两个读写操作分别对应 hwclock 源码中的 HWCLOCK_OPT_HCTOSYS 和 HWCLOCK_OPT_SYSTOHC。  

hwclock 源码处理部分：
```
if (opt & HWCLOCK_OPT_HCTOSYS)
		to_sys_clock(&rtcname, utc);
	else if (opt & HWCLOCK_OPT_SYSTOHC)
		from_sys_clock(&rtcname, utc);
```

#### to_sys_clock
先看 to_sys_clock(&rtcname, utc)，其中 rtcname 是 RTC 设备名，utc 表示时区参数，将 RTC 时钟设置为系统时钟：

```
static void to_sys_clock(const char **pp_rtcname, int utc)
{
	struct timeval tv;
	struct timezone tz;

	tz.tz_minuteswest = timezone/60;
	tz.tz_dsttime = 0;

	tv.tv_sec = read_rtc(pp_rtcname, NULL, utc);
	tv.tv_usec = 0;
	if (settimeofday(&tv, &tz))
		bb_perror_msg_and_die("settimeofday");
}
```
这个函数实现了两部分：
* 调用 read_rtc 从 RTC 设备中获取 RTC 时间并返回
* 调用 settimeofday() 系统调用来设置系统时间。

read_rtc 的实现如下：
```
static time_t read_rtc(const char **pp_rtcname, struct timeval *sys_tv, int utc)
{
	struct tm tm_time;
	int fd;

	fd = rtc_xopen(pp_rtcname, O_RDONLY);

	rtc_read_tm(&tm_time, fd);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);

	return rtc_tm2time(&tm_time, utc);
}
```
先使用 rtc_xopen 函数打开 RTC 设备，rtc_open 是在 open 系统调用上针对 RTC 的一层封装，open 系统调用打开文件成功返回 fd，失败返回错误。而 rtc_xopen 在其基础上，封装了其他功能：
*  判断传入的设备名是否为空，如果为空则为其指定一个默认的设备名再处理，默认设备名通常为：/dev/rtc
* 如果设备处于忙状态，并不会像 open 函数一样返回 EBUSY 错误，而是多次尝试，直到打开或者直到超过尝试上限次数。  
* 一些调试信息的输出

打开设备之后，接着就是调用 rtc_read_tm，源码是这样的：

```
void FAST_FUNC rtc_read_tm(struct tm *ptm, int fd)
{
	memset(ptm, 0, sizeof(*ptm));
	xioctl(fd, RTC_RD_TIME, ptm);
	ptm->tm_isdst = -1; /* "not known" */
}
```

在 rtc_read_tm 中，调用了 xioctl，cmd 为 RTC_RD_TIME，回顾上一章节中，在 RTC 的设备驱动部分，该 xioctl 是 ioctrl 系统调用的封装，就调用到了 RTC 驱动的 .ioctrl 函数，执行硬件的读。  


#### from_sys_clock
from_sys_clock 表示将系统的时间写到 RTC 中，按照 to_sys_clock 的实现可以猜到，该函数应该同样是调用了 ioctrl ，传入的 cmd 为 RTC_SET_TIME。  

下述就是 from_sys_clock 的源码：

```
static void from_sys_clock(const char **pp_rtcname, int utc)
{
	struct timeval tv;
	struct tm tm_time;
	int rtc;

	rtc = rtc_xopen(pp_rtcname, O_WRONLY);
	gettimeofday(&tv, NULL);
	...
	tm_time.tm_isdst = 0;
	xioctl(rtc, RTC_SET_TIME, &tm_time);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(rtc);
}

```

从源码中可以看到，先使用 rtc_xopen 打开传入的文件，rtc_xopen 在上文中有介绍，然后调用 xioctl，传入 cmd 为 RTC_SET_TIME，调用内核驱动中的 .ioctrl 函数。  





