内存模型的介绍 ： https://zhuanlan.zhihu.com/p/220068494

目前 arm 使用的是 FALT_MEM.



memblock 相关的接口：

bottom_up 成员变量控制分配内存的方向，为 0 表示从高地址向低地址分配。

current_limit 表示最高物理地址

memory 区域始终记录的是设备树中指定的物理内存的大小。



memblock 申请内存： early_alloc(size).

申请内存不需要指定 start，系统自动选择 start，申请到的内存被放在 memblock.reserved.regions 中。 

如果申请的内存是连续的，会将连续的 reserved.regions 合并为一个 region。

针对 pte 类型的 create_mapping 调用，需要申请物理内存页来存放页表。 





