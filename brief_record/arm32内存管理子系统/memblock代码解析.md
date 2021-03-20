memblock 会在 debugfs 中创建 memblock 目录。

内存管理系统是针对物理内存的管理。

memblock 的前身是 bootmem。

内存申请本身需要内存来保存管理所需要的数据，当然内存可以进行静态地定义，但是鉴于伙伴系统具有一定的复杂性，需要的内存比较多，因此需要一个简单的内存管理策略，本身不占用太多物理内存，就可以节省内存。 



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





