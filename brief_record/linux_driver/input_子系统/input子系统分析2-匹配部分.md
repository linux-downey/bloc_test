# input 子系统-设备的初始化与注册
分析完 input 子系统的三大组件： dev、handler、handle，我们来看看子系统中的基础部分：子系统的初始化以及设备文件的注册。   

## 子系统初始化
子系统的初始化在 drivers/input/input.c 中：

```
struct class input_class = {
	.name		= "input",
	.devnode	= input_devnode,
};

static int __init input_init(void)
{
	int err;

	err = class_register(&input_class);

	err = input_proc_init();

	err = register_chrdev_region(MKDEV(INPUT_MAJOR, 0),
				     INPUT_MAX_CHAR_DEVICES, "input");

	return 0;
}

subsys_initcall(input_init);
```

subsys_initcall 是内核中的 initcall 机制，默认情况下将被编译进内核中。   

子系统的 input_init 函数主要做了两件事：

* 注册一个 class，并为 input 子系统申请设备号，其中主设备号为  INPUT_MAJOR(13)，而次设备号从 0 开始，最大 size 为 1024.所以系统中默认最多可以存在 1024 个输入设备，当然这是完全够了。当然，这里并没有调用 device_add 来创建设备节点，仅仅是创建 class 和申请了设备号范围，注册设备文件的操作应该由用户完成。  

* 将 input 子系统的内核运行信息导出到 procfs 中，即 /proc 目录下。

```
static int __init input_proc_init(void)
{
	struct proc_dir_entry *entry;

	proc_bus_input_dir = proc_mkdir("bus/input", NULL);

	entry = proc_create("devices", 0, proc_bus_input_dir,
			    &input_devices_fileops);

	entry = proc_create("handlers", 0, proc_bus_input_dir,
			    &input_handlers_fileops);

	return 0;
}
```
input_proc_init 接口在 /proc/ 目录下创建了一个 bus/input 目录，然后在 /proc/bus/input 目录下创建了两个文件，一个 devices，一个 handlers，可以猜到的是，这两个文件一个是描述系统中的 input dev,一个描述系统中的 handlers。  

该文件的访问控制分别由 input_devices_fileops 和 input_handlers_fileops 这两个 struct file_operations 类型的数据结构控制，了解字符设备驱动的都熟悉 struct file_operations 结构的作用，这里就不再赘述，有兴趣的朋友可以去研究研究。  



## handler 的初始化
系统默认的 handler 的初始化在上一章中有涉及到，源代码在 drivers/input/evdev.c 中：

```
static struct input_handler evdev_handler = {
	.event		= evdev_event,
	.events		= evdev_events,
	.connect	= evdev_connect,
	.disconnect	= evdev_disconnect,
	.legacy_minors	= true,
	.minor		= EVDEV_MINOR_BASE,
	.name		= "evdev",
	.id_table	= evdev_ids,
};

static int __init evdev_init(void)
{
	return input_register_handler(&evdev_handler);
}

module_init(evdev_init);

```
通常情况下，该模块是默认编译到内核中的，handler 结构必须先填充 .events/.connect 等结构体成员，这些回调函数负责 dev 的连接、事件的处理和上报。  


## 设备文件的创建
在 input 系统的初始化中，创建了 class 同时申请了设备号，那么，设备节点是在哪里创建的呢？  

上一章中提到，每一次添加 dev 或者 handler 到系统中，都将触发系统的匹配，当匹配上时，将会调用 handler->connect 接口，这个接口除了构建一个 handle 之外，同时也负责创建设备节点。   

```

struct evdev {
    ...
	struct input_handle handle;
	struct list_head client_list;
	struct device dev;
	struct cdev cdev;
    ...
};

static int evdev_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct evdev *evdev;
    int minor;
    
    ...

    minor = input_get_new_minor(EVDEV_MINOR_BASE, EVDEV_MINORS, true);

    evdev->dev.devt = MKDEV(INPUT_MAJOR, minor);
	evdev->dev.class = &input_class;
	evdev->dev.parent = &dev->dev;
	evdev->dev.release = evdev_free;
	device_initialize(&evdev->dev);

	cdev_init(&evdev->cdev, &evdev_fops);

	error = cdev_device_add(&evdev->cdev, &evdev->dev);

    ...

}
```

在 connect 函数中，申请了一个 struct evdev 类型的结构体，这个结构体中包含了 handle、struct cdev 类型的 dev，其中 struct cdev 就是构建字符设备的核心结构体。  

在上述函数中，首先从 input 子系统中申请一个空闲的次设备号，由此得出设备号，再赋值 class，设置 parent 等。  

接着，调用熟悉的 cdev_init 和 cdev_device_add 接口,在 /dev 和 /sys 目录下创建设备接口.  

其中值得关注的就是 struct file_operations 类型的 evdev_fops.这个结构体中的回调函数直接对应着用户对设备接口的访问:

```
static const struct file_operations evdev_fops = {
	.owner		= THIS_MODULE,
	.read		= evdev_read,
	.write		= evdev_write,
	.poll		= evdev_poll,
	.open		= evdev_open,
	.release	= evdev_release,
	.unlocked_ioctl	= evdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= evdev_ioctl_compat,
#endif
	.fasync		= evdev_fasync,
	.flush		= evdev_flush,
	.llseek		= no_llseek,
};
```
其中包括 .open/.read/.write/.poll/ 等操作接口. 对应着用户对设备(例如 /dev/input/event1)的打开/读/写等操作.  












