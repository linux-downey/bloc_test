# devm_ - linux 设备资源的管理

随着 linux 的快速发展，驱动工程师为设备编写驱动程序变得越来越轻松，这得益于 linux 中优秀的驱动框架。开发一款设备驱动程序，通常只需要像做填空题一样把设备资源、硬件操作函数等硬件相关部分填充到相应 driver 部分，然后在设备树中指定 device 资源，注册到系统中，接下来的部分就由系统框架来自动完成。  

当然，理想是丰满的，现实又是另外一回事。就像我们信心满满地写完自认完美的代码，在编译阶段就一行报三个错。即使我们正常地调用系统接口，也有可能出现调用失败的情况，毕竟，驱动代码的编写依赖于系统，而系统的资源总不是无穷无尽的。  

在传统的驱动开发时，编写驱动程序通常是这样的：

```
    if(ret = alloc_resource1() == ERROR)
        return ;
    if(ret = alloc_resource2() == ERROR){
        goto err1;
    }
    if(ret = alloc_resource3() == ERROR){
        goto err2;
    }
    if(ret = alloc_resource4() == ERROR){
        goto err3;
    }
    err1:
        free_resource1();
        return ;
    err2:
        free_resource2();
        free_resource1();
        return ;
    err3:
        free_resource3();
        free_resource2();
        free_resource1();
        return ;    
```

在上述代码中,需要请求多个内核资源,鉴于是操作内核，需要严格检查每一项工作是否正常完成，当当前操作失败时，就需要将所有之前申请的内核资源返还给系统，从而维持系统的稳定运行。  

这样的代码是缺乏美感的，看起来像是有大量的冗余代码，但是似乎又想不到更好的方法。  

驱动工程师解决不了的问题，不代表 linux 系统设计者不能解决，linus 在随后的版本中，添加了自动管理内核资源的接口，目的是解决资源回收的问题。这一类接口有如下特点：
* 接口的命名方式就是在现有的内核资源申请函数前添加 devm_ 前缀，devm 即可译为 device manage，例如传统的申请内核内存接口 kmalloc() 对应的 devm_kmalloc(),使用后者的接口，无需在注销的时候调用kfree() 来释放资源。  
* 接口资源的释放是自动的，完全不需要驱动调用者的额外操作
* 接口是与相应驱动的 device 绑定的，它的实现原理也是基于 device 。

于是，上述代码就可以写成：

```
 if(ret = devm_alloc_resource1() == ERROR)
        return ;
if(ret = devm_alloc_resource2() == ERROR){
    return;
}
if(ret = devm_alloc_resource3() == ERROR){
    return;
}
if(ret = devm_alloc_resource4() == ERROR){
    return;
}
```
是不是看起来瞬间变得简洁明了。  

当然，并非内核中所有的函数都有对应的 devm_ 版本，既然是服务于资源管理，那么肯定是只有向内核申请资源的函数才有必要提供，下列是内核支持自动资源管理的接口列表：

#### CLOCK
devm_clk_get() devm_clk_get_optional() devm_clk_put() devm_clk_bulk_get() devm_clk_bulk_get_all() devm_clk_bulk_get_optional() devm_get_clk_from_childl() devm_clk_hw_register() devm_of_clk_add_hw_provider() devm_clk_hw_register_clkdev()

#### DMA
dmaenginem_async_device_register() dmam_alloc_coherent() dmam_alloc_attrs() dmam_free_coherent() dmam_pool_create() dmam_pool_destroy()

#### DRM
devm_drm_dev_init() 

#### GPIO
devm_gpiod_get() devm_gpiod_get_index() devm_gpiod_get_index_optional() devm_gpiod_get_optional() devm_gpiod_put() devm_gpiod_unhinge() devm_gpiochip_add_data() devm_gpio_request() devm_gpio_request_one() devm_gpio_free()

#### I2C
devm_i2c_new_dummy_device()

#### IIO
devm_iio_device_alloc() devm_iio_device_free() devm_iio_device_register() devm_iio_device_unregister() devm_iio_kfifo_allocate() devm_iio_kfifo_free() devm_iio_triggered_buffer_setup() devm_iio_triggered_buffer_cleanup() devm_iio_trigger_alloc() devm_iio_trigger_free() devm_iio_trigger_register() devm_iio_trigger_unregister() devm_iio_channel_get() devm_iio_channel_release() devm_iio_channel_get_all() devm_iio_channel_release_all()

