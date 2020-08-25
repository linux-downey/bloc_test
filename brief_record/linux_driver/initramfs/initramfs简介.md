# initramfs 实现


mv initrd.img-4.14.108-ti-r124 initrd.img.tar.gz
tar -xvf initrd.img.tar.gz
file initrd.img.tar.gz
gzip -d initrd.img.tar.gz

cpio -i < initrd.img.tar

就获取到 initramfs 的解压缩包。


find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
gzip initramfs.cpio
