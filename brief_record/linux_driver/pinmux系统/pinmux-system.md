# linux - pinmux 的设置
在日常的驱动调试工作中，经常涉及到驱动的移植，通常来说，大多数厂商都会为自家的设备提供一份参考设备驱动程序，一般情况下，驱动移植就是重新适配底层的接口，比如 IO 控制引脚，板级接口的配置，在这一过程中，经常要和 IOMUX 打交道，这部分知识点非常基础，也是必须要熟练掌握的。  


## 为什么会出现 IOMUX
随着需求的扩张，人们对于智能设备的要求也越来越高，硬件的发展速度也越来越快，这一点不仅体现在主频、内存这些核心参数上，同时也体现在 IO pin 上，pin 是内部器件与外部通信的接口，比如最常见的 USB、串口、网口、音视频接口，都需要通过 pin 连接到处理器上。  

从最先接触的双列直插十几个 pin 的 8051 单片机到现在动辄几百 pin 的 CPU，pin 的体积不断缩小，数量不断增多，尽管发展的速度非常快，但是 CPU 的体积是有限制的、因此同样限制了 pin 的数量，而人们对智能设备提出的期望是无限的，对于有两个独立网口的系统来说，人们总会想：要是有 4 个网口我就可以做更多的事。因此，硬件从来不会有足够的时候，同时对于 pin 而言，多少都是不够的。  

芯片体积和 pin 数量之间一直存在这一些矛盾，伴随着这种矛盾发展出一种叫 PINMUX 的映射技术，也叫 IO 复用，简单地说，就是一个 pin 引脚可以通过系统配置切换内部的电路将它作为不同的输出功能，比如一个引脚可以配置成普通 IO 输入输出，同时也可以配置成 uart 的 rx 引脚(需要硬件电路的支持)，根据开发者需求而定。这种带有多种可选功能的 pin 脚通常也被称为 pad。  

## 如何配置 pinmux 寄存器
pinmux 的设置是与硬件强相关的,哪些 pad 支持功能服用以及支持复用于哪些功能都是在硬件设计阶段定下来的,并没有多少自由发挥的余地,驱动工程要做的无非就是找到需要配置的 pad,然后将该 pad 配置成对应的功能.   

既然是硬件相关,那就需要通过芯片编程手册来获取相应的 pinmux 信息,对于不同的硬件平台,pinmux 的设置有或多或少的区别,但是其基本原理大抵是相通的,在手册中找到 pinmux 相关的章节,一般来说,手册都会详细列出每个 pad 所对应的 pin 功能以及配置方法,或者从外设的角度出发,列出每个外设需要使用的 pad 以及其对应的配置方法.   

假设某个设备挂在串口上,而该串口默认的引脚因为某些原因正被 i2c 控制器占用,容易想到的一个办法是:通过查询手册看该串口输出 pin 是否支持映射到其它的 pad,在支持引脚复用的芯片上一般都是没有问题的,于是我们找到另一对对应这个串口的 rx/tx(不考虑硬件流控)输出引脚,该 rx/tx 引脚目前没有被占用或者被没有使用到的外设占用,这时候就可以将其征用.   

驱动的移植完成了第一步:找到需要配置的 pad.接下来还有两步需要完成:
* 配置目标 pad 为串口功能,对于单一的引脚而言可能支持配置成 gpio,串口输出,emmc数据引脚等功能.
* 配置引脚的的电气特性,电气特性包括上下拉的配置,是否开漏,引脚翻转速率等.  

这些功能的配置通常通过一个或者两个寄存器完成,至于寄存器是如何对应的,完全取决于硬件上的实现了.  
 

## 设备树如何配置 pinmux
硬件上的特性都是在设备树中体现的,设备树的存在可以让驱动工程师在不修改代码的情况下移植驱动,设备树源文件的地址通常是保存在 arch/${ARCH}/boot/dts/${VENDER}/ 目录下,而且通常也分布在多个文件中,对于 pinmux 的设置,设备树中通常是这样的步骤:

首先,通常对于外设引脚 pinmux 的配置,通常是以 group 的形式存在的,比如对于串口而言,自然是将 rx/tx 以 group 的形式进行配置,而且这个 group 需要位于 pinmux 节点下作为一个子节点,当 pinmux 功能进行配置时同时也会解析节点下所有子节点,设备树配置形式通常是这样的:

