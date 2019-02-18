# linux设备树的解析  

*** 本设备树解析基于arm平台 ***

## 从start_kernel开始
*** 注：鉴于linux源代码的复杂性，在本篇的代码跟踪中 ，博主就只贴出相关代码，试图构造一个清晰的框架 ***
start kernel原型是这样的：
    asmlinkage __visible void __init start_kernel(void)
    {
        ...
        setup_arch(&command_line);
        ...
    }
设备树的处理基本上就在这个函数中。  

### setup_arch

    void __init setup_arch(char **cmdline_p)
    {
        const struct machine_desc *mdesc;
        mdesc = setup_machine_fdt(__atags_pointer);
        ...
        arm_memblock_init(mdesc);
        ...
        unflatten_device_tree();
        ...
    }

### setup_machine_fdt
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
首先of_scan_flat_dt()这个函数接收两个参数，一个是函数指针，一个为boot_command_line，boot_command_line是一个静态数组，存放着启动参数,而of_scan_flat_dt函数的作用就是扫描设备树中的节点，然后以各节点为参数依次调用其第一个参数中传入的函数。  
在上述代码中，传入的参数分别为early_init_dt_scan_chosen，early_init_dt_scan_root，early_init_dt_scan_memory这三个函数，从名称可以猜测到，这三个函数分别是处理chosen节点、root节点中除子节点外的属性信息、memory节点。  

    int __init early_init_dt_scan_chosen(unsigned long node, const char *uname,int depth, void *data){
        ...
        p = of_get_flat_dt_prop(node, "bootargs", &l);
	    if (p != NULL && l > 0)
		    strlcpy(data, p, min((int)l, COMMAND_LINE_SIZE));
        ...
    }
经过代码分析，第一个被传入的函数参数作用是获取bootargs，然后将bootargs放入boot_command_line中，作为启动参数，而并非处理整个chosen节点。  
接下来再看第二个函数调用：

    int __init early_init_dt_scan_root(unsigned long node, const char *uname,int depth, void *data)
    {
        dt_root_size_cells = OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
        dt_root_addr_cells = OF_ROOT_NODE_ADDR_CELLS_DEFAULT;

        prop = of_get_flat_dt_prop(node, "#size-cells", NULL);
        if (prop)
            dt_root_size_cells = be32_to_cpup(prop);
        prop = of_get_flat_dt_prop(node, "#address-cells", NULL);
        if (prop)
            dt_root_addr_cells = be32_to_cpup(prop);
        ...
    }
通过进一步代码分析，第二个函数执行是为了将root节点中的#size-cells和#address-cells属性提取出来，并非获取root节点中所有的属性，放到全局变量dt_root_size_cells和dt_root_addr_cells中。

接下来看第三个函数调用：

    int __init early_init_dt_scan_memory(unsigned long node, const char *uname,int depth, void *data){
        ...
        if (!IS_ENABLED(CONFIG_PPC32) || depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
        reg = of_get_flat_dt_prop(node, "reg", &l);
        endp = reg + (l / sizeof(__be32));

        while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
            base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		    size = dt_mem_next_cell(dt_root_size_cells, &reg);
            early_init_dt_add_memory_arch(base, size);
        }
    }
函数先判断节点的unit name是memory@0,如果不是，则返回。然后将所有的reg属性取出来，根据address-cell和size-cell的值进行解析，然后调用early_init_dt_add_memory_arch()来申请相应的内存空间。

到这里，setup_machine_fdt()函数对于设备树的第一次扫描解析就完成了，主要是获取了一些设备树提供的总览信息。  
接下来继续回到setup_arch()函数中，继续向下跟踪代码。  


### arm_memblock_init

    void __init arm_memblock_init(const struct machine_desc *mdesc)
    {
        ...
        early_init_fdt_reserve_self();
        early_init_fdt_scan_reserved_mem();
        ...
    }
arm_memblock_init()对于设备树的初始化而言，主要做了两件事：
1. 调用early_init_fdt_reserve_self，根据设备树的大小为设备树分配空间，设备树的totalsize在dtb头部中有指明，因此当系统启动之后，设备树就一直存在在系统中。
2. 扫描设备树节点中的"reserved-memory"节点，为其分配保留空间。

memblock_init对于设备树的部分解析就完成了，主要是为设备树指定保留内存空间。  

接下来继续回到setup_arch()函数中，继续向下跟踪代码。 

### unflatten_device_tree
这一部分就进入了设备树的解析部分：

    void __init unflatten_device_tree(void)
    {
        __unflatten_device_tree(initial_boot_params, NULL, &of_root,     —————— part1
                    early_init_dt_alloc_memory_arch, false);

        of_alias_scan(early_init_dt_alloc_memory_arch);                  —————— part2
        ...
    }

#### of_alias_scan
为了讲解的方便，我们先来看part2，从名字来看，这个函数的作用是解析根目录下的alias，跟踪代码：

    void of_alias_scan(void * (*dt_alloc)(u64 size, u64 align)){
        of_aliases = of_find_node_by_path("/aliases");
	    of_chosen = of_find_node_by_path("/chosen");
        if (of_chosen) {
            if (of_property_read_string(of_chosen, "stdout-path", &name))
                of_property_read_string(of_chosen, "linux,stdout-path",
                            &name);
            if (IS_ENABLED(CONFIG_PPC) && !name)
                of_property_read_string(of_aliases, "stdout", &name);
            if (name)
                of_stdout = of_find_node_opts_by_path(name, &of_stdout_options);
	    }
        for_each_property_of_node(of_aliases, pp) {
            ...
            ap = dt_alloc(sizeof(*ap) + len + 1, __alignof__(*ap));
            if (!ap)
                continue;
            memset(ap, 0, sizeof(*ap) + len + 1);
            ap->alias = start;
            of_alias_add(ap, np, id, start, len);
            ...
        }
    }
