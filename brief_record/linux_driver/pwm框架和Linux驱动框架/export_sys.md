# pwm 驱动接口导出到 /sys 用户空间
上一章我们讨论了 pwm controller 在系统中的注册，本章我们主要讨论 pwm controller 导出到 /sys 用户接口。  

## /sys 简介
sysfs文件系统是内核版本2.6引入的，它挂载在 /sys 目录下，在老版本系统中，内核信息和驱动信息都是放置在 /proc 目录下，这是一个基于 ram 的文件系统，也就是由内核启动时挂载，掉电或者重启将丢失所有数据，随着内核和驱动的逐渐庞大，内核维护者将运行时的驱动信息和内核信息分离，驱动专门由 /sys 目录下的文件描述。   

在阅读本篇博客之前，建议先阅读我的另一篇博客[linux sysfs文件系统简介](http://www.downeyboy.com/2019/03/03/linux_sysfs/)，了解 sysfs 的操作机制，以便更清晰地理解本章内容。   


## 通过 sysfs 操作 pwm
这一章我们主要讲解 pwmchip_sysfs_export() 这个接口，将 pwm 的操作导出到 sysfs 中，在了解该函数的实现之前，我们可以先看看怎么使用它导出的接口。  

事实上，pwmchip_sysfs_export() 是 linux 中的标准接口，所以，在大多数的平台上都有直接沿用了这个接口以将设备的操作导出到 sysfs 中，类似的还有 gpio_export() 函数，通过 sysfs 操作 pwm 在大部分平台都是同样的操作。  

### pwm controller目录
要操作某一路 pwm ，先要在 sysfs 中找到其对应的 pwm controller，通过其 controller 进行操作。  

一种常用的方法就是使用 find 指令，pwm controller 在 sysfs 中的命名通常是 pwmchipn,我们可以使用下面的指令：

```
find /sys -name "pwmchip*"
```

当然，通常一个系统中可能不止一个 pwm controller，会出现 pwmchip0、pwmchip1 等多个设备，这个时候就需要通过芯片的数据手册来确定，通常这些 pwm chip 的上级目录命名为 48302200.pwm 这一类带寄存器基地址的目录名，根据数据手册就可以确定你要操作的 pwm controller 的基地址，从而找到对应的 pwm controller.

### export
如果是标准的内核接口实现,在 sysfs 中对应一个 pwm_chip 的目录结构是这样的：

```
device  export  npwm  power  subsystem  uevent  unexport
```

其中，我们需要特别关注的文件就是 export、npwm、unexport，直接操作 pwm 就是通过这三个文件。  

其中，npwm 表示当前 pwm controller 包含多少路有效的 pwm device，具体操作就是将对应的 pwm device 从当前 pwm controller 中导出，可以用下面的指令：

```
echo 1 > export
```

这个写入到文件的数字表示当前 pwm device 在 pwm controller 中的编号，当然，硬件上的 pwm 输出引脚和软件上的 pwm device 的对应还得你自己去查数据手册。   

操作成功后将会在当前目录下生成一个表示 pwm device 的目录，如果当前目录是 pwmchip1 ，要操作的 pwm device number 为1，那么这个目录的命名为 pwm-1:1,，这个目录下的文件结构一般是这样的：

```
capture  device  duty_cycle  enable  period  polarity  power  subsystem  uevent
```

将 pwm device 导出之后就看到我们想要的了，我们可以直接使用下列的指令控制 pwm 的输出：

```
echo 1000 > period
echo 500 > duty_cycle
echo 1 > enable
```

如果你在 pwm 输出引脚上连上 led 或者其他 pwm 驱动的设备，就可以看到 pwm 的输出已经开始工作了。  

## 接口的实现
上述的操作仅仅是证明该 pwm 可以使用，既然在 linux 中，我们面对的是源代码，自然就得看看这些接口是怎么实现的，知其然并知其所以然。因此，我们需要深入到内核，看看这些接口是怎么注册的。  

而注册这些文件的接口就是 pwmchip_sysfs_export()，接下来查看它的内核代码实现。

```
void pwmchip_sysfs_export(struct pwm_chip *chip)
{
	struct device *parent;
	parent = device_create_with_groups(&pwm_class, chip->dev, MKDEV(0, 0),chip, pwm_chip_groups,"pwmchip%d", chip->base);
}
```

pwmchip_sysfs_export 这个函数的实现就是通过 device_create_with_groups 在 /sys 的 pwm 相关目录下创建一个目录，目录名为 "pwmchip%d", chip->base,由此可以确定在上述 pwm 操作时为什么需要先寻找 pwmchipn 的目录，就是由此进行命名。  

那么，另一个需要确认的问题是：由该接口生成的目录存在于 /sys 的哪个子目录下呢？  

我们来看 device_create_with_groups 的函数声明：

```
struct device *device_create_with_groups(struct class *cls, struct device *parent, dev_t devt, void *drvdata, const struct attribute_group **groups, const char *fmt, ...)
```
可以发现，函数第二个参数是 struct device *parent，从名字可以知道这是父节点，也就对应 sysfs 中的父目录，在上述调用中传入的是 chip->dev，不知道你还记不记得我们之前章节讲到的，每一个 pwm controller 在系统中都将被注册成一个 chip。  

所以，该 pwmchipn 的父目录就是 pwm controller 节点的目录，如果你有浏览过博主的另一篇博客：[linux sysfs文件系统简介](http://www.downeyboy.com/2019/03/03/linux_sysfs/)，就可以了解到，在 /sys 目录下，文件的组织方式有多种，/sys 目录下的几乎每个一级子目录都代表着一种文件组织方式，比如 /sys/device ，就是将所有设备按照 device 的层级结构进行文件排列，同样的，该规则适用于 sys/bus、sys/class 等目录。     

如何找到 pwmchipn 的父节点即 pwm controller？这需要对整个设备结构有一定的了解，当然，在不了解的情况下你同样可以使用 find 这种文件搜索的方式来找到，这是最通用的方式。  

到这里，我们知道了 pwmchipn 目录的注册以及在 /sys 中的注册位置，接下来就是查看其对应的接口实现了。  

### export 实现

在上述的 pwm 操作中，进入目录之后所做的第一个操作是 

```
echo 1 > export 
```

操作成功后将在当前目录下生成一个对应的 pwm_device 的目录，我们来看看它是怎么实现的。  

首先，回头再看一下 pwmchipn 的目录创建部分：

```
device_create_with_groups(&pwm_class, chip->dev, MKDEV(0, 0),chip, pwm_chip_groups,"pwmchip%d", chip->base);
```

它的第四个参数是 const struct attribute_group **groups，传入了 pwm_chip_groups ，这个参数就是用来控制目录的访问行为

```
struct device_attribute dev_attr_export = { 
    .attr = { .name = "export", .mode = 00200 ,} 
    .store = export_store, 
    }

static struct attribute *pwm_chip_attrs[] = {
	&dev_attr_export.attr,
	&dev_attr_unexport.attr,
	&dev_attr_npwm.attr,
	NULL,
};

static const struct attribute_group pwm_chip_group = { .attrs = pwm_chip_attrs, }; static const struct attribute_group *pwm_chip_groups[] = { &pwm_chip_group, ((void *)0), }
```

根据上述的注册行为，真正注册接口的部分由 dev_attr_export 控制，.name 为文件名 "export",.mode 为访问权限设置，.store 表示写文件时对应的回调函数，因为我们需要对 export 文件进行写操作，所有我们接着看 export_store 的具体实现：

```
static ssize_t export_store(struct device *parent,
			    struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct pwm_chip *chip = dev_get_drvdata(parent);
	struct pwm_device *pwm;
	unsigned int hwpwm;
	int ret;

	ret = kstrtouint(buf, 0, &hwpwm);

	pwm = pwm_request_from_chip(chip, hwpwm, "sysfs");

	ret = pwm_export_child(parent, pwm);

	return ret ? : len;
}
```
该函数的实现如下：
* 根据传入的 device 获取 device 的私有数据，即 pwm_chip 数据
* 用户在写当前文件时，写入的为数字，即需要导出的 pwm 编号
* 获取了对应的 pwm_chip 结构体和对应的编号，自然就可以找到相应的 pwm device
* 调用 pwm_export_child() ，将该 pwm device 导出到用户空间，由于 parent 参数等于当前节点的 parent，所以，在当前目录下生成一个 pwm device 对应的操作目录。  

由此，整个 pwm device 的导出就实现了，pwmchipn 目录下还存在的另外两个文件接口的实现：npwm 和 unexport，其实现流程也是与 export 一致，npwm 返回当前 pwm_chip 的 pwm 数量，export 实现与 unexport 相反的操作，一个是导出，一个是撤销。有兴趣的同学可以延展性地研究 npwm 和 unexport 的实现，这里就不过多赘述。  

### pwm device 的接口操作
紧接着，跟随 pwm_export_child 函数，我们来看看 pwm device 导出到用户空间的具体实现：

```
struct pwm_export {
	struct device child;
	struct pwm_device *pwm;
	struct mutex lock;
};

static int pwm_export_child(struct device *parent, struct pwm_device *pwm)
{
	struct pwm_chip *chip = dev_get_drvdata(parent);
	struct pwm_export *export;
	int ret;

	export->pwm = pwm;

	export->child.release = pwm_export_release;
	export->child.parent = parent;
	export->child.type = &pwm_channel_type;
	export->child.devt = MKDEV(0, 0);
	export->child.class = &pwm_class;
	export->child.groups = pwm_groups;
	dev_set_name(&export->child, "pwm-%d:%u", chip->base, pwm->hwpwm);

	ret = device_register(&export->child);
    ...
}
```

该函数主要实现：填充一个 struct device 结构体 export->child，值得注意的是，这里将该 device 的 name 属性设置为 

```
"pwm-%d:%u", chip->base, pwm->hwpwm
```

也就是在上文中我们操作到的 pwm-1:1 目录，，原来它的命名是在这里实现的，最后，调用 device_register(&export->child); 注册这个 device。  

事实上，真正控制文件访问操作的是这一部分：

```
export->child.groups = pwm_groups;
```

还是我们熟悉的那个 const struct attribute_group 类型结构体，了解过 sysfs 的朋友都知道，这一类 attribute 结构体的作用就是控制接口访问的操作。  

接着看它的实现：

```


static struct attribute *pwm_attrs[] = {
	&dev_attr_period.attr,
	&dev_attr_duty_cycle.attr,
	&dev_attr_enable.attr,
	&dev_attr_polarity.attr,
	&dev_attr_capture.attr,
	NULL
};

static const struct attribute_group pwm_group = { .attrs = pwm_attrs, }; static const struct attribute_group *pwm_groups[] = { &pwm_group, ((void *)0), }
```

在这个 pwm_attrs 中，我们又发现了熟悉的东西，包括 period、duty_cycle、enable、polarity、capture，不难猜到，这些 attribute 的实现正好对应我们在上文中操作到的 period enable 等文件，也正是这些文件接口实现了 pwm device 的操作。  

### period 接口
接下来当然就是来看看这些接口实现，同样的,xxx_store 表示写文件的回调接口，而 xxx_show 表示读文件的回调接口，以 period 和 enable 接口为例，我们来看看具体实现：

```
static ssize_t period_store(struct device *child,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	struct pwm_export *export = child_to_pwm_export(child);
	struct pwm_device *pwm = export->pwm;
	struct pwm_state state;
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	pwm_get_state(pwm, &state);
	state.period = val;
	ret = pwm_apply_state(pwm, &state);

	return ret ? : size;
}

struct device_attribute dev_attr_period = { 
    .attr = {.name = ) ) "period", .mode = mode }, 
    .show = period_show, 
    .store = period_store, }


```
该 period_store 接口实现如下：
* 在上述 pwm_export_child,device 为 export 的实例成员，所以可以通过 device 获取到该 pwm device 对应的 export 结构。
* 因为一个 export 结构与一个 pwm device 是对应的，所以可以直接获取到 pwm device 结构
* 先使用 pwm_get_state 获取该 pwm_device 的状态，然后将状态中的 period 字段设置为用户写入的数据，然后使用 pwm_apply_state 接口应用该状态。  

设置 period 的实现就是这样，看起来并不复杂。  

接下来看看 enable 的接口：

### enable 接口

```
static ssize_t enable_store(struct device *child,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	struct pwm_export *export = child_to_pwm_export(child);
	struct pwm_device *pwm = export->pwm;
	struct pwm_state state;
	int val, ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	pwm_get_state(pwm, &state);

	switch (val) {
	case 0:
		state.enabled = false;
		break;
	case 1:
		state.enabled = true;
		break;
	default:
		ret = -EINVAL;
		goto unlock;
	}

	ret = pwm_apply_state(pwm, &state);

}

struct device_attribute dev_attr_enable = { 
    .attr = {.name = ) ) "enable", .mode = mode },   
    .show = enable_show, 
    .store = enable_store, 
}
```

enable 接口其实和 period 设置接口并无二致，都是获取相应的 pwm_device ，接受参数 0 和 1，0 表示失能，1 表示使能，然后使用 pwm_apply_state 将状态更新。  



### pwm_apply_state
当然，从命名可以看出 pwm_apply_state 的作用就是更新 pwm device 的状态，比如重新设置 period、duty_cycle 等，但是它具体实现是怎么样的呢？   

紧跟着它的源代码：

```
int pwm_apply_state(struct pwm_device *pwm, struct pwm_state *state)
{

	if (pwm->chip->ops->apply) {
		err = pwm->chip->ops->apply(pwm->chip, pwm, state);
		pwm->state = *state;
	} else {
		if (state->polarity != pwm->state.polarity) {
			if (!pwm->chip->ops->set_polarity)
				return -ENOTSUPP;

			if (pwm->state.enabled) {
				pwm->chip->ops->disable(pwm->chip, pwm);
				pwm->state.enabled = false;
			}

			err = pwm->chip->ops->set_polarity(pwm->chip, pwm,
							   state->polarity);

			pwm->state.polarity = state->polarity;
		}

		if (state->period != pwm->state.period ||
		    state->duty_cycle != pwm->state.duty_cycle) {
			err = pwm->chip->ops->config(pwm->chip, pwm,
						     state->duty_cycle,
						     state->period);
			if (err)
				return err;

			pwm->state.duty_cycle = state->duty_cycle;
			pwm->state.period = state->period;
		}

		if (state->enabled != pwm->state.enabled) {
			if (state->enabled) {
				err = pwm->chip->ops->enable(pwm->chip, pwm);
				if (err)
					return err;
			} else {
				pwm->chip->ops->disable(pwm->chip, pwm);
			}

			pwm->state.enabled = state->enabled;
		}
	}

	return 0;
}
```

深入到 pwm_apply_state 可以发现，这个函数的实现就是根据传入的 state 对 pwm device 进行真正硬件上的设置。  

在 pwm controller 注册部分讲到，在注册 pwm controller(也就是 pwm_chip) 的时候需要传入 pwm 相关操作的底层函数作为回调函数，在用户操作的时候再调用这些回调函数。  

而这些回调函数就是在这里被调用，通过 pwm->pwm_chip->ops(注册时将填充 pwm_chip->ops,后面将又将 每个 pwm_device 和其所属的 pwm_chip 相关联)中的 set_period、enable 进行真正的硬件操作。**而 pwm 框架在这里起到一个传递的作用，将底层硬件操作函数保存，在需要硬件操作的时候调用。**


## 小结
在这一章中，通过源代码的层层跟踪，我们解析了 pwm 设备导出到 sysfs 中的实现细节，根据 pwm controller 到 pwm device 的层层导出，深入了解当用户往 sysfs 中对应文件中写入参数时系统对真实的硬件 pwm 设备的操作。  




