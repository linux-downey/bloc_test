# linux设备驱动程序--设备树多级子节点的转换
在上一章：[设备树处理之——device_node转换成platform_device](https://www.cnblogs.com/downey-blog/p/10486568.html)中，有提到在设备树的device_node到platform_device转换中，必须满足以下条件：
* 一般情况下，只对设备树中根的一级子节点进行转换，也就是多级子节点(子节点的子节点)并不处理。但是存在一种特殊情况，就是当某个根子节点的compatible属性为"simple-bus"、"simple-mfd"、"isa"、"arm,amba-bus"时，当前节点中的一级子节点将会被转换成platform_device节点。
* 节点中必须有compatible属性。

事实上，在设备树中，通常会存在将描述设备驱动的设备树节点被放置在多级子节点的情况，比如下面这种情况：

	/{
		...
		i2c@44e0b000 {
			compatible = "ti,omap4-i2c";
			...
			tps@24 {
				reg = <0x24>;
				compatible = "ti,tps65217";
				...
				charger {
					compatible = "ti,tps65217-charger";
					...
				};
				pwrbutton {
					compatible = "ti,tps65217-pwrbutton";
					...
				};
			}
		}
		...
	}
显然，i2c@44e0b000会被转换成platform_device,而tps@24、charger、pwrbutton则不会，至少在设备树初始化阶段不会被转换，仍旧以device_node的形式存在在内存中。  

显而易见，这些设备并非是无意义的设备，那么它们是什么时候生成platform_device的呢？  

答案是：由对应根目录的一级子节点处理。  

我们以i2c@44e0b000节点为例，事实上，这个节点对应一个i2c硬件控制器，控制器的起始地址是0x44e0b000，这个节点的作用就是生成一个i2c硬件控制器的platform_device,与同样被加载到内存中的platform_driver相匹配,在内存中构建一个i2c硬件控制器的描述节点，负责对应i2c控制器的数据收发。  

根据设备树的compatible属性匹配机制，在内核源代码中全局搜索，就可以找到与i2c@44e0b000设备节点对应的platform_driver部分：

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
i2c_register_adapter()，根据这个名称可以看出这是根据设备树描述的硬件i2c控制器而生成的一个i2c_adapter,并注册到系统中，这个i2c_adapter负责i2c底层数据收发。  

继续跟踪源码：

	static int i2c_register_adapter(struct i2c_adapter *adap)
	{
		...
		of_i2c_register_devices(adap);
		...
	}
注意到一个of前缀的函数，看到of就能想到这肯定是设备树相关的函数。  

	void of_i2c_register_devices(struct i2c_adapter *adap)
	{
		...
		for_each_available_child_of_node(bus, node) {
			if (of_node_test_and_set_flag(node, OF_POPULATED))
				continue;

			client = of_i2c_register_device(adap, node);
			if (IS_ERR(client)) {
				dev_warn(&adap->dev,
						"Failed to create I2C device for %pOF\n",
						node);
				of_node_clear_flag(node, OF_POPULATED);
			}
		}
		...
	}
这个函数的作用是轮询每个子节点，并调用of_i2c_register_device()，返回i2c_client结构体，值得注意的是，在i2c总线中，driver部分为struct i2c_driver，而device部分为struct i2c_client.  

所以可以看出，of_i2c_register_device()这个函数的作用就是解析设备树中当前i2c中的子节点，并将其转换成相应的struct i2c_client描述结构。  

不妨来验证一下我们的猜想：

	static struct i2c_client *of_i2c_register_device(struct i2c_adapter *adap,struct device_node *node)
	{
		...
		struct i2c_board_info info = {};
		of_modalias_node(node, info.type, sizeof(info.type);
		of_get_property(node, "reg", &len);
		info.addr = addr;
		info.of_node = of_node_get(node);
		info.archdata = &dev_ad;

		if (of_property_read_bool(node, "host-notify"))
			info.flags |= I2C_CLIENT_HOST_NOTIFY;

		if (of_get_property(node, "wakeup-source", NULL))
			info.flags |= I2C_CLIENT_WAKE;

		result = i2c_new_device(adap, &info);
		...
	}

	struct i2c_client * i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
	{
		...
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

		if (info->properties) {
			status = device_add_properties(&client->dev, info->properties);
			if (status) {
				dev_err(&adap->dev,
					"Failed to add properties to client %s: %d\n",
					client->name, status);
				goto out_err;
			}
		}
		device_register(&client->dev);
		return client;
		...
	}
从device_node到i2c_client的转换主要是在这两个函数中了，在of_i2c_register_device()函数中，从device_node节点中获取各种属性的值记录在info结构体中，然后将info传递给i2c_new_device()，i2c_new_device()生成一个对应的i2c_client结构并返回。  

到这里，不难猜测为什么在内核初始化时只将一级子目录节点(compatible属性中含有"simple-bus"、"simple-mfd"、"isa"、"arm,amba-bus"的向下递归一级)转换成platform_device,因为在linux中，将一级子节点视为bus，而多级子节点则由具体的bus去处理。  

同时，对于bus而言，有不同的总线处理方式和不同的driver、device的命名，自然不能将所有节点全部转换成platform_device.  

本文仅仅以i2c为例讲解设备树中多级子节点的转换，朋友们也可以按照这种从设备树出发的代码跟踪方式查看其它bus子节点的转换。  


好了，关于linux设备树多级子节点的转换的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言



***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.







