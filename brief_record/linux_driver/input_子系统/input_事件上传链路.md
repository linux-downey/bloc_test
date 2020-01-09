# input 子系统事件上传
在前面系列章节的介绍中,已经对 input 子系统的实现有了一个大致的讲解,这一章节我们来系统性地探讨一个最重要的问题:时间产生的数据是如何一层层被上传到用户空间的?这才是我们最想要了解的东西.    


## 打开文件
当用户需要获取设备的输入事件时,必须打开设备文件,这个文件名通常是 /dev/input/eventn,当使用系统调用 open 时,会调用到字符设备驱动中的 struct file_operations 结构中的 .open 函数,这个结构在上一章节中有介绍:

```
static const struct file_operations evdev_fops = {
	.owner		= THIS_MODULE,
    ...
	.open		= evdev_open,
	...
};
```

其中,evdev_open 的源码实现如下:

```
static int evdev_open(struct inode *inode, struct file *file)
{
    //根据 cdev 定位到 evdev 结构.  
	struct evdev *evdev = container_of(inode->i_cdev, struct evdev, cdev);
	unsigned int bufsize = evdev_compute_buffer_size(evdev->handle.dev);
	unsigned int size = sizeof(struct evdev_client) +
					bufsize * sizeof(struct input_event);
	struct evdev_client *client;

    //申请一个 struct evdev_client 类型的 client.
	client = kzalloc(size, GFP_KERNEL | __GFP_NOWARN);

	client->bufsize = bufsize;
	client->evdev = evdev;
    //将 evdev 和 client 绑定
	evdev_attach_client(evdev, client);

    //打开一个 evdev 设备
	error = evdev_open_device(evdev);

	file->private_data = client;

	return 0;

}
```

在上一章节有讲到,当 handler 和 dev 匹配成功,调用 handler->connect ,在 connect 函数中申请并构建一个 struct evdev 结构,该 evdev 结构算是整个 evdev 方式上报数据的数据集合体,经常看内核代码的盆友应该清楚这种应用,将驱动所有需要用到的数据集合在一个结构体中,在回调函数中使用该结构体.  

这里通过嵌入到 evdev 的 cdev 结构获取到 evdev 结构,同时构建一个 struct evdev_client 类型的结构体,同时将 evdev 和 client 进行绑定.
绑定由 evdev_attach_client(evdev, client) 完成,它的实现是这样的:

```
static void evdev_attach_client(struct evdev *evdev,
				struct evdev_client *client)
{
	list_add_tail_rcu(&client->node, &evdev->client_list);
}
```
也就是将当前申请的 client 链接到 evdev 的 client_list 中. 

client 可以理解为用于在驱动上传数据使用的 buf,每打开一个进程,就将创造一个 client,链接到同一个 evdev 上,由此可以看出,在驱动上传事件相关的数据时,将会同时给所有已打开该接口文件的进程上传一份.  

同时,evdev_open 函数中还调用了 evdev_open_device,顾名思义,这个函数的作用就是打开设备.它的实现是这样的:

```
int input_open_device(struct input_handle *handle)
{
	struct input_dev *dev = handle->dev;
	int retval;

	if (dev->going_away) {
		retval = -ENODEV;
		goto out;
	}

	handle->open++;

	if (!dev->users++ && dev->open)
		retval = dev->open(dev);

	return retval;
}
```
该函数传入的参数为 handle,也就是 dev 和 handler 匹配成功的产物,

首先判断 dev->going_away,当该值为真时,返回,表示设备已经走远(注销).

然后,handle->open++,这个 open 记录了该设备节点(/dev/input/eventn)被多少个进程打开.  

同时,判断 dev->users++ && dev->open,dev->users 同样是记录有多少个用户打开了该设备(一个属于handle一个属于dev),当 dev->users 为 0 时,表示该设备目前是第一次被打开,需要调用 dev->open 函数.  

由此可以看出,用户在注册 struct input_dev 结构到系统时,可以将一些初始化工作放到 .open 回调函数中进行初始化.  



## 事件上报
在这一头,用户已经打开设备只等接收事件数据了,而另一头,我们就从内核中的事件上报出发,追踪源代码.

