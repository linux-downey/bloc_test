# linux 内核 READ/WRITE/ACEESS_ONCE 接口
在阅读内核代码的过程中，如果你不断地深挖某个模块的实现，通常会遇到 READ_ONCE 或者 ACEESS_ONCE 这一类接口，对于梳理模块的框架结构来说，并不需要详细地了解这几个宏的定义，只需要知道如何使用就好。

READ_ONCE 的作用就是读出某个变量，val = READ_ONCE(global_val) 可以简单地理解成 val = global_val,同样的，对于 WRITE_ONCE 和 ACEESS_ONCE 也是同样的效果，使用这种宏看起来是多此一举。

但是，既然存在就有它存在的理由，只是 linux 内核提供的接口屏蔽了实现细节，我们只需要知道怎么用就 OK，当看到这一系列 *_ONCE 接口时，有个模糊的概念：这些接口是为了解决一些同步问题而出现，因此在使用的时候，为了保险起见，大可以对所有全局对象内存读写操作都添加上这个接口，在没有详细了解 *_ONCE 的实现时，这种做法是比较保险的，可能会损失一些性能，而大部分情况下，这些性能损失和潜在的 bug 比起来不值一提。

不过，内核操作是严谨的，在编写内核代码时需要清晰地知道我们正在做的是，所以越是这种基础的、常用的接口，越是需要弄清楚底层细节。

## volatile
嵌入式工程师对 volatile 这个关键词一定不会陌生，经常会在各种嵌入式程序中见到它的身影。   

程序的运行并不会严格地按照程序的设计逻辑执行，应用程序需要通过编译器将这些程序转化为机器代码，同时这些二进制的机器码在 CPU 上运行，由 CPU 决定执行路径。如果编译器仅仅是简单地对程序进行翻译，CPU 仅仅是按照二进制代码逐条执行，事情并不会多复杂。实际上，编译器以及 CPU 的设计人员对程序执行效率有非常疯狂地执着，丝毫不会放过任何一个可以让程序运行更快的机会。  

对于编译过程而言，编译器并不完全相信程序员，而是自主地对程序员开发的程序进行优化，这种优化通常可以带来更好地性能，比如自动检测并删除冗余的部分以节省空间、或者用更有效率的指令替换那些低效的操作(比如将 i*8 换成 i<<3),或者对不相关的指令进行重新排序以加速程序的执行，程序员普遍可以接受这种优化，因为编译器的优化通常确实可以提高执行效率以及节省内存空间。   

但是，要命的是，编译器优化只保证单线程下的安全，也就是这种优化并不考虑多线程的情况，而且编译器标准也默许了这种行为(毕竟这种优化所带来的执行效率提升非常可观)，这就导致了在多线程或者多核的情况下，共享对象的操作变成了一件非常危险的事，多个执行流同时对一个共享对象的操作往往会带来一些意向不到的 bug。  

相对于编译器开发人员对执行效率的执着，CPU 的设计人员可谓是有过之而无不及，同样的，他们也不会放过任何一个可以加速 CPU 执行指令的机会，对于单个 CPU 核而言，它总是任劳任怨地逐条执行二进制代码中的指令，指令分为多种：计算指令、控制指令、内存访问指令等等，各类指令的执行时间是不一样的，CPU 对寄存器的操作几乎和 CPU 的执行频率一样快，而内存操作指令相对就要慢很多，往往是几十上百倍的差距，这就带来一个问题，当执行一条缓慢的内存访问指令时，CPU 



参考：https://www.kernel.org/doc/html/v4.10/process/volatile-considered-harmful.html
https://zhuanlan.zhihu.com/p/102406978
https://zhuanlan.zhihu.com/p/102370222