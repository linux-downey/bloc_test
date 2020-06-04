# linux 设备树节点的引用
在设备树中，硬件以树状的层次被映射成设备树中的节点，从根开始，分别引出各级总线，有对应真实的物理总线(如i2c、spi等)，也有专门为了统一设备模型而引入的虚拟总线(platform bus)，所有连接在系统总线上的设备都被抽象成节点，尽管某些部分比如定时器、时钟、gpio，在实际的开发过程中，我们并不把它们看成总线，但是对于 linux 系统而言，对这些同样挂载在系统总线上的硬件资源进行统一的抽象，方便进行管理。   

虽然，从设备树的结构来看，各个节点之间层次分明，井水不犯河水。但是从系统的角度来看，各个硬件之间的耦合是必然存在的，最常见的就是：大多数外设都需用使用到时钟部分，引脚的 pinmux 复用。　　

那么，既然软件是基于硬件的抽象，那么这种耦合肯定就会体现在软件上，毕竟，在 linux 中，软件的模型总归是建立在硬件的拓补上的。  

这种关联体现在设备树上就是节点之间的引用，理清这些节点引用对于熟悉整个平台的硬件架构是至关重要的，一个最明显的例子就是我们甚至可以仅通过设备树画出整个系统的时钟树。　　

****

## 引用语法
### 以 gpio 引用为例引出设备树的引用语法
在设备树中，我们经常可以看到这样的内容：

```
gpiobank1:gpio@0x800000{
    gpio-controller;
	#gpio-cells = <2>;
    ...
}
devicectl{
    ...
    gpio_enable = <&gpiobank1 GPIO1 GPIO_ACTIVE_HIGH>;
    ...
}
```
这就是设备树中对于 gpio 模块的引用，在某些模块的使用时，可能用到一些指示灯或者 reset 引脚，需要额外地去操作 gpio，就需要在当前节点中指定需要使用的 gpio，然后在驱动程序中或者设备树中指定的设备信息进行设置。  

在上述的示例中，可以通过 devicectl 节点看出，该节点引用 gpiobank1 的 GPIO1 对应的引脚，将其设置为高，第一个值指定了需要引用的设备树节点，后两个值指定参数，那么，需要引用 gpio 需要多少个参数是由什么决定的呢？  

答案是通过对应 gpiobank(gpio 控制器，提供gpio引脚的输入输出功能，通常一个 gpio 控制器提供 32 个引脚)，也就是 gpiobank1 的 #gpio-cells 节点来决定，在上述示例中，该值为2，所以引用的时候需要两个额外的参数来确定。　　

这其实很好理解，作为 gpio 功能的提供者，控制规则自然是由它来制定。  

仅对于 gpio 而言，一般是两个参数，第一个是 gpio num,或者说是 bank 的偏移值，第二个参数则通常是需要设置的 gpio 状态，上下拉、开漏浮空或者是电平状态。  

****  

### 其他的引用
gpio 的引用算是设备树中非常常见的了，毕竟很多硬件外设都有额外 gpio 的控制需求,相对于 gpio 而言，更为常用的是 pinmux 和 clock.  

### pinmux

对于 CPU 而言，因为成本或者是集成度的关系，通常外设的数量比较多，而 io 引脚的数量并不会很多，在大多数时候一个引脚支持被配置成不同的功能，一个引脚可能是 i2c 的 scl，同时也是 spi 的 miso，当我们需要使用 i2c 或者 spi 时，就需要对其进行相应的配置，前者配置成输出，后者配置成输入(实际情况比这个要复杂)，对应的外设功能才能正常使用。   

这就是设置引脚的 pinmux，所以大部分节点都会有相应的 pinmux 引用，它的形式通常是这样的：

```
devgrp1:grp1{
    VID,pins = <GPIOn gpio_stat>;
};
devgrp2:grp2{
    VID,pins = <GPIOn gpio_stat>;
};
devctrl{
    ...
    pinctrl-0 = <&devgrp1>;
    pinctrl-1 = <&devgrp2>;
    pinctrl-name = "default","idle";
};

```
其中 qinctrl-n 就是 pinmux 的引用，至于为什么会有 pinctrl-0/1/2... 多个配置呢？那是因为在不同的状态下需要将引脚设置成不同的模式，比如正常使用的模式下需要配置成对应接口，而外设挂起或者睡眠时需要将其配置成省电模式，pinctrl-names 就是一一对应了 pinctrl-n 的状态名称。  

