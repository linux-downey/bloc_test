## linux 中bus驱动解析
总线(bus)是linux发展过程中抽象出来的一种设备模型，为了统一管理所有的设备，内核中每个设备都会被挂载在总线上，这个bus可以是对应硬件的bus(i2c bus、spi bus)、可以是虚拟bus(platform bus)。  

## 简述bus的工作流程
bus将所有挂在上面的具体设备抽象成两部分，driver和device。  

driver实现了同类型设备的驱动程序实现，而device则向系统注册具体的设备需要的资源，每当添加一个新的driver(device)到bus中时，都将调用bus的match函数，试图寻找匹配的device(driver)。 

总线大概是这样的：

![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/linux_driver/i2c_bus_driver/linux_bus_graph.png)


如果匹配成功，就调用probe函数，在probe函数中实现设备的初始化、各种配置以及生成用户空间的文件接口。

举个例子，针对AT24CXX(一种常用的存储设备)这种同系列产品，他们的操作方式都是非常相似的，不同的无非是容量大小。  

那么我们就没有必要为AT24C01、AT24C02去分别写一份驱动程序，而是统一为其写一份兼容所有AT24CXX的驱动程序，然后再传入不同的参数以对应具体的型号。  

在linux驱动管理模型中的体现就是：驱动程序对应driver、需要的具体型号的硬件资源对应device，将其挂在bus上。  

将driver注册到bus上，当用户需要使用AT24C01时，以AT24C01的参数构建一个对应device，注册到bus中，bus的match函数匹配上之后，调用probe函数，即可完成AT24C01的初始化，完成在用户空间的文件接口注册。  

以此类推，添加所有的AT24CXX设备都可以以这样的形式实现，只需要构建一份device，而不用为每个设备重写一份驱动，提高了复用性，节省了内存空间。  


## linux bus结构体
linux将设备挂在总线上，对应设备的注册和匹配流程由总线进行管理，在linux内核中，每一个bus，都由struct bus_type来描述：

    struct bus_type {
        const char		*name;
        const char		*dev_name;
        struct device		*dev_root;
        ...
        int (*match)(struct device *dev, struct device_driver *drv);
        int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
        int (*probe)(struct device *dev);
        int (*remove)(struct device *dev);
        void (*shutdown)(struct device *dev);
        struct subsys_private *p;
        ...
};
为了突出重点，省去了一些暂时不需要深入了解的成员，我们来看看其中最主要的几个成员：
>name : 该bus的名字，这个名字是这个bus在sysfs文件系统中的体现，对应/sys/bus/$name.  
>dev_name : 这个dev_name并不对应bus的名称，而是对应bus所包含的struct device的名字，即对应dev_root。  
>dev_root：bus对应的device结构，每个设备都需要对应一个相应的struct device.
>match:bus的device链表和driver链表进行匹配的实际执行回调函数，每当有device或者driver添加到bus中时，调用match函数，为device(driver)寻找匹配的driver(device)。  
>uevent:bus时间回调函数，当属于这个bus的设备发生添加、删除、修改等行为时，都将出发uvent事件。
>probe:当device和driver经由match匹配成功时，将会调用总线的probe函数实现具体driver的初始化。事实上每个driver也会提供相应的probe函数，先调用总线的probe函数，在总线probe函数中调用driver的probe函数。    
>remove:移除挂载在设备上的driver，bus上的driver部分也会提供remove函数，在执行移除时，先调用driver的remove，然后再调用bus的remove以清除资源。  
>struct subsys_private *p：见下文  

## struct subsys_private *p
struct subsys_private *p主要实现了对bus中数据的管理：

    struct subsys_private {
        struct kset subsys;
        struct kset *devices_kset;

        struct kset *drivers_kset;
        struct klist klist_devices;
        struct klist klist_drivers;
        ...
    };
其中struct kset subsys、struct kset *devices_kset、struct kset *drivers_kset都是在sysfs文件系统中创建对应的目录。  

struct klist klist_devices、struct klist klist_drivers是两个主要的数据部分，klist_devices是存储所有注册到bus的device的链表，而klist_drivers是存储所有注册到bus的driver的链表。  
        

## bus的注册
了解了bus的结构，那么，bus是怎么注册的呢？   

spi bus的注册过程在KERNEL/drivers/spi/spi.c中：

    static int __init spi_init(void)
    { 
        ...
        status = bus_register(&spi_bus_type);
        ...
        return 0;
    }

    postcore_initcall(spi_init);

i2c bus的注册过程在KERNEL/drivers/i2c/i2c-core-base.c中：


    static int __init i2c_init(void)
    {
        ...
        bus_register(&i2c_bus_type);
        ...
    }
    postcore_initcall(i2c_init);


而platform bus的注册过程在KERNEL/drivers/base/platform.c中：

    int __init platform_bus_init(void)
    {
        ...
        bus_register(&platform_bus_type);
        ...
    }

