# sysfs 

## 简介
自2.6版本开始，linux内核开始使用sysfs文件系统，它的作用是将设备和驱动程序的信息导出到用户空间，方便了用户读取设备信息，同时支持修改和调整。

与ext系列和fat等文件系统不同的是，sysfs是一个系统在启动时构建在内存中虚拟文件系统，默认被挂载在/sys目录下,官方也不建议用户手动地将挂载点改为其它目录，既然是存储在内存中，自然掉电不保存，不能存储用户数据。

在之前版本中也有同样的虚拟文件系统 procfs 建立了内核与用户系统信息的交互，但是procfs并非针对设备和驱动程序，而是针对整个内核信息的抽象接口。随着驱动代码在内核中越来越多,内核开发人员觉得有必要使用一个独立的抽象接口来描述设备和驱动信息，毕竟直到目前，驱动代码在内核代码中占比非常大，内容也是非常庞杂。这样可以避免 procfs 的混乱，子系统之间的分层和分离总是能带来更清晰地框架。  


## sysfs 的默认目录结构
sysfs一般被挂载在/sys目录下，我们可以通过ls /sys来查看sysfs的内容：

```
block  bus  class  dev  devices  firmware  fs  kernel  module  power
```
首先需要注意的是，sysfs目录下的各个子目录中存放的设备信息并非独立的，我们可以看成不同的目录是从不同的角度来描述某个设备信息。

一个设备可能同时有多个属性，因此对于同一个驱动设备，同时存在于不同的子目录下，比如一个 i2c 硬件控制器,在软件上被抽象为 bus,又剧透 device 的属性,同时还可以分属 class 或者其它目录之下,对于 /sys 下不同的目录,分别描述的信息如下：
* /sys/block:该子目录包含在系统上发现的每个块设备的一个符号链接。符号链接指向/sys/devices下的相应目录。
* /sys/bus:该目录包含 linux 下的总线设备，每个子目录下主要包含两个目录：device 和 driver，后面会讲到 linux的总线驱动模型，几乎都是分层为 device 和 driver 来实现的。
* /sys/class:每一个在内核中注册了class 的驱动设备都会在这里创建一个class设备。
* /sys/dev:这个目录下包含两个子目录：block和char，分别代表块设备和字符设备，特别的是，它的组织形式是以major:minor来描述的，即每一个字符设备或者块设备在这里对应的目录为其相应的设备号major:minor.
* /sys/devices:包含整个内核设备树的描述，其他目录下的设备多为此目录的链接符号。
* /sys/firmware:固件信息,通常在嵌入式系统中会把整个设备树描述文件(呈树状结构)放到当前目录下. 
* /sys/fs:文件系统的相关信息
* /sys/kernel:包含各种正在运行的内核描述文件,可通过这些文件调整内核参数,常用于调试的 debugfs 就挂载在 /sys/kernel/debug/ 下.  
* /sys/module:包含当前系统中被加载的模块信息,不论是编译到内核镜像的还是外部模块,都会出现的当前文件夹下,。
* /sys/power：没找到具体的官方描述，但是根据里面文件内容和命名习惯推测，这里存放的是一些与电源管理相关的模块信息。

## sysfs 应用
对于用户而言,最关心的是怎么用,sysfs 既然属于文件系统,使用普通的文件操作接口就可以对其中的文件进行读写.  

而本章主要讲解的重点在于:对于开发人员来说,sysfs 怎么用?也就是怎么将内核中的信息导出到 /sys 目录下对应的某个文件中,首先我们需要了解 sysfs 对应的基础结构.  

### kobject
kobject 是整个 sysfs 实现的核心结构,甚至可以说是整个 linux 设备管理的核心结构,Linux设备模型的核心是使用Bus、Class、Device、Driver四个核心数据结构，将大量的、不同功能的硬件设备（以及驱动该硬件设备的方法），以树状结构的形式，进行归纳、抽象，从而方便Kernel的统一管理。  

而硬件设备的数量、种类是非常多的，这就决定了Kernel中将会有大量的有关设备模型的数据结构。
这些数据结构一定有一些共同的功能，需要抽象出来统一实现，否则就会不可避免的产生冗余代码。这就是Kobject诞生的背景。

目前为止，Kobject主要提供如下功能：
* 通过parent指针，可以将所有Kobject以层次结构的形式组合起来。
* 使用一个引用计数（reference count），来记录Kobject被引用的次数，并在引用次数变为0时把它释放（这是Kobject诞生时的唯一功能）。
* 和sysfs虚拟文件系统配合，将每一个Kobject及其特性，以文件的形式，开放到用户空间（有关sysfs，会在其它文章中专门描述，本文不会涉及太多内容）。  

