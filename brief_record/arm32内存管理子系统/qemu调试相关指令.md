需要添加调试函数的时候，为了避免优化，需要使用 __attribute__((optimize("O0"))) 声明函数。

// 服务端

qemu-system-arm -S -s -m 256M -M sabrelite -kernel arch/arm/boot/zImage -dtb arch/arm/boot/dts/100ask_imx6ull-14x14.dtb



// 如果要调试汇编代码，要把两个段的 symbol 重定位

add-symbol-file vmlinux -s .head.text 0x10008000

add-symbol-file vmlinux -s .text 0x10100000



// 监听端

arm-linux-gnueabi-gdb vmlinux

target remote localhost:1234





// 设置内存大小

b early_init_dt_scan_memory

n

set base=0x80000000

set size=0x20000000



b bootmem_init 