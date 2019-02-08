# linux i2c驱动框架
在嵌入式中，不管是单片机还是单板机，i2c作为板级通信协议是非常受欢迎的，尤其是在传感器的使用领域，其主从结构加上对硬件资源的低要求，稳稳地占据着主导地位。  

## 单片机中的i2c
每个MCU基本上都会集成硬件i2c控制器，在单片机编程中，对于操作硬件i2c控制器，我们只需要操作一些相应的寄存器即可实现数据的收发。  
那如果没有硬件i2c控制器或者i2c控制器不够用呢？事情也不麻烦，我们可以使用两个gpio来软件模拟i2c协议，代码不过几十行，虽然i2c协议本身有一定的复杂性，但是如果仅仅是实现通信，在单片机上还是非常简单的。  
## linux中的i2c
在上linux系统的单板机中，一般也会集成一个或者多个硬件i2c控制器，使用i2c收发数据的原理归根结底也是通过操作硬件i2c控制器的寄存器来实现，但是linux中i2c框架的复杂度却比单片机中高出好几个量级，有用户层、驱动层、核心层、硬件实现层，对于刚入手的朋友们而言，直接面对这么多非常抽象的层次结构是比较困难的，一头扎进去很可能就直接放弃了。事实上，我们需要关注的不是它有多少层，而是从i2c本身的特性出发，搞清楚为什么需要分层？  

## 为什么不能像单片机一样
在上面的介绍中，我们提到单片机的i2c控制方式，那么linux中的i2c为什么不能像单片机一样的控制方式来控制呢？  
答案是：linux需要支持非常多的i2c设备，由量变而引起质变。
那么有些同学就要问了，i2c一般的使用方式为一主多从结构(多主多从较为少见)，一个硬件i2c控制器在7位寻址模式下能支持127个设备，实在不行扩展到10位，总归是可以的。其实，在这里说的i2c设备多并非硬件上的限制，而是软件上的限制。  
## 单片机中实现i2c程序
我们不妨回想一下，在单片机中编写一个i2c驱动程序的流程：
以sht31(这是一个常用的i2c接口的温湿度传感器)为例，刚入行的新手程序员可能这样写(伪代码)：

    int sht31_read_temprature(){
        int temperature;
        wait_i2c_not_busy();
        set_i2c_reg_to_send_byte();
        wait_i2c_sent_complete();
        temp = i2c_read_data_from_sht31_reg();
        return temperature;
    }   

    int sht31_write_byte(){
        int humidity;
        wait_i2c_not_busy();
        set_i2c_reg_to_send_2bytes();
        wait_i2c_set_complete();
        humidity = i2c_read_data_from_sht31_reg();
        return humidity;
    }   
    ....

## 第一级分层
每次读写函数都对硬件i2c的寄存器进行设置，很显然，这样的代码有很多重复的部分，我们可以将重复的读写部分提取出来作为公共函数，写成这样：

    int sht31_read_data(char *data,int data_len){
        wait_i2c_not_busy();
        set_i2c_reg_to_send_2bytes();
        wait_i2c_set_complete();
        return i2c_read_data_from_sht31_reg();
    }
所以，上例中的读温湿度就可以写成这样：

    int sht31_read_temprature(){
        return sht31_read_data(xxx);
    }
    int sht31_write_byte(){
        return sht31_read_data(xxx);
    }
经过这一步优化，这个驱动程序就变成了两层：
1. i2c操作函数部分，比如i2c与设备的读写。
2. 设备的操作函数，每个设备有不同的寄存器，对于存储设备而言就是存取数据，对于传感器而言就是读写传感器数据，需要读写设备时，直接调用第一步中的接口。  
可以明显看到的是，第一步中的i2c操作函数部分可以被抽象出来。   

### 软件的分层
如果你仔细看了上面的示例，基本上就理解了软件分层是什么概念，它其实就是不断地将公共的部分和重复代码提取出来，将其作为一个统一的模块，应用部分再调用这部分统一的代码，从宏观上来看程序就被分成了两层，也可以说是两部分，由于是调用与被调用的关系，所以层次结构可以更明显地体现他们之间的关系。  

### 分层的好处
最直观地看过去，软件分层第一个好处就是节省代码空间，代码更少，层次更清晰，对于代码的可读性和后期的维护都是非常大的好处。
将相关的数据和操作封装成一个模块，对外提供接口，屏蔽实现细节，便于移植和协作，因为在移植和修改时，只需要修改当前模块的代码，保持对外接口不变即可，减少了移植和维护成本。

