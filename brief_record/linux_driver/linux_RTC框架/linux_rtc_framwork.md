# linux RTC 内核驱动框架

RTC(Real Time Clock),通常被作为系统的实时时钟，与时钟不一样的是：时钟负责提供一种时间刻度，而实时时钟则是提供现实世界的时间，CPU 可以通过读取 RTC 来获取当前时间。  

了解了这些，就会发现 RTC 的功能并不复杂，无非就是设置时间、读取时间，某些高端的 RTC 设备上还会带有存储、闹钟等功能。总的来说，RTC 的内核驱动还是比较简单的。  


## 驱动入口
针对内核驱动而言，RTC 可以分为两种：内部 RTC 和外部 RTC,内部 RTC 是继承在 CPU 内部的，连接到系统总线上，它的操作接口被映射到内存中。而外部 RTC 则是通过接口进行扩展，通常是 i2c 或者 spi 接口。  

从驱动的编写层面来说，内部 RTC 和外部 RTC 的除了在访问方式上，另一个比较大的区别就是设备树节点的描述。

### 内部 RTC 设备树
对于内部 RTC 而言，对应设备树的描述可以是普通节点、或者是 compatible = "simple-bus" 节点的子节点，这样，该设备树节点会被解析成 struct platform_device 结构。  

设备树的描述通常如下:

```
compatible = "VID,xxx-rtc";
reg = <0x10000 0x400>;
interrupts = <0x3f>;
clocks = <0x3f>;
clock-names = "rtc-clock";
...
```
与外部 RTC 一个非常大的区别就是，内部 RTC 可以独占中断线，而外部 RTC 因为通常借助串行通信外设进行通信，中断只能借助串行通信外设来实现，而且实现非常不灵活。  

同时，由于是连接在系统总线上，需要使用 reg 字段来指明映射的内存地址和长度，而且RTC 需要时钟来驱动，所以需要指定时钟源。  

### 外部 RTC 设备树

#### i2c 设备节点

而对于外部 RTC，设备树节点通常是对应通信总线节点下的子节点，比如 i2c 下的 RTC 节点：
```
i2c@8000000 {
    compatible = "VID,xxx-i2c";
    ...
        rtc1 {
            compatible = "VID,xxx-rtc";
            reg = <0x68>;
            ...
        };
};
```
这里的 reg 字段和上述的 reg 不一样，i2c controller 子节点的 reg 将被解析成 i2c 从设备地址，i2c 通过该地址对设备进行操作,当然，这里并没有考虑特殊情况下的 i2c 设置，比如：高速 i2c配置，10位地址。详细资料可以参考[官方文档](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/i2c/i2c.txt)   

在 i2c 设备子节点中，rtc 节点将会被解析为 struct i2c_client 结构。  

#### spi 设备节点
spi 设备节点的形式和 i2c 相差无几，它的描述通常如下：
```
spi@8000000 {
    compatible = "VID,xxx-spi";
    ...
        rtc1 {
            compatible = "VID,xxx-spi";
            spi-max-frequency = <20000000>;
            reg = <0x1>;
            ...
        };
};
```
在 spi 设备中， reg 属性并不会被 spi 主机驱动解析，而是留给用户以进行自定义配置，spi-max-frequency 则是表示当前节点支持的最大 spi 速率。spi 设备树配置的详细资料可以参考[官方文档](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/spi/spi-controller.yaml).  

spi 设备子节点总，该 rtc 节点将被解析为 struct spi_device 结构。


当然，不论是内部还是外部形式的 RTC 设备树节点，完全可以在设备树中进行其他的描述，比如 rtc 的 alarm 相关信息、内部存储等相关信息。在上文中列出的 reg 或者 spi-max-frequency，这些都是由设备树自动处理，而这些自定义的属性信息，我们可以在驱动程序中进行处理，相当于使用设备树进行传参。  


## RTC 的注册
了解了设备树的节点形式，接下来就是进入到驱动程序中。对于不同的设备树节点，有不同的匹配方式，当然其最后的结果都是会进入到驱动程序部分的 probe 函数。  

在本章节中，我们将基于一款经典的 RTC 设备驱动程序进行驱动注册的分析：ds1302.相对于其他的 RTC 设备来说，这款 RTC 较为简单，只实现了我们所需要的读写功能，这正是我们所要的，毕竟我们的目标是研究框架，就没必要引入那么多额外的元素。    

对于驱动程序设备树匹配的部分就不赘述了，不懂的朋友可以参考我之前的博客，我们直接从 probe 函数开始。  

ds1302 的 probe 源码如下：
```C
static int ds1302_probe(struct spi_device *spi)
{
	struct rtc_device	*rtc;
	u8		addr;
	u8		buf[4];
	int		status;

    ...
	addr = RTC_ADDR_CTRL << 1 | RTC_CMD_READ;
	status = spi_write_then_read(spi, &addr, sizeof(addr), buf, 1);
	if (status < 0) {
		dev_err(&spi->dev, "control register read error %d\n",
				status);
		return status;
	}
	
	spi_set_drvdata(spi, spi);

	rtc = devm_rtc_device_register(&spi->dev, "ds1302",
			&ds1302_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		status = PTR_ERR(rtc);
		dev_err(&spi->dev, "error %d registering rtc\n", status);
		return status;
	}

	return 0;
}

```
为了专注于框架的分析，省略去对设备的框架无关操作：主要是检查当前设备支持的 spi 属性是否和设备树提供的匹配，同时对 ds1302 做一些初始化的操作。  

