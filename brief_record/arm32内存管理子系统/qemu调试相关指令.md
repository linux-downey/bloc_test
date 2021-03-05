需要添加调试函数的时候，为了避免优化，需要使用 __attribute__((optimize("O0"))) 声明函数。



 __attribute__((optimize("O0"))) 



// 服务端

qemu-system-arm -S -s -m 256M -M sabrelite -kernel arch/arm/boot/zImage -dtb arch/arm/boot/dts/100ask_imx6ull-14x14.dtb



// 如果要调试汇编代码，要把两个段的 symbol 重定位

add-symbol-file vmlinux -s .head.text 0x10008000

add-symbol-file vmlinux -s .text 0x10100000



针对 percpu 类型的符号，是不能进行调试的，只能通过其它的方法，要不就是通过内存偏移来做，要么就是赋值给其它符号再间接查看。 



// 监听端

arm-linux-gnueabi-gdb vmlinux

target remote localhost:1234





// 设置内存大小

b early_init_dt_scan_memory

n

set base=0x80000000

set size=0x20000000



b bootmem_init 

