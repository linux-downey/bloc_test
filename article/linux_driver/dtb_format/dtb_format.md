# linux设备树格式
设备树的一般操作方式是：开发人员根据开发需求编写dts文件，然后使用dtc将dts编译成dtb文件。dts文件是文本格式的文件，而dtb是二进制文件，在linux启动时被加载到内存中，接下来我们需要来分析设备树dtb文件的格式。   
## 为什么要了解设备树dtb文件的格式
dtb作为二进制文件被加载到内存中，然后由内核读取并进行解析，如果对dtb文件的格式不了解，那么在看设备树解析相关的内核代码时将会寸步难行，而阅读源代码才是了解设备树最好的方式，所以，如果需要更透彻的了解设备树解析的细节，第一步就是需要了解设备树的格式。  

***注：本文档参考官方文档：https://github.com/torvalds/linux/blob/master/Documentation/devicetree/booting-without-of.txt   ***

## dtb格式总览
dtb的格式是这样的：TODO

### dtb header
但凡涉及到数据的记录，就一定会有一个总的描述部分，就像磁盘的超级块，书的目录，dtb当然也不例外，这个描述头部就是dtb的header部分，通过这个header部分，用户可以快速地了解到整个dtb的大致信息。  
header可以用这么一个结构体来描述：

	struct fdt_header {
		fdt32_t magic;			 /* magic word FDT_MAGIC */
		fdt32_t totalsize;		 /* total size of DT block */
		fdt32_t off_dt_struct;		 /* offset to structure */
		fdt32_t off_dt_strings;		 /* offset to strings */
		fdt32_t off_mem_rsvmap;		 /* offset to memory reserve map */
		fdt32_t version;		 /* format version */
		fdt32_t last_comp_version;	 /* last compatible version */

		/* version 2 fields below */
		fdt32_t boot_cpuid_phys;	 /* Which physical CPU id we're
							booting on */
		/* version 3 fields below */
		fdt32_t size_dt_strings;	 /* size of the strings block */

		/* version 17 fields below */
		fdt32_t size_dt_struct;		 /* size of the structure block */
	};

#### magic
设备树的魔数，魔数其实就是一个用于识别的数字，表示设备树的开始，linux dtb的魔数为 0xd00dfeed.

#### totalsize
这个设备树的size，也可以理解为所占用的实际内存空间。

#### off_dt_struct
offset to dt_struct，表示整个dtb中structure部分所在内存相对头部的偏移地址

#### off_dt_strings
offset to dt_string，表示整个dtb中string部分所在内存相对头部的偏移地址

#### off_mem_rsvmap
offset to memory reserve map，dtb中memory reserve map所在内存相对头部的偏移地址，

#### version
设备树的版本，截至目前的最新版本为17. 

#### last_comp_version
最新的兼容版本

#### boot_cpuid_phys
这部分仅在版本2中存在，后续版本不再使用。

#### size_dt_strings
表示整个dtb中string部分的大小

#### size_dt_struct
表示整个dtb中struct部分的大小



### alignment gap
中间的alignment gap部分表示对齐间隙，它并非是必须的，它是否被提供以及大小由具体的平台对数据对齐和的要求以及数据是否已经对齐来决定。  



### memory reserve map
memory reserve map：描述保留的内存部分，这个map的数据结构是这样的：

	{
		uint64_t physical_address;
		uint64_t size;
	}
这部分存储了此结构的列表，整个部分的结尾由一个数据为0的结构来表示(即physical_address和size都为0，总共16字节)。    
这一部分的作用是告诉内核哪一些内存空间需要被保留而不应该被系统覆盖使用，因为在内核启动时常常需要动态申请大量的内存空间，只有提前进行注册，用户需要使用的内存才不会被系统征用而造成数据覆盖。值得一提的是，对于设备树而言，即使不指定保留内存，系统也会默认为设备树保留相应的内存空间。  
同时，这一部分需要64位(8字节)对齐。


### device-tree structure
device-tree structure：每个节点都会被描述为一个struct，节点之间可以嵌套，因此也会有嵌套的struct。  
structure的的结构是这样的：

* 一个node开始信号，OF_DT_BEGIN_NODE，数据为0x00000001
* 对于版本1-3而言，这一部分是节点的全路径，以/开头，而对于版本16及以上，这部分只是unit name(root 除外，它没有unit name)，unit name是以0结尾的字符串
* 可选的对齐字节
* 对于每个属性字段：
	* 由OF_DT_PROP标识，数据为0x00000003
	* 32位的数据，表示属性的size
	* 32位的数据，表示属性名在string block中的偏移地址
	* 属性中的value data.
