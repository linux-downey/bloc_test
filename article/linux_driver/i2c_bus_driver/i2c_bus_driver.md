# linux I2C 设备驱动

## linux中的总线
无论是MCU开发或者是单板机的开发，总线总是充斥在各个角落。对于CPU而言，系统总线包含地址总线、数据总线、控制总线，一般而言，根据外设支持的不同速度，再将总线划分不同等级，这些总线是硬件上实实在在存在的东西，CPU就是通过这些电路来和外围器件交换数据。像I2C、SPI、SERIAL等外设就是挂在系统总线上。  
今天要介绍的就是linux设备驱动程序中的I2C总线，但是这里的I2C总线并非指物理上的I2C控制器与CPU之间的总线连接，而是软件上基于I2C总线的一种抽象，在软件上使用一种分层分离的概念。  
有经验的工程师在编写代码时总会先思考框架，考虑代码的健壮性、可移植性、维护性、可读性。对于可移植性来说，针对不同的平台，应用实现是固定不变的，不同之处就在于硬件。那我们就可以将不变的应用软件与变化的硬件配置进行分离，中间使用某个接口进行衔接，当切换另一个平台时，应用部分就可以保持不变，只是改动硬件部分的代码，这样对代码的稳定性和后期维护来说节省了大量的成本。  

## linux中的总线架构
linux中的总线，不论是连接硬件的I2C总线，SPI总线，又或者是虚拟的platform总线，都遵循了这样一个模式：

对于总线而言，一条总线主要是维护两个部分：driver结构体的device结构体，多个device或者driver分别挂在链表中，通常driver部分对应稳定不变的软件实现部分，而device部分则对应需要改动的部分，只要分配得合理，通常在一类驱动程序中，我们就只要修改device部分即可实现驱动的移植。  
在之前的驱动程序中，我们一般的做法是在入口程序中：  
* 申请设备号，同时绑定file operation结构体。 
* 注册class 
* 注册device
* 将设备号与相应device节点绑定，用户访问设备节点就调用相应file operation结构体中函数。  
这个模板可以说适用于所有字符设备驱动，在写demo的时候这样确实没有问题，但是这样面临一个问题：即使是对于gpio的驱动而言，如果我要实现多个gpio驱动程序，就要重复地使用这一套框架来对每一个gpio实现一次，这样还可以接受，毕竟普通IO引脚不多。  
但是对于I2C或者SPI这类驱动而言，如果我要同时实现多个I2C设备的驱动，温湿度传感器、加速度计、存储设备、I2C控制设备等等，linux源代码中/driver目录下的设备数不胜数，而且还在不断地扩大中，为每一个设备实现一个驱动实在是太不划算。  
何以解忧？这就是总线模型诞生的缘由。linux团队将代码中相对固定的代码抽象出来做成driver，将变化的部分设计成另一块device，所有的device使用同一份driver代码，大大地节省了空间，要知道，这些空间可是宝贵的内存空间。  

## I2C的基本知识扫盲
回到本文的重点——I2C,做过裸板开发或者是单片机开发的朋友肯定对I2C不陌生，I2C是主从结构，主器件使用从机地址进行寻址，它的拓扑结构是这样的：

(图片来自网络，如有侵权，请联系我及时删除)  
基本的流程是这样的：
* 主机发送从机地址
* 从机监听总线，检测到接收地址与自身地址地址匹配，回复。
* 主机启动收发数据
* 从机接收数据，响应
* 数据收发完毕，主机释放总线。  
完整的I2C操作其实是比较复杂的，这里就不再展开讲解,博主将会在随后的章节中进行详解。  

## I2C设备驱动程序框架
I2C设备驱动程序框架分为两个部分：driver和device。  
分别将driver和device加载到内存中，i2c bus在程序加载时会自动调用match函数，根据名称来匹配driver和device，匹配完成时调用probe()
在driver中，定义probe()函数，在probe函数中创建设备节点，针对不同的设备实现不同的功能。  
在device中，设置设备I2C地址，选择I2C适配器。  
I2C适配器：I2C的底层传输功能，一般指硬件I2C控制器。

