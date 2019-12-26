# rtc 设备注册实现
在上一章中，我们介绍了 RTC 的设备驱动程序的编写，由 devm_rtc_device_register 注册一个 RTC 设备，并由此在 sysfs(/sys) 和 devtmpfs(/dev) 下创建相应的用户操作接口。  

但是，仅仅知道接口的调用并不是我们的目标，所以这一章我们将深入到 RTC 设备的注册接口中，看看 RTC 的设备驱动背后到底是怎么实现的。  

## 设备号的注册
看完上一章节的 RTC 设备驱动程序，不知道你是不是会有一些疑惑:注册一个 RTC 设备是通过 rtc_device_register接口，传入的参数也是 struct rtc_device 类型参数。  

但是，按照我们对字符设备驱动的理解，创建字符设备是通过申请设备号、同时创建并添加 cdev 结构实现的，但是我们仅仅是添加注册了一个 rtc_device 到系统中，为什么创建完成之后可以在 /dev 目录下看到 rtc 节点？  

/dev/ 目录下的设备需要申请设备号，并通过 cdev 初始化和创建，的确如此，至于为什么创建 RTC 设备只需要注册一个 struct rtc_device 即可，这是因为内核已经为整类 RTC 设备注册了设备号，，所有 RTC 设备共享同一个主设备号，而不同的设备节点使用不同的次设备号。开发者需要用的仅仅是填充自己的 ops 结构，对开发者来说这非常方便，这也是 linux 内核的实现理念：尽可能提供简单的接口，把软件的复杂性隐藏在内部。  

如果没有系统提供的框架，对于每一个 RTC 设备，都需要我们自己申请设备号同时创建一个对应的设备节点，非常麻烦且看起来非常混乱。  

rtc 设备的初始化在 driver/rtc/class.c 中，
```C
static int __init rtc_init(void)
{
	rtc_class = class_create(THIS_MODULE, "rtc");

	rtc_class->pm = RTC_CLASS_DEV_PM_OPS;
	rtc_dev_init();
	return 0;
}
subsys_initcall(rtc_init);
```

在初始化部分，创建并填充一个全局 rtc_class，然后调用了 rtc_dev_init().  

```C
void __init rtc_dev_init(void)
{
	int err;

	err = alloc_chrdev_region(&rtc_devt, 0, RTC_DEV_MAX, 
}
```
熟悉字符设备驱动程序的朋友对 alloc_chrdev_region() 这个接口并不会陌生，这是向系统申请设备号并存储在全局变量 rtc_devt 中。  

RTC_DEV_MAX 为次设备号的数量，等于 16，也就是说，在默认情况下，系统中允最多许注册 16 个 RTC 设备，当然这也是可以修改的。  

在 rtc 类设备的初始化过程中，并没有创建设备节点，只是申请了一个设备号的范围，只有当用户调用系统提供的 RTC 框架接口时才会注册设备节点，对于这一点，后文中详解。  