#### INPUT
devm_input_allocate_device()

#### IO region
devm_release_mem_region() devm_release_region() devm_release_resource() devm_request_mem_region() devm_request_region() devm_request_resource()

#### IOMAP
devm_ioport_map() devm_ioport_unmap() devm_ioremap() devm_ioremap_nocache() devm_ioremap_wc() devm_ioremap_resource() : checks resource, requests memory region, ioremaps devm_iounmap() pcim_iomap() pcim_iomap_regions() : do request_region() and iomap() on multiple BARs pcim_iomap_table() : array of mapped addresses indexed by BAR pcim_iounmap()

#### IRQ
devm_free_irq() devm_request_any_context_irq() devm_request_irq() devm_request_threaded_irq() devm_irq_alloc_descs() devm_irq_alloc_desc() devm_irq_alloc_desc_at() devm_irq_alloc_desc_from() devm_irq_alloc_descs_from() devm_irq_alloc_generic_chip() devm_irq_setup_generic_chip() devm_irq_sim_init()

#### LED
devm_led_classdev_register() devm_led_classdev_unregister()

#### MDIO
devm_mdiobus_alloc() devm_mdiobus_alloc_size() devm_mdiobus_free()

#### MEM
devm_free_pages() devm_get_free_pages() devm_kasprintf() devm_kcalloc() devm_kfree() devm_kmalloc() devm_kmalloc_array() devm_kmemdup() devm_kstrdup() devm_kvasprintf() devm_kzalloc()

#### MFD
devm_mfd_add_devices()

#### MUX
devm_mux_chip_alloc() devm_mux_chip_register() devm_mux_control_get()
PER-CPU MEM
devm_alloc_percpu() devm_free_percpu()

#### PCI
devm_pci_alloc_host_bridge() : managed PCI host bridge allocation devm_pci_remap_cfgspace() : ioremap PCI configuration space devm_pci_remap_cfg_resource() : ioremap PCI configuration space resource pcim_enable_device() : after success, all PCI ops become managed pcim_pin_device() : keep PCI device enabled after release

#### PHY
devm_usb_get_phy() devm_usb_put_phy()

#### PINCTRL
devm_pinctrl_get() devm_pinctrl_put() devm_pinctrl_register() devm_pinctrl_unregister()

####  POWER
devm_reboot_mode_register() devm_reboot_mode_unregister()

#### PWM
devm_pwm_get() devm_pwm_put()

#### REGULATOR
devm_regulator_bulk_get() devm_regulator_get() devm_regulator_put() devm_regulator_register()

#### RESET
devm_reset_control_get() devm_reset_controller_register()

####  SERDEV
devm_serdev_device_open()

#### SLAVE DMA ENGINE
devm_acpi_dma_controller_register()

#### SPI
devm_spi_register_master()

#### WATCHDOG
devm_watchdog_register_device()



## 
从上文中我们了解了devm_ 机制的历史以及由来，并且知道了如何使用它。  

知道怎么用，就更应该知道为什么这么用？它的实现原理是怎样的？作为一名优秀的工程师，应该无时不刻保持着对事物内在运行原理的好奇心。  

接下来，我们以 devm_clk_get() 为例，就来进一步解析 devm 机制的原理。  

## 源码分析
devm_clk_get():  

```
static void devm_clk_release(struct device *dev, void *res)
{
	clk_put(*(struct clk **)res);
}

struct clk *devm_clk_get(struct device *dev, const char *id)
{
	struct clk **ptr, *clk;

	ptr = devres_alloc(devm_clk_release, sizeof(*ptr), GFP_KERNEL);
    ...

	clk = clk_get(dev, id);
	devres_add(dev, ptr);
    
    return clk;
}

```
上述就是精简之后的 devm_clk_get() 的函数原型，主要包括三部分：
* devres_alloc：这一个函数调用主要是申请设备管理资源，下文详解。
* clk = clk_get():正如上文中所说，devm_clk_get() 仅仅是在clk_get()的基础上添加了一个资源管理部分，其本质还是实现 clk_get() 函数的内容
* devres_add():将资源添加到 device 中  

看到这里就有两个问题：
1. clk_get() 函数是从系统中获取资源，对应释放的应该也是返回的 struct clk 结构体，为什么这里还要再次申请资源？
2. 第三部分是将资源添加到 device 中，那资源的自动释放是在哪里做的呢?