i2c和spi为物理总线，这两种总线通过postcore_initcall()将各自的init函数注册到系统中，postcore_initcall的详解可以参考另一篇博客：[linux init机制](https://www.cnblogs.com/downey-blog/p/10486653.html)。  

而platform作为虚拟总线，platform_bus_init被系统初始化时直接调用，调用流程为：

    start_kernel  
    -> rest_init();
        -> kernel_thread(kernel_init, NULL, CLONE_FS);
            -> kernel_init()
                -> kernel_init_freeable();
                    -> do_basic_setup();
                        -> driver_init();  
                            ->platform_bus_init();

spi_bus_type、i2c_bus_type、platform_bus_type分别为对应的struct bus_type描述结构体。  

对应的spi_bus_type、i2c_bus_type、platform_bus_type实现我将分别在spi、i2c、platform具体框架解析中介绍。  


## bus_register()
可以看到，这三种总线都是由bus_register()接口注册的，那么这个接口到底做了什么呢？  

    int bus_register(struct bus_type *bus)
    {
        int retval;
        struct subsys_private *priv;
        struct lock_class_key *key = &bus->lock_key;

        priv = kzalloc(sizeof(struct subsys_private), GFP_KERNEL);
        if (!priv)
            return -ENOMEM;

        priv->bus = bus;
        bus->p = priv;

        BLOCKING_INIT_NOTIFIER_HEAD(&priv->bus_notifier);

        retval = kobject_set_name(&priv->subsys.kobj, "%s", bus->name);
        if (retval)
            goto out;

        priv->subsys.kobj.kset = bus_kset;
        priv->subsys.kobj.ktype = &bus_ktype;
        priv->drivers_autoprobe = 1;

        retval = kset_register(&priv->subsys);
        if (retval)
            goto out;

        retval = bus_create_file(bus, &bus_attr_uevent);
        if (retval)
            goto bus_uevent_fail;

        priv->devices_kset = kset_create_and_add("devices", NULL,
                            &priv->subsys.kobj);
        if (!priv->devices_kset) {
            retval = -ENOMEM;
            goto bus_devices_fail;
        }

        priv->drivers_kset = kset_create_and_add("drivers", NULL,
                            &priv->subsys.kobj);
        if (!priv->drivers_kset) {
            retval = -ENOMEM;
            goto bus_drivers_fail;
        }

        INIT_LIST_HEAD(&priv->interfaces);
        __mutex_init(&priv->mutex, "subsys mutex", key);
        klist_init(&priv->klist_devices, klist_devices_get, klist_devices_put);
        klist_init(&priv->klist_drivers, NULL, NULL);

        retval = add_probe_files(bus);
        if (retval)
            goto bus_probe_files_fail;

        retval = bus_add_groups(bus, bus->bus_groups);
        if (retval)
            goto bus_groups_fail;

        pr_debug("bus: '%s': registered\n", bus->name);
        return 0;

    }

在上面贴出的代码中，可以看出，bus_register()其实也没做什么特别的事，主要是两个：
* 在sysfs系统中注册各种用户文件接口，将bus的信息和操作接口导出到用户接口。  
* 初始化device和driver链表。  


## 向总线中添加driver/device
既然bus_register只是初始化了相应的资源，在/sys下导出接口文件，那整个bus是如何工作的呢？  

以i2c为例，我们来看看这整个过程：

首先使用i2c_new_device接口来添加一个device：

    struct i2c_client *i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
    {
        struct i2c_client	*client;
        client = kzalloc(sizeof *client, GFP_KERNEL);
        client->adapter = adap;

        client->dev.platform_data = info->platform_data;
        if (info->archdata)
            client->dev.archdata = *info->archdata;
        client->flags = info->flags;
        client->addr = info->addr;

        client->irq = info->irq;
        client->dev.parent = &client->adapter->dev;
        client->dev.bus = &i2c_bus_type;
        client->dev.type = &i2c_client_type;
        client->dev.of_node = info->of_node;
        client->dev.fwnode = info->fwnode;
        ...
        device_register(&client->dev);
        ...
    }
申请一个i2c_client并对其赋值，然后以这个为参数调用device_register(&client->dev)，将dangqiandevice添加到bus中。  

    int device_register(struct device *dev)
    {
        device_initialize(dev);
        return device_add(dev);
    }

    int device_add(struct device *dev)
    {
        ...
         bus_add_device(dev);
        ...
    }
    int bus_add_device(struct device *dev){
        ...
        klist_add_tail(&dev->p->knode_bus, &bus->p->klist_devices);
        ...
        bus_probe_device(dev);
        ...
    }

在device_register中，调用了device_add,紧接着调用bus_add_device，根据函数名称可以看出是将这个device添加到对应的bus中。  

果然，根据bus_add_device()的源代码，可以看到，将当前device链接到其对应bus的devices链表，然后在下面调用bus_probe_device();这个函数的作用就是轮询对应bus的drivers链接，查看新添加的device是否存在匹配的driver。  

对应的，i2c_driver_register()将i2c driver部分添加到bus中，再轮询检查bus的devices链表是否有对应的device能匹配上,有兴趣的可以从i2c_driver_register()开始研究源代码。  



## device和driver的匹配
上文中提到当bus中有新的device和driver添加时，会调用bus的match函数进行匹配，那么到底是怎么匹配的呢？  

简单来说，在静态定义的device中，一般会有.name属性，与driver的.id_table属性相匹配。  

device部分还有可能从设备树转换而来，就有设备树中相应的.compatible属性和driver的of_match_table.compatible属性相匹配。  

事实上对于匹配这一部分，可以直接参考每个bus的match函数实现。  

这一章节只是对linux中的总线做一个概念性的说明，在之后的博客中会详细介绍到相应bus的框架，同时也会详解对应的match()函数实现。  

敬请期待！


好了，关于linux的bus讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.


    




