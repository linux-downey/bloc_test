# linux设备树的解析  
## 从start_kernel开始

    void __init setup_arch(char **cmdline_p)
    {
        const struct machine_desc *mdesc;
        mdesc = setup_machine_fdt(__atags_pointer);
        ...
    }


__atags_pointer这个全局变量存储的就是r2的寄存器值，是设备树在内存中的起始地址,将设备树起始地址传递给setup_machine_fdt，对设备树进行解析。接着跟踪setup_machine_fdt()函数：

    const struct machine_desc * __init setup_machine_fdt(unsigned int dt_phys)
    {
        const struct machine_desc *mdesc, *mdesc_best = NULL;                    
        if (!dt_phys || !early_init_dt_verify(phys_to_virt(dt_phys)))           ——————part 1
		    return NULL;

        mdesc = of_flat_dt_match_machine(mdesc_best, arch_get_next_mach);       ——————part 2

        early_init_dt_scan_nodes();                                             ——————part 3
        ...
    }
第一部分先将设备树在内存中的物理地址转换为虚拟地址，然后再检查该地址上是否有设备树的魔数(magic)，魔数就是一串用于识别的字节码，如果没有或者魔数不匹配，表明该地址没有设备树文件，函数返回，否则验证成功，将设备树地址赋值给全局变量initial_boot_params。  
第二部分of_flat_dt_match_machine(mdesc_best, arch_get_next_mach)，逐一读取设备树根目录下的compatible属性，然后将compatible中的属性一一与内核中支持的硬件单板相对比，匹配成功后返回相应的machine_desc结构体指针。machine_desc结构体中描述了单板相关的一些硬件信息，这里不过多描述。
第三部分就是扫描设备树中的各节点，主要分析这部分代码。

    void __init early_init_dt_scan_nodes(void)
    {
        of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
        of_scan_flat_dt(early_init_dt_scan_root, NULL);
        of_scan_flat_dt(early_init_dt_scan_memory, NULL);
    }
出人意料的是，这个函数中只有一个函数的三个调用，直觉告诉我这三个函数调用并不简单。  
























