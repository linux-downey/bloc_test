# dev 和 handle 的注册和匹配
在上一章节中，着重介绍了 input 子系统中三大核心数据结构以及它们之间的关系。在这一章节中，从源代码出发，看看这三个部分的核心处理细节部分。    


## handler 的注册
handler 是用户进行注册的，专门用来事件的处理和上报，事实上，它还负责 handler 和 dev 的匹配工作。   

在内核中，系统默认注册了 evdev 类型 handler，通常情况下，输入设备可以直接使用该 handler ，所以对于通常的 input 设备驱动开发，我们只需要填充 dev 的部分即可。  

evdev 类型的 handler 被注册在 driver/input/evdev.c 中：

```
static int __init evdev_init(void)
{
	return input_register_handler(&evdev_handler);
}

int input_register_handler(struct input_handler *handler)
{
	struct input_dev *dev;

    ...
    //初始化使用该 handler 的 handle 链表
	INIT_LIST_HEAD(&handler->h_list);

    //将该 handler 链接到全局链表 input_handler_list 中
	list_add_tail(&handler->node, &input_handler_list);


	list_for_each_entry(dev, &input_dev_list, node)
		input_attach_handler(dev, handler);

    ...
}
```

handler 的注册其实非常简单，就两件事：
* 将当前注册到系统的 handler，链接到 input_handler_list 中，input_handler_list是一个全局链表，保存所有 handler。
* 针对当前的 handler，遍历整个的 dev 链表，寻找与当前 handler 匹配的 dev(遍历的部分后文中详解)。


## dev 的注册
在驱动编写时介绍了 dev 的注册由 input_register_device 接口实现，它的源码如下：

```
int input_register_device(struct input_dev *dev)
{
    //检查并设置 dev 的事件对应的位图
    ...
    device_add(&dev->dev);
    // 将当前 dev 添加到全局 dev 链表 input_dev_list 中
    list_add_tail(&dev->node, &input_dev_list);

    list_for_each_entry(handler, &input_handler_list, node)
		input_attach_handler(dev, handler);

    ...
}

```
最核心的部分其实和 handler 的注册几乎一致，将 dev 添加到系统全局链表中，遍历整个 handler 链表，寻找当前与 dev 匹配的 handler。   

多出来的一个操作是 device_add，这个接口应该是非常熟悉了，它负责根据传入的 struct device 结构在用户空间注册各种文件操作接口，其中包括 /dev/ 和 /sys/ 目录下的文件接口。  


## input_attach_handler
既然在两个关键结构(handler 和 dev)部分都有这个匹配函数，自然是需要进一步对其进行解析。  

首先，我们来看看在 evdev.c 中，默认注册到系统中的 handler 的定义：

```
static const struct input_device_id evdev_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			
};

static struct input_handler evdev_handler = {
	.event		= evdev_event,       // 事件上报操作
	.events		= evdev_events,      // 多事件上报操作
	.connect	= evdev_connect,     // dev 和 handler 匹配上之后的链接操作
	.disconnect	= evdev_disconnect,  // 注销时的解链接操作
	.legacy_minors	= true,          //使用老的次设备号范围
	.minor		= EVDEV_MINOR_BASE,  //次设备号的基础值
	.name		= "evdev",           //名称
	.id_table	= evdev_ids,         //id_table,主要用于匹配
};
```

既然是匹配，自然需要 dev 的参与。事实上，用户在编写设备驱动时，基本上只需要设备对应的时间，通常不需要设置匹配相关的参数，比如只需要设置下列的参数：
```
gpio_dev = input_allocate_device();
set_bit(EV_KEY,gpio_dev->evbit);
set_bit(KEY_A,gpio_dev->keybit);
input_register_device(gpio_dev);
```
根本见不到匹配相关参数的影子，那么，dev 和 handler 是如何匹配的呢？  

自然是需要接着看源代码：

```
static int input_attach_handler(struct input_dev *dev, struct input_handler *handler)
{
	const struct input_device_id *id;

	id = input_match_device(handler, dev);

	error = handler->connect(handler, dev, id);

	return error;
}

```
可以看到，整个 attach 包含两部分：匹配(match)和连接(connect)。  

### input_match_device

我们先看匹配部分，由 input_match_device 接口完成：
```
bool input_match_device_id(const struct input_dev *dev,
			   const struct input_device_id *id)
{
	if (id->flags & INPUT_DEVICE_ID_MATCH_BUS)
		if (id->bustype != dev->id.bustype)
			return false;

	...

	if (!bitmap_subset(id->evbit, dev->evbit, EV_MAX) ||
	    !bitmap_subset(id->keybit, dev->keybit, KEY_MAX) ||
	    ...
	    !bitmap_subset(id->propbit, dev->propbit, INPUT_PROP_MAX)) {
		return false;
	}

	return true;
}

static const struct input_device_id *input_match_device(struct input_handler *handler,
							struct input_dev *dev)
{
	const struct input_device_id *id;

	for (id = handler->id_table; id->flags || id->driver_info; id++) {
		if (input_match_device_id(dev, id) &&
		    (!handler->match || handler->match(handler, dev))) {
			return id;
		}
	}

	return NULL;
}



```

首先，在 for 循环中初始化 id = handler->id_table,再调用 input_match_device_id 函数，input_match_device_id 中是针对 handler->id_table 一系列的设置和检查，回头看看 evdev 中 handler 的 id_table 的定义：
```
static const struct input_device_id evdev_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			
};
```
仅仅定义了 driver_info 成员为 1，其它的 flag 都没有设置，input_match_device_id 所以返回真。  

match 中的另一部分为判断 (!handler->match || handler->match(handler, dev))，但是 evdev 中 handler 并未注册 .match 函数，所以同样返回真，所以整个 match 的匹配是成功的。   

仔细一看你会发现，系统默认注册的 evdev 类型的 handler，实际上是可以匹配任何注册的 dev 结构的，这也是内核中通用的 handler。  



## handler->connect()
在 input_attach_handler 中，除了匹配(match)，还有另一部分：连接(connect)。连接的源码直接是调用所注册 handler 的 .connect 成员.
