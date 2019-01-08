## Arduino 目录结构以及背后运行原理(windows 平台)
Arduino是一款便捷灵活、方便上手的开源电子原型平台。包含硬件（各种型号的Arduino板）和软件（Arduino IDE)。由一个欧洲开发团队于2005年冬季开发。其成员包括Massimo Banzi、David Cuartielles、Tom Igoe、Gianluca Martino、David Mellis和Nicholas Zambetti等。
## arduino 平台的特点
* 跨平台：Arduino IDE可以在Windows、Macintosh OS X、Linux三大主流操作系统上运行，而其他的大多数控制器只能在Windows上开发。
* 简单清晰：Arduino IDE基于processing IDE开发。对于初学者来说，极易掌握，同时有着足够的灵活性。Arduino语言基于wiring语言开发，是对 avr-gcc库的二次封装，不需要太多的单片机基础、编程基础，简单学习后，你也可以快速的进行开发。
* 开放性：Arduino的硬件原理图、电路图、IDE软件及核心库文件都是开源的，在开源协议范围内里可以任意修改原始设计及相应代码。
* 发展迅速：Arduino不仅仅是全球最流行的开源硬件，也是一个优秀的硬件开发平台，更是硬件开发的趋势。Arduino简单的开发方式使得开发者更关注创意与实现，更快的完成自己的项目开发，大大节约了学习的成本，缩短了开发的周期。
因为Arduino的种种优势，越来越多的专业硬件开发者已经或开始使用Arduino来开发他们的项目、产品；越来越多的软件开发者使用Arduino进入硬件、物联网等开发领域；大学里，自动化、软件，甚至艺术专业，也纷纷开展了Arduino相关课程
## arduino的目录结构
我们可以从![arduino官网](https://www.arduino.cc/)获取arduino IDE的安装包，windows下的安装不用多介绍。  
在这里，我不想去讨论arduino的使用，因为arduino的使用总是那么简单，作为一个使用arduino的程序员，反而更需要去了解arduino背后的运行机制，这样我们才能在使用时有一种得心应手的感觉。  
### 安装目录
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


### 安装目录对应的内容
* drivers/：在windows下，每个硬件连接到系统中，都需要安装驱动程序(windows10会自动强制更新驱动)，这个目录下包含的就是各种arduino板的windows驱动文件，驱动文件一般是两个:.cat文件和.inf文件，.cat文件为数字签名文件，.inf文件为驱动描述文件。
* examples/：这个目录下存放的是arduino的基础库，基本上所有的arduino开发板都能运行这个目录下的程序。
* hardware/：早期的arduino是运行在AVR平台上的，但是现在支持主流的arm平台，而且用户也可以将其扩展到其他平台上，即使是相同的平台，也会有不同型号之间的差异，硬件资源的不同，MEGA328和ATMEGA2560就是基于AVR平台的两款不同的开发板。这个目录下存放着基于不同平台或者严格来说是基于不同型号单片机的arduino实现。
* java/：arduino IDE是用java写的，所以这个目录下就是一些arduino IDE运行所需的bin文件和库。
* lib/：一些java的归档文件。
* libraries/:这个目录下存放的第三方的库文件，用户写的库文件就可以放在这个目录下。

在这里面需要重点关注的就是hardware/目录，因为这里存放了arduino平台自带开发板的源代码实现，在下一篇文章中我将详细介绍这个目录下的内容。

***
### arduino系统目录
玩过arduino的都知道，arduino IDE安装时只会安装默认的一些arduino官方avr平台开发板，如果我们要添加其他的开发板(官方授权的第三方机构发布或者官方arm平台开发板)就要下载相应的包，这个包包含了一整套开发板所需要的开发资源，例如驱动文件、源码实现、编译工具链等等。当我们把包下载完之后，会惊奇地发现这个包并不会放在安装目录中，而是放在了系统目录下，即C盘(windows系统下)。
我们可以在

    C:\Users\$USER_NAME\AppData\Local\Arduino15\
    ($USER_NAME为用户名，AppData目录是隐藏目录)
目录下找到arduino的整个配置文件，同样的，我们新安装的包也在这个目录下。
在介绍C:\Users\$USER_NAME\AppData\Local\Arduino15\这个目录之前我们先来看看arduino是怎么添加一个新的开发板的。
*** 
### arduino添加开发板
在介绍arduino系统目录之前，***我们以添加Seeeduino 的V4开发板为例***，看看arduino是怎么添加一个新开发板的支持的：  
首先，打开arduino IDE：依次点击左上角 

    File->Preferences
可以看到一个 <span id="jump_tool">Additionnal Boards Manager URLs：</span>栏   
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_director_structure/arduino_add_url_window.png)
点击右边的图标，会弹出一个文本框，在文本框中键入：
<span id="jump_url">

    https://raw.githubusercontent.com/Seeed-Studio/Seeed_Platform/master/package_seeeduino_boards_index.json
