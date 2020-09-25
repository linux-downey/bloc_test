问题：
	设备地址是怎么分配的？
	cmd 串行是如何传输的？单向的传输是如何保证不冲突的？
	校验方式？

emmc 特性：
	时钟 0~200 MHZ
	11 数据线：clock、data strobe、cmd、8 bit 数据线，电源线、reset除外
	
数据保护的机制：
	支持密码
	常驻数据
	上电模式
	临时部分
	
读写模式：
	单 block 读
	多 block 读

数据移除命令：
	Erase：擦除
	Trim：修剪？
	Sanitize？

意外断电时的数据保护方法

自定义能力

省电模式



# 系统特性

## 高级用法
* 掉电通知
* 高优先级中断
* 后台操作
* 分区
* 高级区域
* 实时时钟
* 分区属性
* 上下文管理
* 系统数据标记
* packed 命令
* 动态特性
* 可选的 cache
* 命令队列
* 高级 strobe
* Package Case Temperature？

## 当使用 boot 模式时，可以快速地读取 boot 分区的数据
## 受保护的 block 可以使用签名
## 两种大容量设备：512 Byte 的小扇区和 4K 的块


# emmc 概览
11 根数据线，reset 和电源，以及 emmc 部分的 Device Controller 由 emmc 的标准定义的。 
其它的比如 Host Controller的实现，以及 emmc 的容量、内存的访问由实现决定，不受标准的限制。 

e•MMC规范的先前实现（版本v4.1以上）使用32位字段实现了字节寻址。此寻址机制允许e•MMC密度高达2 GB（包括2 GB）。为了支持更大的密度，寻址机制已更新为支持扇区地址（512 B扇区）。扇区地址应用于容量大于2 GB的所有设备。要确定寻址模式，主机应读取OCR寄存器中的位[30:29]

## 硬件接口：
* CLK：0~200 MHZ，每一个时钟就是一个数据的传输点，DDR 模式下一个脉冲可以传输两个 bit。
* Data Strobe：只会在 HS400 模式下用到，由 emmc device 控制，
* CMD：串行信号线，用于传输 CMD 信号，commond信号有两种，在 init 模式下开漏，在快速 command 模式下推挽，command 由主机发出，由 device 响应，注意：emmc 接口是主从模式的。 
* DAT0~DAT7：数据线在推挽模式下，在重新上电或者 reset 之后，只有 DAT0 被用作传输数据，后续可以配置使用 DAT0~DAT7来传输数据，也可以是 DAT0~DAT3，DAT1~DAT7 有内部上拉，在使用到数据线的传输时，就会断开内部上拉。 


## 内部寄存器
CID：16 字节，设备 ID number
RCA：2 字节，设备地址，主机在初始化阶段给 emmc 动态分配的地址
DSR：2 字节，驱动阶段寄存器，配置 device 的输出驱动
CSD：16 字节，设备相关数据信息
OCR：4字节，操作条件寄存器，用于特殊的广播命令来确定设备的电压等级
EXT_CSD:512 字节，扩展的设备相关数据，

## 复位方式
* 电源上下电
* 复位信号
* 通过发送特殊的命令

## 总线协议
在重启之后，主机需要通过特殊的指令来初始化设备，主机与设备之间的交互分为以下几种：
* command：command 命令是通过 cmd 信号线串行地传输的
* response：设备响应，由主机发起 command，然后设备响应
* data：data 由数据线传输，数据线可配置为 1线、4线、8线传输，也可以 sdr 和 ddr

emmc 基本上是面向 block 的命令，主要的目的就是在主机和设备之间传输数据块，支持单块和多块传输。多块传输时，通过 stop cmd 来终止传输

在 block 写的时候，使用 DAT0 来作为 busy 位，指示当前设备是否处于忙状态而应该停止传输，读的时候不需要设置 busy 位。  



## 协议细节
对于 CMD，CRC 是 8 bits，对于 block 操作，CRC 是 16 bits。  

CMD 协议，
	第一个 bit 为 0，
	第二个 bit 表示传输方向，1表示主机->设备，反之亦然
	中间 38 bits 为 contents
	接下来 7 bits 为 CRC 校验位
	最后一个 bit 为结束位，为 1（将 CMD 拉高即可？device 通过判断接收到 48 bit 作为结束？）

