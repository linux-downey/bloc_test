pinctrl 子系统：

主要的数据结构：
struct pinctrl {
	struct list_head node;         //连到系统中的 pinctrl 链表中
	struct device *dev;            //对应pinctrl的设备
	struct list_head states;       //对应的状态列表
	struct pinctrl_state *state;   //当前的状态
	struct list_head dt_maps;      //映射表，主要是对应 功能和 group 的选择。对应function selector和group selector。
									//因为一个 group 对应多个多个 state，多个settings。
	struct kref users;             //引用计数
};

// state 就是设备树中定义的那些名称，"default""sleep"等等，而 settings 就是那些引脚设置的列表，pinctrl-0/pinctrl-1对应的值。
struct pinctrl_state {
	struct list_head node;
	const char *name;
	struct list_head settings;
};

//其中 settings 由 struct pinctrl_setting 来描述，由其中的struct list_head node;链入到pinctrl_state的链表中。
struct pinctrl_setting {
	struct list_head node;
	enum pinctrl_map_type type;
	struct pinctrl_dev *pctldev;
	const char *dev_name;
	union {
		struct pinctrl_setting_mux mux;
		struct pinctrl_setting_configs configs;
	} data;
};
pin_state 只有一种，但是可能同时对应多种 settings 
settings 的类型由下面描述：

enum pinctrl_map_type {
    PIN_MAP_TYPE_INVALID,
    PIN_MAP_TYPE_DUMMY_STATE,
    PIN_MAP_TYPE_MUX_GROUP,－－－功能复用的setting
    PIN_MAP_TYPE_CONFIGS_PIN,－－－－设定单一一个pin的电气特性
    PIN_MAP_TYPE_CONFIGS_GROUP,－－－－设定单pin group的电气特性
};

对应的 group 选择和功能选择
struct pinctrl_setting_mux {
    unsigned group;－－－－－－－－该setting所对应的group selector
    unsigned func;－－－－－－－－－该setting所对应的function selector
};

//对应的硬件设置，group名称
struct pinctrl_setting_configs {
	unsigned group_or_pin;
	unsigned long *configs;
	unsigned num_configs;
};

上述为整个 pinctrl 对应的数据结构
/*********************************************************************************/
/*********************************************************************************/

pinctrl的功能主要就是设置引脚功能与电气特性。


//每个硬件都有对应的gpio功能，gpio 分为多个bank，通常一个 bank 包含32个gpio，对应一个pinctrl gpio_dev
//一个 bank 可以被分成多个 gpio range ，range 会挂到对应的 gpio device 下面。  

struct pinctrl_gpio_range {
	struct list_head node;
	const char *name;
	unsigned int id;
	unsigned int base;
	unsigned int pin_base;
	unsigned const *pins;
	unsigned int npins;
	struct gpio_chip *gc;
};



// 在 struct device 结构体中，有一个 struct dev_pin_info 的结构：
struct dev_pin_info {
	struct pinctrl *p;
	struct pinctrl_state *default_state;
	struct pinctrl_state *init_state;
#ifdef CONFIG_PM
	struct pinctrl_state *sleep_state;
	struct pinctrl_state *idle_state;
#endif
};
这个结构里面保存着对应 pinctrl 的设置。  

really_probe(), device 和 driver 匹配上的时候将调用
	-> pinctrl_bind_pins(),将设备树中指定的引脚根据 state 进行绑定
		-> dev->pins->p = devm_pinctrl_get(dev);  //在struct device中包含着对应的pinctrl信息，从设备树解析而来
		-> pinctrl_lookup_state() //获取设备树中指定的 state
		-> pinctrl_select_state() //设置对应的state
			-> pinctrl_commit_state(p, state) //提交state
				-> pinmux_enable_setting  //对应 state 设置对应的settings
					-> pin_request()      //对对应的 settings 设置对应的引脚，如果 pin 已经被绑定，就报错。
			


//每个  pinctrl_desc 描述一个pin controller
struct pinctrl_desc {
    const char *name;
    struct pinctrl_pin_desc const *pins;
    unsigned int npins;
    const struct pinctrl_ops *pctlops;
    const struct pinmux_ops *pmxops;
    const struct pinconf_ops *confops;
    struct module *owner;
};