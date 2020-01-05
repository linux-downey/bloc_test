# input 子系统的应用
在 linux 系统中，存在大量的输入设备，其中包括最常见的键盘鼠标触摸屏、甚至到一个简单的 button，按照 linux 的一贯做法，将设备的共性进行抽象，也就有了内核中输入子系统的诞生。  

在了解 linux 内核中输入子系统架构之前，有必要来考虑一个非常简单的问题：当用户需要一个输入设备时，是以什么形式来使用的？  

在 linux 中，一切皆文件，所以必然是通过文件的读写来实现用户空间对输入设备的操作，那么这个输入设备的驱动形态有了一个大致的模型：

输入设备的驱动程序在内核中实现，并将接口导出到用户空间，既然是设备，那么操作接口就在 /dev/ 目录下。

当有输入事件发生时，内核产生相应的输入信息，上报到对应 /dev/ 目录下的文件接口，同时用户进行读写。  


## input 子系统
在进入内核中的 input 子系统之前，不妨想一想，如果要我们来实现这个输入中间层，会怎么做呢？首先，我们需要分析输入设备的特性，提取出一些共性和差异：
* 对于输入设备，内核程序是不知道设备什么时候被触发的，所以通常是通过中断或者轮询的方式检测输入的产生，对于不同的输入设备，检测输入的方式不同。  
* 不同的输入设备产生的输入事件也是不同的，比如鼠标产生位移事件，键盘产生字符输入的事件,产生事件的不同导致处理的形式不相同，比如某些输入设备需要重复上报、数据过滤等。

按照以往的经验，内核对于设备驱动的抽象就是将共性的部分提取出来并直接注册到系统中作为子系统的核心层，而对于差异的部分则留出接口，差异化的数据以传参的方式传入到内核，而差异化的操作则传入回调函数到内核，在特定的时间点被调用。  

在 linux 内核的输入子系统实现中，大体的框架就是这样实现的：用户传入驱动相关的参数与回调函数，内核子系统负责创建用户空间的文件接口、提供设备上报事件接口、事件的上报流程处理，只是将驱动注册和事件上报两个过程分离开来，分离的好处是支持用户自定义事件上报的流程，提高可扩展性。   

## input 子系统接口
在上述的分析中，输入设备之间存在两个差异部分：输入操作的检测与上报、不同输入设备的注册。   

### 输入的检测与上报
对于输入操作的检测与上报，内核输入子系统提供的上报接口如下：
* input_report_key   //上报按键事件，典型设备：键盘
* input_report_rel   //上报相对位移事件，典型设备：鼠标
* input_report_abs   //上报绝对位移事件，典型设备：触摸屏
* input_report_switch //上报开关事件

* input_sync         // 上报同步事件，通常表示当前时间上报完成。
* input_mt_sync      //上报多点触摸同步事件，应用于多点触摸显示屏

内核子系统中，对输入设备的分类是基于事件类型的，通常情况下，input_report_* 接口是和 input_sync 配合使用的，在某些稍微复杂的情况下，通常需要同时上报几种状态，或者同时上报几次。就需要使用 input_sync 来统一将这些上报事件提交给内核，告诉内核，这一次的时间上报已经完成，你把它们提交到用户空间吧。  

而对于输入操作的检测，每个设备都有区别，通常是通过中断的方式来实现的，最常见的i2c触摸屏可以通过 i2c 的硬件中断来实现，当然也可以通过数据轮询的方式来实现。   

比如：对于简单的 button ，可以直接以中断的形式来判断输入,然后在中断处理函数中上报事件：

```
irqreturn_t button_irq_handle0(int irq,void *arg)
{
    input_report_key(gpio_dev,KEY_L,1);
    input_sync(gpio_dev);    
    return (irqreturn_t)IRQ_HANDLED;
}

gpio_request(BUTTON0,"button0");
gpio_direction_input(BUTTON0);
button_irq_num0 = gpio_to_irq(BUTTON0);
request_irq(button_irq_num0,
                            (irq_handler_t)button_irq_handle0,
                            IRQF_TRIGGER_RISING,
                            "BUTTON0",
                            NULL);

```
在上述的 button 示例中，注册 button 引脚对应的中断处理函数，在 button 被按下时，进入中断处理函数，在中断处理函数中将按键事件上报并调用 input_sync 同步。  

