# rtc 设备注册实现
在上一章中，我们介绍了 RTC 的设备驱动程序的编写，由 devm_rtc_device_register 注册一个 RTC 设备，并由此在 sysfs(/sys) 和 devtmpfs(/dev) 下创建相应的用户操作接口。  

但是，仅仅知道接口的调用并不是我们的目标，所以这一章我们将深入到 RTC 设备的注册接口中，看看 RTC 的设备驱动背后到底是怎么实现的。  

## 设备号的注册
看完上一章节的 RTC 设备驱动程序，不知道你是不是会有一些疑惑:注册一个 RTC 设备是通过 rtc_device_register接口，传入的参数也是 struct rtc_device 类型参数。  

但是，按照我们对字符设备驱动的理解，创建字符设备是通过申请设备号、同时创建并添加 cdev 结构实现的，但是我们仅仅是添加注册了一个 rtc_device 到系统中，为什么创建完成之后可以在 /dev 目录下看到 rtc 节点？  

/dev/ 目录下的设备需要申请设备号，并通过 cdev 初始化和创建，的确如此，至于为什么创建 RTC 设备只需要注册一个 struct rtc_device 即可，这是因为内核已经为整类 RTC 设备注册了设备号，开发者需要用的仅仅是填充自己的 ops 结构，对开发者来说这非常方便，这也是 linux 内核的实现理念：尽可能提供简单的接口，把软件的复杂性隐藏在内部。  

想一想如果不这样做？


##　由 devm_rtc_device_register 开始
对内核有所了解的朋友就会知道，devm_ 前缀其实是在原有函数上做了一层设备资源管理，关于设备资源管理部分可以参考我的另一篇博客。  

脱去设备资源管理的外衣，实际上是调用了：rtc_device_register();

```C
struct rtc_device *rtc_device_register(const char *name, struct device *dev,
					const struct rtc_class_ops *ops,
					struct module *owner)
{
    ...
	struct rtc_device *rtc;
	struct rtc_wkalrm alrm;
	int id, err;

	id = rtc_device_get_id(dev);

	rtc = rtc_allocate_device();
	
	rtc->id = id;
	rtc->ops = ops;
	rtc->owner = owner;
	rtc->dev.parent = dev;

	dev_set_name(&rtc->dev, "rtc%d", id);

	err = __rtc_read_alarm(rtc, &alrm);

	if (!err && !rtc_valid_tm(&alrm.time))
		rtc_initialize_alarm(rtc, &alrm);

	rtc_dev_prepare(rtc);

	err = cdev_device_add(&rtc->char_dev, &rtc->dev);

	rtc_proc_add_device(rtc);
    ...
	return rtc;

}
```
同样的，为了专注于框架的研究，删除了一部分错误处理代码，对于 rtc_device_register 函数，主要做了以下几件事：
* 从设备树中获取该 RTC 的 id，如果获取失败，就从系统中寻找一个未注册的 id 使用。
* 申请一个 rtc device，填充一些主要的结构成员。
* 读取并初始化 RTC 中的 alarm，在 Linux 系统中，RTC 设备可以设置 alarm 作为系统的唤醒使用，所以需要对这部分功能做一些初始化，鉴于电源管理部分的复杂性，本章不对系统的唤醒部分做介绍，后续如果有机会再详谈。
* 初始化一个 cdev，并调用 cdev_device_add() 将 cdev 注册到系统中，熟悉字符设备驱动的朋友对于这个并不会陌生。同时在 procfs(/proc)下注册相关接口。  


接下来我们逐个分析 rtc_device_register 中的操作步骤：

### rtc_device_get_id()
获取 RTC 的 id，该 id 将被设置为 rtc 名称中的 num 部分，即 ("rtc%d", id),我们在系统中看到的 rtc0、rtc1，其中0、1 就是由 id 确定的，它的源码时这样的：

```
static int rtc_device_get_id(struct device *dev)
{
	int of_id = -1, id = -1;

	if (dev->of_node)
		of_id = of_alias_get_id(dev->of_node, "rtc");
	else if (dev->parent && dev->parent->of_node)
		of_id = of_alias_get_id(dev->parent->of_node, "rtc");

	if (of_id >= 0) {
		id = ida_simple_get(&rtc_ida, of_id, of_id + 1, GFP_KERNEL);
		if (id < 0)
			dev_warn(dev, "/aliases ID %d not available\n", of_id);
	}

	if (id < 0)
		id = ida_simple_get(&rtc_ida, 0, 0, GFP_KERNEL);

	return id;
}
```
从源码中可以清晰地看到，该函数先是尝试从 alias 中获取 of_id 号，在设备树中，设备节点的命名通常是 rtc@3000 这一类，以外设名为前缀，@后添加该设备的地址，而 alias 字段则是改外设的别名，通常是 rtc0 = rtc@3000，rtc1 = ... 之类，所以尝试在当前节点的别名中获取 of_id 号。   

如果当前节点并没有对应的设备树节点，则尝试从它的父节点对应的设备树节点的 alias 别名获取 of_id.

如果从设备树中获取 of_id ,就以 of_id 为参数在全局 id 映射中找到对应的 id。  

如果上述操作皆没有获取到 id，就直接在全局 id 映射中找到一个未用到且最小的 id 号。  



### rtc_allocate_device()
系统通过 rtc_allocate_device() 申请一个 rtc_device 结构并返回，源码如下：
```C
static struct rtc_device *rtc_allocate_device(void)
{
	struct rtc_device *rtc;

	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return NULL;

	device_initialize(&rtc->dev);

	rtc->irq_freq = 1;
	rtc->max_user_freq = 64;
	rtc->dev.class = rtc_class;
	rtc->dev.groups = rtc_get_dev_attribute_groups();
	rtc->dev.release = rtc_device_release;

	mutex_init(&rtc->ops_lock);
	spin_lock_init(&rtc->irq_lock);
	spin_lock_init(&rtc->irq_task_lock);
	init_waitqueue_head(&rtc->irq_queue);

	/* Init timerqueue */
	timerqueue_init_head(&rtc->timerqueue);
	INIT_WORK(&rtc->irqwork, rtc_timer_do_work);
	/* Init aie timer */
	rtc_timer_init(&rtc->aie_timer, rtc_aie_update_irq, (void *)rtc);
	/* Init uie timer */
	rtc_timer_init(&rtc->uie_rtctimer, rtc_uie_update_irq, (void *)rtc);
	/* Init pie timer */
	hrtimer_init(&rtc->pie_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rtc->pie_timer.function = rtc_pie_update_irq;
	rtc->pie_enabled = 0;

	return rtc;
}
```

该函数在 driver/rtc/class.c 文件中定义，值得注意的是，这个文件通常是被编译进内核的，也就是在该文件中存在 init 函数，并被
