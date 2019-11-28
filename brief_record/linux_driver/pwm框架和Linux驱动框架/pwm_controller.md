# pwm controller 框架
在上一章中我们简单介绍了 pwm 在设备树中的描述语法，严格来说，设备树仅仅是一个描述文件，也可以看成是一个配置文件。所以，更重要的工作是这个描述文件是怎么被读取和使用的，从而延伸到整个 pwm controller 驱动框架是如何在内核中成型的。  

这一章我们持续深入，基于 beaglebone 平台，linux 4.14版本内核，来看看设备树中 controller 节点是怎么被注册到内核中的。  
(本章重在介绍内核框架，尽量摒弃平台相关的部分，仅以 beaglebone 平台的代码作为引入)。

## 从设备树到驱动
要查看驱动代码，自然是从设备树下手，找到其对应的设备树节点，然后根据它的 compatible 属性对内核源码进行全局搜索，就可以找到其对应的驱动。  

beaglebone 中 pwm controller 的节点是这样的：

```
ehrpwm0: pwm@48300200 {
				compatible = "ti,am3352-ehrpwm",
					     "ti,am33xx-ehrpwm";
				#pwm-cells = <3>;
				reg = <0x48300200 0x80>;
				clocks = <&ehrpwm0_tbclk>, <&l4ls_gclk>;
				clock-names = "tbclk", "fck";
				status = "disabled";
			};
```


## pwm 驱动主要结构体介绍

### struct pwm_chip
struct pwm_chip 描述一个 controller，也可以理解成每一个 pwm_chip 对应一个 pwm 硬件控制器，主要是描述一个控制器的属性。它的定义如下：

```
struct pwm_chip {
	struct device *dev;                    
	struct list_head list;        
	const struct pwm_ops *ops;    
	int base;
	unsigned int npwm;

	struct pwm_device *pwms;
	struct pwm_device * (*of_xlate)(struct pwm_chip *pc,
					const struct of_phandle_args *args);
	unsigned int of_pwm_n_cells;
};
```
在一个 pwm_chip 中，主要值得关注的有以下几个成员：
* list：该 list 是链表节点，所有的 pwm_chip 被链接到 pwm_chips 中，由系统统一管理，方便对所有的 pwm controller 进行遍历。
* ops：这是核心控制函数部分，该 ops 中包含了 pwm 的配置、使能，这一部分是硬件相关的操作，用户调用相应 pwm 操作接口时就会调用到这些底层函数以实现硬件控制
* base : 这里的 base 并非寄存器的基地址，内核使用位图对所有的 pwm 设备进行管理，而 base 就是该 controller 在位图中的基地址
* npwm：当前控制器包含的 pwm device 的数量
* pwms：pwm device 数组，与数量相对应 
* of_xlate：这是一个函数指针，负责处理 pwm 的节点引用
* of_pwm_n_cells：对应于设备树中 #pwm-cells 属性，表示 pwm 节点引用时需要传入多少个参数


## controller 的注册
对应设备树的节点描述，就会存在对应的 driver 将这些资源注册到系统中，并提供接口。  

根据设备树中的 compatible 属性，找到对应驱动，然后从 probe 函数进行分析：

```
struct ehrpwm_pwm_chip {
	struct pwm_chip chip;
	void __iomem *mmio_base;
	...
};

static int ehrpwm_pwm_probe(struct platform_device *pdev)
{
	...
	struct ehrpwm_pwm_chip *pc;
	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);

	clk = devm_clk_get(&pdev->dev, "fck");
	clk_prepare(clk);


	struct pwm_chip *chip;
	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &ehrpwm_pwm_ops;
	pc->chip.of_xlate = of_pwm_xlate_with_flags;
	pc->chip.of_pwm_n_cells = 3;
	pc->chip.base = -1;
	pc->chip.npwm = NUM_PWM_CHANNEL;
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pc->mmio_base = devm_ioremap_resource(&pdev->dev, r);
	pwmchip_add(&pc->chip);
	
	platform_set_drvdata(pdev, pc);
}
```

