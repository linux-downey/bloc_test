# linux RTC framwork

1、rtc 分为好几种，内部rtc(直接连到总线上，比如ds1612)，外部rtc(spi,i2c，比如ds1307)，在驱动程序设计上的区别。
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


