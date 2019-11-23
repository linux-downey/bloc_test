# pwm 与设备树
作为一个外设，pwm 硬件控制器自然也通过设备树进行管理，这个 Linux 驱动的统一规则。这一章节我们就来讲讲设备树中 pwm 的设置。  

## controller节点
每一个 pwm 硬件控制器都将在设备树中被表述为一个 pwm controller 节点，controller 在 Linux 中是一个通用的概念，对应相应的硬件控制器，也可以理解为 provider(生产者或提供者)，也就是 pwm 输出的提供者。而使用 pwm 资源的被称为 consumer(消费者)，例如 backlight 节点中需要使用到 pwm 控制器所提供的 pwm 输出，它就是一个 consumer。  

这种关系在生活中随处可见，当我们借书时，需要从书架取出，还书时就放回到架子中。这里的书架就是 provider，它负责管理书籍的初始化、更新和存放，而 consumer 只需要拿和放。  


一个 controller 节点的定义一般是这样的：

```
pwm: pwm@7000a000 {
		compatible = "VID,cust_name";
		reg = <0x7000a000 0x100>;
		#pwm-cells = <3>;
        clocks = <&clk CLK_INDEX1>,
                <&clk CLK_INDEX2>;
        clock-names = "clk1_pwm", "clk2_pwm";
        status = "okay";
        ...
	};
```

因为 pwm 的功能实现足够简单，所以也没有太多额外的属性定义：
* pwm: pwm@7000a000 : 对于节点名而言，通常是标号加节点名，标号(pwm)主要是方便其他节点进行引用，且具有描述性的作用，而节点名的命名规则通常是 描述名+外设基地址。
* compatible : 驱动匹配时使用的字符串，标准结构为 "厂商名，自定义名称"，比如："ti,omap_pwm"
* reg : 外设寄存器基地址以及寄存器地址的长度，当我们操作 pwm 时实际上就是读写寄存器的值。
* #pwm-cells ： 这部分是比较重要的，指定了 pwm 的控制方式，一个 pwm 硬件控制器往往提供多路 pwm 的输出，且 pwm 的输出也包含一系列的参数，consumer 需要使用 pwm 的时候需要指定参数，而这个参数决定了 consumer 在引用时需要提供的参数个数，主要针对设备树节点中的引用。    
* clocks 和 clock-names：pwm 的输出为方波，自然需要输入对应的时钟，这两部分对应输入的时钟，clock-names对应时钟名，作为时钟的一个名称，必要时以此作为索引。在某些平台上，使用定时器的输出作为时钟输入，时钟可能指定为定时器节点。

通常，独立于 Linux 标准设备树语法之外，对于不同的平台可能会有一些不同的自定义字段，毕竟不同的平台可能有不同的使用方式，归根结底，设备树的作用就是对硬件的描述，软件上就需要在对应的驱动中解析这些自定义字段以进行设置。  

当然，仅有设备树节点是不够的，设备树更像是一个配置文件，指定一些关键配置，然后在对应的驱动中，读取这些配置进行相应的初始化操作，这就涉及到 pwm controller 的驱动部分，这部分是 pwm 框架的核心，我们将在后续文章中详细介绍。  


## consumer节点
有 provider(生产者) 自然就有 consumer(消费者)，用户驱动程序如果想直接使用某一路 pwm 的输出功能，自然地，那就要到 provider 那里取。这部分操作大部分都是可以直接在设备树中完成。

就像上文中提到的图书馆与借阅者的示例，如果你需要到图书馆借书，需要提供一系列的信息：需要借基本、书名、书架号、书的版本等等。“借用” pwm 设备也是一样，同样需要提供相应的信息以确保能定位到相应的 pwm 设备。 事实上，Linux 中大部分外设节点的 "借用" 都是依照这个模式(这种借用在设备树中被称为节点引用)。  

下列是一个 pwm 节点引用的设备树节点：

```
bl: backlight {
		pwms = <&pwm1 0 5000000 PWM_POLARITY_INVERTED>;
		pwm-names = "backlight";
	};
```

这是一个非常简单的应用，从 backlight 可以看出，这是一个背光控制的驱动程序，需要用到 pwm 信号来控制 led，主要是两个节点：
* pwms : 这个节点就是向 controller 申请 pwm 资源，一个 < > 确定一路 pwm 的引用，第一个值是对应的 controller 节点，后续的三个参数则是指定对应 pwm 的参数，比如 period、duty-cycle 和 polarity。需要多少个参数来描述一个 pwm 则是由上文中 controller 部分指定的。
        需要注意的是，名称必须为 pwms ，而不能使用自定义的名称。
* pwm-names:  为 pwm 设备指定一个名称，作为识别。

如果是多路 pwm，也可以使用列表引用，比如下面的操作：
```
bl: backlight {
		pwms = <&pwm1 0 5000000 PWM_POLARITY_INVERTED
                &pwm1 1 5000000 PWM_POLARITY_INVERTED
                >;
		pwm-names = "backlight","backlight1";
	};
```




参考：[官方设备树文档](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/pwm/pwm.txt)