带着这两个疑问，我们接着往下看：

### struct devres 管理资源
先深入资源申请部分，跟踪函数的调用：

```
static inline void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp)
{
	return devres_alloc_node(release, size, gfp, NUMA_NO_NODE);
}

void * devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp, int nid)
{
	struct devres *dr;

	dr = alloc_dr(release, size, gfp | __GFP_ZERO, nid);
	if (unlikely(!dr))
		return NULL;
	return dr->data;
}
```
首先，从 devm_clk_get() 的实现中可以看到，一共传入了三个参数：
* 指向 devm_clk_release 的函数指针，从名字中的 release 不难猜到，这个函数是注册的一个注销回调函数，在资源需要释放的时候就会调用这个函数来释放 clk 资源，看 devm_clk_release 的实现确实也是这样的。  
*  void *res：这是申请的资源对应的指针，根据该指针来找到申请资源的地点。

紧接着，看到 devres_alloc 的实现，最终调用了 devres_alloc_node()，作用是为 struct devres *dr 申请了一片内存，但是这里有两点比较奇怪：
* 第一：申请内存的 size 不是 size(struct devres),因为我们平常为一个结构体申请内存一般是申请刚好能装下这个结构体的空间，也就是指定 sizeof(struct devres).
* 第二：这里返回是 dr->data ,而不是 dr 结构体。


既然是申请了 struct devres *dr，有必要看看 struct devres 的定义：
```

struct devres_node {
	struct list_head		entry;
	dr_release_t			release;
};

struct devres {
	struct devres_node		node;
	/* -- 3 pointers */
	unsigned long long		data[];	/* guarantee ull alignment */
};
```
从 struct devres 部分的定义可以看到，其中包含一个 struct devres_node结构体，以及一个没有定义长度的数组，实际上这就是传说中的零长数组。  

而 struct devres_node 部分则是一个链表节点和一个 release 函数指针，这一部分不难猜测：在一个 device 中，申请的资源可能不止一个，那么这些资源由链表的形式进行管理，而链表头则放在 device 结构体中，这样每一个需要管理的资源都可以直接由 device 结构索引并管理了，而在需要释放的时候，就调用相对应的 release 函数。   

内核资源的组织形式大概是了解了，就是通过链表来实现，那么还有一个问题：向内核申请的资源是各种各样的形式，如何存放这部分资源呢？在当前的 clk 中，申请的内核资源为 struct clk* 类型的，也就是一个指针类型，释放的时候调用 clk_put() 就可以了。  

但是对于其他的，例如 irq 部分的，释放接口是这样的：

```
free_irq(irq, dev_id);
```

除了 irq 之外，还有 dev_id ，总之，对于不同的资源申请与释放,需要保存的内核资源数量是不同的，如果要做到所有资源都用统一的接口来管理，那就面临着资源对象大小不定的挑战。  

对于这种情况，很多朋友瞬间想到一种解决方案：使用 void* 指针，它可以处理这种情况。那么，void* 到底能不能解决这个问题呢？  

答案是，不行！不知道你还记不记得 linux 的面相对象机制，以 container_of() 宏为核心机制的面相对象的结构体组织形式，使得一些关键的基础数据结构得以嵌入到其他结构体中，以基础数据结构反向获取属主(暂且这么说，A中包含基础数据结构B，A为属主)结构体的数据。如果你足够细心，就会发现所有嵌入的基础数据结构都是定义为示例，而非指针，如果定义指针，是不能反向获取属主。

这时候 零长数组 就派上用场了，也就是上述的 

```
unsigned long long		data[];
```  

这是 GNU C 的扩展特性，与 void* 相区别的是，它提供了一个**结构体内确定地址的指针！**，根据它的地址可以反向获取到 struct devres 结构，然后再获取到 struct devres_node 结构体，到这里，整个资源的索引和释放都变得顺理成章了。   

这也解答了上述提出的一个问题：

```
申请内存的 size 不是 size(struct devres)，而是传入的参数 size。
```
答案就是，用户在调用 devm_ 系列函数的时候，将需要保存的系统资源大小传递给函数，在申请资源的时候就申请相应 size 的资源总量，资源多于 struct devres_node 的部分由 data 去访问。  
例如，假设一个 devm_ 接口，在释放资源时需要用到额外的三个指针大小的数据空间，申请部分就是这样的：

