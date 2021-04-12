# linux 网卡驱动程序 - 0 硬件以及设备树配置
一说到网卡驱动程序，脑海中不自觉就蹦出了各种网络的概念，从而牵扯出那些复杂的网络模型、各种层级的协议，让人望而生畏。  

事实上，linux中的网卡驱动程序并不难，至少没有想象中的难。  

所谓的驱动程序，它的功能无非是驱动硬件工作，那么，我们第一个需要弄清楚的问题就是：网卡的工作是什么？

事实上，答案很简单：网卡和串口、i2c 等硬件外设没有区别，都只是负责收发数据。同时，得益于 linux 内核框架的优秀设计，linux 网络驱动子系统的实现完全与协议无关，也就是说：linux 网卡驱动程序只需要负责接收数据，然后调用硬件发送数据。又或者是从网络接口中接收数据，然后将数据上传给上层，完全无需关心协议的处理，对于编写驱动而言，这是一件非常幸福的事。  

在上一章节中，我们介绍了网卡工作所在的网络层 - 数据链路层和物理层，虽然编写网卡驱动程序并不完全依赖于对数据链路层的理解，但是理解链路层和物理层的工作更有利于我们分析调试网卡驱动。  


## 网卡的构成
以太网的接口电路通常由两部分组成：MAC 控制器和 PHY 物理层接口，分别对应数据链路层和物理层的处理，常用的以太网卡分为两种：集成 mac 和 phy 的网卡和不集成 phy 的网卡。  

经典的 dm9000 网卡就是集成了 mac 和 phy ，同时该网卡通常直接连接在系统总线上，数据的收发直接通过操作寄存器的方式，优势就是开发简单，硬件和软件不需要额外的工作。

phy 中整合了大量的模拟硬件，而 mac 是典型的全数字硬件，从成本又或者是芯片面积上考虑，越来越多的系统集成厂商更倾向于将 mac 控制器集成到芯片中，让使用者决定 phy 的匹配，这样用户也可以根据实际使用情况更灵活地选择 phy，当然，通常最优先考虑的方面是成本。  

在以太网的数据传输中，传输数据时方向为：上层数据 -> mac 控制器 -> phy -> 物理链路，当 mac 和 phy 集成在一起时，开发者完全不需要考虑 mac 和 phy 之间的数据交互，由系统集成商完成。   

那么，当这两者独立存在的时候，数据是如何传递的呢？  

首先，需要明确的是：网卡驱动程序是针对 mac 控制器的，所以在网卡驱动程序中，我们要考虑一个问题：
* mac 是如何跟 phy 进行通信的？  
* 在驱动程序中 mac 将数据传递给 phy，phy 是怎样将数据发送出去的，是不是同时需要提供 phy 的驱动程序？  

mac 和 phy 之间的通信通过 MII(media independent interface) 接口进行，该接口又被称为媒体独立接口，它是 IEEE-802.3 协议定义的标准接口，媒体无关的意思就是在 mac 控制器不变的情况下，通过此接口连接任何符合标准的 phy 设备都是可以正常工作的。  

该接口包含了两个通信端口：一个数据接口和一个管理接口 MDIO.  

对于管理接口，使用的是 MDIO 总线，这里我们并不去详究该总线协议细节，可以单纯地把 MDIO 总线看成是串口总线一样的传输数据的协议。  

而数据接口更像是一个透传接口，802.3 标准规定了 phy 如何通过数据接口从 mac 接收数据以及如何将数据发送到物理网口，对于开发者而言无疑是一个好消息，这意味着驱动开发的时候像集成网卡一样只需要关注如何将数据从 mac 控制器发出，而不用去关注 phy 是如何接收且发送到网口的，因为这是标准的实现。  

那么，是不是开发者就不需要关心 phy 的驱动程序了呢？其实不然，MII 接口标准可以保证数据的收发，但是别忘了 phy 还有一个管理接口 MDIO，通过该管理接口，网卡可以对 phy 进行各种配置，这里涉及到 MDIO bus 的驱动程序，这里就不进一步赘述了。  

市面上常用的 phy 有 microchip 的 lan87xx 系列，对应驱动 drivers/net/phy/smsc.c 。
和 台湾九阳电子的 ip10xx 系列，对应的驱动 drivers/net/phy/icplus.c。  

有兴趣的可以去看看 phy 的驱动程序实现。  




## 网卡设备树的配置
linux 框架的良好设计以及普及程度，使得我们在项目开发过程中，通常会有硬件配套的驱动，而我们要做的，就是对驱动程序的移植，由厂商提供驱动程序模板，而开发者只需要修改模板中硬件差异的部分。   