## I2C设备驱动程序
### driver端示例
直接来看下面的示例代码：
i2c_bus_driver.c:
```C
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* 结构体数组 结构体第一个参数为名称，第二个参数为private数据*/
static const struct i2c_device_id downey_drv_id_table[] = {
    {"downey_i2c",0},
    {},
};

static int major;
static struct class *i2c_test_cls;
static struct device *i2c_test_dev;
static const char* CLASS_NAME = "I2C_TEST_CLASS";
static const char* DEVICE_NAME = "I2C_TEST_DEVICE";

static struct i2c_client *downey_client;


static int i2c_test_open(struct inode *node, struct file *file)
{
    printk(KERN_ALERT "i2c init \n");
    return 0;
}

static ssize_t i2c_test_read(struct file *file,char *buf, size_t len,loff_t *offset)
{
    int cnt = 0;
    uint8_t reg = 0;
    uint8_t val = 0;
    copy_from_user(&reg,buf,1);
    /*i2c读byte，通过这个函数可以从设备中指定地址读取数据*/
    val = i2c_smbus_read_byte_data(downey_client,reg);
    cnt = copy_to_user(&buf[1],&val,1);
    return 1;
}

static ssize_t i2c_test_write(struct file *file,const char *buf,size_t len,loff_t *offset)
{
    uint8_t recv_msg[255] = {0};
    uint8_t reg = 0;
    int cnt = 0;
    cnt = copy_from_user(recv_msg,buf,len);
    reg = recv_msg[0];
    printk(KERN_INFO "recv data = %x.%x\n",recv_msg[0],recv_msg[1]);
    /*i2c写byte，通过这个函数可以往设备中指定地址写数据*/
    if(i2c_smbus_write_byte_data(downey_client,reg,recv_msg[1]) < 0){
        printk(KERN_ALERT  " write failed!!!\n");
        return -EIO;
    }
    return len;
}

static int i2c_test_release(struct inode *node,struct file *file)
{
    printk(KERN_INFO "Release!!\n");
    
    return 0;
}

static struct file_operations file_oprts = 
{
    .open = i2c_test_open,
    .read = i2c_test_read,
    .write = i2c_test_write,
    .release = i2c_test_release,
};

/*当i2c bus检测到匹配的device - driver，调用probe()函数，在probe函数中，申请设备号，创建设备节点，绑定相应的file operation结构体。*/
static int downey_drv_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    /*保存参数client，在i2c读写操作时需要用到这个参数，其中保存了适配器、设备地址等信息*/
    printk(KERN_ALERT "addr = %x\n",client->addr);
    downey_client = client;
    major = register_chrdev(0,DEVICE_NAME,&file_oprts);
    if(major < 0 ){
        printk(KERN_ALERT "Register failed!!\r\n");
        return major;
    }
    printk(KERN_ALERT "Registe success,major number is %d\r\n",major);

    /*以CLASS_NAME创建一个class结构，这个动作将会在/sys/class目录创建一个名为CLASS_NAME的目录*/
    i2c_test_cls = class_create(THIS_MODULE,CLASS_NAME);
    if(IS_ERR(i2c_test_cls))
    {
        unregister_chrdev(major,DEVICE_NAME);
        return PTR_ERR(i2c_test_cls);
    }

    /*以DEVICE_NAME为名，参考/sys/class/CLASS_NAME在/dev目录下创建一个设备：/dev/DEVICE_NAME*/
    i2c_test_dev = device_create(i2c_test_cls,NULL,MKDEV(major,0),NULL,DEVICE_NAME);
    if(IS_ERR(i2c_test_dev))
    {
        class_destroy(i2c_test_cls);
        unregister_chrdev(major,DEVICE_NAME);
        return PTR_ERR(i2c_test_dev);
    }
    printk(KERN_ALERT "i2c_test device init success!!\r\n");
    return 0;
}

/*Remove :当匹配关系不存在时(device或是driver被卸载)，调用remove函数，remove函数是probe函数的反操作，将probe函数中申请的资源全部释放。*/
static int downey_drv_remove(struct i2c_client *client)
{
    printk(KERN_ALERT  "remove!!!\n");
    device_destroy(i2c_test_cls,MKDEV(major,0));
    class_unregister(i2c_test_cls);
    class_destroy(i2c_test_cls);
    unregister_chrdev(major,DEVICE_NAME);
    return 0;
}

static struct i2c_driver downey_drv = {
    /*.driver中的name元素仅仅是一个标识，并不作为bus匹配的name识别*/
    .driver = {
        .name = "random",
        .owner = THIS_MODULE,
    },
    .probe = downey_drv_probe,
    .remove = downey_drv_remove,
    /*.id_table中存储driver名称，作为bus匹配时的识别*/
    .id_table = downey_drv_id_table,
    // .address_list = downey_i2c,
};


int drv_init(void)
{
    int ret = 0;
    printk(KERN_ALERT  "init!!!\n");
    ret  = i2c_add_driver(&downey_drv);
    if(ret){
        printk(KERN_ALERT "add driver failed!!!\n");
        return -ENODEV;
    }
    return 0;
}


void drv_exit(void)
{
    i2c_del_driver(&downey_drv);
    return ;
}

MODULE_LICENSE("GPL");
module_init(drv_init);
module_exit(drv_exit);
```