* 如果有子节点，递归地对子节点进行描述。
* 节点结束信号，OF_DT_END_NODE ，数据为0x00000002.
每个节点的信息都按照上述结构被描述，需要注意的是，所有用于描述一个特定节点的属性都必须在任何子节点之前定义，虽然设备树的层次结构不会因此产生二义性，但是linux kernel的解析程序要求这么做。  




### device-tree strings
device-tree strings：在dtb中有大量的重复字符串，比如"model","compatile"等等，为了节省空间，将这些字符串统一放在某个地址，需要使用的时候直接使用索引来查看，需要注意的是，属性部分格式为key = value，key部分被放置在strings部分，而value部分的字符串并不会放在这一部分，而是直接放在structure中。  



### dtb 示例

整个头部还是很简单且清晰明了的，下面我们以一个示例文件来演示header结构：

	/dts-v1/;
	/ {

		compatible = "hd,test_dts", "hd,test_xxx";
		#address-cells = <0x1>;
		#size-cells = <0x1>;
		model = "HD test dts";
		
		chosen {
			stdout-path = "/ocp/serial@ffff";
		};
		memory@80000000 {
			device_type = "memory";
			reg = <0x80000000 0x10000000>;
		};
		led1:led@2000000 {
			compatible = "test_led";
			#address-cells = <0x1>;
			#size-cells = <0x1>;
			reg = <0x200 0x4>;
		};
	};

编译当前dts文件，获取对应的dtb文件。 

鉴于dtb文件为二进制文件，普通编辑器打开显示乱码，我们使用ultraEdit查看，它将数据以16进制形式显示：


整个dtb文件还是比较简单的，图中的红色框出的部分为header部分的数据，可以看到：

	第一个四字节对应magic，数据为 D00DFEED.
	第二四字节对应totalsize，数据为 000001BC，可以由整张图片看出，这个dtb文件的大小由0x0~0x1bb,大小刚好是0x1bc
	第三个四字节对应off_dt_struct，数据为00000038。
	第四个四字节对应off_dt_strings，数据为00000174，可以由整张图片看到，从0x174开始刚好是字符串开始的地方
	第五个四字节对应off_mem_rsvmap，数据为00000028
	第六个四字节对应version，数据为00000011，十进制为17
	第七个四字节对应last_comp_version，数据为00000010，十进制为16，表示兼容版本16
	第八个四字节对应boot_cpuid_phys，数据为00000000，仅在版本2中使用，这里为0
	第九个四字节对应size_dt_strings，数据为00000048，表示字符串总长。
	第十个四字节对应size_dt_struct，数据为0000013c，表示struct部分总长度。

##  mem_rsvmap部分
在上文中看到，前0x28字节为头部，而头部的off_mem_rsvmap部分数据为0x00000028，即从0x28开始表示mem_rsvmap部分，而dtb文件中从0x28开始的两个8字节为0，在上文中提到：整个部分的结尾由一个数据为0的结构来表示，所以这里并没有设置mem_rsvmap。

## structure 部分
上文回顾：每一个属性都是以 key = value的形式来描述，value部分选。
来到0x00000038(0x28+16d),接下来8个字节为00000003，根据上述structure中的描述，这是OF_DT_PROP，即标示属性的开始。  
接下来4字节为00000018，表明该属性的value部分size为24字节。  
接下来4字节是当前属性的key在string 部分的偏移地址，这里是00000000，由头部信息中off_dt_strings可以得到，string部分的开始为00000174，偏移地址为0，所以对应字符串为"compatible".
之后就是value部分，这部分的数据是字符串，可以直接从右侧栏看出，总共24字节的字符串"hd,test_dts", "hd,test_xxx",因为字符串之间以0结尾，所以程序可以识别出这是两个字符串。  
可以看出，到这里，compatible = "hd,test_dts", "hd,test_xxx";这个属性就被描述完了，对于属性的描述还是非常简单的。  

按照固有的规律，接下来就是对#address-cells = <0x1>的解析，然后是#size-cells = <0x1>...
然后就是递归的子节点chosen，memory@80000000等等都是按照上文中提到的structure解析规则来进行解析，最后以00000002结尾。  
而整个结构的结束由00000009来描述。  

## string 部分
可以由上一小节structure部分中看出，string部分的作用是被structure部分引用。












