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

对于 INPUT_PASS_TO_DEVICE,将提前调用