(本段摘自 [wowo 科技博客](http://www.wowotech.net/device_model/kobject.html),如有侵权,请联系我删除,另:建议所有对内核有兴趣的朋友关注wowo的博客.)  

在Linux中，Kobject几乎不会单独存在。它的主要功能，就是内嵌在一个大型的数据结构中，为这个数据结构提供一些底层的功能实现,所有设备相关的 kobject 组成一个设备描述网络,描述了各个设备之间的联系,将 kobject 嵌入到其它大型结构的底层实现方法为 container_of,在内核中这种"反向查找"(通过成员找到父结构地址)的方式随处可见,详情可以参考我的另一篇博客:TODO.   

kobject 的结构如下:

```c++
struct kobject{
    /* 当前kobj的名称,用作节点的命名 */
    const char		*name;
    /* 链表节点,通过当前节点链接到 kset->list 中 */
    struct list_head	entry;
    /* 当前kobj的父节点，在文件系统中的表现就是父目录 */
	struct kobject		*parent;
    /* kobj属于的kset */
	struct kset		*kset;
    /* kobj的类型描述，最主要的是其中的属性描述，包含其读写方式,见下文 */
	struct kobj_type	*ktype;
    /*当前kobj的引用，只有当引用为0时才能被删除*/
	struct kref		kref;
    /* kobj 对应的目录节点 */
    struct kernfs_node	*sd;

    /*一些标志位*/
    ...
}
```

### kobj_type
kobj_type 用于描述一个 kobject 的属性,它的成员是这样的:

```c++
struct kobj_type {
    /* 用户级的释放资源回调函数,遵循系统申请的由系统自动释放,自己申请的由自己释放原则 */
	void (*release)(struct kobject *kobj);
    /* kobject 对应的操作函数集,成员为两个函数指针,一个是 show 对应用户的 read 行为,store 对应用户的 write 行为 */
	const struct sysfs_ops *sysfs_ops;
    /* 文件的属性集,这是一个指针数组,包含多个属性,一个属性对应一个属性文件,属性主要包括文件名,mode. */
	struct attribute **default_attrs;

    /* 命名空间相关的回调函数,暂不讨论 */
	const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
	const void *(*namespace)(struct kobject *kobj);
};
```
对于一个 kobject 而言,sysfs_ops 和 default_attrs 是必须要设置的


### kset
kset 结构用于描述一组 kobject 成员, 需要注意的是, kset 并不直接参与 sysfs 下文件的组织,每一个 kset 结构中都包含有一个 kobject,文件路径的组织行为还是通过 kset->kobject 来完成, 设置了 kset 成员的 kobject,如果没有显式地指定 parent,其 parent 节点将会被设置为 kset->kobject.但是如果一个 kobject 既设置了 parent 又设置了 kset,而且 parent 不等于 kset->kobject,该文件对应的目录处于 parent 子目录下而不是 kset->kobject 子目录下.       

通俗地说,kset 的设置并不直接决定文件之间的父子层级,假设 kobject A 的 kset 成员为 B,A 并不一定存在于 B 的子目录或者多级子目录下, kset 对 kobject 的管理体现在其它方面.  

就目前而言,kset 一个最大的应用在于设备的热插拔管理,即设备的添加和删除,从属于某个 kset 的 kobject 一旦发生文件的修改,就会触发内核的 uevent 机制,通过 netlink 或者 userhelpler 的方式通知到用户空间,用户空间做出相应的操作,用户空间 udev 专门处理这一类事件,以处理内核发生的热插拔事件,为事件生成相对应的文件节点或者执行其它操作,比如加载模块.   

假设这样一个场景,一个 U 盘被插入到 USB 中,硬件中断被触发,USB 协议对这一行为进行处理,同时生成相应的 sysfs 下的设备节点,当节点被生成时,将会调用到当前 kobject 节点所属的 kset->uevent_ops 接口添加设备信息,然后将这些设备信息通过 netlink 发送到用户空间,而用户空间的 udevd 监听该 netlink 套接字,接受信息并做相应的处理.在这种情况下,kset 对所有所属的 kobject 执行设备热插拔的管理. 

下面我们来看看 kset 的定义:

```c++
struct kset {
    /* 链表头,所有所属的 kobject 通过 kobject->entry 挂在这个链表上 */
	struct list_head list;
    /* 同步机制 */
	spinlock_t list_lock;
    /* kset 所属的 kobject,使用该 kobject 将当前节点链接到 kobject 树状结构中 */
	struct kobject kobj;
    /* uevent 相关的 ops */
	const struct kset_uevent_ops *uevent_ops;
}
```

整个 kset 结构还是非常简单的,一个链表,一个 kobject,还有一个 uevent 相关的 ops.  

按照管理,ops 就是一系列回调函数操作集,在 uevent 触发时被调用,向用户空间发送内核消息,需要注意的是,这些 ops 并不执行向用户空间发送 netlink 消息的工作,只是添加一些自定义的 env,var 等参数,linux 内核中大多数回调函数的机制都是为了实现用户自定义行为.毕竟,发送消息这个行为是可以被抽象的,而发送的消息内容需要使用者填写. 

关于 uevent 相关的内容我们将在后续的内容中再详细讨论.   

## 文件接口的使用
相对于 devfs 或者是其它的 fs 而言,sysfs 创建文件的方式是比较奇怪的,至少看起来并不直观,对于 sysfs 中文件的创建,第一个比较反直觉的是:**Linux driver开发者，很少会直接使用Kobject以及它提供的接口，而是使用构建在Kobject之上的设备模型接口**.

第二个比较反直觉的是:如果非要在 kobject 和文件节点之间做一个关联,那么 kobject 对应 sysfs 中的一个目录而不是一个文件,按照惯例,kobject 是设备节点管理的最小单位,它应该对应一个文件,实际上却不是.   

下面这条语句创建了一个目录,目录名为 kobj_parent:

```
struct kobject *kobj_p = kobject_create_and_add("kobj_parent",kernel_kobj->parent);
```
那我们直接访问的文件是怎么生成的呢?  

第三个比较反直觉的是: sysfs 中文件的操作回调函数并不直接通过 sysfs_ops 进行,而是通过 attribute 属性来实现文件的读写,但是 struct attribute 结构只有 name 和 mode 成员,又没有 show 和 store 这两个回调函数,这又是如何实现操作的?   

在接下来的文章中我们将一一解答这些问题.  


## 使用原始接口创建文件
既然上面说到,开发者很少直接使用 kobject 以及它提供的接口,官方也不建议这么做,那么,如果这么做又行不行呢?不妨来试一试.   

创建 kobject 文件接口使用 kobject_init_and_add 函数,这属于静态创建 kobject 的方式:

```c++

static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,char *buf){
        return 1;
}

static ssize_t kobj_attr_store(struct kobject *kobj, struct attribute *attr,const char *buf, size_t count){
        return count;
}
static const struct sysfs_ops static_kobj_sysfs_ops = {
        .show   = kobj_attr_show,
        .store  = kobj_attr_store,
};

static struct kobj_type ktp = {
        .release        = static_kobj_release,
        .sysfs_ops      = &static_kobj_sysfs_ops,
};
static struct kobject kobj;
static int __init kobj_create_static_init(void){
    kobject_init_and_add(&kobj, &ktp, NULL, "%s", "test_kobj");
    ...
}
```
该函数需要提供四个参数:
* kobj:需要注意的是,这个 kobj 不能是未分配空间的空指针或未初始化的指针
* ktype:ktype 类型可以参考上文,对于静态创建 kobject ,主要任务就是初始化 sysfs_ops 和 default_attrs 两个成员,sysfs 中设置的 show 和 store 回调函数会在对应的读写时被调用.  
* parent:父级 kobject
* fmt,...:名称,可以使用格式化的方式创建.  

初始化 kobject 并添加到内核中时,相应的目录就添加到了 sysfs 相应目录下,接下来创建文件:

```c++
static struct attribute foo_attr = { 
        .name = "foo",
        .mode = 0664,
};
static struct attribute bar_attr = { 
        .name = "bar",
        .mode = 0664,
};
static const struct attribute *attrs[] = { 
        &foo_attr,
        &bar_attr,
        NULL,
};
static int __init kobj_create_static_init(void){
    sysfs_create_files(&kobj,attrs);
}
```

在上述使用 kobject_init_and_add 初始化的 kobject 下创建了两个文件:foo 和 bar,接下来就可以使用文件操作接口进行操作了,实际上这时你会发现一个奇怪的现象:无论是对 foo 的读还是对 bar 的读,都会调用到 kobject->ktype->sysfs_ops->show,也就是当前 kobject 目录下的所有文件操作都会调用到 kobject 所属的 show 回调函数,判断操作的是哪个文件就需要驱动开发者自行去判断.   

可以想象,如果一个 kobject 所属目录下文件或者子目录很多的情况下,这种函数内自行判断是非常不方便的.   


## 创建文件接口的通用操作
内核中的普遍做法并不是静态地创建文件接口,而是借助 attribute 属性来创建文件,使用属性来绑定文件以及文件操作,在原始的 attribute 中,每一个文件对应一个 attribute 项,但是每个 attribute 项只有 name 和 mode 属性,如果可以将文件操作(show和store)和 attribute 绑定,就可以实现每个文件的单独访问了.  

这种绑定操作由 kobject_create_and_add 完成:

```c++
static struct kobject *kobj = kobject_create_and_add("test_kobj", NULL);
```
kobject_create_and_add 只需要传入两个参数,第一个为目录名,第二个为父级目录,为 NULL 时表示父目录为 /sys/.  

可以发现,这种动态创建方式没有指定 ktype,而是使用了系统默认的 ktype,对应的源代码部分为:

```c++
kobject_create_and_add -> kobject_create:

struct kobject *kobject_create(void)
{
    ...
    kobject_init(kobj, &dynamic_kobj_ktype);
    ...
}

```


看看系统默认的 ktype 实现:

```c++
static struct kobj_type dynamic_kobj_ktype = {
    ...
    /* 使用系统自定义的 kobj_sysfs_ops */
	.sysfs_ops	= &kobj_sysfs_ops,
};

const struct sysfs_ops kobj_sysfs_ops = {
	.show	= kobj_attr_show,
	.store	= kobj_attr_store,
};

```

使用系统提供的 ktype :kobj_sysfs_ops,同样地,当访问某个 kobject 生成目录下的文件时,会调用到当前 kobject->ktype->sysfs 中的回调函数,我们来看看其中的 show 接口:

```c++
static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->show)
		ret = kattr->show(kobj, kattr, buf);
	return ret;
}
```

如果你对内核的套路比较熟悉,看到这段代码自然就豁然开朗了:在上文的示例中,当我们使用 sysfs_create_file 或者其它接口创建文件时,需要传入文件的 attribute,此时如果我们像将 kobject 嵌入其它结构一样将 attribute 嵌入到另一个父级结构中,就可以在回调函数中通过 container_of 接口反向获取这个父级结构,如果我们在该父级结构中添加 show 和 store 接口,就实现了 show/store 函数与 attribute 的绑定,又因为 attribute 是和具体文件绑定的,所以就实现了文件操作(read/store) 与文件的绑定.   

所以,创建文件接口可以是这样:

```c++
static ssize_t foo_show(struct kobject *kobj,struct kobj_attribute *attr,char *buf){
        return 1;
}
static ssize_t foo_store(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t count){
        return count;
}

struct kobj_attribute foo_attr = {
    .attr = {
            .name = "foo",
            .mode = 0644,
    },
    .show = foo_show,
    .store = foo.store
}

static const struct attribute *attrs[] = { 
        &foo_attr.attr,
        NULL,
};
static int __init kobj_create_static_init(void)
{
    sysfs_create_files(kobj,attrs);
    ...
}

```
结合上面的代码,在用户读 sysfs 下文件时,会调用到 kobject->ktype->sysfs->show 接口,在系统默认的 kobj_attr_show 函数中,通过 attribute 反向找到 kobj_attribute 结构,然后调用 kobj_attribute->show.  

这也就解释了为什么内核中创建接口都是使用 attribute 实现,明明 attribute 不带 show 和 store 接口.因为父级的 kobject_attribute 绑定了 show 和 store 接口,通过 attribute 找到父级再调用父级的 read/store,也就实现了每个文件都有对应的 read/sore 接口.    

### 内核宏
上述的示例中演示了 struct kobj_attribute 的创建,除了 kobj_attribute,还可以使用 struct device_attribute,struct bus_attribute 类型的父级 attribute,这几种父级 attribute 都是定义了一个 struct attribute 成员和 show,store 成员,唯一不同的地方在于 show,store 第一个参数不一样,根据不同的场景可以选择不同的类型.  

比如 device_attribute 类型的父级 attribute 使用的是最多的,在通常的设备驱动中,几乎所有驱动都是 device 相关的,而 device_attribute->show 的第一个参数传入为 struct device* 结构,非常方便参数的传递. 

对于各类父级 attribute 的定义,内核提供相应的宏:

```c++
#define __ATTR(_name, _mode, _show, _store) {				\
	.attr = {.name = __stringify(_name),				\
		 .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },		\
	.show	= _show,						\
	.store	= _store,						\
}
```

所以,struct kobj_attribute foo_attr = __ATTR(foo, 0644, foo_show, foo_store) 等价于:

```c++
struct kobj_attribute foo_attr = {
    .attr = {
            .name = "foo",
            .mode = 0644,
    },
    .show = foo_show,
    .store = foo.store
}
```

对于其它类型的父级 attribute 定义还可以使用宏:BUS_ATTR,DEVICE_ATTR 等.  


## 创建文件组
在内核的实际应用中,单独创建文件的接口使用得比较少,使用频率最高的接口是 sysfs_create_group,通过 kobject 创建一个文件组,对应到文件系统中就是创建一个目录,目录中包含多个属性文件.   

这也很好理解,通常一个设备存在不止一种属性需要导出到用户空间,一般是一个集合,以组的形式将设备相关信息导出到用户空间.一个组会创建一个新的目录,放在指定的父目录下.    

文件组的导出涉及到另一个结构:

```c++
struct attribute_group {
    /* 组名,对应目录名 */
	const char		*name;
    /* 可选函数,权限设置,是否可见 */
	umode_t			(*is_visible)(struct kobject *,
					      struct attribute *, int);
	umode_t			(*is_bin_visible)(struct kobject *,
						  struct bin_attribute *, int);
	/* 指针数组,组内文件对应的 attribute,每一个文件对应一个 attribute 项 */
    struct attribute	**attrs;
    /* 文件组特有类型的attribute,对应二进制文件 */
	struct bin_attribute	**bin_attrs;
};
```
值得注意的是,group 中支持 bin 类型的文件操作,如果你觉得有些疑惑的话可以看看上文中的 show/store 回调函数参数,默认的 show/store 函数数据部分只有一个 char* 接口,表明只处理字符串,而 bin_attribute 提供对二进制数据的处理.与普通 attribute 不同的是,bin_attribute 中直接包含了 show/store 接口,所以不需要使用一个父级目录来进行文件与文件操作的绑定.  

创建组文件的接口为:

```c++
int sysfs_create_group(struct kobject *kobj,const struct attribute_group *grp)
```

该接口接受两个参数,第一个是 kobj,第二个就是开发者填充的 grp 结构,和 sysfs_create_files 类似,使用父级 attribute.attr 数组来初始化 grp->attribute 即可.也可以初始化 bin_attribute 类型的二进制文件以创建二进制接口.  

下面就是一个创建 grp 的示例代码片段:

```c++
static struct kobject *kobj;
static ssize_t bar_show(struct kobject* kobjs,struct kobj_attribute *attr,char *buf){
        return 1;
}

static ssize_t foo_show(struct kobject* kobjs,struct kobj_attribute *attr,char *buf){
        return 1;
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count){
        return count;
}

static struct kobj_attribute foo_attr = __ATTR_RO(bar);
static struct kobj_attribute bar_attr = __ATTR(foo,0664,foo_show,foo_store);

static struct attribute *bar_attrs[] = {
        &foo_attr.attr,
        &bar_attr.attr,
        NULL,
};

static ssize_t foobar_read(struct file *file, struct kobject *kobj,struct bin_attribute *attr, char *buf, loff_t off, size_t count){
        return count;
}

static ssize_t foobar_write(struct file *filp, struct kobject *kobj,struct bin_attribute *bin_attr,char *buf, loff_t pos, size_t count)
{
        printk("Call bin write!\n");
        return count;
}

BIN_ATTR(foobar,0664,foobar_read,foobar_write,0);

static struct bin_attribute *foobar_bin[] = {
        &bin_attr_foobar,
        NULL,
};

static struct attribute_group attr_g = {
        .name = "kobject_test",
        .attrs = bar_attrs,
        .bin_attrs = foobar_bin,
};

static int __init sysfs_ctrl_init(void){

        kobj = kobject_create_and_add("test_kobj",kernel_kobj->parent);
        sysfs_create_group(kobj, &attr_g);
        return 0;
}
```

## 从用户空间到内核
在上面的分析中,我们做了一个经验使然的假设:用户对文件的读写将会调用对应 kobject 中的 sysfs_ops 结构中的回调函数,用户空间的读是如何调用到 sysfs_ops->show 的呢?   

/dev 下的设备文件是通过 file_operations 结构体来设置读写函数，用户空间通过 read/write 系统调用间接地调用到 file_operations->read/write,对应的调用路径如下：

```
用户空间 read 触发系统调用
                ->sys_read
                    ->vfs_read
                        ->file->f_op->read
```

实际上，对于 sysfs 下的 kobject 而言，同样是通过 file_opertions 结构进行调用传递的，只是系统进行了封装。  

这个 file_operations 结构体被定义在 fs/kernfs/file.c 中：

```c++
const struct file_operations kernfs_file_fops = {
	.read		= kernfs_fop_read,
	.write		= kernfs_fop_write,
	.llseek		= generic_file_llseek,
	.mmap		= kernfs_fop_mmap,
	.open		= kernfs_fop_open,
	.release	= kernfs_fop_release,
	.poll		= kernfs_fop_poll,
	.fsync		= noop_fsync,
};
```

我们通过其中的 .read 函数进行分析:

```c++
static ssize_t kernfs_fop_read(struct file *file, char __user *user_buf,size_t count, loff_t *ppos)
{
    ...
    return kernfs_file_direct_read(of, user_buf, count, ppos);
}

static ssize_t kernfs_file_direct_read(struct kernfs_open_file *of,char __user *user_buf, size_t count,loff_t *ppos)
{
    ...
    const struct kernfs_ops *ops;
    /* 获取 struct kernfs_ops 类型的 ops */
    ops = kernfs_ops(of->kn);
    /* 调用 ops->read 接口 */
	if (ops->read)
		len = ops->read(of, buf, len, *ppos);

    /* 将 ops->read 读到的数据 copy 到用户空间 */
    copy_to_user(user_buf, buf, len)
    ...
}
```

从上述的 read 源码可以看到， file_operations->read 函数并没有调用 kobject->sysfs_ops ，而是调用了一个 kernfs_ops->read 接口,接下来就来看看这两个 ops 有什么关系。  

这一切还要从 kobject_create_and_add 这个添加 kobject 的接口说起，kobject 的添加也就是创建目录和文件的过程，我们来看看它的调用路径：

```

kobject_create_and_add(name,parent)   // 创建和添加 kobject 到内核
    ->kobject_add(kobj, parent, "%s", name);  //执行添加 kobject，格式化命名
        ->kobject_add_varg(kobj, parent, fmt, args);  //添加子函数，处理命名
                ->kobject_add_internal(kobj);          
                    ->create_dir(kobj);                 //为 kobject 创建目录
                        ->populate_dir(kobj);             //分配目录资源
                            ->sysfs_create_file(kobj, attr);    //创建文件
                                ->sysfs_create_file_ns(kobj, attr, NULL);   //带命名空间的创建文件
                                    ->sysfs_add_file_mode_ns(kobj->sd, attr, false, attr->mode, ns);

```

通过一系列的调用，kobject_create_and_add 间接调用到了 sysfs_add_file_mode_ns，我们来看看 sysfs_add_file_mode_ns 的执行：

```c++
int sysfs_add_file_mode_ns(struct kernfs_node *parent,const struct attribute *attr, bool is_bin,umode_t mode, const void *ns)
{
    ...
    const struct kernfs_ops *ops;
    struct kobject *kobj = parent->priv;
	const struct sysfs_ops *sysfs_ops = kobj->ktype->sysfs_ops;
    if (sysfs_ops->show && sysfs_ops->store) {
        if (mode & SYSFS_PREALLOC)
            ops = &sysfs_prealloc_kfops_rw;
    ...
}
```

从源码中可以看到，如果 sysfs_ops->show/store 被设置，struct kernfs_ops 类型的 *ops 就被赋值为 sysfs_prealloc_kfops_rw：

```c++
static const struct kernfs_ops sysfs_prealloc_kfops_rw = {
	.read		= sysfs_kf_read,
	.write		= sysfs_kf_write,
	.prealloc	= true,
};

static ssize_t sysfs_kf_read(struct kernfs_open_file *of, char *buf,
			     size_t count, loff_t pos)
{
	const struct sysfs_ops *ops = sysfs_file_ops(of->kn);
	struct kobject *kobj = of->kn->parent->priv;
	len = ops->show(kobj, of->kn->priv, buf);
    ...
}
```

而 sysfs_kf_read 函数的实现就是调用了 sysfs_ops->read,因此 struct kernfs_ops 类型的 ops 和 kobject->ktype->sysfs 就建立了相互调用的关系，所以，从 file_operations->read 到 sysfs->read 的调用路径就找到了，至于从 sysfs->read 到具体文件的 show 函数，上文中也已经讲到了。  


结束！、




