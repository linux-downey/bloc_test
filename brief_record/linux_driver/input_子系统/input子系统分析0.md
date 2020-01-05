# input 子系统分析-dev、handler、handle

在上一章中，介绍了 input 设备驱动的编写，在这一章中，我们开始深入到子系统背后，看看 input 子系统的内部实现。  

内核中关于 input 子系统的实现在两个文件中：driver/input/input.c 和 driver/input/evdev.c 。  

整个结构体由三个核心结构体组成：
* struct input_dev;      
* struct input_handler;  
* struct input_handle;   

老规矩，为了照顾一些赶时间的朋友，我们先说结论，再贴代码。  

事实上，在内核中，设备的注册与事件的上报是分离的，设备的注册主要负责将 input 设备接口导出到用户空间，包括 /dev/、/sys/ 和 /proc 目录下文件的注册，这部分数据由 struct input_dev 来描述。  

而事件的上报，则是将驱动产生的输入事件进行处理(过滤、分发等)，然后传递到用户打开的接口文件中，这一部分由 struct handler 进行描述。

内核中将 input_dev(设备)与 input_handler(事件上报)分离，不同的设备可以支持不同的上报方式，内核中提供默认的事件上报方式，即 evdev，用户完全可以为某个自定义设备定义事件上报形式，这种模块化注册的方式在内核中非常常见。

### input_handler
对于输入设备内核驱动的编写，是不需要接触到 handler 的，系统默认提供了一种事件上报方式：evdev。  

在 driver/input/evdev.c 中，系统使用 input_register_handler 注册了一个 handler ，该接口将当前注册的 handler 连接到全局链表 input_handler_list 中。  

### input_dev
而 input_dev 结构是通过 input_register_device 接口注册到系统中的，同样的，系统中也维护了一个链表 ：input_dev_list，这个链表保存了所有注册到系统中的 input_dev.  


现在你的脑海中应该出现了像对联一样的两条链表，话说回来，当硬件产生事件上报时，必然是经由 handler 的接口将事件上报到设备接口，那么，input_handler 和 input_dev 是如何连接上的呢?  

在 input_register_device 和 input_register_handler 调用中，每插入一个 dev/handler，根据 dev/handler 的 id_table 尝试将 dev 链表中的节点与 handler 链表中的节点进行匹配，匹配成功之后调用 handler 的 connect 函数，构建一个 input_handle 结构。  


这个场景是不是很熟悉？是的，设备总线的匹配也是这样的模式，driver 部分维护一个链表，device 部分维护一个链表，每当有一个新的 driver/device 注册到系统中时，根据 driver/device 的 id table 尝试将 driver 和device 进行匹配，匹配成功就调用 driver 部分的 probe 函数。思路几乎是同出一辙。

到这里，dev、handler、handle 这三者的关系就比较明朗了：

dev 负责设备的注册，handler 负责事件的处理与上报，handle 则是两个匹配上之后生成的结合体，在事件上报的上层处理中，使用 handle 结构。这种配合有点像一方提供不同颜色的墨水，另一方提供不同规格的钢笔，系统从中各选出一种规格(match)组合到一起(connect),就可以用来创作了。  

接下来我们来看各结构体的定义,dev(struct input_dev) 可以参考上一章节：

## input_handler 的定义

input_handler 结构体的成员是这样的：

```
struct input_handler {

    //该 handler 的私有数据
	void *private;

    //事件上报函数
	void (*event)(struct input_handle *handle, unsigned int type, unsigned int code, int value);
    //多个事件上报函数
	void (*events)(struct input_handle *handle,
		       const struct input_value *vals, unsigned int count);
    //事件过滤函数
	bool (*filter)(struct input_handle *handle, unsigned int type, unsigned int code, int value);

    //input_dev 和 input_handler 进行匹配，返回匹配结果
	bool (*match)(struct input_handler *handler, struct input_dev *dev);

    //当input_dev 和 input_handler 匹配成功时，将两者进行连接，生成 input_handle
	int (*connect)(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id);
	
    //connect 的相反操作
    void (*disconnect)(struct input_handle *handle);
	
    //根据给定的 handle 开始处理事件的上报，该函数在 connect 函数被调用之后进行调用。
    void (*start)(struct input_handle *handle);

    //老的次设备号
	bool legacy_minors;
    //次设备号
	int minor;
    //该 handler 的名称
	const char *name;
    //handler 的 id table ，用作匹配。
	const struct input_device_id *id_table;

    //链表，成员为每一个与本 handler 有关的 input_handle 结构，
	struct list_head	h_list;

    //链表节点，被链入到 input_handler_list 中。
	struct list_head	node;
};
```

## input_handle 的定义

```
struct input_handle {

    //该 handle 的私有数据
	void *private;

    //计数器，记录当前的 handle 是否被打开(正在使用)
	int open;

    //当前 handle 的名称
	const char *name;

    //该 handle 对应的 dev
	struct input_dev *dev;

    //该 handle 对应的 handler
	struct input_handler *handler;

    //链表节点，将当前节点链接到对应 dev 的 h_list 中
	struct list_head	d_node;

    //链表节点，将当前节点链接到对应 handler 的 h_list 中
	struct list_head	h_node;
};
```





