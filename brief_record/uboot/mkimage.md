
./tools/mkimage -n board/freescale/mx6ullevk/imximage.cfg.cfgtmp -T imximage -e 0x87800000 -d u-boot-dtb.bin u-boot-dtb.imx  

-d  datafile

u-boot-dtb.imx 比 u-boot-dtb.bin 多了 6249 个字节

params.type 等于 10，IH_TYPE_IMXIMAGE

tparams->header_size = 3072, 头部加了 3072 个字节，尾部的 3177 个字节加的就是全 0 

这 3072 个字节中大部分数据都是来自于 board/freescale/mx6ullevk/imximage.cfg 文件。尾部数据为0。   


tparams->hdr 在 imx 的回调函数中生成的。 

## cat 将 dtb 放到 image 中。 
cat u-boot-nodtb.bin dts/dt.dtb > u-boot-dtb.bin


## objcopy
arm-linux-gnueabihf-objcopy --gap-fill=0xff  -j .text -j .secure_text -j .secure_data -j .rodata -j .hash -j .data -j .got -j .got.plt -j .u_boot_list -j .rel.dyn -j .efi_runtime -j .efi_runtime_rel -O binary  u-boot u-boot-nodtb.bin

只 copy 指定的段到 bin 文件中，

## 整个链接过程
LD      u-boot
OBJCOPY u-boot-nodtb.bin
CAT     u-boot-dtb.bin
MKIMAGE u-boot-dtb.imx


## imx 头部
头部包含了 dcd 配置文件，配置文件中包含各种外设寄存器的设置，主要是 ddr。也就是说，ddr 在 bootrom 阶段就已经初始化完成了。   

头部的长度为 0xc00，也就是 3072，头部的各个字段会告诉 bootrom ，bootadta 的地址、dcd 文件的地址、uboot 的加载地址等等，这些实现都是很灵活的。 


1、处理并保存参数，参数保存在 struct image_tool_params 中。 
2、根据参数的 type 获取到静态定义的 image_type. 定义在 tools/imximage.c 中。返回 struct image_type_params 类型的 image。里面有很多回调函数。
3、调用 type 中的 check_params 回调函数，检查参数
4、检查 -x flag，这是 XIP 的选项，通常不会用到
5、检查 -f flag，这是 FIT 相关的，目前没用到，如果没有定义 -f，就打开 imagefile，这是最后的生成文件，由 mkimage 的最后一个参数给出
6、打开 datafile，这个 data file 就是源 image 文件，获取最终的文件大小 params.file_size，就是原文件大小 + 头部大小。 
7、如果 tparams->vrec_header 被定义，就调用，自定义生成 header
8、调用 copy_file 
9、调用  set_header 回调
10、调用 print_header 回调

结束













单板对应的 header。只占了 52 个字节。 
struct imx_header {
	union {
		imx_header_v1_t hdr_v1;
		imx_header_v2_t hdr_v2;
	} header;
};




先得把 mmap 搞定，这个东西很常用。 
