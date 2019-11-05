设备树的 reserved_memory 节点：

官方文档地址：https://github.com/torvalds/linux/blob/v4.14/Documentation/devicetree/bindings/reserved-memory/reserved-memory.txt

/*********************************/
#address-cells = <2>;
#size-cells = <2>;

这两个属性需要和根节点下的属性一致。
/*********************************/

/*********************************/
ranges; 节点必须存在，且值为空。
/*********************************/

/*********************************/
reserved memory分为两种节点，一种为动态节点，一种为静态节点，需要是两种中的一种。
静态节点：
	必须提供一个 reg 属性，指明开始地址和size。
动态节点：
	必须提供一个size，保留内存的大小。
	可选地提供一个对其，表示对其的属性
	可选的提供一个内存申请范围，值可以是一个数组，指定可以进行申请的内存范围。

如果同时指定了 reg(表示静态) 和 size(表示动态)，则静态优先。
/*********************************/


/*********************************/
其他特性：
	compatible：值可能为两种：
		shared-dma-pool:表明这是用于dma设备的缓冲池，被系统所接管
		厂商指定字符串，匹配特定驱动
	no-map:没有对应的值，表示不对该物理地址进行映射，系统不进行接管。
	reuseable：没有对应的值，表示该内存应该可以重复使用，而不应该被一个驱动程序长期占用，
				适合应用于易失数据或者缓存数据，用过之后立马会搬到其他地方。
	
/*********************************/

/*********************************/
如果存在 linux,cma-default 属性，linux会将内存作为 CMA 内存使用
如果存在 linux,dma-default 属性，linux会将内存作为 DMA 内存使用
/*********************************/


示例：
/ {
	#address-cells = <1>;
	#size-cells = <1>;

	memory {
		reg = <0x40000000 0x40000000>;
	};

	reserved-memory {
		#address-cells = <1>;
		#size-cells = <1>;
		ranges;

		/* global autoconfigured region for contiguous allocations */
		linux,cma {
			compatible = "shared-dma-pool";
			reusable;
			size = <0x4000000>;
			alignment = <0x2000>;
			linux,cma-default;
		};

		display_reserved: framebuffer@78000000 {
			reg = <0x78000000 0x800000>;
		};

		multimedia_reserved: multimedia@77000000 {
			compatible = "acme,multimedia-memory";
			reg = <0x77000000 0x4000000>;
		};
	};

	/* ... */

	fb0: video@12300000 {
		memory-region = <&display_reserved>;
		/* ... */
	};

	scaler: scaler@12500000 {
		memory-region = <&multimedia_reserved>;
		/* ... */
	};

	codec: codec@12600000 {
		memory-region = <&multimedia_reserved>;
		/* ... */
	};
};


定义了静态的内存映射之后，在需要使用的驱动中，比如 vedio 和 音频系统中，使用 memory-region 来指定 dma 内存部分。

/*********************************/


/*********************************/
除此之外，reserved memory节点还支持ramoops节点，将内核的 调试、panic、oops、trace信息记录到 ram 中。
文档地址在：https://github.com/torvalds/linux/blob/v4.14/Documentation/devicetree/bindings/reserved-memory/ramoops.txt
/*********************************/

关于 cma 的文档：http://www.wowotech.net/memory_management/cma.html