### 代码简述
* 入口函数drv_init()在i2c bus的driver链表中添加一个driver节点，当然这个操作是在加载模块时完成
* i2c_add_driver函数以struct i2c_driver类型结构体为参数，开发者需要填充driver信息，probe(),remove()，以及id_table,其中id_table中包含了driver名称，在i2c bus做匹配时使用。  
* 当driver-device匹配成功时，调用driver中的probe函数，在函数中，创建设备节点等等(之前章节中的驱动框架还记得吧)。  
* 当匹配关系不存在时，调用remove函数，执行probe的反操作。  
* 需要注意的是，在这里的示例中，这仅仅是一个demo，所以没有做任何操作，事实上，如果你要实现一个设备的驱动，需要在read或write函数中实现相应功能，这里仅仅是将用户写的数据写入i2c设备，读取设备数据到用户空间。

### device端代码示例
i2c_bus_driver.c:
```
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/regmap.h>
// #include <linux/paltform_device.h>


static struct i2c_adapter *adap;
static struct i2c_client  *client;
#define  I2C_DEVICE_ADDR   0x68

/**指定i2c device的信息
 * downey_i2c 是device中的name元素，当这个模块被加载时，i2c总线将使用这个名称匹配相应的drv。
 * I2C_DEVICE_ADDR  为设备的i2c地址 
 * */
static struct i2c_board_info downey_board = {
    I2C_BOARD_INFO("downey_i2c",I2C_DEVICE_ADDR),
};


int dev_init(void)
{
    /*获取i2c适配器，适配器一般指板上I2C控制器，实现i2c底层协议的字节收发，特殊情况下，用普通gpio模拟I2C也可作为适配器*/
    adap = i2c_get_adapter(2);
    if(IS_ERR(adap)){
        printk(KERN_ALERT  "I2c_get_adapter failed!!!\n");
        return -ENODEV;
    }
    /*创建一个I2C device，并注册到i2c bus的device链表中*/
    client = i2c_new_device(adap,&downey_board);
    /*使能相应适配器*/
    i2c_put_adapter(adap);
    if(!client){
    printk(KERN_ALERT  "Get new device failed!!!\n");
    return -ENODEV;
    }
    return 0;
}

void dev_exit(void)
{
    i2c_unregister_device(client);
    return ;
}

MODULE_LICENSE("GPL");
module_init(dev_init);
module_exit(dev_exit);

```

### 代码简述
* 在入口函数dev_init函数中先获取适配器，后续使用i2c_put_adapter将适配器放入执行列表，适配器一般指板上I2C控制器，实现i2c底层协议的字节收发，特殊情况下，用普通gpio模拟I2C也可作为适配器。
* 创建一个新的bus device，并将其链入bus 的device链表
* 创建device设备需要struct i2c_board_info类型结构体作为参数，函数包含匹配时使用的name元素，以及设备的i2c地址。

### 编译加载运行
driver和device作为两个独立的模块，需要分别编译，分别生成i2c_bus_driver.ko和i2c_bus_device.ko(编译过程我就不再啰嗦了)。  
然后加载driver：

    sudo insmod i2c_bus_driver.ko
log信息

    Dec 31 07:21:49 beaglebone kernel: [13114.715050] init!!!
加载device:

    sudo insmod i2c_bus_device.ko
log信息：

    
    Dec 31 07:21:49 beaglebone kernel: [13114.717420] addr = 68
    Dec 31 07:21:49 beaglebone kernel: [13114.726671] Registe success,major number is 241
    Dec 31 07:21:49 beaglebone kernel: [13114.739575] i2c_test device init success!!

查看log可以发现，当加载完i2c_bus_device.ko时，driver中probe函数被调用，打印出设备地址，注册的设备号，表示注册成功。接下来就是写一个用户程序来测试驱动。  
### 实验环境
* 开发板：beagle bone green开发板
* 内核版本 ：4.14.71-ti-r80
* i2c设备 ：9轴传感器，i2c地址0x68
## 用户代码
```C
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

static char buf[256] = {1};

int main(int argc,char *argv[])
{
    int ret = 0;
    uint8_t buf[2] = {0};
    char cmd[6] = {0};
    int reg_addr = 0;
    int value = 0;
    int fd = open("/dev/I2C_TEST_DEVICE",O_RDWR);
    if(fd < 0)
    {
        perror("Open file failed!!!\r\n");
    }
    while(1){
        /*for example : write 0x00 0x08*/
        /*The val should be 0 when the cmd is read.*/
        printf("Enter your cmd:<read/write> <reg_addr> <val> : \n");
        scanf("%s",cmd);
        scanf("%x",&reg_addr);
        scanf("%x",&value);
        printf("%s :%x :%x\n",cmd,reg_addr,value);
        if(0 == memcmp(cmd,"write",5)){
            buf[0] = reg_addr;
            buf[1] = value;
            int ret = write(fd,buf,2);
            if(ret < 0){
                perror("Failed to write!!\n");
            }else{
                printf("Write value %x to reg addr %x success\n",value,reg_addr);
            }
        }
        else if(0 == memcmp(cmd,"read",4)){
            buf[0] = reg_addr;
            ret = read(fd,buf,1);
            if(ret < 0){
                perror("Read failed!!\n");
            }else{
                printf("Read %x from addr %x\n",buf[1],reg_addr);
            }
            
        }
        else{
            printf("Unsupport cmd\n");
        }
        memset(cmd,0,sizeof(cmd));
    }
    close(fd);
    
    return 0;
}
```
用户程序实现从终端读取用户指令，然后读写传感器的寄存器,代码都经过测试，自己试试吧！