如代码所示，整个 controller 的注册可以分为三个部分：
* 第一个部分：从设备树中解析出时钟输入，pwm 的输出是依赖于时钟的，对于 pwm 的输出控制可以转化为对时钟的 enable 或 disable。
* 第二个部分：构造一个 pwm_chip 结构体，并注册到系统中，构造的过程包括设置回调函数、设置设备树解析函数、设置当前 pwm controller 所提供的 pwm device 通道数，以及获取寄存器基地址。
* 第三个部分：填充一个自定义的结构体 struct ehrpwm_pwm_chip *pc; ，结构体包含该 controller 所有相关的数据，然后将其绑定在 deivce 中。  

接下来主要是对后两个部分进行详细的分析。

### struct pwm_device
讨论 pwmchip_add(pwm controller的注册) 函数之前，我们有必要来了解一下另一个结构体：struct pwm_device:
```
struct pwm_device {
	const char *label;                  //设备名
	unsigned long flags;               
	unsigned int hwpwm;                 //硬件设备号，表示 pwm 设备在当前控制器中的编号
	unsigned int pwm;                   //pwm编号，表示 pwm 在所有 pwm 设备中的编号
	struct pwm_chip *chip;              // pwm 所属的 pwm_chip ，也就是 pwm 控制器
	void *chip_data;                   // pwm 的私有数据
	struct pwm_args args;              //保存的参数
	struct pwm_state state;            //pwm 的状态，该状态包括 period 、dutycycle、极性
};
```
通常，按照内核中的命名习惯，chip 本意为芯片，可指代一个外设，也可以看成一个外设控制器，所以在内核中 xxx_chip 通常表示一个控制器，例如 gpio_chip,irq_chip 等。  

而 device 意为设备，即表示一个具体的设备，所以 pwm_device 结构体描述的是某一路具体的设备，主要包括 pwm 设备名、编号、所属的硬件控制器、以及私有数据等等。

### pwmchip_add()
接下来再解析pwmchip_add 这个核心的函数：从函数名可以看出，这个函数的作用就是将驱动填充的 pwmchip 注册到系统中，
	
```
int pwmchip_add_with_polarity(struct pwm_chip *chip,enum pwm_polarity polarity)
{
	struct pwm_device *pwm;
	ret = alloc_pwms(chip->base, chip->npwm);
	chip->base = ret;

	for (i = 0; i < chip->npwm; i++) {
		pwm = &chip->pwms[i];

		pwm->chip = chip;
		pwm->pwm = chip->base + i;
		pwm->hwpwm = i;
		pwm->state.polarity = polarity;

		if (chip->ops->get_state)
			chip->ops->get_state(chip, pwm, &pwm->state);

		radix_tree_insert(&pwm_tree, pwm->pwm, pwm);
	}

	bitmap_set(allocated_pwms, chip->base, chip->npwm);

	INIT_LIST_HEAD(&chip->list);
	list_add(&chip->list, &pwm_chips);

	pwmchip_sysfs_export(chip);
}

int pwmchip_add(struct pwm_chip *chip)
{
	return pwmchip_add_with_polarity(chip, PWM_POLARITY_NORMAL);
}

```
pwmchip_add 函数直接调用 pwmchip_add_with_polarity，pwmchip_add_with_polarity 中主要做了两件事：
* 根据 pwm_chip 提供的 npwm 参数，申请相应数量的 pwm_device 结构，并填充它们。  
* 从 pwm 设备管理位图中找出一片空闲的区域，存放当前需要加入的 pwm device。  
* 将 pwm_device 添加到系统中,系统对 pwm_device 的管理有两种：位图和基数树，consumer 可以通过这两种结构进行索引，另外，由于 pwm_device 和 pwm_chip 也是相关联的，所以也可以通过 pwm_chip 获取到需要操作的 pwm_device,毕竟在使用的时候，是通过操作 pwm_device 来操作 pwm 的输出的。  


在 pwmchip_add 函数的最后，使用 pwmchip_sysfs_export() 函数将 pwm 设备的操作导出到 /sys 用户空间，关于接口的导出，我们在下一章进行详细地讨论。  




