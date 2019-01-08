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
#### firewares
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
上文中有提到，一个包对应的是一个厂商在一种内核上的开发，所以很可能对应多个开发板，而每个单片机的硬件或多或少是有些差别的，比如定时器的数量及精度、I/O口的ADC、PWM功能，或者I/O端口的数量。  
这个目录主要是将不同的硬件开发板的I/O口映射成同一个标准，仅针对I/O口，所以每种类型开发板下一般会有如下文件：
* pins_arduino.h
* variant.cpp(部分开发板)
* variant.h(部分开发板)
通过这些映射，用户可以通过arduino API直接操作模拟口A0、A1...，或者数字口1,2,3...,对应操作MCU的IO口，这样做的好处就是保持兼容性。

#### board.txt配置文件
这是一个非常重要的开发板配置文件，这里面主要是关于开发板的定义，上面我们有提到，arduino可以兼容很多的平台，但是各平台之间在硬件和配套软件上有很多区别，那么arduino IDE在编译时怎样去识别以及选择哪一种配置方式呢？答案是通过读取这个配置文件，下面我们来介绍这个配置文件的内容(以arduino UNO为例)：
![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/arduino_board_directory/uno_config.png)  
首先，这个配置文件都是键值对的方式，key = value。
每一个开发板对应一个board ID，例如arduino UNO的ID为uno，有了这个board ID，arduino IDE就可以识别相应的开发板。
我们先来看看最常用arduino UNO的board.txt的配置内容(board ID为uno)：
* uno.name=:uno.name=Arduino/Genuino Uno ：这是开发板的名称，对应显示在arduino IDE的board选项栏中
* uno.vid.X=YYYY,uno.pid.X=YYYY：有些朋友对VID(vender ID)和PID(product ID)并不熟悉，其实这两个东西是在windows下安装驱动程序时需要的描述ID，表示出自某个厂商的某个产品。需要向官方机构注册和购买。
* uno.upload.tool=avrdude :在前面提到，arduino实现串口烧录程序需要内置一段bootloader，这个bootloader的作用就是通过某种协议或基于某种协议的软件，通过串口来烧写固件。这种烧写软件的实现有多种，例如avrdude，bossac，stm32flash等等，这一项配置就是选择烧录软件(又叫做上传工具)
* uno.upload.maximum_size=32256：上传的可执行文件中代码部分(对应可执行文件中的代码段)最大size限制，一般对应arduino开发板的flash大小。
* uno.upload.maximum_data_size=2048 ：上传的可执行文件中数据段(对应可执行文件中数据段部分)的最大size，一般对应arduino开发板的ram大小。
* uno.upload.speed=115200：设置烧写固件的烧写速度
* uno.bootloader.tool=avrdude：bootloader指定的烧写方式
* uno.bootloader.*_fuses=XXXX：这个fuses指的是熔丝位，熔丝位的存在是为了防止用户随意更改bootloader，那这个是怎么工作的呢？我们常接触的几乎所有单片机都支持swd、JTAG方式的程序烧写方式，而且JTAG接口是国际标准，所以对于任何的单片机，我们只要找到JTAG的接口，就可以对其进行读写程序的操作，这样是很不安全的，所以在bootloader中出厂设置一个标志位，在第一次烧写过后将这个标志位改写，这时候就不再允许固件的再次烧写，但是可以使用厂商自带的软件去重新将这个标志位重新设置以支持再次烧写，因为arduino开发板本来就是开放的，所以它会公开这个fuses的值以便用户hacking，一般情况下为了安全这个fuses值是不公开的。
* uno.bootloader.file=optiboot/optiboot_atmega328.hex：bootloader hex文件的地址
* uno.build.mcu=atmega328p：标明开发板的mcu型号
* uno.build.f_cpu=16000000L：标明cpu的执行频率
* uno.build.board=AVR_UNO ：指定开发板
* uno.build.core=arduino：arduino IDE会根据这个配置使用\core\arduino目录下的源代码进行编译，前文有提到，core目录下存放的内核相关的所有arduino实现源代码。
* uno.build.variant=standard：指定variant的目录，根据上文所说，variants/目录中存放的是IO端口到arduino API端口的映射，arduino IDE会根据这个配置读取
variants\standard\目录下的引脚配置方式。
关于官方arduino UNO的board.txt的配置就介绍完了，但是这并不是全部的配置选项，有兴趣的可以查看源代码实现，但是需要知道的是，这个配置文件中所有内容都是为arduino IDE编译时选择相应平台相应硬件而服务。
#### platform.txt配置文件
这个文件中包含的是CPU相关的体系结构的定义，编译器，构建过程参数，烧录工具等等。同样的，不同平台之间使用的往往是不同的交叉编译工具链和编译选项，所以arduino IDE在编译时也必须从多种平台的编译工具链和编译选项中找到相对应的部分。
我们来看看官方的arduino avr对应的platform.txt配置文件(部分)：

    name=Arduino AVR Boards
    version=1.6.20

    compiler.warning_flags=-w
    compiler.warning_flags.none=-w
    compiler.warning_flags.default=
    compiler.warning_flags.more=-Wall
    compiler.warning_flags.all=-Wall -Wextra

    # Default "compiler.path" is correct, change only if you want to override the initial value
    compiler.path={runtime.tools.avr-gcc.path}/bin/
    compiler.c.cmd=avr-gcc
    compiler.c.flags=-c -g -Os {compiler.warning_flags} -std=gnu11 -ffunction-sections -fdata-sections -MMD -flto -fno-fat-lto-objects
    compiler.c.elf.flags={compiler.warning_flags} -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections
    compiler.c.elf.cmd=avr-gcc
    compiler.S.flags=-c -g -x assembler-with-cpp -flto -MMD
    compiler.cpp.cmd=avr-g++
    compiler.cpp.flags=-c -g -Os {compiler.warning_flags} -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics -MMD -flto
    compiler.ar.cmd=avr-gcc-ar
    compiler.ar.flags=rcs
    compiler.objcopy.cmd=avr-objcopy
    compiler.objcopy.eep.flags=-O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0
    compiler.elf2hex.flags=-O ihex -R .eeprom
    compiler.elf2hex.cmd=avr-objcopy
    compiler.ldflags=
    compiler.size.cmd=avr-size

    # This can be overridden in boards.txt
    build.extra_flags=

    # These can be overridden in platform.local.txt
    compiler.c.extra_flags=
    compiler.c.elf.extra_flags=
    compiler.S.extra_flags=
    compiler.cpp.extra_flags=
    compiler.ar.extra_flags=
    compiler.objcopy.eep.extra_flags=
    compiler.elf2hex.extra_flags=

    # AVR compile patterns

    ## Compile c files
    recipe.c.o.pattern="{compiler.path}{compiler.c.cmd}" {compiler.c.flags} -mmcu={build.mcu} -DF_CPU={build.f_cpu} -DARDUINO={runtime.ide.version} -DARDUINO_{build.board} -DARDUINO_ARCH_{build.arch} {compiler.c.extra_flags} {build.extra_flags} {includes} "{source_file}" -o "{object_file}"

    ## Compile c++ files
    recipe.cpp.o.pattern="{compiler.path}{compiler.cpp.cmd}" {compiler.cpp.flags} -mmcu={build.mcu} -DF_CPU={build.f_cpu} -DARDUINO={runtime.ide.version} -DARDUINO_{build.board} -DARDUINO_ARCH_{build.arch} {compiler.cpp.extra_flags} {build.extra_flags} {includes} "{source_file}" -o "{object_file}"

    ## Compile S files
    recipe.S.o.pattern="{compiler.path}{compiler.c.cmd}" {compiler.S.flags} -mmcu={build.mcu} -DF_CPU={build.f_cpu} -DARDUINO={runtime.ide.version} -DARDUINO_{build.board} -DARDUINO_ARCH_{build.arch} {compiler.S.extra_flags} {build.extra_flags} {includes} "{source_file}" -o "{object_file}"

    ## Create archives
    # archive_file_path is needed for backwards compatibility with IDE 1.6.5 or older, IDE 1.6.6 or newer overrides this value
    archive_file_path={build.path}/{archive_file}
    recipe.ar.pattern="{compiler.path}{compiler.ar.cmd}" {compiler.ar.flags} {compiler.ar.extra_flags} "{archive_file_path}" "{object_file}"

    ## Combine gc-sections, archives, and objects
    recipe.c.combine.pattern="{compiler.path}{compiler.c.elf.cmd}" {compiler.c.elf.flags} -mmcu={build.mcu} {compiler.c.elf.extra_flags} -o "{build.path}/{build.project_name}.elf" {object_files} "{build.path}/{archive_file}" "-L{build.path}" -lm

    ## Create output files (.eep and .hex)
    recipe.objcopy.eep.pattern="{compiler.path}{compiler.objcopy.cmd}" {compiler.objcopy.eep.flags} {compiler.objcopy.eep.extra_flags} "{build.path}/{build.project_name}.elf" "{build.path}/{build.project_name}.eep"
    recipe.objcopy.hex.pattern="{compiler.path}{compiler.elf2hex.cmd}" {compiler.elf2hex.flags} {compiler.elf2hex.extra_flags} "{build.path}/{build.project_name}.elf" "{build.path}/{build.project_name}.hex"

    ## Save hex
    recipe.output.tmp_file={build.project_name}.hex
    recipe.output.save_file={build.project_name}.{build.variant}.hex

    ## Compute size
    recipe.size.pattern="{compiler.path}{compiler.size.cmd}" -A "{build.path}/{build.project_name}.elf"
    recipe.size.regex=^(?:\.text|\.data|\.bootloader)\s+([0-9]+).*
    recipe.size.regex.data=^(?:\.data|\.bss|\.noinit)\s+([0-9]+).*
    recipe.size.regex.eeprom=^(?:\.eeprom)\s+([0-9]+).*

    ## Preprocessor
    preproc.includes.flags=-w -x c++ -M -MG -MP
    recipe.preproc.includes="{compiler.path}{compiler.cpp.cmd}" {compiler.cpp.flags} {preproc.includes.flags} -mmcu={build.mcu} -DF_CPU={build.f_cpu} -DARDUINO={runtime.ide.version} -DARDUINO_{build.board} -DARDUINO_ARCH_{build.arch} {compiler.cpp.extra_flags} {build.extra_flags} {includes} "{source_file}"

    preproc.macros.flags=-w -x c++ -E -CC
    recipe.preproc.macros="{compiler.path}{compiler.cpp.cmd}" {compiler.cpp.flags} {preproc.macros.flags} -mmcu={build.mcu} -DF_CPU={build.f_cpu} -DARDUINO={runtime.ide.version} -DARDUINO_{build.board} -DARDUINO_ARCH_{build.arch} {compiler.cpp.extra_flags} {build.extra_flags} {includes} "{source_file}" -o "{preprocessed_file_path}"

    # AVR Uploader/Programmers tools
    tools.avrdude.path={runtime.tools.avrdude.path}
    tools.avrdude.cmd.path={path}/bin/avrdude
    tools.avrdude.config.path={path}/etc/avrdude.conf

    tools.avrdude.network_cmd={runtime.tools.arduinoOTA.path}/bin/arduinoOTA

    tools.avrdude.upload.params.verbose=-v
    tools.avrdude.upload.params.quiet=-q -q
    # tools.avrdude.upload.verify is needed for backwards compatibility with IDE 1.6.8 or older, IDE 1.6.9 or newer overrides this value
    tools.avrdude.upload.verify=
    tools.avrdude.upload.params.noverify=-V
    tools.avrdude.upload.pattern="{cmd.path}" "-C{config.path}" {upload.verbose} {upload.verify} -p{build.mcu} -c{upload.protocol} "-P{serial.port}" -b{upload.speed} -D "-Uflash:w:{build.path}/{build.project_name}.hex:i"

    tools.avrdude.program.params.verbose=-v
    tools.avrdude.program.params.quiet=-q -q
    # tools.avrdude.program.verify is needed for backwards compatibility with IDE 1.6.8 or older, IDE 1.6.9 or newer overrides this value
    tools.avrdude.program.verify=
    tools.avrdude.program.params.noverify=-V
    tools.avrdude.program.pattern="{cmd.path}" "-C{config.path}" {program.verbose} {program.verify} -p{build.mcu} -c{protocol} {program.extra_params} "-Uflash:w:{build.path}/{build.project_name}.hex:i"

    tools.avrdude.erase.params.verbose=-v
    tools.avrdude.erase.params.quiet=-q -q
    tools.avrdude.erase.pattern="{cmd.path}" "-C{config.path}" {erase.verbose} -p{build.mcu} -c{protocol} {program.extra_params} -e -Ulock:w:{bootloader.unlock_bits}:m -Uefuse:w:{bootloader.extended_fuses}:m -Uhfuse:w:{bootloader.high_fuses}:m -Ulfuse:w:{bootloader.low_fuses}:m

    tools.avrdude.bootloader.params.verbose=-v
    tools.avrdude.bootloader.params.quiet=-q -q
    tools.avrdude.bootloader.pattern="{cmd.path}" "-C{config.path}" {bootloader.verbose} -p{build.mcu} -c{protocol} {program.extra_params} "-Uflash:w:{runtime.platform.path}/bootloaders/{bootloader.file}:i" -Ulock:w:{bootloader.lock_bits}:m
    tools.avrdude_remote.upload.pattern=/usr/bin/run-avrdude /tmp/sketch.hex {upload.verbose} -p{build.mcu}
    tools.avrdude.upload.network_pattern="{network_cmd}" -address {serial.port} -port {upload.network.port} -sketch "{build.path}/{build.project_name}.hex" -upload {upload.network.endpoint_upload} -sync {upload.network.endpoint_sync} -reset {upload.network.endpoint_reset} -sync_exp {upload.network.sync_return}

    # USB Default Flags
    # Default blank usb manufacturer will be filled in at compile time
    # - from numeric vendor ID, set to Unknown otherwise
    build.usb_manufacturer="Unknown"
    build.usb_flags=-DUSB_VID={build.vid} -DUSB_PID={build.pid} '-DUSB_MANUFACTURER={build.usb_manufacturer}' '-DUSB_PRODUCT={build.usb_product}'
鉴于编译和上传部分的复杂性，如果展开讲需要大量的篇幅，而且一般用户并不要对这一部分作过多修改，所以这里博主只是简要地对配置内容进行描述：
* 从文件头部可以看出，首先需要设置对应的名称和版本，为了验证这个名称的作用，博主试了一下，发现名称在这里只是一个标识，如果名称错误也可以正常使用到对应的platform.txt文件。
* 然后就是编译选项，指定相应的编译工具链，编译工具链的地址
* 指定某些可以在其它配置文件中覆盖此文件中配置的选项
* 指定编译源文件的地址，依赖库的地址
* 指定是否生成hex文件以及hex和bin文件生成的目录，方便用户使用swd或者jtag接口下载
* 标准串口烧录接口(或翻译成上传工具)的选项，端口选择方式，目标文件格式及地址等等。

#### programmers.txt配置文件
主要是一些外部烧写工具的配置，通常用于在裸板(即不含bootloader)烧录bootloader或者裸跑程序。当不需要使用第三方工具时，此配置文件可以为空。

好了，关于arduino的板级目录框架讨论就到此为止了，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***下一篇我们着重讲解arduino的编译过程***

***个人邮箱：linux_downey@sina.com***
***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.