CMD 线上的 response 可以是 48bits 或者 136 bits：
	R1，R3，R4，R5 的长度是 48 bits，R2 的 response 是 136 bits

数据在数据线上传输时的状态：
	1线时，先传输字节的最高位，逐次地传输每个字节
	4线 SDR 时，DAT3 对应高位，DAT0 对应低位，比如，第一次传输一个 byte 的 b7～b4，第二次传输一个 byte 的 b3～b0
	4线 DDR 时，clk 的上升沿传输奇数字节，下降沿传输偶数字节，而并不是两个电平沿传输同一个字节

	8线 SDR 时，DAT7 对应高位，DAT0 对应低位，每次传输一个字节
	8线 DDR 时，clk 的上升沿传输奇数字节，下降沿传输偶数字节

CRC 的传输有点特殊，CMD 的 CRC 是串行传输，没什么好说的

data 的 CRC 为 2 字节，但是在 4 线或者 8 线传输的时候并不是 4（2） 次传完，对于 CRC 的传输，每条并行线都是传输 CRC 的同一个位，也就是 8 线的时候，第一个电平所有线都传输 CRC 的 b15. （暂时存疑？）

8bits HS400，带 data strobe 的，page33，暂时不管


Boot 阶段的 positive ack，上升沿传输 010，开始0+010（上升沿传输）+结束1
Boot 阶段的 negative ack，上升沿传输 101

HS200 和 HS400 这两种高速模式只支持  1.8V 和 1.2V


在主机写数据的时候，CLK 和 data 线都是由主机自行控制，而在读数据时，clk 是主机控制，而 data 由设备控制，在总线上传输的数据可能因为传输本身或者其它的干扰情况而延迟，所以需要调整采样点，主机可以通过 CMD21 读取 device 的参数来得到最佳的采样点。  


对 data strobe 的理解：在主机接收数据的时候，对采样点是不确定的，因为这需要经历两个 delay：1、时钟到达设备端，2、设备端从准备到真正开始发送数据，这里的 delay 时间是不定的，而 data strobe 当数据真正发送时，与数据一起，这样主机端接收数据时通过判断 data strobe 进行采样，就可以省去调整采样点的时间，同时更加稳定。  data strobe 也是方波，而数据不定，所以可以通过判断 data strobe 来确定数据是什么时候开始发的。   


# emmc 功能描述

## emmc 的几种模式
Boot mode：在重新上电、reset 或者接收到 CMD0(参数为 0xF0F0F0F0) 之后，emmc 会进入到 boot mode。  

Device identification mode：boot mode 完成之后，或者主机不支持 boot mode，emmc 会进入到当前 mode，直到接收到 SET_RCA command (CMD3)，这个命令用于设置地址。 

Interrupt mode:主机和设备会同时进入到中断模式，这种模式下没有数据的传输，唯一允许的是中断服务。  

data transfer mode：主机向设备分配完地址之后，就进入到了数据传输模式，

Inactive mode：当设备处于无效的电压范围，或者主机的访问模式无效时，或者主机直接发送  GO_INACTIVE_STATE command (CMD15) 时，将进入到非激活模式。  

设备的状态 对应的 具体的模式，以及对应的 CMD line 的引脚输出模式，参考手册 page38。  


## emmc 中的分区结构
一个用户分区，用于存放数据，用户可以在这基础上继续进行分区(常见做法)
两个可能存在的 boot 分区，用于 boot 启动的分区，特点是可以快速读取
一个 RPMB 分区，这个分区主要用于安全方面

emmc 的某些区域可以被实现为 OTP memory。 

命令与分区的限制：
boot 分区：Command class 6 (Write Protect) and class 7 (Lock Device) 不能使用
RPMB 分区：只有 Class0, Class2 and Class4 被允许，
通用分区：Command classes 0, 2, 4, 5, 6 可以使用，

扩展属性：
每个通用分区都可以使用一些扩展属性，
* default：没有额外属性
* System code：很少更新、或者是包含重要的系统文件
* Non-Persistent：经常会被修改的分区
通过这些扩展属性，emmc 可以进行一些针对性的优化


主机分区的流程：
page 43。 






