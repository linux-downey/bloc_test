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
在设备树之前，比较通用的方式是在内核中注册并维护一张表，将 controller 中支持的 pwm device 逐一注册到表中，在需要使用的时候通过全局查询的办法来获取对应的 pwm_device .  

当用户需要向系统申请 pwm 设备时，直接调用 pwm_get() 接口：

```
struct pwm_device *pwm_get(struct device *dev, const char *con_id)；
```

该函数接收两个参数：device 结构和 con_id,

因为每个 pwm device 都由 struct pwm_device 来描述，其中包含了一个 struct device结构，所以通过对比 device 中的 dev_id 可以唯一定位一个 pwm device。    

con_id 通常是作为一个系统内唯一的字符串索引，因此也可以直接通过 con_id 唯一定位到 pwm device。  

但是实际情况有时会复杂一些，比如同一个 controller 的 pwm device 继承了同一个父节点的 device 而导致 device 中的 dev_id 一致，所以有时候需要两个参数来进行匹配。  

当然，这些匹配机制都是非常灵活的，取决于 controller 的注册者使用什么样的索引机制，接下来看看这个函数的源代码：

```
const char *dev_id = dev ? dev_name(dev) : NULL;
	struct pwm_device *pwm;
	struct pwm_chip *chip;
	unsigned int best = 0;
	struct pwm_lookup *p, *chosen = NULL;
	unsigned int match;
	int err;

	if (IS_ENABLED(CONFIG_OF) && dev && dev->of_node)
		return of_pwm_get(dev->of_node, con_id);

	mutex_lock(&pwm_lookup_lock);

	list_for_each_entry(p, &pwm_lookup_list, list) {
		match = 0;

		if (p->dev_id) {
			if (!dev_id || strcmp(p->dev_id, dev_id))
				continue;

			match += 2;
		}

		if (p->con_id) {
			if (!con_id || strcmp(p->con_id, con_id))
				continue;

			match += 1;
		}

		if (match > best) {
			chosen = p;

			if (match != 3)
				best = match;
			else
				break;
		}
	}

	mutex_unlock(&pwm_lookup_lock);

	if (!chosen)
		return ERR_PTR(-ENODEV);

	chip = pwmchip_find_by_name(chosen->provider);

	if (!chip && chosen->module) {
		err = request_module(chosen->module);
		if (err == 0)
			chip = pwmchip_find_by_name(chosen->provider);
	}

	if (!chip)
		return ERR_PTR(-EPROBE_DEFER);

	pwm = pwm_request_from_chip(chip, chosen->index, con_id ?: dev_id);
	if (IS_ERR(pwm))
		return pwm;

	pwm->args.period = chosen->period;
	pwm->args.polarity = chosen->polarity;

	return pwm;
}
```

该函数的实现并不复杂，主要有以下几个部分：
* 遍历 pwm_lookup_list 这个链表，这个链表中的节点为 struct pwm_lookup 类型，这个类型数据中主要包含了目标 pwm_chip 的指针和针对该 pwm device 在 pwm_chip 中的索引
* 将传入的参数 device 和 con_id 与系统链表中每个节点的 device(dev_id成员)和 cond_id 进行对比，以权重的方式确定目标节点，即 device 匹配上权重 +2，cond_id 匹配上权重 +1,最后选取权重最大值作为匹配成功对象，当然，如果权重为 0 表示匹配不成功。
* 根据匹配获取的 pwm_chip 和 pwm device 在 pwm_chip 中的索引，调用 pwm_request_from_chip 函数获取 pwm device，在 pwm_request_from_chip 函数中，将会判断该 pwm device 是否已被占用，只有在未被占用时，才会返回 pwm device，并标记其为被占用，这是一种同步机制。

获取到 pwm device 之后，就和设备树方式中的操作差不多，调用 pwm_get_state() 和 pwm_apply_state() 进行设置。  

### pwm_lookup_list 的注册
在上文源码的分析中，是根据 dev_id 和 cond_id 在 pwm_lookup_list 中进行匹配，那么，pwm_lookup_list 是哪里来的呢？  

答案很显然，是 pwm_controller 在编写驱动时进行静态注册的，其实注册的过程很简单，就是根据硬件提供的 pwm 设备构建一个 struct pwm_lookup *table 结构，然后使用 pwm_add_table（） 接口将该 pwm_device 对应的 table 链接到全局链表 pwm_lookup_list 中。  

struct pwm_lookup 的定义如下：

```
struct pwm_lookup {
	struct list_head list;
	const char *provider;
	unsigned int index;
	const char *dev_id;
	const char *con_id;
	unsigned int period;
	enum pwm_polarity polarity;
	const char *module; /* optional, may be NULL */
};
```
其中包含了上文中所提到的 dev_id,con_id、period、index等成员，一目了然。  

值得我们关注的一点是 provider 这个成员，从名字可以看出，provider 指的就是 pwm device 的父节点 controller(软件上为 pwm_chip)的名字，在上文的源码分析中，我们提到是先找到父节点的 pwm_chip ，然后根据 index 再定位到唯一的 pwm_device，而父节点的寻找就是通过这个 provider 进行匹配。  

那么，这个 provider 和谁进行匹配呢？不知道你还记不记得在前面的 controller 的章节中提到：所有的 pwm_chip 都会链接到一个全局链表 pwm_chips,是的，这个 provider 字符串就是通过遍历 pwm_chips 链表，对比每个 chip->dev.dev_id 是否等于 provider 字符串来进行匹配的。  