## 第二级分层
在第一级分层中，i2c被抽象出两层：i2c硬件读写层和i2c的应用层(暂且这么命名吧)，读写层只负责读写数据，而应用层则根据不同设备进行不同的读写操作，调用读写层接口。  
划分成读写层和驱动层之后，在空间上和程序复用性上已经走了一大步。  
但是在之后的开发中又发现一个问题：对于单个的厂商而言，生产的设备往往具有高度相似的寄存器操作方式，比如对于我们常用的sht3x(温湿度传感器)而言，这些系列的传感器设备之间的不同仅仅是测量范围、测量精度的不同，其他的寄存器配置其实是一样的，对于这些同系列的设备，我们同样可以抽象出一个驱动层，以sht3x为例，同系列设备的操作变成这样：
1. i2c硬件读写层。
2. sht3x的公共操作函数，通过调用上层接口实现初始化、设置温度阈值等函数。  
3. 具体设备的操作函数，对于sht31而言，通过传入sht31的参数，调用上层接口来读写sht31中的数据，需要传入的参数主要是i2c地址，设置精度等sht3x系列之间的差异化部分。  
这样，对于sht3x而言，用户在使用这一类设备的时候就只需要简单地调用诸如sht3x_init(u8 i2c_addr),sht3x_set_resolution(u8 resolution)这一类的函数即可完成设备的操作，通过传入不同的参数执行不同的操作。  
这里贴上一个图来加深理解：

到这里，对于一个单片机上的i2c设备而言，基本上已经有了比较好的层次结构：
1. i2c硬件读写层
2. 驱动层(主要是解决同系列设备驱动重复问题)
3. 设备层(应用程序调用上一层接口实现设备读写)
将上述分层的1.2步集成到系统中，用户只需要调用相应接口直接就可以操作到设备，对用户来说简化了操作流程，对系统来说节省了程序空间。

## linux：我们不一样
### 第一种解决方案  
与单片机中程序不一样的是：linux中内核空间和用户空间是区分开来的，驱动程序将会被加载到内核中，当然是不能让用户直接来操作驱动程序的，那我们可以改成这样：由驱动层直接在用户空间注册文件接口，用户程序通过用户文件对设备进行操作，于是分层模型变成了这样：
1. i2c硬件读写层
2. 驱动层(主要是解决同系列设备驱动重复问题,同时在用户空间注册用户接口以供访问)
3. 应用层(又可成为用户层，应用程序通过用户接口对设备进行设置，读写等等)
问题就这样得到解决。  
用户在操作文件接口的时候，依次地通过文件接口传递目标设备的资源对设备进行初始化，各种设置，然后读写即可，对于sht3x而言，这些资源包括i2c地址、精度、阈值等等。  
但是...
如果你是个有追求的程序开发者，或者是个挑剔的用户，会发现这种方式的缺陷所在：
1. 对于系统而言，操作一个设备需要多次进行设置和读写，意味着多次地从用户态切换到内核态再切换回用户态，这么大的系统开销是不能接受的。
2. 对挑剔的用户或者说应用程序而言，如果我使用一个sht31传感器，我希望打开用户文件之后直接就能读取到传感器的目标数据(温湿度)，而不是还要进行一系列的设置，这样太麻烦。但是这也是没办法的事，因为驱动层要对应一系列设备，无法做到针对单一设备，所以进行一些配置是无法避免的。  

### 第二种解决方案
既然驱动部分无法针对单一设备，而是针对诸如sht3x这一类设备，那我们为每个单一设备添加一份设备程序来提供资源，比如sht31、sht35分别提供一份，一个驱动程序可以对应多个设备程序。我们把上一节分层中丢掉的设备层拿回来，驱动模型就变成了这样：
1. i2c硬件读写层
2. 驱动部分(提供系列设备的公共驱动部分)-------设备部分(提供单一设备的资源，驱动程序获取资源进行相应配置，与驱动层为一对多的关系)，统称为驱动层
3. 应用层(通过上层提供的接口对设备进行访问)
这样的分层有个好处，在第二层驱动层中，直接实现了每个单一设备的程序，对应用层提供简便的访问接口，这样在用户层不用再进行复杂的设置，就可以直接对设备进行读写。  
由于linux采用宏内核机制，通常将设备驱动程序直接放在内核镜像中，我们也不可能同时将所有的设备部分固化到系统中，所以采用一种匹配机制：

    将驱动部分(即driver)先加载到系统中，当添加一个设备部分(device，单一设备)时，对相应的driver根据某些条件进行匹配操作，只有当匹配上时，驱动(driver)部分获取设备部分提供的资源，对目标设备进行配置，生成用户操作接口。  