```
iomux:iomux@30000000{
    compatible = "vendor,iomux";
    pinctrl_uart0_grp:uart0_grp{
        pinctrl,pins = < ${MUXREG1},${PINCONF1},${MUXREG_VAL1},${PINCONF_VAL1}
                         ${MUXREG2},${PINCONF2},${MUXREG_VAL2},${PINCONF_VAL2}
                        >
    }
}
```
由此,就定义了一个 pinmux grp 的配置, MUXREG1,PINCONF1 分别表示该 pad 对应的功能和电气特性的寄存器地址,MUXREG_VAL1,PINCONF_VAL1分别对应配置值.   

对于不同的平台,grp 中字段的名称,寄存器的内容,数量以及对应值可能是不一样的,这取决于硬件的实现以及驱动如何解析,总之,设备树的定义和驱动的解析是相对应的,只有框架性的内容有强制性的格式规定,大部分情况下都是驱动开发者为适配驱动代码而定义的格式.   

看起来这些引脚的设定比较复杂,需要熟读手册,但是在实际的开发中,我们完全可以打开厂商提供的设备树文件,参考系统自带的配置,然后按照它的形式修修改改就可以了,不过在手册中找到对应寄存器设置这个工作还是躲不掉的.  


仅仅定义了 pinmux 的设置方式是不够的,还需要知道如何使用它,这就像是 C 语言中的函数定义,还需要被调用,调用它的方式是这样的:

```
uart:uart@31000000{
    compatible = "vendor,uart";
    ...
    pinctrl-names = "default","sleep";
    pinctrl-0 = <&pinctrl_uart0_grp>;
    pinctrl-1 = <&pinctrl_uart0_sleep_grp>;
    ...
    status = "okay";
}
```
在对应的 uart 节点中,对上文中设置的 pinctrl_uart0_grp 节点进行引用,也就是将 pinctrl_uart0_grp 节点绑定到当前的 pinctrl 设置中.  

可以注意到,在上面的节点信息中,还有一个 pinctrl-names 节点,值分别为 "defaule","sleep",这对应的是当前 uart 对应引脚需要设置的状态,问题是 uart 的设置一般就是一种,为什么需要两个呢?   

在不同的需求下,可能对 pinmux 的设置有不同的要求,通常出于电源管理的考虑,包含的 sleep 节点是用于在系统休眠时对 pinmux 的设置. pinctrl-names 和后续的 pinctrl-n 是一一对应的, pinctrl-names 的第一个字符串对应 pinctrl-0 的 pinmux 设置,第二个对应 pinctrl-1 的 pinmux 设置.  

也就是在该示例中,当串口要正常使用时,配置为 default 形式的 pinux,当系统休眠时,配置 sleep 形式的 pinmux,也就是 pinctrl-1.  

需要注意的是,pinctrl-names 和 pinctrl-n 这种对于 pinmux 引用的描述形式尽管没有写到设备树的基本规则中,但是大部分平台都是使用这种命名和格式来解析 pinmux 与外设的绑定,基本上是通用的.  
   

## 代码如何配置
设备树中进行了外设和 pinmux 的绑定,如何在实际的代码中进行调用呢?内核自然提供了相应的接口,这些一节一部分是由内核完成的,一部分是由厂商的 BSP 工程师完成的.  

我们已经了解到,外设和对应的 pinmux grp 已经通过某种形式进行了绑定,这种形式一般是在该外设的结构体中由某一个指针指向了该 pinmux 的设置,这里我们暂时不管,我们需要知道的是如何在驱动代码中应用这些 pinmux 设置.  

```
void pinmux_set(stcut device *dev)
{
    struct pinctrl *p;
    struct pinctrl_state *default_state;
    p = devm_pinctrl_get(dev->of_node);
    default_state = pinctrl_lookup_state(p,"default");  //将 pinmux 设置为 default 模式
    pinctrl_select_state(p,default_state);
}
```
获取该 uart 对应的 struct device 结构,通过 dev->of_node 就可以获取到设备树的节点,然后通过 devm_pinctrl_get 函数获取到当前设备节点下引用的所有 pinmux 设置,保存在 struct pinctrl 类型结构体中.  

pinctrl_lookup_state 函数可以根据 pinmux 节点对应的名称获取到对应的 pinmux 设置参数,比如上文中设置的 "default" 和 "sleep",获取到的 pinmux 设置参数保存在 struct pinctrl_state 类型的结构体中.   

获取到对应的设置方式,就可以通过 pinctrl_select_state 接口设置想要的 pinmux 状态了.  

当然,在实际的操作中需要逐一地判断函数的返回值.  

## sysdebug 接口

## gpio 设置需要注意的部分
如果是设置单一的或者是一组 gpio,需要特别注意的是 sysdebug 接口所显示的 pin num 和真实的 pin num 是不一致的,需要使用 of_get_named_gpio 接口,同时设备树需要设置成:

```
    gpios = <&gpion x statu>;
```






