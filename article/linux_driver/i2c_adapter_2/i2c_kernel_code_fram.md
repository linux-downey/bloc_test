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
1、首先，在总线的device部分，使用
    
    struct i2c_adapter *adap = i2c_get_adapter(2)
这个接口，获取一个struct i2c_adapter结构体指针，并关联到i2c_client中。

2、然后，在总线driver的probe部分，在/dev目录下创建文件，并关联对应的file_operations结构体。  

3、在file_operations结构体的write函数中，使用
    
    s32 i2c_smbus_write_byte_data(const struct i2c_client *client,u8 command,u8 value);
这个接口，直接向i2c设备中写数据(command和value)。  

4、 而第三点中i2c_client就是device源码部分注册到bus中的i2c_client，且包含对应的adapter，同时包含i2c地址，设备名等信息。  

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

那么，这个i2c_adapter到底是什么东西呢？   

事实上，一个硬件i2c控制器由i2c_adapter描述。

## 硬件i2c控制器
硬件i2c控制器是一个可编程器件，用于生成i2c时序，实现数据收发，且维护收发buf，对外提供寄存器接口。  

硬件控制器这一类外设一般直接挂在CPU总线上，CPU可直接寻址访问。  

当主机需要通过i2c接口收发数据时，直接通过读写硬件i2c控制器寄存器即可，硬件控制器会将主机传送过来的数据自动完成发送，接收到的数据直接放在buf中供主机读取。    

## i2c_adapter的使用方式
**(注:在源码示例中，博主使用的i2c smbus的方式收发数据，为了讲解与理解的方便，这里i2c收发数据方式使用i2c_transfer接口，数据传输原理是一样的)。**  