### device的另一种创建
在上述i2c的device创建中，我们使用了i2c_new_device()接口，值得一提的是，这个接口并不会检测设备是否存在，只要对应的device-driver存在，就会调用driver中的probe函数。  
但是有时候会有这样的需求：在匹配时需要先检测设备是否插入，如果没有i2c设备连接在主板上，就拒绝模块的加载，这样可以有效地管理i2c设备的资源，避免无关设备占用i2c资源。 
新的创建方式接口为：

    struct i2c_client *i2c_new_probed_device(struct i2c_adapter *adap,struct i2c_board_info *info,unsigned short const *addr_list,int (*probe)(struct i2c_adapter *, unsigned short addr))
这个函数添加了在匹配模块时的检测功能：
* 参数1：adapter，适配器
* 参数2：board info，包含名称和i2c地址
* 参数3：设备地址列表，既然参数2中有地址，为什么还要增加一个参数列表呢？咱们下面分解
* 参数4：probe检测函数，此probe非彼probe，这个probe函数实现的功能是检测板上是否已经物理连接了相应的设备，当传入NULL时，就是用默认的probe函数，建议使用默认probe。 

为了一探究竟，我们来看看i2c_new_probed_device的源代码实现：
```C
struct i2c_client *i2c_new_probed_device(struct i2c_adapter *adap,struct i2c_board_info *info,unsigned short const *addr_list,int (*probe)(struct i2c_adapter *, unsigned short addr))
{
	int i;
    /*如果传入probe为NULL，则使用默认probe函数*/
	if (!probe)
		probe = i2c_default_probe;
    /*轮询传入的addr_list，检测指定地址列表中地址是否合法*/
	for (i = 0; addr_list[i] != I2C_CLIENT_END; i++) {
		/* Check address validity */
		if (i2c_check_7bit_addr_validity_strict(addr_list[i]) < 0) {
			dev_warn(&adap->dev, "Invalid 7-bit address 0x%02x\n",
				 addr_list[i]);
			continue;
		}

        /*检测地址是否被占用*/
		/* Check address availability (7 bit, no need to encode flags) */
		if (i2c_check_addr_busy(adap, addr_list[i])) {
			dev_dbg(&adap->dev,
				"Address 0x%02x already in use, not probing\n",
				addr_list[i]);
			continue;
		}

        /*检测对应地址上设备是否正常运行*/
		/* Test address responsiveness */
		if (probe(adap, addr_list[i]))
			break;
	}
    /*检测不到对应地址的设备，或对应设备正在被占用*/
	if (addr_list[i] == I2C_CLIENT_END) {
		dev_dbg(&adap->dev, "Probing failed, no device found\n");
		return NULL;
	}
    /*检测到可用地址，将其赋值给board  info结构体*/
	info->addr = addr_list[i];
	return i2c_new_device(adap, info);
}
```
根据源码中的显示，i2c_new_probed_device主要是执行了这样的操作： 
* 轮询传入的addr_list，检测指定地址列表中地址是否合法，值得注意的是，在轮询addr_list时，判断列表中元素是否等于I2C_CLIENT_END，所以我们在给addr_list赋值时，应该以I2C_CLIENT_END结尾
* 检测地址是否被占用
* 检测对应地址上设备是否处于运行状态
* 将找到的地址赋值给info->addr
* 调用i2c_new_device
看到这里就一目了然了，一切细节在源码面前都无处可藏。其实就在对相应地址做一次检测而已，到最后还是调用i2c_new_device。  
不过不知道你有没有发现，i2c_new_device传入的参数2的addr部分被忽略了，所以board info中的地址其实是无关紧要的，因为函数会在addr list中查找可用的地址然后赋值给board info的addr元素，而原本的addr被覆盖。所以，如果你在写内核代码时感到疑惑，看源码就好了！  


好了，关于linux驱动中i2c驱动的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