从上文贴出的程序来看，of_alias_scan()函数先是处理设备树chosen节点中的"stdout-path"或者"stdout"属性(两者最多存在其一)，然后将stdout指定的path赋值给全局变量of_stdout_options，并将返回的全局struct device_node类型数据赋值给of_stdout，指定系统启动时的log输出。  
接下来为aliases节点申请内存空间，如果一个节点中同时没有name/phandle/linux,phandle，对于这些特殊节点将不会申请内存空间。  
of_chosen和of_aliases都是struct device_node型的全局数据。  


#### __unflatten_device_tree
我们再来看最主要的设备树解析函数：

    void *__unflatten_device_tree(const void *blob,struct device_node *dad,struct device_node **mynodes,void *(*dt_alloc)(u64 size, u64 align),bool detached){
        int size;
        ...
        size = unflatten_dt_nodes(blob, NULL, dad, NULL);
        ...
        mem = dt_alloc(size + 4, __alignof__(struct device_node));
        ...
        unflatten_dt_nodes(blob, mem, dad, mynodes);
    }

主要的解析函数为unflatten_dt_nodes(),在__unflatten_device_tree()函数中，unflatten_dt_nodes()被调用两次，第一次是扫描得出设备树转换成device node需要的空间，然后系统申请内存空间，第二次就进行真正的解析工作，我们继续看unflatten_dt_nodes()函数：
值得注意的是，在第二次调用unflatten_dt_nodes()时传入的参数为unflatten_dt_nodes(blob, mem, dad, mynodes);  
第一个参数是设备树存放首地址，第二个参数是申请的内存空间，第三个参数为父节点，初始值为NULL，第四个参数为mynodes，初始值为of_node.

    static int unflatten_dt_nodes(const void *blob,void *mem,struct device_node *dad,struct device_node **nodepp)
    {
        ...
        for (offset = 0;offset >= 0 && depth >= initial_depth;offset = fdt_next_node(blob, offset, &depth)) {
            populate_node(blob, offset, &mem,nps[depth],fpsizes[depth],&nps[depth+1], dryrun);
            ...
         }
    }
这个函数中主要的作用就是从根节点开始，对子节点依次调用populate_node()，从函数命名上来看，这个函数就是填充节点，为节点分配内存。  
我们继续往下追踪：

    static unsigned int populate_node(const void *blob,int offset,void **mem,
				  struct device_node *dad,unsigned int fpsize,struct device_node **pnp,bool dryrun){
        struct device_node *np;
        ...
        np = unflatten_dt_alloc(mem, sizeof(struct device_node) + allocl,__alignof__(struct device_node));
        of_node_init(np);
        np->full_name = fn = ((char *)np) + sizeof(*np);
        if (dad != NULL) {
			np->parent = dad;
			np->sibling = dad->child;
			dad->child = np;
		}
        ...
        populate_properties(blob, offset, mem, np, pathp, dryrun);
        np->name = of_get_property(np, "name", NULL);
		np->type = of_get_property(np, "device_type", NULL);
        if (!np->name)
			np->name = "<NULL>";
		if (!np->type)
			np->type = "<NULL>";
        ...
    }
通过跟踪populate_node()函数，可以看出，首先为当前节点申请内存空间，使用of_node_init()函数对node进行初始化，of_node_init()函数也较为简单：

    static inline void of_node_init(struct device_node *node)
    {
        kobject_init(&node->kobj, &of_node_ktype);
        node->fwnode.ops = &of_fwnode_ops;
    }
设置kobj，接着设置node的fwnode.ops。  
然后再设置一些参数，需要特别注意的是：对于一个struct device_node结构体，申请的内存空间是sizeof(struct device_node)+allocl,这个allocl是节点的unit_name长度(类似于chosen、memory这类子节点描述开头时的名字，并非.name成员)。  
然后通过np->full_name = fn = ((char *)np) + sizeof(*np);将device_node的full_name指向结构体结尾处，即将一个节点的unit name放置在一个struct device_node的结尾处。  
同时，设置其parent和sibling节点。  
接着，调用populate_properties()函数，从命名上来看，这个函数的作用是为节点的各个属性分配空间。  
紧接着，设置device_node节点的name和type属性，name由设备树中.name属性而来，type则由设备树中.device_type而来。  

一个设备树中节点转换成一个struct device_node结构的过程渐渐就清晰起来，现在我们接着看看populate_properties()这个函数，看看属性是怎么解析的：

    static void populate_properties(const void *blob,int offset,void **mem,struct device_node *np,const char *nodename,bool dryrun){
        for (cur = fdt_first_property_offset(blob, offset);
	     cur >= 0;
	     cur = fdt_next_property_offset(blob, cur)) {
             fdt_getprop_by_offset(blob, cur, &pname, &sz);
             unflatten_dt_alloc(mem, sizeof(struct property),__alignof__(struct property));
         }
    }