##　由 devm_rtc_device_register 开始
向内核中注册 RTC 设备的接口就是简单地调用 devm_rtc_device_register(),并传入设备名和 ops 回调操作函数集。 

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

	...

	return rtc;
}
```

该函数在 driver/rtc/class.c 文件中定义，在上文中有讲到： RTC 的初始化部分也是在文件中完成，同时初始化了全局变量 rtc_class 和设备号 rtc_devt。  

在当前函数中，将 class 赋值给 rtc->dev.class。  

同时获取 tc_get_dev_attribute_groups(); 的返回值，赋值给 rtc->dev.groups,这个函数的返回为 rtc_attr_groups，这是针对 sysfs 的操作回调函数。  

class 和 rtc_attr_groups 负责创建 sysfs 目录下的文件操作函数集。

而 rtc_allocate_device 余下的部分，和 RTC 的 alarm 机制相关，这里暂时不做讨论。  

在 rtc_allocate_device 成功返回之后，rtc_device_register() 中随机对其中的成员进行赋值：

```
rtc->id = id;
rtc->ops = ops;
rtc->owner = owner;
rtc->dev.parent = dev;
dev_set_name(&rtc->dev, "rtc%d", id);
```

其中包括 id、传入的 ops、设置名称等重要的操作。  


###  cdev 注册设备节点
略过 alarm 的部分，继续看到 rtc_device_register() 中的最后一部分：注册设备节点。   

在 RTC 框架的初始部分，已经初始化了 class 和设备号 rtc_devt。  

如果要在 /dev 目录下注册设备节点，只需要初始化并添加一个 cdev 结构即可。这部分操作在两个函数中完成：rtc_dev_prepare() 和 cdev_device_add()，cdev_device_add()自然是不用说，在之前的博客中已经解析过，这个函数主要是通过传入的 cdev 结构注册并创建一个 /dev 下的设备节点。  

而 rtc_dev_prepare() 的实现如下：
```
void rtc_dev_prepare(struct rtc_device *rtc)
{

	rtc->dev.devt = MKDEV(MAJOR(rtc_devt), rtc->id);

	cdev_init(&rtc->char_dev, &rtc_dev_fops);
	rtc->char_dev.owner = rtc->owner;
}
```
其中，主设备号是在 RTC 框架初始化时获取的，次设备号则由 id 决定，id 的获取在上文中有解释，然后调用 cdev_init() 初始化 cdev 结构。  

值得我们注意的是传入的 rtc_dev_fops，这是字符设备对应的操作函数集，那么问题就来了，在注册 RTC 时传入了一个 struct rtc_fileoperation 函数集，注册字符设备也传入了一个字符函数集，到底哪个被调用？

其实不难想到，在注册 cdev 时，struct file_operations 字符设备回调函数集是必须存在的，在访问字符设备节点是，必然是先访问字符设备回调函数集，所以我们注册的 rtc 设备的RTC 回调函数集必然是在字符设备回调函数中调用。  

既然有疑惑，那就继续跟踪源码。  

### 回调函数
其回调函数定义是这样的：
```
static const struct file_operations rtc_dev_fops = {
	.owner		= THIS_MODULE,
	.read		= rtc_dev_read,
	.poll		= rtc_dev_poll,
	.unlocked_ioctl	= rtc_dev_ioctl,
	.open		= rtc_dev_open,
	.release	= rtc_dev_release,
	.fasync		= rtc_dev_fasync,
};
```
其中包括 open/read/poll 等操作函数，read 函数看起来像是对应 rtc 回调函数中的 read，但是却不见 write 函数，那 rtc_fops 中的 .write 是如何被调用的呢？  

事实上，rtc_fops 中的 read 和 write 都是由 fops 中的 unlocked_ioctl 调用的，也就是说，在用户空间，无论是 RTC 的读还是写，都是通过 ioctl 接口来实现的，我们不妨再看看上述 rtc_dev_ioctl 的源码实现：

```C
static long rtc_dev_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct rtc_device *rtc = file->private_data;
	const struct rtc_class_ops *ops = rtc->ops;
	struct rtc_time tm;
	struct rtc_wkalrm alarm;
	void __user *uarg = (void __user *) arg;

	switch (cmd) {
	
	...
	case RTC_RD_TIME:
		mutex_unlock(&rtc->ops_lock);

		err = rtc_read_time(rtc, &tm);
		if (err < 0)
			return err;

		if (copy_to_user(uarg, &tm, sizeof(tm)))
			err = -EFAULT;
		return err;

	case RTC_SET_TIME:
		mutex_unlock(&rtc->ops_lock);

		if (copy_from_user(&tm, uarg, sizeof(tm)))
			return -EFAULT;

		return rtc_set_time(rtc, &tm);
	....
	default:
		/* Finally try the driver's ioctl interface */
		if (ops->ioctl) {
			err = ops->ioctl(rtc->dev.parent, cmd, arg);
			if (err == -ENOIOCTLCMD)
				err = -ENOTTY;
		} else
			err = -ENOTTY;
		break;
	}

}
```
从上述源码中可以看到：对于 ioctl 中的 RTC_RD_TIME cmd，调用了 rtc_read_time(),其源码如下：

```
int rtc_read_time(struct rtc_device *rtc, struct rtc_time *tm)
{
	err = __rtc_read_time(rtc, tm);
	return err;
}
static int __rtc_read_time(struct rtc_device *rtc, struct rtc_time *tm)
{
	...
	err = rtc->ops->read_time(rtc->dev.parent, tm);
	...
	return err;
}
```
在该函数中，直接调用了 rtc->ops->read_time(),即用户在注册 RTC device 时传入的 read 函数。  

而对于写函数，也是如此。