首先,事件上报的源头自然是事件触发,这个触发可能是 IO 中断,i2c/spi/USB 中断等,总归是内核接收到了输入的信号(数据).然后,在中断处理部分(或者是tasklet/任务队列/定时回调)进行数据的上报.  

事件上报的接口有很多:

```
void input_report_key(struct input_dev *dev, unsigned int code, int value);
void input_report_rel(struct input_dev *dev, unsigned int code, int value)
...
```

这一类 input_report_* 函数负责上报事件,其实都是基于 input_event 的封装,其核心函数还是 input_event.

```
void input_event(struct input_dev *dev,
		 unsigned int type, unsigned int code, int value)
{
	unsigned long flags;

	if (is_event_supported(type, dev->evbit, EV_MAX)) {
		input_handle_event(dev, type, code, value);
	}
}
```
input_event 接受四个参数:
* dev :input dev 结构,这是由用户初始化并注册的.
* type:事件的类型,比如按键事件/位移事件...
* code:事件类型对应的枚举值参数, 比如 KEY_A,KEY_B 这些预定义的特定的键值.  
* value:表示输入的状态,比如按下表示 1,松开表示 0.

对于事件的设置,就是设置 dev 中相应的 bit,在 input_event 中将会检查当前上报的事件用户是否有设置,如果没有设置就不支持当前事件的上报.  

事件检查通过之后,调用 input_handle_event :
```
static void input_handle_event(struct input_dev *dev,
			       unsigned int type, unsigned int code, int value)
{
    //检查事件类型并对部分事件类型进行特殊处理
	int disposition = input_get_disposition(dev, type, code, &value);

	if (disposition != INPUT_IGNORE_EVENT && type != EV_SYN)
		add_input_randomness(type, code, value);

	if ((disposition & INPUT_PASS_TO_DEVICE) && dev->event)
		dev->event(dev, type, code, value);

	if (disposition & INPUT_PASS_TO_HANDLERS) {
		struct input_value *v;

		if (disposition & INPUT_SLOT) {
			v = &dev->vals[dev->num_vals++];
			v->type = EV_ABS;
			v->code = ABS_MT_SLOT;
			v->value = dev->mt->slot;
		}
    //构建一个 input_valu结构体
		v = &dev->vals[dev->num_vals++];
		v->type = type;
		v->code = code;
		v->value = value;
	}

    //
	if (disposition & INPUT_FLUSH) {
		if (dev->num_vals >= 2)
			input_pass_values(dev, dev->vals, dev->num_vals);
		dev->num_vals = 0;
	} else if (dev->num_vals >= dev->max_vals - 2) {
		dev->vals[dev->num_vals++] = input_value_sync;
		input_pass_values(dev, dev->vals, dev->num_vals);
		dev->num_vals = 0;
	}

}
```
在 input_handle_event 中,首先判断事件类型并对一些事件类型进行特殊处理,比如:对于按键/位移事件 disposition 的值为 INPUT_PASS_TO_HANDLERS,而对于音频/重复上报事件值为 INPUT_PASS_TO_ALL,INPUT_PASS_TO_ALL 的值为 (INPUT_PASS_TO_HANDLERS | INPUT_PASS_TO_DEVICE).  

对于 INPUT_PASS_TO_DEVICE,将调用 dev->event 的事件处理函数,该函数主要针对 EV_LED 或者 EV_SND,进行一些操作比如点亮 led,播放声音.   

其余的大部分设置都是 INPUT_PASS_TO_HANDLERS,当 disposition 的值为这个时,通过当前上报事件的 type/code/value 构造一个 input_value 结构,该结构被存储在 (struct input_dev)dev->vals 数组中.   

所有上报的事件都被存储在 (struct input_dev)dev->vals 中,那么什么时候会上报呢?  

上述源码中可以看到,当 disposition & INPUT_FLUSH 为逻辑 1 时,调用 input_pass_values 上报事件数据,也就是 input_value,或者 dev->vals 存放的事件数据快满了就触发上报事件数据.   

INPUT_FLUSH 是在 input_get_disposition 函数中设置的:
```
static int input_get_disposition(struct input_dev *dev,
			  unsigned int type, unsigned int code, int *pval)
{
	int disposition = INPUT_IGNORE_EVENT;
	case EV_SYN:
		switch (code) {
			...
		case SYN_REPORT:
			disposition = INPUT_PASS_TO_HANDLERS | INPUT_FLUSH;
			break;
			...
		}
		break;
	...
```

