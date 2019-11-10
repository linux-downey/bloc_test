pwm 的软件框架：
	硬件上，对应一个 pwm 控制器，该 pwm 控制器将会在 dts 中被描述成一个一级节点。
	
dts 的描述一般是这样的：
pwm1: pwm@30660000 {
		compatible = "fsl,imx8mq-pwm", "fsl,imx27-pwm";
		reg = <0x0 0x30660000 0x0 0x10000>;
		interrupts = <GIC_SPI 81 IRQ_TYPE_LEVEL_HIGH>;
		clocks = <&clk IMX8MQ_CLK_PWM1_ROOT>,
			 <&clk IMX8MQ_CLK_PWM1_ROOT>;
		clock-names = "ipg", "per";
		#pwm-cells = <2>;
		status = "disabled";
	};
	
其中包含了 compatible 属性，pwm 控制器的寄存器基地址以及寄存器范围，驱动中需要对其进行相应的映射。


	软件上对于一个 pwm 控制器的结构是 pwm_chip,一个 pwm_chip 通常包含多个 pwm 通道，也可能只是一个。
	pwmchip_add()将 pwm_chip 注册到系统中，pwm_chip 包含的内容：
	
	struct pwm_chip {
	
		struct device *dev;              //对应的dev结构
		struct list_head list;           //通过该 list ，将 pwm 链入到 pwm_chips 链表中，方便遍历
		const struct pwm_ops *ops;       //回调函数，对应用户的 pwm_request/pwm_enable 等操作。
		int base;                        //第一个 pwm 的数字
		unsigned int npwm;               // 该chip包含多少个pwm dev

		struct pwm_device *pwms;         // 数组，每一个数组元素对应一个 pwm_device.

		struct pwm_device * (*of_xlate)(struct pwm_chip *pc,
					const struct of_phandle_args *args);
		unsigned int of_pwm_n_cells;     //设备树中使用多少个 cell 来描述 pwm 节点。 
	};


所有的 pwm_chip 都被连接在 pwm_chips 链表头上，


//pwm 对应的回调函数
	struct pwm_ops {
		int (*request)(struct pwm_chip *chip, struct pwm_device *pwm);
		void (*free)(struct pwm_chip *chip, struct pwm_device *pwm);
		int (*config)(struct pwm_chip *chip, struct pwm_device *pwm,
				  int duty_ns, int period_ns);
		int (*set_polarity)(struct pwm_chip *chip, struct pwm_device *pwm,
					enum pwm_polarity polarity);
		int (*capture)(struct pwm_chip *chip, struct pwm_device *pwm,
				   struct pwm_capture *result, unsigned long timeout);
		int (*enable)(struct pwm_chip *chip, struct pwm_device *pwm);
		void (*disable)(struct pwm_chip *chip, struct pwm_device *pwm);
		int (*apply)(struct pwm_chip *chip, struct pwm_device *pwm,
				 struct pwm_state *state);
		void (*get_state)(struct pwm_chip *chip, struct pwm_device *pwm,
				  struct pwm_state *state);
	#ifdef CONFIG_DEBUG_FS
		void (*dbg_show)(struct pwm_chip *chip, struct seq_file *s);
	#endif
		struct module *owner;
	};



从用户角度来看：
	用户操作 pwm 的基础数据结构是 pwm_device,一个 pwm_device 对应一路 pwm 的操作。
	
	pwm_get()  或者 of_pwm_get(),意为获取一个 pwm device 的句柄，以在后续对 pwm 进行相应的操作。
	of_pwm_get() 表示从设备树中获取一个 pwm_device.具体的设备树语法是这样的：
	
		pwm: pwm {
		#pwm-cells = <2>;
		};

		[...]

		bl: backlight {
			pwms = <&pwm 0 5000000>;
			pwm-names = "backlight";
		};
	在对应的节点中，&pwm 表示指定对应的 pwm 控制器，0 表示该控制器下的 pwm device 的编号，5000000表示占空比，
	单位是 ns，在 probe 函数中，直接使用 pwm_get() 就可以获取到对应的 pwm device 以进行操作了。  
	
	
	非设备树的 pwm_get 方式，就是查找 pwm_lookup_list 这个list，并根据 dev_id 和 传入的 cond_id 确定对应的
	pwm_device , 这些 pwm_device 是提前注册到 pwm_chip 中的。


除了获取对应的 pwm_device ,还支持对 pwm 的其他操作：
	pwm_config()
	pwm_enable()
	
在最新的接口中，取消了 上述的的接口，使用了 pwm_applay_state() 来设置以及使能。
struct pwm_state {
	unsigned int period;
	unsigned int duty_cycle;
	enum pwm_polarity polarity;
	bool enabled;
};

pwm_apply_state() 将调用注册时传入的 chip->ops->aplly.  