```
src = kmalloc(sizeof(devres_node)+3*sizeof(int*),...)
```

而额外的数据则由 data[0],data[1],data[2]来访问。  


### devres_add(dev, ptr)
资源创建过后，当然是要加到对应的 device 中，才能起到由 device 进行资源管理的作用。  

```
static void add_dr(struct device *dev, struct devres_node *node)
{
    ...
	list_add_tail(&node->entry, &dev->devres_head);
}

void devres_add(struct device *dev, void *res)
{
	struct devres *dr = container_of(res, struct devres, data);
	add_dr(dev, &dr->node);
    ...
}

devres_add(dev, ptr);
```

添加资源管理到 device 也正如上文中描述的那样，通过资源也就是零长数组 data 反向获取 struct devres结构体，然后将 struct devres_node 中的链表节点挂在 device->devres_head 链表头节点。  


## 资源的释放
将资源添加 device 中算是完成了资源管理中非常重要的一部分。但是，别忘了 devm_ 机制的初衷是什么，是系统自动管理内核资源的申请释放。在传统的内核中，申请早就已经存在，而自动释放才是重点。  

那新版本的接口到底是怎么实现资源的自动释放的呢？

至少从上文中可以了解到，我们已经把 device 申请的系统资源绑定在 device 上并且可以索引，然后根据传入的 release 函数释放资源，那么，到底在什么时候这个资源会被释放呢？  

我们可以了解的是，linux 内核中，所有的设备资源都是和 device 结构绑定在一起的，当设备被创建的时候，部分设备资源就已经被创建。  

那么，反过来想，在设备 device 被注销的时候就需要释放所有的资源，这个思路是没有错的，所以在调用类似 i2c_unregister_device、input_unregister_device 肯定有设备资源的注册行为，也就会调用上文中提到的 struct devres 中的 release 接口释放资源。  

除了在 device 被创建时申请资源以外，还有另一种情况，比如 devm_clk_get()、dev_request_irq(),通常这种接口是在 device 和 driver 匹配成功之后

在 probe 函数中调用，同样反过来想，在 probe 失败的时候或者删除 device/driver 部分导致 detect 的时候将要释放这一部分资源。

可以参考源码总线匹配函数，当匹配失败时，其中匹配失败时，将调用释放资源函数：


```
static int really_probe(struct device *dev, struct device_driver *drv)
{
    ...
    if (test_remove) {
        devres_release_all(dev);
    }
    pinctrl_bind_failed:
        devres_release_all(dev);
    ...
}

```

而 devres_release_all 的源码可以猜到，就是从对应的 devic->devres_head 头结点开始，依次轮询往后释放挂在该链表上的资源。

```
int devres_release_all(struct device *dev)
{
    ...
	return release_nodes(dev, dev->devres_head.next, &dev->devres_head,
			     flags);
}  

```

进一步看 release_nodes 的源码是这样的：

```
static int release_nodes(struct device *dev, struct list_head *first,
			 struct list_head *end, unsigned long flags)
	__releases(&dev->devres_lock)
{
    ...
	list_for_each_entry_safe_reverse(dr, tmp, &todo, node.entry) {
		
		dr->node.release(dev, dr->data);
		kfree(dr);
	}
    ...
}
```

不出所料，就是依次获取 devres 结构体，然后通过执行 devres_node.release() 函数，以 dr->data 为参数。你应该还记得，data 就是存放资源的零长数组，而 release 函数也就是我们在注册时传递进去的回调函数，负责释放资源。到这里，一切都清晰了。  



device_unregister
put_device
在drivers/base/core.c中。

static void device_release(struct kobject *kobj)
{
	/*
	 * Some platform devices are driven without driver attached
	 * and managed resources may have been acquired.  Make sure
	 * all resources are released.
	 *
	 * Drivers still can add resources into device after device
	 * is deleted but alive, so release devres here to avoid
	 * possible memory leak.
	 */
	devres_release_all(dev);

	kfree(p);
}

static struct kobj_type device_ktype = {
	.release	= device_release,
	.sysfs_ops	= &dev_sysfs_ops,
	.namespace	= device_namespace,
};


这个ktype 被 device_initialize()调用，也就是创建device的时候
device_register()






