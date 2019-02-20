# 设备树处理之——device_node转换成platform_device
*** 以下讨论基于linux4.14，arm平台 ***

## platform device
设备树的产生就是为了替代driver中过多的platform_device部分的静态定义，将硬件资源抽象出来，由系统统一解析，这样就可以避免各驱动中对硬件资源大量的重复定义，这样一来，几乎可以肯定的是，设备树中的节点最终目标是转换成platform device结构，在驱动开发时就只需要添加相应的platform driver部分进行匹配即可。    
在上一节中讲到设备树dtb文件中的各个节点转换成device_node的过程，每个设备树子节点都将转换成一个对应的device_node节点，那么：
是不是每个由设备树节点转换而来的device_node结构体都将转换成对应的？
如果是device_node转换成platform device，这个转换过程又是怎么样的呢？
带着这两个疑问，我们进入源代码中寻找答案。


## platform_device转换的开始
事实上，如果从C语言的开始函数start_kernel进行追溯，是找不到platform device这一部分转换的源头的，事实上，这个转换过程的函数是of_platform_default_populate_init(),它被调用的方式是这样一个声明：

    arch_initcall_sync(of_platform_default_populate_init);
在linux中，同系列的调用声明还有：

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
这些宏最终都是调用__define_initcall(fn, n),这个数字代表系统启动时被调用的优先级，数字越小，优先级越低，用这一系列宏声明一个新的函数就是将这个函数指针放入内存中一个指定的段内。

    #define __define_initcall(fn, id) \
	static initcall_t __initcall_##fn##id __used \
	__attribute__((__section__(".initcall" #id ".init"))) = fn;
即放入到".initcalln.init"中，n代表优先级，当系统启动时，会依次调用这些段中的函数。  

下面我们就进入到of_platform_default_populate_init()中，查看它的执行过程：

    
    static int __init of_platform_default_populate_init(void)
    {  
        ...
        of_platform_default_populate(NULL, NULL, NULL);
        ...
    }
在函数of_platform_default_populate_init()中,调用了of_platform_default_populate(NULL, NULL, NULL);，传入三个空指针：

    const struct of_device_id of_default_bus_match_table[] = {
        { .compatible = "simple-bus", },
        { .compatible = "simple-mfd", },
        { .compatible = "isa", },
        #ifdef CONFIG_ARM_AMBA
            { .compatible = "arm,amba-bus", },
        #endif /* CONFIG_ARM_AMBA */
            {} /* Empty terminated list */
    };

    int of_platform_default_populate(struct device_node *root,const struct of_dev_auxdata *lookup,struct device *parent)
    {
        return of_platform_populate(root, of_default_bus_match_table, lookup,
                        parent);
    }
of_platform_default_populate()调用了of_platform_populate()，需要注意的是，在调用of_platform_populate()时传入了参数of_default_bus_match_table，这个table是一个静态数组，设置了一系列的.compatible属性，这个table是做什么用的呢？在这里就不卖关子了，我们先把这部分提前梳理一遍，方便建立一个概念：

    