可以看出,当传入的事件 type 为 EV_SYN 时,disposition 被赋值为 INPUT_PASS_TO_HANDLERS | INPUT_FLUSH,此时触发事件数据的上报,其实 EV_SYN 这个 type 对应的函数接口为 input_sync() .

所以,这就是为什么当我们上传事件的时候,最后需要调用 input_sync() 进行数据同步了,只有这样事件才会真正上报.


接下来,我们继续看事件上报函数 input_pass_values 的实现:
```
static void input_pass_values(struct input_dev *dev,
			      struct input_value *vals, unsigned int count)
{
	struct input_handle *handle;
	struct input_value *v;

	handle = rcu_dereference(dev->grab);
	if (handle) {
		count = input_to_handler(handle, vals, count);
	} else {
		list_for_each_entry_rcu(handle, &dev->h_list, d_node)
			if (handle->open) {
				count = input_to_handler(handle, vals, count);
				if (!count)
					break;
			}
	}

	...
	/*重复上报事件*/
}
```

该函数一共三个参数:
* input_dev
* struct input_value *vals:需要上传的事件数据数组
* unsigned int count:事件数组的个数

在函数实现中,首先判断 dev->grab,这是一个抓取设备,抓取设备表示所有该输入设备的输入事件上报只会由该设备接受.而不会像普通设备那样上报到每个打开设备文件的进程中.    

接下来就是对普通输入设备的处理,list_for_each_entry_rcu(handle, &dev->h_list, d_node) 表示:遍历 dev->hlist 中的每个 handle,也就是由当前 dev 构建的 handle ,如果 handle->open 不为 0 ,就调用 input_to_handler 函数上报事件,在上文中有提到,handle->open 记录的是当前打开设备文件的任务数.  

接着看 input_to_handler 上报事件函数:
```
static unsigned int input_to_handler(struct input_handle *handle,
			struct input_value *vals, unsigned int count)
{
	struct input_handler *handler = handle->handler;
	struct input_value *end = vals;
	struct input_value *v;

	if (handler->filter) {
		for (v = vals; v != vals + count; v++) {
			if (handler->filter(handle, v->type, v->code, v->value))
				continue;
			if (end != v)
				*end = *v;
			end++;
		}
		count = end - vals;
	}

	if (!count)
		return 0;

	if (handler->events)
		handler->events(handle, vals, count);
	else if (handler->event)
		for (v = vals; v != vals + count; v++)
			handler->event(handle, v->type, v->code, v->value);

	return count;
}
```
该函数已经是事件上报的最后阶段,首先判断 handler->filter 回调函数是否被设置,如果被设置,就先执行 filter,过滤掉指定的事件.  

否则,就调用 handler->events 或者 handler->event 来上报事件,而 handler->event/events 则是由注册 handler 时被定义的:
```
static struct input_handler evdev_handler = {
	.event		= evdev_event,
	.events		= evdev_events,
	...
};

static int __init evdev_init(void)
{
	return input_register_handler(&evdev_handler);
}
```
又回到了 evdev 中 handler 的注册,接下来就是来看看 events/event 两个回调函数的实现:

```
static void evdev_event(struct input_handle *handle,
			unsigned int type, unsigned int code, int value)
{
	struct input_value vals[] = { { type, code, value } };

	evdev_events(handle, vals, 1);
}
```
evdev_event 的实现非常简单,直接反手就把任务甩给 events 去处理,也就是把事件数设置为1,也是够懒的.  

接着看 events 的实现:

