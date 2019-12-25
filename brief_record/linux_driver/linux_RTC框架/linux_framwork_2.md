# rtc 设备注册实现
在上一章中，我们介绍了 RTC 的设备驱动程序的编写，由 devm_rtc_device_register 注册一个 RTC 设备，并由此在 sysfs(/sys) 和 devtmpfs(/dev) 下创建相应的用户操作接口。  

但是，仅仅知道接口的调用并不是我们的目标，所以这一章我们将深入到 RTC 设备的注册接口中，看看 RTC 的设备驱动背后到底是怎么实现的。  


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
* 从设备树中获取该 RTC 的 id，如果获取失败，就从系统中寻找一个未注册的 id 使用，该 id 将被设置为 rtc 名称中的 num 字段，即 ("rtc%d", id),我们在系统中看到的 rtc0、rtc1，其中0、1 就是由 id 确定的。
* 申请一个 rtc device，填充一些主要的结构成员。
* 读取并初始化 RTC 中的 alarm，在 Linux 系统中，RTC 设备可以设置 alarm 作为系统的唤醒使用，所以需要对这部分功能做一些初始化，鉴于电源管理部分的复杂性，本章不对系统的唤醒部分做介绍，后续如果有机会再详谈。
* 初始化一个 cdev，并调用 cdev_device_add() 将 cdev 注册到系统中，熟悉字符设备驱动的朋友对于这个并不会陌生。同时在 procfs(/proc)下注册相关接口。  


接下来我们逐个分析 rtc_device_register 中的操作步骤：



