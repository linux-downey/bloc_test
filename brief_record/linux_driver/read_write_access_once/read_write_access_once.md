# linux 内核 READ/WRITE/ACEESS_ONCE 接口
在阅读内核代码的过程中，如果你不断地深挖某个模块的实现，通常会遇到 READ_ONCE 或者 ACEESS_ONCE 这一类接口，对于梳理模块的框架结构来说，并不需要详细地了解这几个宏的定义，只需要知道如何使用就好。  

READ_ONCE 的作用就是读出某个变量，val = READ_ONCE(global_val) 可以简单地理解成 val = global_val,同样的，对于 WRITE_ONCE 和 ACEESS_ONCE 也是同样的效果，使用这种宏看起来是多此一举。  

但是，既然存在就有它存在的理由，只是 linux 内核提供的接口屏蔽了实现细节，我们只需要知道怎么用就 OK，当看到这一系列 \*_ONCE 接口时，有个模糊的概念：这些接口是为了解决一些同步问题而出现，因此在使用的时候，为了保险起见，大可以对所有全局对象内存读写操作都添加上这个接口，在没有详细了解 \*_ONCE 的实现时，这种做法是比较保险的，可能会损失一些性能，而大部分情况下，这些性能损失和潜在的 bug 比起来不值一提。  

不过，内核操作是严谨的，在编写内核代码时需要清晰地知道我们正在做的是，所以越是这种基础的、常用的接口，越是需要弄清楚底层细节。 

## volatile
嵌入式工程师对 volatile 这个关键词一定不会陌生，





参考：https://www.kernel.org/doc/html/v4.10/process/volatile-considered-harmful.html
https://zhuanlan.zhihu.com/p/102406978
https://zhuanlan.zhihu.com/p/102370222



