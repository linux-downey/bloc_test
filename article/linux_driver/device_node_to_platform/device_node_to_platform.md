# 设备树处理之——device_node转换成platform_device
*** 以下讨论基于linux4.14，arm平台 ***

## platform device
设备树的产生就是为了替代driver中过多的platform_device部分的静态定义，将硬件资源抽象出来，由系统统一解析，这样就可以避免各驱动中对硬件资源大量的重复定义，这样一来，几乎可以肯定的是，设备树中的节点最终目标是转换成platform device结构，在驱动开发时就只需要添加相应的platform driver部分进行匹配即可。    
在上一节中讲到设备树dtb文件中的各个节点转换成device_node的过程，每个设备树子节点都将转换成一个对应的device_node节点，那么：
是不是每个由设备树节点转换而来的device_node结构体都将转换成对应的？  

首先，对于所有的device_node，如果要转换成platform_device，必须满足以下条件：
* 一般情况下，只对设备树中根的子节点进行转换，也就是子节点的子节点并不处理。但是存在一种特殊情况，就是当某个根子节点的compatible属性为"simple-bus"、"simple-mfd"、"isa"、"arm,amba-bus"时，当前节点中的子节点将会被转换成platform_device节点。  
* 节点中必须有compatible属性。    

如果是device_node转换成platform device，这个转换过程又是怎么样的呢？  
在老版本的内核中，platform_device部分是静态定义的，其实最主要的部分就是resources部分，这一部分描述了当前驱动需要的硬件资源，一般是IO，中断等资源。  
在设备树中，这一类资源通常通过reg属性来描述，中断则通过interrupts来描述，所以，设备树中的reg和interrupts资源将会被转换成platform_device内的struct resources资源。  
那么，设备树中其他属性是怎么转换的呢？答案是：不需要转换，在platform_device中有一个成员struct device dev，这个dev中又有一个指针成员struct device_node *of_node,linux的做法就是将这个of_node指针直接指向由设备树转换而来的device_node结构。  
例如，有这么一个struct platform_device* of_test.我们可以直接通过of_test->dev.of_node来访问设备树中的信息(设备树节点到device_node的转换参考一篇博客).  

大体流程讲完了，接下来从源代码中进行求证。  

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
of_platform_default_populate()调用了of_platform_populate()，需要注意的是，在调用of_platform_populate()时传入了参数of_default_bus_match_table[]，这个table是一个静态数组，这个静态数组中定义了一系列的compatible属性："simple-bus"、"simple-mfd"、"isa"、"arm,amba-bus"，按照我们上文中的描述，当某个根节点下的一级子节点的compatible属性为这些属性其中之一时，它的一级子节点也将由device_node转换成platform_device.到底是不是这样呢？接着往下看：

    int of_platform_populate(struct device_node *root,const struct of_device_id *matches,const struct of_dev_auxdata *lookup,struct device *parent){
        root = root ? of_node_get(root) : of_find_node_by_path("/");

        for_each_child_of_node(root, child) {
            rc = of_platform_bus_create(child, matches, lookup, parent, true);
            if (rc) {
                of_node_put(child);
                break;
            }
	    }
        ...
    }
首先，从设备树中获取根节点的device_node结构体，然后对每个根目录下的一级子节点调用of_platform_bus_create()，从命名上来看，这部分解析的目的是建立各个bus的platform_device结构，需要注意的是对于of_platform_bus_create(child, matches, lookup, parent, true)，matchs参数是上文中提到的compatible静态数组，而lookup和parent依旧为NULL。  
接着跟踪代码：
    static int of_platform_bus_create(struct device_node *bus,const struct of_device_id *matches,const struct of_dev_auxdata *lookup,struct device *parent, bool strict)
    {
        dev = of_platform_device_create_pdata(bus, bus_id, platform_data, parent);

	    if (!dev || !of_match_node(matches, bus))
		    return 0;

        for_each_child_of_node(bus, child) {
            pr_debug("   create child: %pOF\n", child);
            rc = of_platform_bus_create(child, matches, lookup, &dev->dev, strict);
            if (rc) {
                of_node_put(child);
                break;
            }
        }
        ...
    }
