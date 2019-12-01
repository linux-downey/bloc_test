# pwm consumer 的驱动实现
在之前的章节中主要讲了 pwm controller 的驱动实现，pwm controller 是作为 provider 的角色常驻在系统中（当然也可以通过配置取消），那么，如果驱动工程师想要使用 controller 提供的这些资源，应该怎么做呢？  

使用 controller 提供的基础服务以实现其它外围设备的驱动程序，也就是设备驱动程序。按照内核的一贯特性，设备驱动程序并不会太难，只是需要在基本的驱动框架下做一些填空题，将驱动硬件、平台相关的数据传递给框架。  

这一章我们通过设备树和静态注册的两种方式来讲解是设备驱动是如何实现的。  

## 设备树方式

### 设备树设置
使用设备树进行配置是非常方便的，是目前比较常用的做法，实现也是比较简单的，只需要对 controller 节点进行引用即可，设备树节点的引用在之前的章节有介绍过，大概是这样的：

```
pwm1:pwm@4830200 {
    ...
    #pwm-cells = <0x3>;
    ...
}

pwm_dev {
    compatible = "vid,dev_id"
    pwms = <&pwm1 index period polarity>;
}

```
在 controller 节点中，#pwm-cells 决定引用一路 pwm 需要几个参数，在这个 controller 中设置为三个，分别为 pwm 编号索引、period 、 极性。  

通常情况下，#pwm-cells 设置为 2 或 3，分别对应 pwm 编号索引、period，当设置为 3 时增加最后一个极性参数。  


### 驱动实现
设备树仅仅是一种描述，还需要在驱动中对设备树的描述进行处理，只是有了设备树的配置，驱动处理也将变得非常方便。  

驱动的匹配部分还是那一些老套路，通过 compatible 属性进行 device 和 driver 的匹配，然后调用 driver 部分的 probe 函数，驱动初始化都在 probe 函数中执行。  

我们的目标是对硬件 pwm 进行操作，根据前面的章节介绍的：pwm_chip 对应一个 pwm controller(也就是一个 pwm 硬件控制器)， pwm_device 从属于 pwm_chip,对应某一路具体的 pwm 设备. 

所以需要获取到相应的 pwm_device 进行操作，而获取 pwm device 的操作非常简单，只需要一个函数调用：

```
struct pwm_device *pwm = devm_of_pwm_get(&pdev->dev,pdev->dev.of_node,NULL);
```

返回值 pwm 就是我们需要的的 pwm_device，获取到 pwm_device ，就可以直接对其进行相应的操作，比如：获取当前 pwm device 的状态：

```
struct pwm_state state;
pwm_get_state(pwm,&state);
```

state 中包含当前 pwm device 中的 period 、duty_cycle 等参数值，表示 pwm device 当前的状态。  

然后对这一路 pwm 进行设置：

```
state.period = period;
state.duty_cycle = duty_cycle;
state.enabled = true;
pwm_apply_state(pwm,&state);
```

做完这些，你就会发现 pwm 已经正常工作起来了。当然，在实际的编码中，你需要严格地对返回值进行检查。  


### 设备树解析过程
尽管 pwm 已经在硬件上驱动起来了，但是我们似乎还有一个问题没有解决：devm_of_pwm_get() 是怎么工作的？为什么就可以直接返回我们需要的 pwm device，当然前提是我们在设备树中进行了相应的节点引用。  

devm_of_pwm_get 的内核源码实现：
```
struct pwm_device *devm_of_pwm_get(struct device *dev, struct device_node *np,
				   const char *con_id)
{
    ...
    struct pwm_device  *pwm;
    pwm = of_pwm_get(np, con_id);
    ...
    return pwm;
}
```

以 devm 开头的函数都是添加了设备管理机制，由系统在合适的时候进行设备资源的回收，事实上，它只是在 of_pwm_get 的基础上进行了一层封装。接着看 of_pwm_get() 函数的源码：

```
struct pwm_device *of_pwm_get(struct device_node *np, const char *con_id)
{
	struct pwm_device *pwm = NULL;
	struct of_phandle_args args;
	struct pwm_chip *pc;
	int index = 0;
	int err;

	if (con_id) {
		index = of_property_match_string(np, "pwm-names", con_id);
	}

	err = of_parse_phandle_with_args(np, "pwms", "#pwm-cells", index,
					 &args);

	pc = of_node_to_pwmchip(args.np);

	pwm = pc->of_xlate(pc, &args);

	if (!con_id) {
		err = of_property_read_string_index(np, "pwm-names", index,
						    &con_id);
		if (err < 0)
			con_id = np->name;
	}

	pwm->label = con_id;

	return pwm;
}
EXPORT_SYMBOL_GPL(of_pwm_get);
```

该函数实现就是根据设备树节点 struct device_node *np 来获取 pwm device 节点，至于第二个参数 con_id，是可选选项，通常只需要提供设备树节点即可，而当前设备树节点则由 dev.of_node 获得。 

函数流程是这样的：
* 调用 of_parse_phandle_with_args() 函数，传入当前设备树节点 np，根据设备树节点的 pwms 属性中的引用获取到目标 controller 的设备树节点和参数，从目标 controller 的设备树节点中获取到 pwm_chip 结构。
* 调用 pwm_chip 结构的 of_xlate() 函数，在 pwm controller 的章节中有提到，pwm_chip->of_xlate() 函数专门处理设备树节点与引用之间的关系，根据设备树提供的参数负责从 pwm_chip 中获取指定的 pwm_device.
* 设置 pwm_device 的 label，返回获取的 pwm_device.


## 静态注册的方式
在设备树之前，比较通用的方式是在内核中维护一张表，将 controller 中支持的 pwm device 逐一注册到表中，在需要使用的时候通过全局查询的办法来获取对应的 pwm_device .  

具体的接口是这样的：

```
pwm_get()
```


