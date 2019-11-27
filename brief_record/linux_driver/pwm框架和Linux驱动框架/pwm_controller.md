# pwm controller 框架
在上一章中我们简单介绍了 pwm 在设备树中的描述语法，严格来说，设备树仅仅是一个描述文件，也可以看成是一个配置文件。所以，更重要的工作是这个描述文件是怎么被读取和使用的，从而延伸到整个 pwm controller 驱动框架是如何在内核中成型的。  

这一章我们持续深入，基于 beaglebone 平台，linux 4.14版本内核，来看看设备树中 controller 节点是怎么被注册到内核中的。  
(本章重在介绍内核框架，尽量摒弃平台相关的部分，仅以 beaglebone 平台的代码作为引入)。

## 从设备树到驱动
要查看驱动代码，自然是从设备树下手，找到其对应的设备树节点，然后根据它的 compatible 属性对内核源码进行全局搜索，就可以找到其对应的驱动。  

beaglebone 中 pwm controller 的节点是这样的：

```
ehrpwm0: pwm@48300200 {
				compatible = "ti,am3352-ehrpwm",
					     "ti,am33xx-ehrpwm";
				#pwm-cells = <3>;
				reg = <0x48300200 0x80>;
				clocks = <&ehrpwm0_tbclk>, <&l4ls_gclk>;
				clock-names = "tbclk", "fck";
				status = "disabled";
			};
```


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
在一个 pwm_chip 中，主要值得关注的有以下几个成员：
* list：该 list 是链表节点，所有的 pwm_chip 被链接到 pwm_chips 中，由系统统一管理，方便对所有的 pwm controller 进行遍历。
* ops：这是核心控制函数部分，该 ops 中包含了 pwm 的配置、使能，这一部分是硬件相关的操作，用户调用相应 pwm 操作接口时就会调用到这些底层函数以实现硬件控制
* base : 这里的 base 并非寄存器的基地址，内核使用位图对所有的 pwm 设备进行管理，而 base 就是该 controller 在位图中的基地址
* npwm：当前控制器包含的 pwm device 的数量
* pwms：pwm device 数组，与数量相对应 
* of_xlate：这是一个函数指针，负责处理 pwm 的节点引用
* of_pwm_n_cells：对应于设备树中 #pwm-cells 属性，表示 pwm 节点引用时需要传入多少个参数


## controller 的注册
对应设备树的节点描述，就会存在对应的 driver 将这些资源注册到系统中，以备 consumer 的使用。


