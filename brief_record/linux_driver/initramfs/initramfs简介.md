# initramfs 实现


mv initrd.img-4.14.108-ti-r124 initrd.img.tar.gz
tar -xvf initrd.img.tar.gz
file initrd.img.tar.gz
gzip -d initrd.img.tar.gz

cpio -i < initrd.img.tar

就获取到 initramfs 的解压缩包。


find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
gzip initramfs.cpio



为什么存在 initramfs:
1,不能把所有的驱动编译进内核
2,文件系统检查,万一文件系统所处的物理磁盘有问题呢
3,boot 被加密怎么办
4,/usr 目录处于不同的存储介质上怎么办




