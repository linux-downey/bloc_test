# pwm controller 框架
在上一章中我们简单介绍了 pwm 在设备树中的描述语法，严格来说，设备树仅仅是一个描述文件，也可以看成是一个配置文件。所以，更重要的工作是这个描述文件是怎么被读取和使用的，从而延伸到整个 pwm controller 驱动框架是如何在内核中成型的。  

这一章我们持续深入，基于 beaglebone 平台，linux 4.14版本内核，来看看设备树中 controller 节点是怎么被注册到内核中的。  
(本章重在介绍内核框架，尽量摒弃平台相关的部分，仅以 beaglebone 平台的代码作为引入)。

## 从设备树到驱动
要查看驱动代码，自然是从设备树下手，找到其对应的设备树节点，然后根据它的 compatible 属性对内核源码进行全局搜索，就可以找到其对应的驱动。  


beaglebone 中 pwm controller 的节点是这样的：



## pwm 驱动主要结构体介绍
### struct pwm_chip
struct pwm_chip 描述一个 controller，也可以理解成每一个 pwm_chip 对应一个 pwm 硬件控制器，主要是描述一个控制器的属性。它的定义如下：

```
struct pwm_chip {
	struct device *dev;                 
	struct list_head list;
	const struct pwm_ops *ops;
	int base;
	unsigned int npwm;

	struct pwm_device *pwms;
	struct pwm_device * (*of_xlate)(struct pwm_chip *pc,
					const struct of_phandle_args *args);
	unsigned int of_pwm_n_cells;
};
```