****

### clock引用
无论在什么平台上，时钟系统都是必不可少的部分，每一个外设的运行都需要提供时钟，所以，通常情况下，外设的运行都需要对时钟进行配置。  

一般情况下，整个系统的时钟由时钟管理模块统一进行注册和管理，各个外设的驱动程序在需要使用的时候就向系统申请时钟即可，请求接口一般是这样：

```
struct clk *clk_get(struct device *dev, const char *con_id);
```

从系统获取时钟有两种方式：根据 device 获取和根据 con_id 获取。  

这样实现的原理是： 

每个外设都会对应一个设备节点 struct device，根据对应的 device 完全可以找到对应的预先关联的 clk。  

在注册时钟的时候，某些平台会静态地定义一个时钟树表，将时钟节点与某个唯一的静态字符串相关联，这样只需要提供字符串就可以获得相应的 clk。

所以，获取时钟可以是这样的两种接口：

```
    struct *clk = clk_get(dev,NULL);
    struct *clk = clk_get(NULL,"sysclk");
```

然后使用下面的接口进行操作：

```
    clk_enable(clk);
    clk_prepare(clk);
    clk_set_rate(clk);
```

尽管所有外设的运行都需要时钟，但并不是每个外设的运行都需要在设备树单独配置时钟，这依赖于平台的实现，某些平台直接以静态表的方式来定义时钟树，而某些则使用设备树引用的方式来操作时钟，使用设备树的方式越来越受欢迎，毕竟可以做到更灵活地配置。  
 
接下来我们就来看看设备树中时钟接口的引用：

```
clk_sys1:clk@8300000{
    clock-cells = <0>;  //通常只需要指定时钟源即可，设置时钟频率通过其他方法
    frequency = <16000000>;  //16M
};

clk_sys2:clk@8400000{
    clock-cells = <0>;  
    frequency = <24000000>;  
};

devctrl{
    clocks = <&clk_sys1 &clk_sys2>
};
```
上述示例指示了 devctrl 有两个时钟源 clk_sys1 和 clk_sys2，至于运行时使用哪个时钟，则是驱动中进行配置。  

至于其他的比如 pwm、audio之类，大家可以去研究研究设备树，每个硬件平台的设备树都有或大或小的差异，毕竟，设备树就是为了描述硬件差异而存在的。  

****

## 内核对引用的处理
在日常的开发中，当我们需要引用另一个设备树节点时，我们只需要添加对应的设备树引用属性即可，比如 led、button、clock 引用，修改的时候也只需要在其上进行修改即可达到目的。  

这造成了一个错觉：似乎引用节点的解析是包含在设备树解析阶段的。  

事实上，这是 linux 各种成熟的框架带给我们各种便利，而引用本身的解析是在对应外设的 probe 函数进行的。下面就以两个示例来说明内核对引用的处理：

****

### pinctrl 的处理
对于　pinctrl 而言，我们可以看下面的示例，这是 i.mx8 平台的某一个 i2c 节点的处理:

```
pinctrl_i2c1: i2c1grp {
    fsl,pins = <
        MX8MQ_IOMUXC_I2C1_SCL_I2C1_SCL  0x4000007f
        MX8MQ_IOMUXC_I2C1_SDA_I2C1_SDA  0x4000007f
                >;
};
pinctrl_i2c1_idle: i2c1grp {
    fsl,pins = <
        MX8MQ_IOMUXC_I2C1_SCL_I2C1_SCL  0x40000010
        MX8MQ_IOMUXC_I2C1_SDA_I2C1_SDA  0x40000010
                >;
};

i2c@30a20000 {
    ...
    compatible = "fsl,imx21-i2c";
    pinctrl-names = "default","idle";
    pinctrl-0 = <&pinctrl_i2c1>;
    pinctrl-1 = <&pinctrl_i2c1_idle>;
    ...
}
```

