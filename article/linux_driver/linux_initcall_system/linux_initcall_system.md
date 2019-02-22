rest_init()
kernel_thread(kernel_init, NULL, CLONE_FS);
kernel_init()
kernel_init_freeable();
do_basic_setup();
do_initcalls();

在阅读linux源代码时，经常可以看到xxx_initcall(func)这一类的声明，不知道这种声明有什么作用。  
又经常碰到某个函数没有显式地在函数执行流被调用，但是根据log信息可以看到它却被调用了，比较特殊的地方只是这个函数被xxx_init(func)声明。 

# linux的initcall机制(针对编译进内核的驱动)
## initcall机制的由来
我们都知道，linux对驱动程序提供静态编译进内核和动态加载两种方式，当我们试图将一个驱动程序编译进内核时，用户通常提供一个xxx_init()函数接口以启动这个驱动程序同时提供某些服务。  
那么，根据常识来说，这个xxx_init()函数肯定是要在系统启动的某个时候被调用，才能启动这个驱动程序，最简单直观地做法就是：开发者试图添加一个驱动程序时，在系统init程序的某个地方直接添加调用自己驱动程序的xxx_init()函数，在内核启动时自然会调用到这个程序。  
但是，回头一想，这种做法在单人开发的小系统中或许可以，但是在linux中，如果驱动程序是这么个添加法，那就是一场灾难，这个道理我想不用我多说。  

不难想到另一种方式，就是集中提供一个地方，如果你要添加你的驱动程序，你就将你的初始化函数在这个地方进行添加，在内核启动的时候统一扫描这个地方，再执行这一部分的所有被添加的驱动程序。  
那到底怎么添加呢？直接在C文件中作一个列表，在里面添加初始化函数？我想随着驱动程序数量的增加，这个列表会让人头昏眼花。  
当然，对于linux而言，这些都不是事，linux的做法是：
底层实现上，在内核镜像文件中，自定义一个段，这个段里面专门用来存放这些初始化函数的地址，内核启动时，只需要在这个段地址处取出函数指针，一个个执行即可。  
对上层而言，linux内核提供xxx_init(init_func)宏定义接口,驱动开发者只需要将驱动程序的init_func使用xxx_init()来修饰，这个函数就被自动添加到了上述的段中，开发者完全不需要关心实现细节。  
对于各种各样的驱动而言，可能存在一定的依赖关系，需要遵循先后顺序来进行初始化，考虑到这个，linux也对这一部分做了分级处理。  

## initcall的源码
在平台对应的init.h文件中，可以找到xxx_initcall的定义：

	/*Only for built-in code, not modules.*/
	#define early_initcall(fn)		__define_initcall(fn, early)

	#define pure_initcall(fn)		__define_initcall(fn, 0)
	#define core_initcall(fn)		__define_initcall(fn, 1)
	#define core_initcall_sync(fn)		__define_initcall(fn, 1s)
	#define postcore_initcall(fn)		__define_initcall(fn, 2)
	#define postcore_initcall_sync(fn)	__define_initcall(fn, 2s)
	#define arch_initcall(fn)		__define_initcall(fn, 3)
	#define arch_initcall_sync(fn)		__define_initcall(fn, 3s)
	#define subsys_initcall(fn)		__define_initcall(fn, 4)
	#define subsys_initcall_sync(fn)	__define_initcall(fn, 4s)
	#define fs_initcall(fn)			__define_initcall(fn, 5)
	#define fs_initcall_sync(fn)		__define_initcall(fn, 5s)
	#define rootfs_initcall(fn)		__define_initcall(fn, rootfs)
	#define device_initcall(fn)		__define_initcall(fn, 6)
	#define device_initcall_sync(fn)	__define_initcall(fn, 6s)
	#define late_initcall(fn)		__define_initcall(fn, 7)
	#define late_initcall_sync(fn)		__define_initcall(fn, 7s)

xxx_init_call(fn)的原型其实是__define_initcall(fn, n)，n是一个数字或者是数字+s，这个数字代表这个fn执行的优先级，数字越小，优先级越高，带s的fn优先级低于不带s的fn优先级。    
继续跟踪代码，看看__define_initcall(fn,n):

	#define __define_initcall(fn, id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" #id ".init"))) = fn;
值得注意的是，__attribute__()是gnu C中的扩展语法，它可以用来实现很多灵活的定义行为，这里不细究。  

__attribute__((__section__(".initcall" #id ".init")))表示编译时将目标符号放置在括号指定的段中。
而#在宏定义中的作用是将目标字符串化，##在宏定义中的作用是符号连接，将多个符号连接成一个符号，并不将其字符串化。    

__used是一个宏定义，

	#define  __used  __attribute__((__used__))
使用前提是在编译器编译过程中，如果定义的符号没有被引用，编译器就会对其进行优化，不保留这个符号，而__attribute__((__used__))的作用是告诉编译器这个静态符号在编译的时候即使没有使用到也要保留这个符号。

为了更方便地理解，我们拿举个例子来说明，开发者声明了这样一个函数：pure_initcall(test_init);
所以pure_initcall(test_init)的解读就是：

	首先宏展开成：__define_initcall(test_init, 0)
	然后接着展开：static initcall_t __initcall_test_init0 = test_init;这就是一个简单的变量定义。 
	同时声明__initcall_test_init0这个变量即使没被引用也保留符号，且将其放置在内核镜像的.initcall0.init段处。
	
需要注意的是，根据官方注释可以看到early_initcall(fn)只针对内置的核心代码，不能描述模块。  


## xxx_initcall修饰函数的调用
既然我们知道了xxx_initcall是怎么定义而且目标函数的放置位置，那么使用xxx_initcall()修饰的函数是怎么被调用的呢？  
我们就从内核C函数起始部分也就是start_kernel开始往下挖,这里的调用顺序为：
start_kernel
->