随着设备树的推出，通常这项工作可以直接在设备树中完成。  

我们来看看集成网卡 dm9000 的设备树配置：

```
ethernet@18000000 {
		compatible = "davicom,dm9000";
		reg = <0x18000000 0x2 0x18000004 0x2>;
		interrupt-parent = <&gpn>;
		interrupts = <7 4>;
		local-mac-address = [00 00 de ad be ef];
		davicom,no-eeprom;
		reset-gpios = <&gpf 12 GPIO_ACTIVE_LOW>;
		vcc-supply = <&eth0_power>;
	};
```
这是 linux 源码中 Documentation/devicetree/bindings/net/davicom-dm9000.txt 下的 dm9000 配置示例。  

compatible：设备节点和 driver 节点的匹配字符串，需要和 driver 中的 compatible 进行匹配。  
reg ：该 reg 参数需要指定寄存器的物理地址和大小，同时需要两组参数，一组是 dm9000 的地址寄存器地址，一组是 dm9000 的数据寄存器地址，这个参数是硬件差异项，因为厂商在集成网卡到 CPU 的时候，将会根据芯片设计而决定它的外设地址。  
interrupt-parent：中断的父节点，通常网卡都是直接连接在 GIC(中断控制器) 上的，在复杂的设计中也可能存在 GIC 的级联，这一项硬件相关。  
interrupts：网卡设备的中断号，同样是硬件设计相关。  
local-mac-address:硬件 mac 地址的设置。  
davicom,no-eeprom:对于很多网卡，会在网卡中集成一个 i2c 接口的 eeprom，以保存网卡的一些重要信息，这里表示 dm9000 不带 eeprom，如果带一个 i2c 接口的 eeprom，则需要在对应的 i2c bus 下创建一个子节点以操作 eeprom。  
reset-gpios：复位引脚，通过控制该引脚的高低电平来控制 dm9000 的上电和掉电，硬件设计相关。  
vcc-supply：供电，同样是硬件设计相关。

了解设备树的朋友都知道，在设备树中，可以定义一些非标准的，只与特定硬件相关的参数，比如这里的 reg 属性，由于 dm9000 的特殊性，需要用两组参数来指定外设操作地址，而其他的网卡可能只需要提供一个外设基地址和 size ，这些就是根据硬件的特性来决定的，对于其他的集成网卡，这里就不再一一举例了。

dm9000 是集成型网卡，事实上，对于不集成 phy 的网卡，需要在设备树中同时指定使用的 mac 控制器和 PHY，mac 和 phy 需要在设备树中同时指定，因为通常 phy 也是需要对应的设备驱动程序的。  

对于非集成的网卡，通常 mac 控制器的设备树节点设置和集成式的差不多，只不过通常会在子节点中添加对应的 phy 节点，我们来看下面的例子：

```
ethernet{
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_enet2>;
        phy-mode = "rmii";
        phy-handle = <&ethphy1>;
        phy-reset-gpios = <&gpio5 6 GPIO_ACTIVE_LOW>;
        phy-reset-duration = <26>;
        status = "okay";

        mdio {
                #address-cells = <1>;
                #size-cells = <0>;

                ethphy0: ethernet-phy@0 {
                        compatible = "ethernet-phy-ieee802.3-c22";
                        smsc,disable-energy-detect;
                        reg = <0>;
                };
                ethphy1: ethernet-phy@1 {
                        compatible = "ethernet-phy-ieee802.3-c22";
                        smsc,disable-energy-detect;
                        reg = <1>;
                };
        };
};

```
在该以太网节点中，指定了 mac 和 phy 的交互协议为 rmii，与其他设备节点不一样的是，phy 的 compatible 属性是固定的，通常是 ethernet-phy-ieee802.3-c22 和 const: ethernet-phy-ieee802.3-c45 这两种，或者是 ethernet-phy-idxx.xx,它会匹配固定的驱动程序以实现标准的数据传输功能。  

mii 的管理接口通过 mdio 总线，所以 phy 节点的定义在 mdio bus 的子节点下，在驱动中通过 mdio 的方式进行数据配置。  

关于设备树的更多配置资料可以参考[官方文档-phy](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/net/ethernet-phy.yaml),[官方文档-mac](https://github.com/torvalds/linux/blob/master/Documentation/devicetree/bindings/net/ethernet-controller.yaml).











网络模型以及设备驱动程序在网络驱动程序中的应用
硬件上的连接
设备树的配置
设备驱动示例分析
设备驱动程序详解


