这个示例中省略了设备的注册部分，但是足够说明设备输入的检测和上报的方式。而对于其它的输入设备，也是同样的逻辑。  


### 设备的注册
事件的上报依赖于设备的注册，只有设备注册到系统中，才会在用户空间生成对应的文件接口，输入事件的数据才会被传入到用户空间供用户空间的进程读取。  

input 设备的注册函数是 input_register_device() 函数，它的原型是这样的：

```
int input_register_device(struct input_dev *dev);
```

传入的参数为 struct input_dev 类型的参数，这也是输入设备的核心数据。  

我们来看看它的定义：

```
struct input_dev {
	const char *name;      //该结构的名称
	const char *phys;      //该设备在系统中的物理路径
	const char *uniq;      //设备唯一的识别字符串(不一定存在)
	struct input_id id;    //该设备的id table，在设备与事件上报的匹配中使用

	unsigned long propbit[BITS_TO_LONGS(INPUT_PROP_CNT)]; //设备属性的位图

    /*下列位图负责保存用户设置的事件*/
	unsigned long evbit[BITS_TO_LONGS(EV_CNT)];
	unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
	unsigned long relbit[BITS_TO_LONGS(REL_CNT)];
	unsigned long absbit[BITS_TO_LONGS(ABS_CNT)];
	unsigned long mscbit[BITS_TO_LONGS(MSC_CNT)];
	unsigned long ledbit[BITS_TO_LONGS(LED_CNT)];
	unsigned long sndbit[BITS_TO_LONGS(SND_CNT)];
	unsigned long ffbit[BITS_TO_LONGS(FF_CNT)];
	unsigned long swbit[BITS_TO_LONGS(SW_CNT)];

	unsigned int hint_events_per_packet; //设备生成的事件数，事件上报程序根据它来判断该事件需要的缓冲区
    
	unsigned int keycodemax;   //当前 key code map 的最大值，key code 就是按键的枚举值
	unsigned int keycodesize;  //当前 key code map 的数量
	void *keycode;  //设备的 key code

    //获取 key code
	int (*setkeycode)(struct input_dev *dev,
			  const struct input_keymap_entry *ke,
			  unsigned int *old_keycode);
    //设置 key code，用于自定义 keycode
	int (*getkeycode)(struct input_dev *dev,
			  struct input_keymap_entry *ke);


	struct ff_device *ff;  //如果设备支持 force feedback(指反馈系统) 模式，表示force feedback 设备。

	unsigned int repeat_key; //用于保存最后一个key code，应用于autorepeat 模式上
	struct timer_list timer;  //定时器，应用于 autorepeate 模式。
 
	int rep[REP_CNT];  //在 autorepeate 模式中当前的key值

	struct input_mt *mt; //表示 multitouch(多点触摸屏) 的状态

	struct input_absinfo *absinfo;  //绝对位移的设置信息

    /*以下四个是当前key/led/sound/switch 状态的映射*/
	unsigned long key[BITS_TO_LONGS(KEY_CNT)];
	unsigned long led[BITS_TO_LONGS(LED_CNT)];
	unsigned long snd[BITS_TO_LONGS(SND_CNT)];
	unsigned long sw[BITS_TO_LONGS(SW_CNT)];

    //以下为系统回调函数，分别对应用户的调用操作
	int (*open)(struct input_dev *dev);
	void (*close)(struct input_dev *dev);
	int (*flush)(struct input_dev *dev, struct file *file);
	int (*event)(struct input_dev *dev, unsigned int type, unsigned int code, int value);

    //设置抓取handle，抓取handle一旦设置，将会成为所有设备输入的接收者
	struct input_handle __rcu *grab;

    //自旋锁
	spinlock_t event_lock;
    //互斥量
	struct mutex mutex;

    //当前打开该设备的用户数量
	unsigned int users;

    //标记注销事件，用户打开设备时将返回错误码：-ENODEV
	bool going_away;

	struct device dev; //对应的device

    //handle 链表
	struct list_head	h_list;

    //通过这个节点加入到 input_dev_list
	struct list_head	node;

    //上报事件的数据数量
	unsigned int num_vals;
	//上报时间的最大数量
    unsigned int max_vals;
    //该结构体表示
	struct input_value *vals;

    //该设备是否加入到设备资源管理，实现设备资源的自动回收
	bool devres_managed;
};
```

