## arduino开发板目录结构
在上一篇博客中，我们讲解了arduino IED的安装目录和系统目录的框架，对arduino的开发板管理有了一定的理解，在这一章，我们着重来看看每一类开发板的包里面到底包含了哪些信息。

### 开发板包的地址
官方的arduino-avr平台的软件包内容可以在这里找到:
    $INSTALL-DIR\Arduino\avr\ 
除了arduino IDE安装时自带的arduino-avr类开发板之外，之后所有用户下载的包(包括官方arduino M0之类)都会被默认存放到这里：

    C:\Users\$USER_NAME\AppData\Local\Arduino15\packages\$MANUFACTURER\hardware\$BOARD_MODEL\$VERSION\
    ($USER_NAME:windows用户名)
    ($MANUFACTURER：厂商名)
    ($BOARD_MODEL：开发板型号)
    ($VERSION：版本号)

### 开发板包目录结构
* bootloaders
* cores
* extras
* firmwares
* libraries
* variants
* boards.txt
* platform.txt
* programmers.txt
接下来我们逐个地讲解每个目录(文件)的作用：
#### 包与开发板的对应
在讲解这些目录之前，我们需要知道的是，一个开发板的包并不仅仅对应单一的开发板，而是对应同一种内核的多个开发板.所以一个包内常常包括多个开发板。
#### bootloaders
由名字可以知道，这个目录下装的是bootloader，即启动代码。我们都知道arduino烧写程序是直接通过串口烧写的，而单片机裸片一般只支持JTAG、或者Jlink的SWD烧写方式，如果要支持串口烧写，那就必须在开发板出厂前，将一段内置的程序烧写进单片机，这个内置程序就是bootloader，其作用就是通过串口接收用户程序，将接收到的用户程序放置到flash某个预定的位置，然后执行跳转指令去执行用户程序。  
所以，arduino才能实现串口烧写程序。  
由于arduino是一个开源系统，所以你通常可以在bootloder目录下找到bootloader源代码的实现，当然也可以进行hacking，甚至将串口烧写换成SPI烧写程序都没有问题。
#### cores
这个目录下放置的是内核对应的arduino实现源码！  
在源代码下，一切细节将无处可藏。  
你可以通过阅读这里的源代码，解决你初学arduino时的各种疑惑：
* 为什么arduino执行是只需要在setup()和loop()中进行填空？我们熟悉的main()去哪儿了？
* 为什么setup()中的函数只执行一次，而loop()函数中的内容循环执行？
* 为什么只要是支持arduino平台的各种开发板都支持诸如digitalWrite()、Serial.begin()、millis()这些函数，每个平台的实现是一样的吗？
* 如果arduino平台官方API不支持某个功能，但是单片机硬件支持，我能不能自己以arduino的方式实现并使用？
...
只要你有探索的欲望，这些问题都可以通过阅读源代码解决
#### extras
顾名思义，这里放置一些额外的东西，作为保留目录，这个目录中通常没有内容。
#### firewarms
这个目录下放置一些其他的内核相关的应用程序固件，但是通用固件通常放置在tools/(与hardware并列的目录，见下文)目录下，这个目录经常被闲置甚至被删除。
#### libraries
这个目录下通常放的是官方扩展库和第三方扩展库，(官方内建库放置在arduino安装目录examples\中).  
这些库包括：

    EEPROM
    HID
    SoftwareSerial
    SPI
    Wire
这些库还是会被经常用到的，尤其是Wire(即I2C)和SPI.库中还有一些示例，算是相当贴心了。
当然，用户也可以进行扩展，比如加入I2S,USBHost之类的库。
同样的，基于开源原则，这里面所有库都有源代码实现(不得不说，源代码是程序员进步的阶梯)。
#### variants
