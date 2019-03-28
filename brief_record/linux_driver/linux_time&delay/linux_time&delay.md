# jiffies的使用
HZ的值，在beagle bone green上是 250
即jiffies一秒更新250次，每4ms更新一次

在比较jiffies的时候，不要直接比较jiffies的大小，可能出现的溢出问题很可能导致比较结果的错误。  

直接使用内核提供的接口来比较：
time_after,time_before
time_after_eq,time_before_eq
分别对应大于，小于，大于等于，小于等于，参数比较的结果成立将返回真。 


jiffies_64的访问不像jiffies那样直接，在64位系统中，jiffies和jiffies64是同一个变量，


但是在32位系统中，jiffies64的读取并非原子操作，很可能在读高32位和低32位的过程中，jiffies的值发生了变化，从而获取了不正确的时间，对于这个问题，最好是使用内核导出的api接口来读取内核时间：

u64 get_jiffies_64(void);
在<linux/types.h>中定义了u64类型


当需要度量特别短的时间的时候，可以使用处理器特定的计数寄存器来实现，但是这种实现方式并不支持跨平台。  

一般情况下，使用TSC寄存器,arm上面并没有。  

有一个通用的函数：get_cycles()


# 代码的延迟执行
cpu_relax()  CPU不执行任何事，相当于阻塞运行
除非在SMP系统中，才会有进程的调度。




