```
static void evdev_events(struct input_handle *handle,
			 const struct input_value *vals, unsigned int count)
{
	struct evdev *evdev = handle->private;
	struct evdev_client *client;
	ktime_t ev_time[EV_CLK_MAX];

	ev_time[EV_CLK_MONO] = ktime_get();
	ev_time[EV_CLK_REAL] = ktime_mono_to_real(ev_time[EV_CLK_MONO]);
	ev_time[EV_CLK_BOOT] = ktime_mono_to_any(ev_time[EV_CLK_MONO],
						 TK_OFFS_BOOT);

	...
	list_for_each_entry_rcu(client, &evdev->client_list, node)
		evdev_pass_values(client, vals, count, ev_time);

}

static void evdev_pass_values(struct evdev_client *client,
			const struct input_value *vals, unsigned int count,
			ktime_t *ev_time)
{
	struct evdev *evdev = client->evdev;
	const struct input_value *v;
	struct input_event event;
	bool wakeup = false;

	event.time = ktime_to_timeval(ev_time[client->clk_type]);

	for (v = vals; v != vals + count; v++) {
		if (__evdev_is_filtered(client, v->type, v->code))
			continue;

		if (v->type == EV_SYN && v->code == SYN_REPORT) {
			/* drop empty SYN_REPORT */
			if (client->packet_head == client->head)
				continue;

			wakeup = true;
		}

		event.type = v->type;
		event.code = v->code;
		event.value = v->value;
		__pass_event(client, &event);
	}

}

```
在 handler->evnets 实现中，先获取时间，因为时间也是事件数据上传的一部分，然后对于 evdev->client_list 中的每一个 client ，调用 evdev_pass_value 接着将数据往上报，还记不记得这个 client 是哪里来的？当用户空间使用 open 系统接口时，会调用到当前字符驱动的 ops->open 函数，这个 client 就是在 open 回调函数中创建的。  

evdev_pass_values 的实现也非常简单，对一些无用的重复的事件类型进行过滤，然后再调用 __pass_event 继续将数据上传。  

```
static void __pass_event(struct evdev_client *client,
			 const struct input_event *event)
{
	client->buffer[client->head++] = *event;
	client->head &= client->bufsize - 1;

	if (unlikely(client->head == client->tail)) {
		/*
		 * This effectively "drops" all unconsumed events, leaving
		 * EV_SYN/SYN_DROPPED plus the newest event in the queue.
		 */
		client->tail = (client->head - 2) & (client->bufsize - 1);

		client->buffer[client->tail].time = event->time;
		client->buffer[client->tail].type = EV_SYN;
		client->buffer[client->tail].code = SYN_DROPPED;
		client->buffer[client->tail].value = 0;

		client->packet_head = client->tail;
	}

	if (event->type == EV_SYN && event->code == SYN_REPORT) {
		client->packet_head = client->head;
		kill_fasync(&client->fasync, SIGIO, POLL_IN);
	}
}
```
最后的上传结果就在 __pass_event 中，将事件数据保存到 client->buffer 中，从 client->head/tail 可以看出，这个 buffer 是一个环形 buffer。

整个事件的上报圆满完成。   

但是，看到这里很多朋友肯定会有疑问，为什么数据上报到最后还是在内核空间，这些事件数据并没有到用户空间啊，那到底是怎么到空间的？

## 用户读取数据
数据准备好了，自然是需要用户去读取，当用户调用 read 系统接口时，调用对应的 ops->read 接口，这是设备文件接口对应的 ops 结构：
```
static const struct file_operations evdev_fops = {
	.read		= evdev_read,
};

static ssize_t evdev_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	struct evdev_client *client = file->private_data;
	struct evdev *evdev = client->evdev;
	struct input_event event;
	size_t read = 0;

	while (read + input_event_size() <= count &&
		       evdev_fetch_next_event(client, &event)) {

			if (input_event_to_user(buffer + read, &event))
				return -EFAULT;

			read += input_event_size();
		}

	}

	return read;
}

int input_event_to_user(char __user *buffer,
			const struct input_event *event)
{
		if (copy_to_user(buffer, event, sizeof(struct input_event)))
			return -EFAULT;
	}

	return 0;
}

```
在 read 回调函数中，先获取 evdev_client 结构，该结构中保存着内核上报的数据，然后取出其中的数据，调用 input_event_to_user ，在 input_event_to_user 中，使用我们非常熟悉的 copy_to_user 接口，将整个 event 数据传递到用户空间，用户就读取到了输入事件的数据。  

所以，用户读取到的数据实际上是 struct input_event 结构的数据;
```
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
```
该数据结构比较简单，仅仅是在 input_value 上增加了时间参数。  

当然，这其中也涉及到进程的睡眠与唤醒，有兴趣的朋友可以仔细看看。  

