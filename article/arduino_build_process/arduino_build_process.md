## arduino IDE的编译流程
在前面两个章节中，我们讨论了《arduino IDE的系统目录结构》和《包目录结构及内容》，对arduino的整理框架应该是有了一定的认识，在这一章节我们将具体讨论用户在使用arduino IDE时背后的实现细节。

## arduino IDE的使用
我们先来看看arduino的常规操作，编程、编译以及烧写：
### 编写程序
arduino的编程非常简单，arduino IDE 提供两个内建函数

    setup()
    loop()
setup()就像入口函数，开机时先运行setup(),可以将一些初始化操作放在里面
loop()则可以看做是main()函数中的while(1)，它会循环执行，默认的操作就是先执行setup(),再循环执行loop();
而arduino提供的API请参考[Language Reference](https://www.arduino.cc/reference/en/)  
### 编译程序
编译程序就更简单了，首先，我们需要先选择一个目标开发板，依次点击：

    Tools->Board：xxx->$TARGET BOARD
    ($TARGET BOARD为目标开发板)
然后点击IDE的左上角的这个图标即可。  


需要注意的是，编译的时候如果我们想查看编译过程的log信息(推荐！！！)，我们需要进行如下设置：

    File->Preferences
然后勾选以下选项：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_build_process/set_compile_log.png)  
### 烧写程序
编译完成之后，就可以进行烧写程序的操作，在烧写之前，需要先把开发板连接上电脑，安装驱动之后，PC将会分配一个设备端口。在IDE中我们需要指定烧写的计算机端口：

    Tools->Port->$TARGET COM
    ($TARGET COM是目标端口)
然后点击左上角这个图标即可。
同样的，在烧录(上传)的时候如果我们想查看烧录(上传)log，在上述设置中勾选相应选项即可。
到这里，arduino的使用过程就这么愉快地完成了，是不是确实非常简单呢？
## 背后的实现
如果仅仅是介绍arduino的基本操作流程，那这篇文章着实是没有任何意义。接下来我们来解析用户操作对应的arduino背后的实现原理。
### 开发板列表的显示
在第一章中我们讲到了该如果添加一个新的开发板以备使用，当我们按官方流程添加一个开发板时，就可以在

    Tools->Boadr:xxx
显示的右拉菜单中看到自己添加的开发板，然后选中就可以使用。  
那么，从背后的实现来看，一个开发板为什么会显示在arduino IDE的board 列表中呢？  
首先，在前面章节我们有提到，安装一个arduino开发板之前，如果是非官方板(官方板有自带官方json文件package_index.json)，需要先添加一个json文件，然后下载安装包，即可完成安装。
json文件中对开发板的描述是这样的(以seeeduino V3开发板为例)：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_build_process/stack_V3.png)  
第一行中有个"name"属性，这个属性对应arduino IDE board栏中显示的名称
我们再看下面的"archiveFileName"属性，这个指定了arduino安装压缩包的名称，在安装时arduino将其解压到hardware目录下，并将名称中.tar.bz2去掉，比如：下载的压缩包名为"Seeeduino_Stalker_V3.tar.bz2",而我们在hardware目录下看到的开发板目录为:Seeeduino_Stalker_V3。  
此时，arduino IDE只有在判定json文件中存在正确的开发板描述和在指定目录下(C:\Users\$USER_NAME\AppData\Local\Arduino15\packages\hardware\$MANUFACTORER 或者 $INSTALL_DIR\arduino\hardware\arduino\$MANUFACTORER)存在开发包的包文件(通过包名判断)时，就会将开发板显示在board list中供用户选择使用。

### 编译过程
#### 编译准备
在lunux平台中，我们熟知的是用make工具来遵循Makefile规则来编译源文件，所有编译行为都由用户指定，自由灵活但是复杂度较高。而在大多数IDE中，用户失去了这样的自由，但是同时获得了一键操作的简便性，将源代码按照一定的规则放在指定的目录(或者是按照规则创建工程)，然后点击某个IDE中的按钮即可，arduino IDE中也不例外。  
arduino提供了一个用户可配置的目录放置源代码，我们可以进行设置，依次点击：

    Tools->Preferences
在指定位置添加目录地址即可：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_build_process/add_directory.png)  
那么arduino编译之前，搜索源文件的过程是怎样的呢？
首先，当用户点击编译按钮时，arduino会自动添加一个头文件，作用相当于：

    #include <Arduino.h>
这个文件是arduino的运行时库，其中包含了大部分的基础函数，就像C语言的基础库"stdio.h"，arduino的运行依赖于这个库，但用户不用管，因为IDE已经自动添加了。
读取到所有源文件中依赖的文件，从以下目录中寻找代码中依赖的源文件：
* 安装目录下的libraries/
* 系统目录中的libraries/(C:\Users\$USER_NAME\AppData\Local\Arduino15\packages\hardware\$MANUFACTORER\$VERSION\libraries)
* 上面提到的用户自己配置的项目

#### 编译时操作
在这里，我们来探讨用户在点击编译按钮之后发生了什么：
首先，arduino IDE将会读取用户所选用的开发板名称，然后对比board.txt中.name属性，如果一致就表明找到了board ID。我们再来回顾一下board.txt内容：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_build_process/board_txt.png)  
其中，uno就是board ID，这种配置形式有点像C/C++中的结构体。  
uno.name属性就是arduino IDE中显示的开发板名称。  
获取了board ID之后，arduino就可以通过board ID获取到相应的编译配置信息：  
* 获取相对应的variant\ IO 配置文件
* 获取相对应的core目录，所有平台相关的arduino实现源代码
* 获取编译时flash、ram大小，mcu运行频率
* 其他用户自定义的板级配置选项
arduino再次读取同目录下的platform.txt文件，获取所有的编译配置选项，就可以开始启动编译过程(arduino支持C和C++文件编译，如果是C文件需要添加extern "C"{})。
arduino的编译会生成一个临时目录，用来存放编译中间文件和hex、bin文件，当在下次编译时，没有重选开发板且仅改动部分.c/.cpp文件，通过对比时间戳可知哪一些文件没有被改动，然后直接复用.o目标文件，这样可以节省编译时间。
编译所有.c/.cpp文件之后会生成相应的.o文件，然后将所有.o文件连接成一个静态库.a文件，最后链接成一个.hex文件(也可能是.bin文件)。

我们同样可以通过arduino IDE编译时的log信息获取到编译时的信息：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_build_process/compile.png)  
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_build_process/compile2.png)  
从图中我们可以看到arduino IDE这些操作：
* 读取board ID
* 读取platform.txt
* 寻找对应的core\目录
* 选取编译工具链，获取编译选项这些。
从第二幅编译截图中可以看出这些过程
* 临时目录中生成.a文件
* 临时目录中生成.elf以及hex文件
* 指出程序当前已占用ram和flash的大小
临时目录存放在**C:\Users\$USER_NAME\AppData\Local\Temp**目录下

## 烧写
烧写部分其实比较简单，与编译过程类似。  
arduino IDE会从board.txt目录下读取开发板指定的烧写软件  
然后在platform.txt中读取烧写选项
烧写固件。

关于arduino 编译流程以及配置文件的详细解释可以参考官方资料：
编译：https://github.com/arduino/Arduino/wiki/Build-Process  

配置文件：https://github.com/arduino/Arduino/wiki/Arduino-IDE-1.5-3rd-party-Hardware-specification  

https://github.com/arduino/Arduino/wiki/Arduino-IDE-1.6.x-package_index.json-format-specification  


好了，关于arduino的编译流程讨论就到此为止了，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言


***个人邮箱：linux_downey@sina.com***
***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