对于节点的转换，是由of_platform_device_create_pdata(bus, bus_id, platform_data, parent)函数来实现的。  
紧接着，在第二行的函数调用中，判断of_match_node(matches,bus)函数的返回值，这个matchs就是compatible的静态数组，这个函数的作用就是判断当前节点的compatible属性是否包含上文中compatible静态数组中的元素，如果不包含，函数返回。  
如果当前compatible属性中包含静态数组中的元素，即当前节点的compatible属性有"simple-bus"、"simple-mfd"、"isa"、"arm,amba-bus"其中一项，递归地对当前节点调用of_platform_bus_create()，即将符合条件的子节点转换为platform_device结构。  
   

关于节点转换的细节部分我们接着跟踪of_platform_device_create_pdata(bus, bus_id, platform_data, parent)函数，此时的参数platform_data为NULL。  

    static struct platform_device *of_platform_device_create_pdata(struct device_node *np,const char *bus_id,void *platform_data,struct device *parent)
    {
        struct platform_device *dev;
        dev = of_device_alloc(np, bus_id, parent);
        dev->dev.bus = &platform_bus_type;
	    dev->dev.platform_data = platform_data;


        if (of_device_add(dev) != 0) {
		    platform_device_put(dev);
		    goto err_clear_flag;
	    }
    }
struct platform_device终于现出了真身，在这个函数调用中，显示申请并初始化一个platform_device结构体，将传入的device_node链接到成员：dev.fo_node中
赋值bus成员和platform_data成员，platform_data成员为NULL。  
再使用of_device_add()将当前生成的platform_device添加到系统中。 

对于of_platform_device_create_pdata()函数中的实现，我们需要逐一讲解其中的函数实现：

### of_device_alloc()

    struct platform_device *of_device_alloc(struct device_node *np,const char *bus_id,struct device *parent)
    {
        //统计reg属性的数量
        while (of_address_to_resource(np, num_reg, &temp_res) == 0)
		    num_reg++;
        //统计中断irq属性的数量
	    num_irq = of_irq_count(np);
        //根据num_irq和num_reg的数量申请相应的struct resource内存空间。
        if (num_irq || num_reg) {
            res = kzalloc(sizeof(*res) * (num_irq + num_reg), GFP_KERNEL);
            if (!res) {
                platform_device_put(dev);
                return NULL;
            }
            //设置platform_device中的num_resources成员
            dev->num_resources = num_reg + num_irq;
            //设置platform_device中的resource成员
            dev->resource = res;

            //将device_node中的reg属性转换成platform_device中的struct resource成员。  
            for (i = 0; i < num_reg; i++, res++) {
                rc = of_address_to_resource(np, i, res);
                WARN_ON(rc);
            }
            //将device_node中的irq属性转换成platform_device中的struct resource成员。 
            if (of_irq_to_resource_table(np, res, num_irq) != num_irq)
                pr_debug("not all legacy IRQ resources mapped for %s\n",
                    np->name);
	    }
        //将platform_device的dev.of_node成员指针指向device_node。  
        dev->dev.of_node = of_node_get(np);
        //将platform_device的dev.fwnode成员指针指向device_node的fwnode成员。
	    dev->dev.fwnode = &np->fwnode;
        //设备parent为platform_bus
	    dev->dev.parent = parent ? : &platform_bus;

    }
首先，函数先统计设备树中reg属性和中断irq属性的个数，然后分别为它们申请内存空间，链入到platform_device中的struct resources成员中。除了设备树中"reg"和"interrupt"属性之外，还有可选的"reg-names"和"interrupt-names"这些io中断资源相关的设备树节点属性也在这里被转换。  
将相应的设备树节点生成的device_node节点链入到platform_device的dev.of_node中。  


### of_device_add

    int of_device_add(struct platform_device *ofdev){
        ...
        return device_add(&ofdev->dev);
    }
将当前platform_device中的struct device成员注册到系统device中，并为其在用户空间创建相应的访问节点。  


## 总结
总的来说，将device_node转换为platform_device的过程还是比较简单的。
整个转换过程的函数调用关系是这样的：

                                of_platform_default_populate_init()
                                            |
                                of_platform_default_populate();
                                            |
                                of_platform_populate();
                                            |
                                of_platform_bus_create()
                    _____________________|_________________
                    |                                      |
            of_platform_device_create_pdata()       of_platform_bus_create()
            _________________|____________________
           |                                      |
     of_device_alloc()                        of_device_add()         
    

好了，关于linux设备树中device_node到platform_device的转换过程的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.




    