可以看到，ds1302 是基于 spi 的 RTC 驱动，因为在设备树中作为 spi controller 节点，所以被解析成 struct spi_device 结构，当需要进行 spi 的读写时，直接调用 spi 相关 api，将 struct spi_device 结构传入即可完成数据的传输。因为在 spi 设备树节点中，已经指定了需要使用的 spi 控制器、传输速率、传输模式等。  

这里我们需要特别关注的是：devm_rtc_device_register()，这是 RTC 驱动的关键部分：将一个 RTC 结构注册到内核中。   

注册成功之后，将会在 /sys/class 目录下生成驱动相关操作文件，以及在 /dev/ 下生成相应的设备文件，通常是 /dev/rtcn。在用户空间就可以通过这些接口对 RTC 进行读写操作。  

在函数 devm_rtc_device_register(&spi->dev, "ds1302",&ds1302_rtc_ops, THIS_MODULE) 中，它的函数原型是这样的：
```C
struct rtc_device *devm_rtc_device_register(struct device *dev,
					                    const char *name,
					                    const struct rtc_class_ops *ops,
					                    struct module *owner)
```

共传入四个参数：
* dev:这是一个 struct device 类型的参数，这个参数是针对内核的资源管理，主要是传递给内核资源的自动回收机制，并不针对 RTC 的注册。  
* name:RTC 的名字，事实上，这个名字并没有被用在内核中，而是仅仅在 debug 的过程使用，当驱动导出用户操作文件到 /sys/ 目录下或者 /dev/ 下的设备文件名是以 /dev/rtcn 命名，又或者以driger.name 命名，通常是 rtc-ds1302。  
* ops：这个就是我们非常熟悉的 ops 类函数集，按照过往的经验，它就是 RTC 设备读写的回调函数集了，当用户操作该 rtc 设备时，就会调用到该函数集中对应的函数。  
* owner：指定当前驱动的属主，在 insmod 的时候进行绑定，主要用于模块的引用计数，从而控制模块的加载卸载。  


既然说到回调函数，自然是要看看 RTC 的回调函数集：
```
struct rtc_class_ops {
	int (*ioctl)(struct device *, unsigned int, unsigned long);
	int (*read_time)(struct device *, struct rtc_time *);
	int (*set_time)(struct device *, struct rtc_time *);
	int (*read_alarm)(struct device *, struct rtc_wkalrm *);
	int (*set_alarm)(struct device *, struct rtc_wkalrm *);
	int (*proc)(struct device *, struct seq_file *);
	int (*set_mmss64)(struct device *, time64_t secs);
	int (*set_mmss)(struct device *, unsigned long secs);
	int (*read_callback)(struct device *, int data);
	int (*alarm_irq_enable)(struct device *, unsigned int enabled);
	int (*read_offset)(struct device *, long *offset);
	int (*set_offset)(struct device *, long offset);
	void (*power_off_program)(struct device *);
};
```
整个回调函数集内核不少，主要分为三类：
* RTC 的基本操作，包括读(read)、写(set)、读回调、关机指令等。
* 闹钟的读(read)、写(set),因为某些 RTC 设备还支持闹钟功能，甚至还有一些带有中断引脚，将 RTC 的中断引脚连接到 CPU 的 IO 上，就可以实现中断通知功能
* readd/set offset:这两个函数主要针对于带有存储器的 RTC 设备，设置读写偏移地址。  


对于 ds1302 而言，功能实现非常简单，就是简单的读写：
```C
static int ds1302_rtc_set_time(struct device *dev, struct rtc_time *time)
{
	struct spi_device	*spi = dev_get_drvdata(dev);
	u8		buf[1 + RTC_CLCK_LEN];
	u8		*bp = buf;
	int		status;

	/* Enable writing */
	bp = buf;
	...

	status = spi_write_then_read(spi, buf, 2,
			NULL, 0);
    ...

	return spi_write_then_read(spi, buf, sizeof(buf),
			NULL, 0);
}

static int ds1302_rtc_get_time(struct device *dev, struct rtc_time *time)
{
	struct spi_device	*spi = dev_get_drvdata(dev);
	u8		addr = RTC_CLCK_BURST << 1 | RTC_CMD_READ;
	u8		buf[RTC_CLCK_LEN - 1];
	int		status;
    ...
	status = spi_write_then_read(spi, &addr, sizeof(addr),
			buf, sizeof(buf));
    ...
	return rtc_valid_tm(time);
}

static const struct rtc_class_ops ds1302_rtc_ops = {
	.read_time	= ds1302_rtc_get_time,
	.set_time	= ds1302_rtc_set_time,
};
```

两个读写函数也没有什么特别的，读函数就是通过 spi 总线将数据从 RTC 设备中读出来，放到传入的指针参数处。而写函数就是将用户传入的 struct rtc_time 结构的时间参数通过 SPI 对 RTC 设备进行对应的设置。  






2、devm_rtc_device_register：传入的三个参数
3、rtc_device_register 的调用：
    rtc_allocate_device：
        申请一个 rtc 设备，
            rtc->dev.class = rtc_class;  //rtc_class 由 drivers/rtc/class.c 的 init 函数注册
	        rtc->dev.groups = rtc_get_dev_attribute_groups(); //这个表示在 /sys 下的文件创建
            rtc->dev.release = rtc_device_release;
        
    rtc_dev_prepare：创建 cdev 设备：
        cdev_init(&rtc->char_dev, &rtc_dev_fops);

        rtc_dev_fops 中有对应的 file_operations 结构，

            其中.unlocked_ioctl	= rtc_dev_ioctl


