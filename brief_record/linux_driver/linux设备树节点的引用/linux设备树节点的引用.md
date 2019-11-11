# linux 设备树节点的引用
在设备树中，硬件以树状的层次被映射成设备树中的节点，从根开始，分别引出各级总线，有对应真实的物理总线(如i2c、spi等)，也有专门为了统一设备模型而引入的虚拟总线(platform bus)，所有连接在系统总线上的设备都被抽象成节点，尽管某些部分比如定时器、时钟、gpio，在实际的开发过程中，我们并不把它们看成总线，但是对于 linux 系统而言，对这些同样挂载在系统总线上的硬件资源进行统一的抽象，方便进行管理。   

虽然，从设备树的结构来看，各个节点之间层次分明，各个总线之间没有明显的联系。但是从系统的角度来看，各个硬件之间的耦合是必然存在的，最常见的就是：大多数总线都需用使用到时钟部分，引脚的 pinmux 复用。那么，既然硬件上有耦合，那么肯定就会体现在软件上，毕竟，在 linux 中，软件的模型总归是建立在硬件的拓补上的。

那么，就存在节点之间的相互引用。


## 引用语法
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
这就是设备树中对于 gpio 模块的引用，在某些模块的使用时，可能用到一些指示灯或者 reset 引脚，需要额外地去操作 gpio，就需要在当前节点中引用对应的 gpio。  

在上述的示例中，可以通过 devicectl 节点看出，该节点引用 gpiobank1 的 GPIO1 对应的引脚，将其设置为高，那么，需要引用 gpio 需要多少个参数是由什么决定的呢？  

答案是通过对应 gpio bank，也就是 gpiobank1 的 #gpio-cells 节点来决定，在上述示例中，该值为2，所以引用的时候需要两个额外的参数来确定。  


## 内核对引用的处理
与 gpio 具有相同待遇的还有 clock，几乎每个模块都需要 clock 的支持，在大部分的系统中，每个模块的运行都需要指定 enable 相应的时钟，所以通常需要在设备树中指定需要用到的时钟来源，以便在驱动 probe 成功时开启它以确保外设的运行，



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
* **struct device_node \*np** : 这个参数表示引用者的节点，属于请求资源方，引用节点的语句被包含在该节点中，比如 gpio = \<&gpiobankn GPIOn GPIO_STAT\>, clocks = <&clk_pll> 。
* **const char \*list_name** : 引用节点对应语句的名称，比如上一个参数中提到的 "gpio"，"clocks".
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

### 其他的引用
上述就是在设备树中引用节点的通用编写规则以及内核解析，除了上述示例中的 gpio，还有其他常见的，比如 pwm：

```
pwm1:pwm@8100000{
    pwm-cells = <2>;  //第一个参数为 period，第二个参数为占空比(duty_cycle)
    ...
};
devctrl{
    pwm_ctl = <&pwm1 1000 500>;
};
```

还有更常用的 clock：

```
clk_sys1:clk@8300000{
    clock-cells = <0>;  //通常只需要指定时钟源即可，设置时钟频率通过其他方法
    frequency = <16000000>;  //16M
};
devctrl{
    clocks = <&clk_sys1 &clk_sys2>
};
```

至于其他的，大家可以去研究研究设备树，每个硬件平台的设备树都有或大或小的差异，毕竟，设备树就是为了描述硬件差异而存在的。  


## 应用场景
在日常的开发中，当我们需要引用另一个设备树节点时，我们只需要添加对应的设备树引用属性即可，比如 led、button、clock 引用，修改的时候也只需要在其上进行修改即可达到目的。  

这造成了一个错觉：似乎引用节点的解析是包含在设备树解析阶段的。  

事实上，是linux 各种成熟的框架带给我们各种便利，而引用本身的解析是在对应外设的 probe 函数进行的。  

例如，设备树中 leds 节点和 button 节点是 linux 中成熟的控制 gpio 输入输出的框架，当我们需要控制 gpio 的输出时，不论它是不是用作 led，都可以使用 leds 这个框架来实现控制(但是也需要相应的 pinmux 的正确设置作为前提)，在 leds 节点添加相应子节点即可。  

我们就来看看 leds 的控制部分解析节点引用的部分：

```

```



