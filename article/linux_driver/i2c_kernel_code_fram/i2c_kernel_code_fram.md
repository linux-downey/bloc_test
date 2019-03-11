# linux设备驱动程序-i2c(2)-adapter和设备树的解析

**(注： 基于beagle bone green开发板，linux4.14内核版本)**    

在本系列linux内核i2c框架的前两篇，分别讲了：  
[linux设备驱动程序-i2c(0)-i2c设备驱动源码实现](https://www.cnblogs.com/downey-blog/p/10493289.html)    
[linux设备驱动程序-i2c(1):i2c总线的添加与实现](https://www.cnblogs.com/downey-blog/p/10493216.html)  

而在[linux设备驱动程序--串行通信驱动框架分析](https://www.cnblogs.com/downey-blog/p/10491953.html)中，讲到linux内核中串行通信驱动框架大体分为三层：
* 应用层(用户空间接口操作)
* 驱动层(包含总线、i2c-core的实现、以及总线的device和driver部分)
* i2c硬件读写层

在上一章节我们讲了整个总线的实现以及device和driver的匹配机制，这一章节我们要来讲讲i2c硬件读写层的实现。  

## i2c的适配器
我们来回顾一下，在本系列文章的第一章[linux设备驱动程序-i2c(0)-i2c设备驱动源码实现](https://www.cnblogs.com/downey-blog/p/10493289.html)源码中是怎么使用i2c和设备进行通信的呢？  
* 首先，在总线的device部分，使用
    struct i2c_adapter *adap = i2c_get_adapter(2)
    这个接口，获取一个struct i2c_adapter结构体指针，并关联到i2c_client中。  
* 然后，在总线driver部分的probe部分，在/dev目录下创建文件，并关联对应的file_operations结构体。  
* 在file_operations结构体的write函数中，使用
    s32 i2c_smbus_write_byte_data(const struct i2c_client *client,u8 command,u8 value);
    这个接口，直接向i2c设备中写数据(command和value)。  
* 而这个i2c_client就是device源码部分注册到bus中的i2c_client，且包含对应的adapter，同时包含i2c地址，设备名等信息。  

如果再往深挖一层，会发现i2c_smbus_write_byte_data()的源码实现是这样的：

    s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command,
			      u8 value)
    {
        union i2c_smbus_data data;
        data.byte = value;
        return i2c_smbus_xfer(client->adapter, client->addr, client->flags,
                    I2C_SMBUS_WRITE, command,
                    I2C_SMBUS_BYTE_DATA, &data);
    }
    EXPORT_SYMBOL(i2c_smbus_write_byte_data);

可以看到，在i2c smbus中主导通信的就是这个adapter。  

那么，这个i2c_adapter到底是什么东西呢？其实，完全可以把它看成对应一个物理上i2c硬件控制器。

## 硬件i2c控制器
什么是硬件i2c控制器呢？为了照顾基础薄弱的同学，博主还是讲一讲吧。  


用户编程并不需要操作i2c adapter。


## 从设备树开始

首先需要了解的是：系统在开始启动时，bootloader将设备树在内存中的开始地址传递给内核，内核开始对设备树进行解析，将设备树中的子节点转换成struct device_node节点，再由struct device_node节点转换成struct platform_device节点，如果此时在系统中存在对应的struct platform_driver节点，则调用driver驱动程序中的probe函数，在probe函数中进行一系列的初始化。   

## struct i2c_adapter的注册
正如我在之前章节中所说的，每一个struct i2c_adapter描述一个硬件i2c控制器，其中包含了对应的硬件i2c控制器的数据收发，同时，每一个struct i2c_adapter都直接集成在系统中，而不需要驱动开发者去实现(除非做一款全新芯片的驱动移植)，那么，这个i2c adapter是怎样被注册到系统中的呢？  

在beagle bone green这块开发板中，有三个i2c控制器：i2c0~i2c2，我们以i2c0为例，查看系统的设备树文件，可以找到对i2c0的描述：

    __symbols__ {
        i2c0 = "/ocp/i2c@44e0b000";
    }
    ...
    i2c@44e0b000 {
			compatible = "ti,omap4-i2c";
            ...
            baseboard_eeprom@50 {
				compatible = "atmel,24c256";
				reg = <0x50>;
				#address-cells = <0x1>;
				#size-cells = <0x1>;
				phandle = <0x282>;
				baseboard_data@0 {
					reg = <0x0 0x100>;
					phandle = <0x23c>;
				};
			};
    }
    ...

可以看到，i2c0对应的compatible为"ti,omap4-i2c"，如果你有了解过linux总线的匹配规则，就知道总线在对driver和device进行匹配时依据compatible字段进行匹配(当然会有其他匹配方式，但是设备树主要使用这一种方式)，依据这个规则，在整个linux源代码中搜索"ti,omap4-i2c"这个字段就可以找到i2c0对应的driver文件实现了。  

在i2c-omap.c中找到了这个compatible的定义：

    static const struct of_device_id omap_i2c_of_match[] = {
        {
            .compatible = "ti,omap4-i2c",
            .data = &omap4_pdata,
        },
        ...
    }

同时，根据platform driver驱动的规则，需要填充一个struct platform_driver结构体，然后注册到platform总线中，因此，我们也可以在同文件下找到相应的初始化部分：

    static struct platform_driver omap_i2c_driver = {
        .probe		= omap_i2c_probe,
        .remove		= omap_i2c_remove,
        .driver		= {
            .name	= "omap_i2c",
            .pm	= OMAP_I2C_PM_OPS,
            .of_match_table = of_match_ptr(omap_i2c_of_match),
        },
    };

    static int __init omap_i2c_init_driver(void)
    {
        return platform_driver_register(&omap_i2c_driver);
    }

既然platform总线的driver和device匹配上，就会调用相应的probe函数，根据.probe = omap_i2c_probe,我们再查看omap_i2c_probe函数：

    static int omap_i2c_probe(struct platform_device *pdev)
    {
        ...    //get resource from dtb node
        ...    //config i2c0 via set corresponding regs
        i2c_add_numbered_adapter(adap);
        ...    //deinit part
    }

在probe函数中我们找到一个i2c_add_numbered_adapter()函数，再跟踪代码到i2c_add_numbered_adapter()：

    int i2c_add_numbered_adapter(struct i2c_adapter *adap)
    {
        ...  //assert part
        return __i2c_add_numbered_adapter(adap);
    }
根据名称可以隐约猜到了，这个函数的作用是添加一个i2c adapter到系统中，接着看：

    static int __i2c_add_numbered_adapter(struct i2c_adapter *adap)
    {
        ...
        return i2c_register_adapter(adap);
    }

看到这里，整个i2c adapter的注册就已经清晰了，首先在设备树中会有对应的硬件i2c控制器子节点，在系统启动时，系统将设备节点转换成struct platform_device节点，与系统中预置的struct platform_driver相匹配，调用struct platform_driver驱动部分的probe函数，完成一系列的初始化和设置，生成一个i2c adapter，注册到系统中。  

当用户需要使用此adapter时，就可以直接通过索引number来获取这个adapter，进行数据通信。  


## i2c总线的初始化
i2c总线也是集成在系统中的，驱动开发者不需要关心i2c总线的初始化，只需要将相应的i2c_client和i2c_driver挂载在总线上进行匹配即可。  
那么，i2c总线在系统中是如何初始化得来的呢？   

答案就在文件i2c-core-base.c中，它的过程是这样的：

    static int __init i2c_init(void)
    {
        ...
        bus_register(&i2c_bus_type);
        ...
    }  

从函数名可以看出，将i2c_bus_type注册到bus系统中，我们再来看看i2c_bus_type是何方神圣：


    struct bus_type {
        ...
        int (*match)(struct device *dev, struct device_driver *drv);
        int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
        int (*probe)(struct device *dev);
        int (*remove)(struct device *dev);
        void (*shutdown)(struct device *dev);
        int (*online)(struct device *dev);
        int (*offline)(struct device *dev);
        int (*suspend)(struct device *dev, pm_message_t state);
        int (*resume)(struct device *dev);

        struct subsys_private *p;
        ...
    };

    struct bus_type i2c_bus_type = {
        .name		= "i2c",
        .match		= i2c_device_match,
        .probe		= i2c_device_probe,
        .remove		= i2c_device_remove,
        .shutdown	= i2c_device_shutdown,
    };
当这个模块被加载进系统时，就会执行i2c_init函数来进行初始化。  

struct bus_type结构体描述了linux中的各种bus，比如spi bus，i2c bus，platform bus等等，可以看到，在i2c_bus_type中，定义了match(),probe(),remove()和shutdown()函数，match()函数就是当有新的i2c_client或者i2c_driver添加进来时，试图寻找与新设备匹配的项，返回匹配的结果。  

remove()和shutdown()，顾名思义，这两个函数是管理的驱动移除行为的。    

而对于probe函数，在之前的章节中提到：当相应的device和driver经由总线匹配上时，会调用driver部分提供的probe函数。 

那么：      
* ***总线的probe函数和具体驱动driver部分的probe函数是怎么一个关系呢？***
* ***match函数和probe函数到底是怎么被调用的，以及执行一些什么行为呢？***  

带着这两个疑问，我们接着往下看。  

## match()和probe()
在这里我们不免要回顾一下前面章节所说的，作为一个驱动开发者，如果我们要开发某些基于i2c的设备驱动，需要实现的框架流程是怎样的：

* 填充一个i2c_driver结构体，如果以设备树方式匹配，需要填充of_match_table部分，如果是其他总线方式匹配，需要填充.driver.name或者.id_table属性，然后提供一个.probe函数以及一个remove函数，probe中对设备进行操作，remove负责回收资源。 将i2c_driver注册到i2c总线中。  
* 如果是设备树形式，提供相应的设备树节点。 如果是总线匹配形式，提供一个i2c_client部分注册到i2c总线中。  
* 当双方注册到i2c总线时，device(i2c_client)和driver部分匹配，调用driver提供的probe函数。  
linux下的标准总线方式的驱动框架，但是问题是，为什么device和driver都注册进去之后，就会调用driver部分提供的probe函数呢？  

## 走进源代码
为了解决这些问题，最好的办法就是看源代码，假设我们是以设备树的方式进行匹配，device(即i2c_client)部分已经被注册到系统中，此时我们向系统中注册(insmod由driver部分程序编译的模块)相应的driver部分：
    ...
    static struct i2c_driver drv = {
        ...
        .probe = drv_probe,
        .remove = drv_remove,
        .id_table = drv_id_table,
    };
    static __init xxx_init(){
        i2c_add_driver(&drv);
    }
    ...

跟踪i2c_add_driver：

    #define i2c_add_driver(driver)  i2c_register_driver(THIS_MODULE, driver)
    int i2c_register_driver(struct module *owner, struct i2c_driver *driver)
    {
        ...
        driver->driver.bus = &i2c_bus_type;  //设置当前driver的bus类为i2c_bus_type
        driver_register(&driver->driver);
        ...
    }
    int driver_register(struct device_driver *drv)
    {
        ...
        bus_add_driver(drv);
        ...
    }

经过一系列的代码跟踪，找到了bus_add_driver()，根据名称可以知道，这个函数就是将当前i2c_driver添加到i2c_bus_type(即i2c总线)中。  

接着往下看：

    int bus_add_driver(struct device_driver *drv)
    {
        ...
        struct bus_type *bus;
        struct driver_private *priv;
        bus = bus_get(drv->bus);            //获取的bus为上一节中介绍的i2c_bus_type
        priv->driver = drv;
        klist_add_tail(&priv->knode_bus, &bus->p->klist_drivers);
        ...
        ...
        driver_attach(drv);
        ...
    }

从第一部分可以看到，将当前drv链入到bus->p->klist_drivers链表中，那么可以猜到，在注册device部分的时候，就会将device链入到bus->p->klist_devices中。  

然后，再看driver_attach(drv):

    int driver_attach(struct device_driver *drv)
    {
        return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
    }

    int bus_for_each_dev(struct bus_type *bus, struct device *start,void *data, int (*fn)(struct device *, void *))
    {
        ...
        while ((dev = next_device(&i)) && !error)
            error = fn(dev, data);
        ...
    }
    static int __driver_attach(struct device *dev, void *data)
    {
        ...
        int ret;
        struct device_driver *drv = data;
        ret = driver_match_device(drv, dev);
        check_ret();
        driver_probe_device(drv, dev);
        ...
    }
driver_attach调用bus_for_each_dev，传入当前驱动bus(即i2c_bus_type)，当前驱动drv，以及一个函数__driver_attach。  

bus_for_each_dev对每个device部分调用__driver_attach。  

在__driver_attach中，对每个drv和dev调用driver_match_device()，并根据函数返回值决定是否继续执行调用driver_probe_device()。  

从函数名来看，这两个函数大概就是我们在前文中提到的match和probe函数了，我们不妨再跟踪看看，先看看driver_match_device()函数：

    static inline int driver_match_device(struct device_driver *drv,struct device *dev)
    {
        return drv->bus->match ? drv->bus->match(dev, drv) : 1;
    }

果然不出所料，这个函数十分简单，如果当前驱动的所属的bus有相应的match函数，就调用match函数，否则返回1.  

当前driver所属的总线为i2c_bus_type，根据上文i2c总线的初始化部分可以知道，i2c总线在初始化时提供了相应的match函数，所以，总线的match函数被调用，并返回匹配的结果。  

接下来我们再看看driver_probe_device()函数：

    int driver_probe_device(struct device_driver *drv, struct device *dev)
    {
        ...
        really_probe(dev, drv);
        ...
    }
    static int really_probe(struct device *dev, struct device_driver *drv)
    {
        ...
        if (dev->bus->probe) {
		ret = dev->bus->probe(dev);
		if (ret)
			goto probe_failed;
        } 
        else if (drv->probe) {
            ret = drv->probe(dev);
            if (ret)
                goto probe_failed;
        }
        ...
    }
在driver_probe_device中又调用了really_probe，在really_probe()中，先判断当前bus总线中是否注册probe()函数如果有注册，就调用总线probe函数，否则，就调用当前drv注册的probe()函数。  

到这里，我们解决了上一节中的一个疑问：总线和driver部分都有probe函数时，程序是怎么处理的？  

答案已经显而易见了：优先调用总线probe函数。   

而且我们理清了总线的match()函数和probe()函数是如何被调用的。  

*** 那么，上一节中还有一个疑问没有解决：总线的match和probe函数执行了一些什么行为？ ***
### 总线probe函数
我们直接根据i2c总线初始化时设置的probe函数来查看：

    struct bus_type i2c_bus_type = {
        .match		= i2c_device_match,
        .probe		= i2c_device_probe,
    };
    static int i2c_device_probe(struct device *dev)
    {
        ...
        struct i2c_driver	*driver;
        driver = to_i2c_driver(dev->driver);
        if (driver->probe_new)
            status = driver->probe_new(client);
        else if (driver->probe)
            status = driver->probe(client,
                        i2c_match_id(driver->id_table, client));
        else
            status = -EINVAL;
        ...
    }
对于总线probe函数，获取匹配成功的device，再由device获取driver，优先调用driver->probe_new,因为driver中没有设置，直接调用driver的probe函数。  

总线probe和driver的probe函数的关系就是：当match返回成功时，优先调用总线probe，总线probe中再调用driver的probe函数，如果总线probe函数不存在，就直接调用driver的probe函数，所以当我们在系统中添加了相应的device和driver部分之后，driver的probe函数就会被调用。  

### 总线match函数 
对于总线match函数，我们直接查看在i2c总线初始化时的函数定义：

    struct bus_type i2c_bus_type = {
        .match		= i2c_device_match,
        .probe		= i2c_device_probe,
    };

    static int i2c_device_match(struct device *dev, struct device_driver *drv)
    {
        struct i2c_client	*client = i2c_verify_client(dev);
        struct i2c_driver	*driver;

        if (i2c_of_match_device(drv->of_match_table, client))
            return 1;

        if (acpi_driver_match_device(dev, drv))
            return 1;

        driver = to_i2c_driver(drv);

        if (i2c_match_id(driver->id_table, client))
            return 1;

        return 0;
    }
i2c_device_match就是i2c_driver与i2c_device匹配的部分，在i2c_device_match函数中，可以看到，match函数并不只是提供一种匹配方式：  

* i2c_of_match_device，看到of我们就应该马上意识到这是设备树的匹配方式。  
* acpi_driver_match_device则是acpi的匹配方式，acpi的全称为the Advanced Configuration & Power Interface，高级设置与电源管理。  
*  i2c_match_id(),通过注册i2c_driver时提供的id_table进行匹配，这是设备树机制产生之前的主要配对方式。  

接下来再深入函数内部，查看匹配的细节部分：  

### 设备树匹配方式
    const struct of_device_id *i2c_of_match_device(const struct of_device_id *matches,
                struct i2c_client *client)
    {
        const struct of_device_id *match;
        match = of_match_device(matches, &client->dev);
        if (match)
            return match;

        return i2c_of_match_device_sysfs(matches, client);
    }

在i2c_of_match_device中，调用了of_match_device()和i2c_of_match_device_sysfs()两个函数，这两个函数代表了两种匹配方式，先来看看of_match_device：  

    const struct of_device_id *of_match_device(const struct of_device_id *matches,const struct device *dev)
    {
        if ((!matches) || (!dev->of_node))
            return NULL;
        return of_match_node(matches, dev->of_node);
    }
    const struct of_device_id *of_match_node(const struct of_device_id *matches,const struct device_node *node)
    {
        const struct of_device_id *match;
        match = __of_match_node(matches, node);
        return match;
    }
    static const struct of_device_id *__of_match_node(const struct of_device_id *matches,const struct device_node *node)
    {
        ...
        const struct of_device_id *best_match = NULL;
        for (; matches->name[0] || matches->type[0] || matches->compatible[0]; matches++) {
            __of_device_is_compatible(node, matches->compatible,
                            matches->type, matches->name);
            ...
        }
        ...
    }
of_match_device调用of_match_node。  

of_match_node调用__of_match_node函数。  

如果你对设备树有一定的了解，就知道系统在初始化时会将所有设备树子节点转换成由struct device_node描述的节点。  

在被调用的__of_match_node函数中，对device_node中的compatible属性和driver部分的of_match_table中的compatible属性进行匹配，由于compatible属性可以设置多个，所以程序中会对其进行逐一匹配。  

我们再回头来看设备树匹配方式中的i2c_of_match_device_sysfs()匹配方式：

    static const struct of_device_id*  i2c_of_match_device_sysfs(const struct of_device_id *matches,struct i2c_client *client)
    {
        const char *name;
        for (; matches->compatible[0]; matches++) {
            if (sysfs_streq(client->name, matches->compatible))
                return matches;
            name = strchr(matches->compatible, ',');
            if (!name)
                name = matches->compatible;
            else
                name++;
            if (sysfs_streq(client->name, name))
                return matches;
        }
        return NULL;
    }
由i2c_of_match_device_sysfs()的实现可以看出:当设备树中不存在设备节点时，程序试图去匹配i2c_client(device部分)的.driver.name属性，因为设备树的默认规则，compatible属性一般形式为"vender_id,product_id"，当compatible全字符串匹配不成功时，取product_id部分再进行匹配，这一部分主要是兼容之前的不支持设备树的版本。  


*** acpi匹配方式较为少用且较为复杂，这里暂且不做详细讨论 ***

## id_table匹配方式
同样的，我们查看i2c_match_id(driver->id_table, client)源代码：

    const struct i2c_device_id *i2c_match_id(const struct i2c_device_id *id,
						const struct i2c_client *client)
    {
        while (id->name[0]) {
            if (strcmp(client->name, id->name) == 0)
                return id;
            id++;
        }
        return NULL;
    }
这种匹配方式一目了然，就是对id_table(driver部分)中的.name属性和i2c_client(device部分)的.name属性进行匹配。那么i2c_client的.name是怎么来的呢？  

在非设备树的匹配方式中，i2c_client的.name属性由驱动开发者指定，而在设备树中，i2c_client由系统对设备树进行解析而来,i2c_client的name属性为设备树compatible属性"vender_id,product_id"中的"product_id",所以，在进行匹配时，默认情况下并不会严格地要求
of_match_table中的compatible属性和设备树中compatible属性完全匹配，driver中.drv.name属性和.id.name属性也可以与设备树中的"product_id"进行匹配。  
  

好了，关于linux i2c总线的match和probe机制的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

