在上述的整个示例中，i2c 需要配置对应的两个引脚为 i2c 功能，pinctrl_i2c1 为配置节点，需要注意的是，在设备树中，这一类节点仅仅是定义，只有在相应的硬件节点中被引用，这个节点的配置才会被相应的驱动使能。如果仅仅是定义，对引脚的 pinmux 配置是不会产生任何影响的。　　
                
紧接着，在 i2c 节点中引用了 pinctrl_i2c1 的配置，这个 pinmux 设置的调用流程如下(因为本章主要针对节点引用，就不对过程代码进行解析了，有兴趣的朋友可以自己跟一跟)：  

```
i2c_imx_probe()  
    i2c_imx_init_recovery_info()
        devm_pinctrl_get()
            pinctrl_get()
                create_pinctrl()
                    //在该函数中，创建了pinux引用与引脚设置状态的关联，比如 default 对应 i2c 正常模式，idle 对应休眠模式
                    pinctrl_dt_to_map()  
```

流程解析：
* 首先根据设备树中的 compatible 属性，找到对应源码的 probe 函数。
* 一直追踪函数到 create_pinctrl/pinctrl_dt_to_map,创建 i2c 设备(pinctrl-0/1)  与 引脚pinmux设置(pinctrl_i2c1) 节点的 的绑定，一旦建立绑定，在 i2c 的开关控制函数(attach 和 dettach)中分别调用 default 和 idle 对应的引脚设置。  

****

### 对 gpio 引用的处理

设备树中 leds 节点是 linux 中常用的控制 gpio 输出的框架，当我们需要控制 gpio 的输出时，不论它是不是用作 led，都可以使用 leds 这个框架来实现控制(但是也需要相应的 pinmux 的正确设置作为前提)，在 leds 节点添加相应子节点即可。  

我们就来看看 leds 在设备书中是如何解析的：

```
leds {
    compatible = "gpio-leds";
    user {
        ...
        gpios = <&gpiobank2 15 ACTIVE_LOW>;
        ...
    };
};

```
上述节点中，user 指示灯使用了 gpiobank2 的 15 号引脚，并设置为低电平，它的函数调用流程是这样的：

```
gpio_led_probe()
    gpio_leds_create()
        led.gpiod = devm_fwnode_get_gpiod_from_child()
            devm_fwnode_get_index_gpiod_from_child()
                fwnode_get_named_gpiod()
                    struct gpio_desc *of_get_named_gpiod_flags()
                        of_parse_phandle_with_args(np,propname, "#gpio-cells", index,&gpiospec);

```
同样的，通过对应的 probe 函数进行跟踪，最后调用 of_parse_phandle_with_args() 函数对节点进行处理，最后将处理的结果保存起来。  

**事实上，pinctrl的处理是一个特例，对于其他引用的处理，几乎都是使用 of_parse_phandle_with_args 函数进行解析，解析输出的结果有：目标节点的 device node，以及参数信息，下面我们详细解析这个函数。**


### of_parse_phandle_with_args 源码解析
这个函数是解析节点引用的核心函数，它的源码部分是这样的：  

```
static int __of_parse_phandle_with_args(const struct device_node *np,
					const char *list_name,
					const char *cells_name,
					int cell_count, int index,
					struct of_phandle_args *out_args)
{
	struct of_phandle_iterator it;
	int rc, cur_index = 0;
	of_for_each_phandle(&it, rc, np, list_name, cells_name, cell_count) {
		rc = -ENOENT;
		if (cur_index == index) {
			if (!it.phandle)
				goto err;

			if (out_args) {
				int c;

				c = of_phandle_iterator_args(&it,
							     out_args->args,
							     MAX_PHANDLE_ARGS);
				out_args->np = it.node;
				out_args->args_count = c;
			} else {
				of_node_put(it.node);
            }
			return 0;
		}
		cur_index++;
	}
}

int of_parse_phandle_with_args(const struct device_node *np, const char *list_name,
				const char *cells_name, int index,
				struct of_phandle_args *out_args)
{
    ...
	return __of_parse_phandle_with_args(np, list_name, cells_name, 0,index, out_args);
}
```

