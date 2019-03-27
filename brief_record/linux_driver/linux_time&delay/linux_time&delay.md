# jiffies的使用
HZ的值，在beagle bone green上是 250
即jiffies一秒更新250次，每4ms更新一次

jiffies_64的访问不像jiffies那样直接，在64位系统中，jiffies和jiffies64是同一个变量，

但是在32位系统中，jiffies64的读取并非原子操作，很可能在读高32位和低32位的过程中，jiffies的值发生了变化，从而获取了不正确的时间，对于这个问题，最好是使用内核导出的api接口来读取内核时间：

u64 get_jiffies_64(void);
































