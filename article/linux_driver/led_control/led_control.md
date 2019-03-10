# gpio驱动程序
上一章节[linux设备驱动程序--hello_world](https://www.cnblogs.com/downey-blog/p/10500828.html)章节主要介绍了linux字符设备驱动程序的框架，从这一章节开始我们讲解各种外设的控制，包括gpio，i2c，dma等等，既然是外设，那就涉及到具体的目标板，博主在这里使用的开发板是开源平台beagle bone green，内核版本为4.14.   

今天我们来讲解gpio的设备驱动程序。

## gpio相关的库函数
为了linux的可移植性和统一，linux提供一套函数库供用户使用，内容涵盖了GPIO/I2C/SPI等外设的控制，关于函数库可以参考[官方网站](https://www.kernel.org/doc/html/v4.17/driver-api/gpio/intro.html)  

这一章我们需要用到gpio相关的库函数：

    //检查gpio number是否合法
    int gpio_to_irq(unsigned gpio)
    //根据gpio number申请gpio资源，label为gpio名称  
    int gpio_request(unsigned gpio, const char *label)
    //释放gpio 资源
    void gpio_free(unsigned gpio)
    //设置gpio 为输入
    int gpio_direction_input(unsigned gpio)
    //设置gpio 为输出
    int gpio_direction_output(unsigned gpio, int value)
    //设置gpio的值
    gpio_set_value(unsigned gpio, int value)
    //设置gpio的消抖时间，主要用于按键消抖
    int gpio_set_debounce(unsigned gpio, unsigned debounce)
    //获取gpio对应的中断线路
    int gpio_to_irq(unsigned gpio)
    //gpio中断，当产生中断时调用handle函数
    int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char * name, void * dev)

## linux gpio设备驱动程序
在前面的章节我们知道了怎么写一个字符设备驱动程序的框架，现在我们就只需要往框架里面添加相应的处理代码就可以了。  

现在尝试实现这样的需求：
* 在beagle bone green开发板上的gpio上连接一个指示灯
* 当用户打开/dev目录下的设备文件时，完成对gpio的初始化
* 往文件中写入OPEN实现打开灯，往文件中写入CLOSE关闭灯
* 关闭设备文件时，释放gpio资源

下面就是我们实现的代码，gpio_led_control.c:
```C
#include <linux/init.h>  
#include <linux/module.h>
#include <linux/device.h>  
#include <linux/kernel.h>  
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/string.h>


MODULE_AUTHOR("Downey");
MODULE_LICENSE("GPL");

static int majorNumber = 0;
/*Class 名称，对应/sys/class/下的目录名称*/
static const char *CLASS_NAME = "led_control_class";
/*Device 名称，对应/dev下的目录名称*/
static const char *DEVICE_NAME = "led_control_demo";

static int led_control_open(struct inode *node, struct file *file);
static ssize_t led_control_read(struct file *file,char *buf, size_t len,loff_t *offset);
static ssize_t led_control_write(struct file *file,const char *buf,size_t len,loff_t* offset);
static int led_control_release(struct inode *node,struct file *file);

#define LED_PIN   26 
static int gpio_status;


static char recv_msg[20];

static struct class *led_control_class = NULL;
static struct device *led_control_device = NULL;

/*File opertion 结构体，我们通过这个结构体建立应用程序到内核之间操作的映射*/
static struct file_operations file_oprts = 
{
    .open = led_control_open,
    .read = led_control_read,
    .write = led_control_write,
    .release = led_control_release,
};


static void gpio_config(void)
{
    if(!gpio_is_valid(LED_PIN)){
        printk(KERN_ALERT "Error wrong gpio number\n");
        return ;
    }
    gpio_request(LED_PIN,"led_ctr");
    gpio_direction_output(LED_PIN,1);
    gpio_set_value(LED_PIN,1);
    gpio_status = 1;
}


static void gpio_deconfig(void)
{
    gpio_free(LED_PIN);
}

static int __init led_control_init(void)
{
    printk(KERN_ALERT "Driver init\r\n");
    /*注册一个新的字符设备，返回主设备号*/
    majorNumber = register_chrdev(0,DEVICE_NAME,&file_oprts);
    if(majorNumber < 0 ){
        printk(KERN_ALERT "Register failed!!\r\n");
        return majorNumber;
    }
    printk(KERN_ALERT "Registe success,major number is %d\r\n",majorNumber);

    /*以CLASS_NAME创建一个class结构，这个动作将会在/sys/class目录创建一个名为CLASS_NAME的目录*/
    led_control_class = class_create(THIS_MODULE,CLASS_NAME);
    if(IS_ERR(led_control_class))
    {
        unregister_chrdev(majorNumber,DEVICE_NAME);
        return PTR_ERR(led_control_class);
    }

    /*以DEVICE_NAME为名，参考/sys/class/CLASS_NAME在/dev目录下创建一个设备：/dev/DEVICE_NAME*/
    led_control_device = device_create(led_control_class,NULL,MKDEV(majorNumber,0),NULL,DEVICE_NAME);
    if(IS_ERR(led_control_device))
    {
        class_destroy(led_control_class);
        unregister_chrdev(majorNumber,DEVICE_NAME);
        return PTR_ERR(led_control_device);
    }
    printk(KERN_ALERT "led_control device init success!!\r\n");

    return 0;
}

/*当用户打开这个设备文件时，调用这个函数*/
static int led_control_open(struct inode *node, struct file *file)
{
    printk(KERN_ALERT "GPIO init \n");
    gpio_config();
    return 0;
}

/*当用户试图从设备空间读取数据时，调用这个函数*/
static ssize_t led_control_read(struct file *file,char *buf, size_t len,loff_t *offset)
{
    int cnt = 0;
    /*将内核空间的数据copy到用户空间*/
    cnt = copy_to_user(buf,&gpio_status,1);
    if(0 == cnt){
        return 0;
    }
    else{
        printk(KERN_ALERT "ERROR occur when reading!!\n");
        return -EFAULT;
    }
    return 1;
}

/*当用户往设备文件写数据时，调用这个函数*/
static ssize_t led_control_write(struct file *file,const char *buf,size_t len,loff_t *offset)
{
    /*将用户空间的数据copy到内核空间*/
    int cnt = copy_from_user(recv_msg,buf,len);
    if(0 == cnt){
        if(0 == memcmp(recv_msg,"on",2))
        {
            printk(KERN_INFO "LED ON!\n");
            gpio_set_value(LED_PIN,1);
            gpio_status = 1;
        }
        else
        {
            printk(KERN_INFO "LED OFF!\n");
            gpio_set_value(LED_PIN,0);
            gpio_status = 0;
        }
    }
    else{
        printk(KERN_ALERT "ERROR occur when writing!!\n");
        return -EFAULT;
    }
    return len;
}

/*当用户打开设备文件时，调用这个函数*/
static int led_control_release(struct inode *node,struct file *file)
{
    printk(KERN_INFO "Release!!\n");
    gpio_deconfig();
    return 0;
}

/*销毁注册的所有资源，卸载模块，这是保持linux内核稳定的重要一步*/
static void __exit led_control_exit(void)
{
    device_destroy(led_control_class,MKDEV(majorNumber,0));
    class_unregister(led_control_class);
    class_destroy(led_control_class);
    unregister_chrdev(majorNumber,DEVICE_NAME);
}

module_init(led_control_init);
module_exit(led_control_exit);
```  
在init函数中对gpio进行相应的初始化，当用户在文件进行写操作时，根据传入的参数来判断打开或者关闭灯，在用户关闭文件时释放资源。   

为此，需要添加一个用户程序来对设备文件进行读写，gpio_led_contro_user.c:
```C
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
static char buf[256] = {1};
int main(int argc,char *argv[])
{
    int fd = open("/dev/led_control_demo",O_RDWR);
    if(fd < 0)
    {
        perror("Open file failed!!!\r\n");
        return -1;
    }
    while(1){
        printf("Please input <on> or <off>:\n");
        scanf("%s",buf);
        if(strlen(buf) > 3){
            printf("Ivalid input!\n");
        }
        else
        {
            int ret = write(fd,buf,strlen(buf));
            if(ret < 0){
                perror("Failed to write!!");
        }
    }
    }
    close(fd);
    return 0;
}
```
当用户程序执行时，用户程序一直获取用户的输入，根据用户输入"on"或者"off"，然后将其写入设备文件，触发系统调用，设备文件根据设备号找到内核中相应的file_operation结构体，相应write函数被调用，执行相应的点灯操作。  

## 编译加载运行
### 连接led
在例程中我们使用了gpio26作为led引脚，所以我们需要连接一个led(视情况加一个电阻)到gpio26引脚上，具体引脚位置需要自行查看开发板手册。  

### 查看log
首先我们可以打开两个终端窗口，一个为查看log信息，一个用来进行相关指令操作。  

### 编译加载模块
在查看log信息终端，我们需要循环查看/var/log/kern.log文件：

    tail -f /var/log/kern.log 
这样内核printk()输出的信息就可以在这里看到了，方便进行调试。  

然后需要编译内核驱动文件gpio_led_control.c，先修改Makefile(这里就不再展开，可以参考前面章节)。  

然后编译：

    make
编译成功，在本目录下生成相应的.ko文件，加载内核文件：

    sudo insmod gpio_led_control.ko
查看log终端会显示相应的信息。  

### 编译运行用户程序
再编译用户文件：

    gcc gpio_led_contro_user.c -o user
执行用户文件：

    sudo ./user
窗口输出：

    Please input <on> or <off>:
这是我们输入on或者off就可以控制led的亮灭了(其实就是控制gpio的高低电平)。

### 将gpio映射到/sys目录下
对于gpio而言，linux驱动库实现了将gpio引脚信息映射到/sys目录下，用户可以很方便地直接通过相关文件的操作来读写gpio的值，达到控制gpio的目的，这个接口API原型为：
    
    //第一个参数表示导出的引脚，第二个参数表示是否可改变IO的输出方向。
    int gpio_export(unsigned gpio, bool direction_may_change)
当然，相对应的释放函数为
    void gpio_unexport(unsigned gpio)

### 自己动手试试  
在gpio初始化的函数中添加这个接口，在加载完成之后查看/sys/class/gpio/目录下是否有相应的gpio$num(这里是gpio26)文件(需要注意的是，在上例中，当用户程序打开设备时才进行gpio的初始化，关闭文件时释放gpio的资源，所以需要打开文件再操作)。  

如果有相应的文件，试试下面的指令：

    echo 0 > /sys/class/gpio/gpio26/value
    echo 1 > /sys/class/gpio/gpio26/value
如果你有兴趣也可以研究研究里面其他的文件，这里不过多描述，留作家庭作业。  

## gpio中断的实现
上文实现了通过操作设备文件来控制开发板的gpio，接下来我们看看gpio中断的实现，一个按键点灯程序，当加载模块后，按键反转灯的状态：
* 添加gpio按键中断代码。  
* 不再需要创建设备文件节点，直接通过按键来操作led。  

接下来就是按键中断的示例代码 gpio_key_led_control.c：   
```C
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#define BUTTON     27
#define LED        26

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Downey");
MODULE_DESCRIPTION("Gpio irq test!!\n");

int button_irq_num = 0;
bool led_status = 1;


static  irqreturn_t button_irq_handle(int irq, void *dev_id)
{
    printk(KERN_INFO "Enter irq!!\n");
    if(0 == led_status)
    {
        gpio_set_value(LED,1);
        led_status = 1;
    }
    else
    {
        led_status = 0;
        gpio_set_value(LED,0);
    }
    return (irqreturn_t)IRQ_HANDLED;
}


static int gpio_config(void)
{
    int ret = 0;
    if(!gpio_is_valid(BUTTON) || !gpio_is_valid(LED))
    {
        printk(KERN_ALERT "Gpio is invalid!\n");
        return -ENODEV;
    }
    gpio_request(BUTTON,"button");
    gpio_direction_input(BUTTON);
    gpio_set_debounce(BUTTON,20);

    button_irq_num = gpio_to_irq(BUTTON);
    printk(KERN_INFO "NUM = %d",button_irq_num);
    ret = request_irq(button_irq_num,
                            (irq_handler_t)button_irq_handle,
                            IRQF_TRIGGER_RISING,
                            "BUTTON1",
                            NULL);
    printk(KERN_INFO "GPIO_TEST: The interrupt request result is: %d\n", ret);
    
    gpio_request(LED,"LED");
    gpio_direction_output(LED,1);
    gpio_set_value(LED,1);
    return 0;
}


static void gpio_deconfig(void)
{
    gpio_direction_output(LED,0);
    free_irq(button_irq_num,NULL);
    gpio_free(BUTTON);
    gpio_free(LED);
}

int __init gpio_irq_init(void)
{
    gpio_config();
    printk(KERN_INFO "gpio_irq_init!\n");
    return 0;
}


void __exit gpio_irq_exit(void)
{
    gpio_deconfig();
    printk(KERN_INFO "gpio_irq_exit!\n");
}

module_init(gpio_irq_init);
module_exit(gpio_irq_exit);
```

## 编译加载执行
### 连接led和按键
首先为了试验，我们需要将按键连接在gpio27上，将led连接在gpio26上。(视情况添加电阻)  

### 编译加载
修改Makefile，然后编译：

    make
加载：
    sudo insmod gpio_key_led_control.ko
### 测试 
这部分例程并没有注册文件接口，而是直接在内核中通过硬件中断检测是否有按键时间产生，来执行点亮和熄灭指示灯的操作。  

现在就可以测试按键是否有效了，如果出现什么问题，可能需要调试代码，别忘了根据printk()输出的log信息来判断错误。  



好了，关于linux驱动程序-gpio控制就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.