结构体数据中，包含了针对按键、触摸屏、位移等事件的支持，以及一些回调函数，最核心的部分就是对应输入设备的位图，在编写驱动程序时也是主要针对这部分进行操作。

比如，当我们要设置按键事件，支持键盘输入 A 和 B 按键时，需要进行对应的设置：

```
set_bit(EV_KEY,dev->evbit);
set_bit(KEY_A,dev->keybit);
set_bit(KEY_B,dev->keybit);
```

evbit 表示事件类型，即当前设置按键类型。  
keybit 表示 key code，表示按键类型中的按键码，比如 KEY_A 表示键盘的 A 按键。  

其中，dev 是 struct input 类型的结构体，该结构体需要先向系统申请：

```
dev = input_allocate_device();
```

然后，将设置完的 struct input dev 注册到系统中：
```
input_register_device(dev);
```

至此，就完成了input 设备的注册。  

## input 驱动的完整流程

这里我们重新整理一下 input 子系统中驱动设备的编写流程：

* 使用 input_allocate_device 接口申请一个 struct input *dev。
* 设置 dev 中的按键事件，就是设置位图中的对应位。
* 将该 dev 注册到系统中，系统负责将其与事件上报驱动进行连接，并在用户空间(/dev/下)注册用户操作接口。 
* 初始化输入设备，初始化输入设备的检测，比如中断的注册。
* 通过  input_event() 接口，传入上面设置的 dev ，上报事件。


## 示例
下面是一个按键的示例：

```
#define BUTTON0   26
struct input_dev *gpio_dev;
static struct timer_list timer;

static void function(unsigned int long data)
{
        if(gpio_get_value(BUTTON0)){
                input_report_key(gpio_dev,KEY_L,1);
                input_sync(gpio_dev);
        }
        return ;
}


void timer_init(void)
{
        init_timer(&timer);
        timer.function = function;
}

irqreturn_t button_irq_handle0(int irq,void *arg)
{
        mod_timer(&timer,jiffies + 5);
        return (irqreturn_t)IRQ_HANDLED;
}

static int gpio_input_init(void)
{
    int err;
    timer_init();

    gpio_dev = input_allocate_device();
    set_bit(EV_KEY,gpio_dev->evbit);
    set_bit(KEY_A,gpio_dev->keybit);
    input_register_device(gpio_dev);

    err = gpio_request(BUTTON0,"button0");
    gpio_direction_input(BUTTON0);

    if(err){
            pr_err("could not request gpio pin\n");
            del_timer_sync(&timer);
            input_unregister_device(gpio_dev);
            return 0;
    }
    button_irq_num0 = gpio_to_irq(BUTTON0);
    err = request_irq(button_irq_num0,
                            (irq_handler_t)button_irq_handle0,
                            IRQF_TRIGGER_RISING,
                            "BUTTON0",
                            NULL);
    if(err){
            del_timer_sync(&timer);
            input_unregister_device(gpio_dev);
            return 0;
    }
    return 0;
}

static void gpio_input_exit(void)
{
        input_unregister_device(gpio_dev);
        del_timer_sync(&timer);
        free_irq(button_irq_num0,NULL);
}

module_init(gpio_input_init);
module_exit(gpio_input_exit);

MODULE_LICENSE("GPL");
```

在该示例中，值得注意的是使用一个定时器对按键进行软件消抖。  