整个源码部分基于设备树和设备节点的处理，比较复杂，有兴趣的朋友可以去研究研究源码。  

这里对源码部分做主要的讲解，以确保各位在不细究源码的情况下弄清楚这个函数的作用，毕竟研究源码还是比较费时间的(注意是费时间不是浪费时间！如果说练习编程有什么捷径，一定包括研究经典的源码这一条)。  

of_parse_phandle_with_args 这个函数一共有五个参数，分别是：
* **struct device_node \*np** : 这个参数表示引用者的节点，属于请求资源方，引用节点的语句被包含在该节点中，指包含 gpios = \<&gpiobankn GPIOn GPIO_STAT\> 属性的节点。
* **const char \*list_name** : 引用节点对应语句的名称，比如上一个参数中提到的 "gpios".
* **const char \*cells_name** : 在被引用节点中，有一个属性专门用来规定该节点被引用时需要的参数数量，cells_name 就是这个属性的名称。举个例子，描述一个 gpio 通常是两个参数，第一个是该 gpio 在 gpiobank 中的偏移，一般以 GPIOn 宏来表示，另一个就是 gpio 的状态，比如使能高、使能低，上拉下拉等,根据硬件不同也可能需要三个甚至更多的参数描述。  
    \
    所以在对应的 gpiobank control 节点中就会有一个名为 #gpio-cells 节点来指定参数数量，此时 cells_name 就等于 "#gpio-cells"
* **int index** : 在引用节点时，可以同时存在多个引用，比如一个外设的时钟来源有两个，那么就可以这样写： clocks = <&clk_pll1 &clk_pll2>, 那么这时候就需要指定哪一个需要在实际应用被使用，毕竟在引用的时候就是在请求使用它了。  
* **struct of_phandle_args \*out_args** ：从 out_args 这个名字可以看出，这是解析完成之后存放参数的地方,它的定义是这样的：
    ```
        struct of_phandle_args {
            struct device_node *np;
            int args_count;
            uint32_t args[MAX_PHANDLE_ARGS];
        };
    ```
    np中保存了被引用的目标节点 device_node, args_count 和 args 则分别对应参数的个数与参数的值。

整个函数的实现就是解析引用节点，利用参数解析找到被引用的节点信息并保存在 out_args 中。  
还不懂没关系，我们继续看下面的实例：

```
gpiobank1:gpio@800000{
    gpio-controller;
	#gpio-cells = <2>;
    ...
}
devicectl{
    ...
    gpio_enable = <&gpiobank1 GPIO1 GPIO_ACTIVE_HIGH
                    &gpiobank1 GPIO2 GPIO_ACTIVE_HIGH>;
    ...
}
```

其中，devicectl 为引用节点，gpiobank1 为被引用节点，在 gpiobank1 中 "#gpio-cells" 属性定义了引用一个 gpio 需要两个参数。  

所以，在 devicectl 中的 gpio_enable 第一部分为节点引用，节点引用在编译的时候就会确定一个唯一的 ID(phandle)，以确保可以直接通过该 ID 进行索引。

后接两个参数，而在上述示例中引用了两个 gpio，在使用时就需要提供 index 来确定要使用哪一个节点。   

在调用 of_parse_phandle_with_args 解析完成之后，结果保存在 out_args 中，out_args 中的成员： np 为 gpiobank1 的设备树节点，arg_count = 2,args 的值为 GPIO1，GPIO_ACTIVE_HIGH 或者 GPIO2，GPIO_ACTIVE_HIGH，这取决于解析时传入的 index。  


至于获取了引用节点之后需要怎么处理，那就根据具体的驱动实现来了，毕竟已经得到了目标节点和参数，足以定位到一个确定的资源，这个资源可能是 gpio，可能是 clk，也可能是某一路 pwm。

## 小结
本章主要介绍了设备树中的节点引用，以设备树中最常见的 pinmux、gpio 和 clock 为例讲解使用语法以及内核对节点引用的解析，希望各位朋友能有所收获。


好了，关于 Linux设备树节点的引用 讨论就到此为止了，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***个人邮箱：linux_downey@sina.com***
***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.

