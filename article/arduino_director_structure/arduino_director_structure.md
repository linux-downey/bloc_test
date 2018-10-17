## Arduino 目录结构以及背后运行原理(windows 平台)
Arduino是一款便捷灵活、方便上手的开源电子原型平台。包含硬件（各种型号的Arduino板）和软件（Arduino IDE)。由一个欧洲开发团队于2005年冬季开发。其成员包括Massimo Banzi、David Cuartielles、Tom Igoe、Gianluca Martino、David Mellis和Nicholas Zambetti等。
### arduino 平台的特点
* 跨平台：Arduino IDE可以在Windows、Macintosh OS X、Linux三大主流操作系统上运行，而其他的大多数控制器只能在Windows上开发。
* 简单清晰：Arduino IDE基于processing IDE开发。对于初学者来说，极易掌握，同时有着足够的灵活性。Arduino语言基于wiring语言开发，是对 avr-gcc库的二次封装，不需要太多的单片机基础、编程基础，简单学习后，你也可以快速的进行开发。
* 开放性：Arduino的硬件原理图、电路图、IDE软件及核心库文件都是开源的，在开源协议范围内里可以任意修改原始设计及相应代码。
* 发展迅速：Arduino不仅仅是全球最流行的开源硬件，也是一个优秀的硬件开发平台，更是硬件开发的趋势。Arduino简单的开发方式使得开发者更关注创意与实现，更快的完成自己的项目开发，大大节约了学习的成本，缩短了开发的周期。
因为Arduino的种种优势，越来越多的专业硬件开发者已经或开始使用Arduino来开发他们的项目、产品；越来越多的软件开发者使用Arduino进入硬件、物联网等开发领域；大学里，自动化、软件，甚至艺术专业，也纷纷开展了Arduino相关课程
### arduino的目录结构
我们可以从![arduino官网](https://www.arduino.cc/)获取arduino IDE的安装包，windows下的安装不用多介绍。  
在这里，我不想去讨论arduino的使用，因为arduino的使用总是那么简单，作为一个使用arduino的程序员，需要做的第一步就是要了解arduino背后的运行机制，这样我们才能在使用时有一种得心应手的感觉。  
#### 安装目录
不管你是自定义安装还是默认安装，最后总会存在一个安装目录，我们来看看这个安装目录下有哪些内容：
* drivers/
* examples/
* hardware/
* java/
* lib/
* libraries/
* reference/
* tools/
* tools-builder/
* 其他arduino IDE相关可执行文件
#### 安装目录对应的内容
drivers/：在windows下，每个硬件连接到系统中，都需要安装驱动程序(windows10会自动强制更新驱动)，这个目录下包含的就是各种arduino板的windows驱动文件，驱动文件一般是两个:.cat文件和.inf文件，.cat文件为数字签名文件，.inf文件为驱动描述文件。
examples/：这个目录下存放的是arduino的基础库，基本上所有的arduino开发板都能运行这个目录下的程序。
hardware/：早期的arduino是运行在AVR平台上的，但是现在支持主流的arm平台，而且用户也可以将其扩展到其他平台上，即使是相同的平台，也会有不同型号之间的差异，硬件资源的不同，MEGA328和ATMEGA2560就是基于AVR平台的两款不同的开发板。这个目录下存放着基于不同平台或者严格来说是基于不同型号单片机的arduino实现。
java/：arduino IDE是用java写的，所以这个目录下就是一些arduino IDE运行所需的bin文件和库。
lib/：一些java的归档文件。
libraries/:这个目录下存放的第三方的库文件，用户写的库文件就可以放在这个目录下。
reference/：里面是一些参考文件
tool/、tools-builder/：arduino IDE运行时依赖的一些工具
在这里面需要重点关注的就是hardware/目录，因为这里存放了arduino平台自带开发板的源代码实现。

#### arduino数据目录
玩过arduino的都知道，arduino IDE安装时只会安装默认的一些arduino官方avr平台开发板，如果我们要添加其他的开发板(官方授权的第三方机构发布或者官方arm平台开发板)就要下载相应的包，这个包包含了一整套开发板所需要的开发资源，例如驱动文件、源码实现、编译工具链等等。当我们把包下载完之后，会惊奇地发现这个包并不会放在安装目录中，而是放在了系统目录下，即C盘(windows系统下)。
我们可以在

    C:\Users\$USER_NAME\AppData\Local\Arduino15\
目录下找到arduino的整个配置文件，同样的，我们新安装的包也在这个目录下。***值得注意的是,$USER_NAME为用户名，AppData目录是隐藏目录 ***
在介绍C:\Users\$USER_NAME\AppData\Local\Arduino15\这个目录之前我们先来看看arduino是怎么添加一个新的开发板的。
#### arduino添加开发板
