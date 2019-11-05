在设备树中，会对每个bank设置一个节点

根据节点的 compatible 属性，找到对应的 driver 实现

设置 gpio_bank,为每个 bank 绑定一个 gpio_chip

gpio_chip 中实现了对应的函数，比如 request、get_direction、set_direction 等，都是在对应的 driver 中被赋值。

对每个 bank 都会有一个对应的 gpio_device 结构体，其中包含 struct gpio_desc\* descs;查找的时候根据这个descs进行
所有的配置。  


omap_gpio_chip_init()  //初始化一个chip，为chip中每个函数指针赋值
	在gpiochip_add_data() // 申请一个 gpio_device 结构，gpio_device 和 chip互相用指针对应，分配 ngpio个gpio_desc
							每一个gpio都将会对应一个gpio_desc.再在程序后面设置 gpio_desc。
							
							
							
							
gpio_desc 描述：

struct gpio_desc {
	struct gpio_device	*gdev;
	unsigned long		flags;
/* flag symbols are bit numbers */
#define FLAG_REQUESTED	0
#define FLAG_IS_OUT	1
#define FLAG_EXPORT	2	/* protected by sysfs_lock */
#define FLAG_SYSFS	3	/* exported via /sys/class/gpio/control */
#define FLAG_ACTIVE_LOW	6	/* value has active low */
#define FLAG_OPEN_DRAIN	7	/* Gpio is open drain type */
#define FLAG_OPEN_SOURCE 8	/* Gpio is open source type */
#define FLAG_USED_AS_IRQ 9	/* GPIO is connected to an IRQ */
#define FLAG_IS_HOGGED	11	/* GPIO is hogged */
#define FLAG_SLEEP_MAY_LOOSE_VALUE 12	/* GPIO may loose value in sleep */

	/* Connection label */
	const char		*label;
	/* Name of the GPIO */
	const char		*name;
};




从 gpio_request() 
	gpio_to_desc(gpio)  //从 gpio 号找到对应的 gpio_desc
		list_for_each_entry(gdev, &gpio_devices, list)  //从 gpio_devices list 中找到对应的 gpio_devices,每一个bank对应一个gpio_device
		从 gpio_device 结构体中，通过gpio号的偏移，找到对应的 descs，return &gdev->descs[gpio - gdev->base];
		所以，一个引脚对应一个 gpio_desc,一个 bank 的 gpio desc 都被存放在 gpio_device->gpio_descs[]中，一个bank对应一个gpio_device,
		同时，一个bank对应一个gpio_chip()
	gpiod_request(desc, label);  //然后使用 gpio_desc ，label 再request gpio
		__gpiod_request(desc, label);  
			struct gpio_chip	*chip = desc->gdev->chip;  //获取gpio_chip
			chip->request(chip, gpio_chip_hwgpio(desc))    //执行 chip->request(),这是在注册的时候被指定的。
			chip->get_direction()                          //执行 chip->direction(),这是在注册的时候被指定的。
		
从 gpio_set_value()
	#define gpio_set_value  __gpio_set_value
		gpiod_set_raw_value(gpio_to_desc(gpio), value);
			_gpiod_set_raw_value(desc, value);
			
				chip->set(chip, gpio_chip_hwgpio(desc), value);  //最后也是调用了对应的 chip->set()函数