在[linux设备驱动程序-i2c(0)-i2c设备驱动源码实现](https://www.cnblogs.com/downey-blog/p/10493289.html)源码中，用户只需要在驱动的device部分调用：

    struct i2c_adapter *adap = i2c_get_adapter(2)
获取一个i2c硬件控制器的描述结构体，然后在通信时以这个结构体为参数即可。  

而i2c_get_adapter()接口的参数为硬件i2c控制器的num，通常，一个单板上不止一个i2c控制器，这个num指定了i2c控制器的序号。  

在驱动程序源码实现中，并不需要i2c_adapter的相关实现，那么，可以确定的是，i2c底层数据收发已经集成到了系统中，只需要用户去选择使用哪一个adapter即可。  

那么，它到底是怎么工作的呢？  

办法很简单，继续跟踪源码即可，先看一下i2c数据发送函数：

数据的收发都基于同一个操作：先填充一个i2c_msg结构体，然后再使用i2c_tranfer函数发送数据。  

    struct i2c_msg xfer[2];
    xfer[0].addr = i2c->addr;
	xfer[0].flags = 0;
	xfer[0].len = reg_size;
	xfer[0].buf = (void *)reg;

	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_NOSTART;
	xfer[1].len = val_size;
	xfer[1].buf = (void *)val;
    i2c_transfer(i2c->adapter, xfer, 2);

然后跟踪i2c_transfer()的实现：

    int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
    {
        ...
        ret = __i2c_transfer(adap, msgs, num);
        ...
    }
    int __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
    {
        ...
        ret = adap->algo->master_xfer(adap, msgs, num);
        ...
    }
可以清楚地从源码中看到，事实上，真正的发送数据的函数是这一个：adapter->algo->master_xfer()，那么这个adapter->algo->master_xfer函数指针是怎么被初始化的呢？  

要了解这个，我们必须先了解一个硬件i2c控制器对应的i2c_adapter是怎么被添加到系统中的。  

## 从设备树开始
**(linux内核版本：4.14，基于beagle bone开发板)**
首先，系统在开始启动时，bootloader将设备树在内存中的开始地址传递给内核，内核开始对设备树进行解析，将设备树中的子节点(不包括子节点的子节点)转换成struct device_node节点，再由struct device_node节点转换成struct platform_device节点，如果此时在系统中存在对应的struct platform_driver节点，则调用driver驱动程序中的probe函数，在probe函数中进行一系列的初始化。   

## struct i2c_adapter的注册
正如前文所说，每一个struct i2c_adapter描述一个硬件i2c控制器，其中包含了对应的硬件i2c控制器的数据收发，同时，每一个struct i2c_adapter都直接集成在系统中，而不需要驱动开发者去实现(除非做芯片的驱动移植)，那么，这个i2c adapter是怎样被注册到系统中的呢？  

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

可以看到，i2c0对应的compatible为"ti,omap4-i2c"，如果你有了解过linux总线的匹配规则，就知道总线在对driver和device进行匹配时依据compatible字段进行匹配(当然会有其他匹配方式，但是设备树主要使用这一种方式)。  

依据这个规则，在整个linux源代码中搜索"ti,omap4-i2c"这个字段就可以找到i2c0对应的driver文件实现了。  

在i2c-omap.c(不同平台可能文件名不一样，但是按照上面从设备树开始找的方法可以找到对应的源文件)中找到了这个compatible的定义：

    static const struct of_device_id omap_i2c_of_match[] = {
        {
            .compatible = "ti,omap4-i2c",
            .data = &omap4_pdata,
        },
        ...
    }

同时，根据platform driver驱动的规则，需要填充一个struct platform_driver结构体，然后注册到platform总线中，这样才能完成platfrom bus的匹配，因此，我们也可以在同文件下找到相应的初始化部分：

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

看到这里，整个i2c adapter的注册就已经清晰了，首先在设备树中会有对应的硬件i2c控制器子节点，在系统启动时，系统将设备节点转换成struct platform_device节点。  

然后系统中注册好的struct platform_driver相匹配，调用struct platform_driver驱动部分的probe函数，完成一系列的初始化和设置，生成一个i2c adapter，注册到系统中。  


## adapter->algo->master_xfer的初始化
整个流程adapter的添加流程已经梳理完成，回到我们之前的问题：  

    用于实际通信中的adapter->algo->master_xfer函数指针是怎么被初始化的？

答案就在i2c适配器对应的platform driver驱动部分，i2c-omap.c文件中：

在platform driver对应的probe函数中:

    static int omap_i2c_probe(struct platform_device *pdev)
    {
        struct i2c_adapter	*adap;
        ...
        adap->algo = &omap_i2c_algo;
        r = i2c_add_numbered_adapter(adap);
        ...
    }
在这个函数中对adapter的algo元素进行赋值，接着看omap_i2c_algo：

    static const struct i2c_algorithm omap_i2c_algo = {
        .master_xfer	= omap_i2c_xfer,
        .functionality	= omap_i2c_func,
    };

找到了相应的.master_xfer成员，基本可以确定omap_i2c_xfer就是主机真正控制i2c收发数据的函数，adapter->algo->master_xfer指针就是指向这个函数：

    static int omap_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
    {
        ...
        omap_i2c_xfer_msg(adap, &msgs[i], (i == (num - 1)));
        ...
    }
继续跟踪omap_i2c_xfer_msg函数：

    static int omap_i2c_xfer_msg(struct i2c_adapter *adap，struct i2c_msg *msg, int stop)
    {
        ...
        omap_i2c_write_reg(omap, OMAP_I2C_CNT_REG,omap->buf_len);
        ...
        omap_i2c_write_reg(omap, OMAP_I2C_BUF_REG, w);
        ...
    }
从部分成员可以看出，adapter->algo->master_xfer指针指向函数的实现就是操作i2c硬件控制器实现i2c的读写，这一部分不再细究，对应芯片手册的部分。    

到这里，adapter的初始化与注册到系统的流程就完成了。  




好了，关于linux i2c总线的adapter注册的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

