</span>
每个链接占一行，不同链接要另立一行。   

![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_director_structure/add_seeed_url.png) 
然后，点击arduino IDE中 

    Tools-> Boards:XXX-> Boards Manager
将会出现这样一个文本框：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_director_structure/board_manager.png)
然后在搜索栏中搜索需要的板子：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_director_structure/search_seeeduino_V4.png)  
点击install，等待下载安装完成即可，有时会出现网络不畅下载失败的情况，多尝试几次即可，到这里，我们就成功地安装了一个开发板。
我们可以依次点击：

    Tools->Board
然后在右边弹出的菜单中看到我们已经安装的板子。  
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_director_structure/add_results.png)
***

### arduino在添加开发板的原理
首先，在添加一个包时，需要从互联网上下载，这就面临一个问题：包的下载地址在哪里？包的描述、下载校验码等从何知道？
上面我们在安装时第一步[添加json文件](#jump_url)的链接就是解决这一个问题，arduino会先从链接地址下载这个json文件，然后从json文件中读到包的信息，包含下载地址、依赖包的下载地址等等，然后依次地下载安装，我们可以简单地介绍一个示例:
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_director_structure/json_file_example.png)  

从图中我们可以看到，json文件中描述了：
* 包名、生产厂商、版本号、包类型
* 包下载地址，包名、包的校验、包的size
* 软件包依赖的编译工具链包、烧写工具包、以及第三方库的包
但是，在arduino的安装目录下，我们找不到新安装包的信息，这些信息被下载安装到了C:\Users\$USER_NAME\AppData\Local\Arduino15\这个目录下。  
在这个目录下，我们需要关注的是以下三个部分：
* packages
* staging
* 多个json文件
#### json文件
当我们点击：

    Tools-> Boards:XXX-> Boards Manager
arduino IDE将会主动地去下载[上述添加的url栏](#jump_tool)中添加的json文件，默认还有arduino官方的json文件，所以当前目录下的所有json文件其实就是不同开发板包的描述文件。  
#### staging目录
当我们点击安装时，arduino IDE会将json文件中提到的开发板的包以及依赖软件包全部下载到/staging目录下。所以打开staging目录，可以看到很多bz2的压缩包，这些就是下载下来的目标开发板的依赖包。
#### packages目录
除了下载之外，还有一个安装过程，安装过程即是将staging目录下下载的压缩包解压到packages/目录下,packages里存放的是arduino安装包的核心部分：
* 第一级目录即packages/目录下存放的是厂商分类，例如arduino、seeeduino、esp32等。
* 第二级目录下，例如packages/arduino/下放的是：
    * hardware：各种开发板包的资料
    * tools：工具，如编译工具链、下载软件等等。
* 第三级目录下，例如packages/arduino/hardware下，存放的是不同内核之间的源代码实现，以及配置文件，与上面提到的[hardware中各package对应的内容](#jump)一致。而packages/arduino/tools下就是对应的工具软件了。  
***


好了，关于arduino的系统目录框架讨论就到此为止了，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***下一篇我们着重讲解单个开发板软件包的目录结构***

***个人邮箱：linux_downey@sina.com***
***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
（完）
