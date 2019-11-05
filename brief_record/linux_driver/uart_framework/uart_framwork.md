串口的驱动主要是三个部分：

uart_register_driver()  // 注册一个 uart driver

uart_port  //对应一个uart端口

uart_ops //对应的uart操作部分





以 beagle bone为例：使用 uart 控制器为 8250。  

第一部分：系统初始化时，先将 8250 的 driver 部分注册到系统中:

对应的代码为：8250_core.c

在init函数中，

//一个driver对应一类设备
static struct uart_driver serial8250_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "serial",
	.dev_name		= "ttyS",
	.major			= TTY_MAJOR,
	.minor			= 64,
	.cons			= SERIAL8250_CONSOLE,
};


static const struct file_operations tty_fops = {
	.llseek		= no_llseek,
	.read		= tty_read,
	.write		= tty_write,
	.poll		= tty_poll,
	.unlocked_ioctl	= tty_ioctl,
	.compat_ioctl	= tty_compat_ioctl,
	.open		= tty_open,
	.release	= tty_release,
	.fasync		= tty_fasync,
	.show_fdinfo	= tty_show_fdinfo,
};


uart_register_driver(&serial8250_reg);   //注册一个uart_driver
	normal = alloc_tty_driver(drv->nr);  //
	drv->tty_driver = normal;            //最主要的部分就是初始化一个tty_driver
	tty_set_operations(normal, &uart_ops);  //为对应的 tty_driver 绑定一个 tty_ops 
	tty_register_driver(normal)    		// 最后，注册一个tty驱动
		alloc_chrdev_region(&dev, driver->minor_start,driver->num, driver->name);  //申请设备号
		tty_cdev_add(driver, dev, 0, driver->num);   //添加一个cdev
			driver->cdevs[index]->ops = &tty_fops;   //设置对应的 struct file_operations 结构体
			err = cdev_add(driver->cdevs[index], dev, count);  //创建一个设备文件
		tty_register_device(driver, i, NULL);  //创建/dev设备
		proc_tty_register_driver(driver);   //在proc下创建/dev设备
		
		
在文件8250_omap.c中，对应的uart_probe函数，
omap8250_probe(struct platform_device *pdev)  //在该函数中，对port进行对应的设置
	serial8250_register_8250_port()  //
		uart_add_one_port(&serial8250_reg,&uart->port);  //注册port
			

		
		

uart主要的部分在 port.startup中，





从应用层访问 tty 设备，将对应访问到：
	file_operations tty_fops.open()
		open 将调用 tty_driver 的 open 函数
			调用对应的 port ->ops->activate
				调用 uart_startup()
					在start_up中注册对应的串口中断，然后进行数据的收发，将数据传输到对应的缓冲。
	
	file_operations tty_fops.read()
		read 将直接调用struct tty_ldisc *ld->ops->read
		
	file_operations tty_fops.write()
		将调用 tty_driver->ops->write()
	


驱动主要开发部分：
	填充并注册一个 uart_driver
	添加一个uart_port ，uart_add_one_port()
		在uart_port.startup中，将注册对应的串口中断，将数据收发分配到对应的缓冲区中。    
		
	