事实上，在非常经典的linux2.6及之前版本中，i2c设备驱动程序就是这样实现的。  
它的具体分层实现是这样的：
### i2c硬件读写层
linux i2c设备驱动程序中的硬件读写层由struct i2c_adapter来描述，它的内容是这样的：
    struct i2c_adapter {
        ...
        const struct i2c_algorithm *algo; /* the algorithm to access the bus */
        void *algo_data;
        int nr;                          
        char name[48];
        ...
    };
其中struct i2c_algorithm *algo结构体中master_xfer函数指针指向i2c的硬件读写函数，我们可以简单地认为一个struct i2c_adapter结构体描述一个硬件i2c控制器。在驱动编写的过程中，这一层的实现由系统提供。驱动编写者只需要调用i2c_get_adapter()接口来获取相应的adapter.  

### 驱动层
#### linux总线机制
在上面的分层讨论中可知：驱动层由驱动部分和设备部分组成，驱动部分由struct i2c_driver描述，而设备部分由struct i2c_device部分组成。这两部分虽然被我们分在同一层，但是这是相互独立的，在前文中说到：当添加进一个device或者driver，会根据某些条件寻找匹配的driver或者device，那这一部分匹配谁来做呢？  
这就不得不提到linux中的总线机制，i2c总线担任了这个衔接的角色，诚然，i2c总线也属于总线的一种，我们先来看看描述总线的结构体：
 
    struct bus_type {
        ....
        const char		*name;
        int (*match)(struct device *dev, struct device_driver *drv);
        int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
        int (*probe)(struct device *dev);
        int (*remove)(struct device *dev);
        void (*shutdown)(struct device *dev);
        int (*online)(struct device *dev);
        int (*offline)(struct device *dev);
        int (*suspend)(struct device *dev, pm_message_t state);
        int (*resume)(struct device *dev);
        struct subsys_private *p;
    };

    struct subsys_private {
        ...
        struct klist klist_devices;
        struct klist klist_drivers;
        unsigned int drivers_autoprobe:1;
        ...
    };
这个结构体描述了linux中各种各样的sub bus,比如spi，i2c，platform bus，可以看到，这个结构体中有一系列的函数，在struct subsys_private结构体定义的指针p中，有struct klist klist_devices和struct klist klist_drivers这两项，当我们向i2c bus注册一个driver时，这个driver被添加到klist_drivers这个链表中，当向i2c bus注册一个device时，这个device被添加到klist_devices这个链表中，每有一个添加行为，都将调用总线的match函数，遍历两个链表，为新添加的device或者driver寻找对应的匹配，一旦匹配上，就调用probe函数，在probe函数中执行设备的初始化和创建用户操作接口。  

### 应用层
在probe函数被执行时，在/dev目录下创建对应的文件，例如/dev/sht3x，用户程序通过读写/dev/sht3x文件来操作设备，同时，也可以在/sys目录下生成相应的操作文件来操作设备，应用层直接面对用户，所以接口理应是简单易用的。  

### 驱动开发者的工作
一般来说，如果只是开发i2c驱动，而不需要为新的芯片移植驱动的话，我们只需要在驱动层做相应的工作，并向应用层提供接口。i2c硬件读写层已经集成在系统中。  

## 小小结
总的来说，分层即是一种抽象，将重复部分的代码不断地提取出来作为一个独立的模块，因为模块之间是调用与被调用的关系，所以看起来就是一种层次结构。  
高内聚，低耦合，这是程序设计界的六字箴言。  
不管是linux还是其他操作系统，又或者是其他应用程序，将程序模块化都是一种很好的编程习惯，linux内核的层次结构也是这种思想的产物。  

### 抽象化仍在进行中
上文中提到，当驱动开发者想要开发驱动时，在驱动层编写一个driver部分，添加到i2c总线中，同时编写一个device部分，添加到i2c总线中。driver部分主要包含了所有的操作接口，而device部分提供相应的资源。  
但是，随着设备的增长，同时由于device部分总是静态定义在文件中，导致这一部分占用的内核空间越来越大，而且，最主要的问题时，对于资源来说，大多都是一些重复的定义：比如时钟、定时器、引脚中断、i2c地址等同类型资源。  
按照一贯的风格，对于大量的重复定义，我们必须对其进行抽象化，于是linus一怒之下对其进行大刀阔斧的整改，于是设备树横空出世，是的，设备树就是针对各种总线(i2c bus，platform bus等等)的device资源部分进行整合。  

将设备部分的静态描述转换成设备树的形式，由内核在加载时进行解析，这样节省了大量的空间。  

对于i2c驱动程序的编写，可以参考我的另外一篇博客。  
接下来，博主还会带你走进linux内核代码，剖析整个i2c框架的函数调用流程。  

好了，关于linux驱动框架的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
