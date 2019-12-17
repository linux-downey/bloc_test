# /dev下文件的创建

在前面的章节中，我们介绍了 cdev 的创建以及添加，在调用 device_create() 之后，就可以在 /dev 目录下看到我们创建的文件接口了，/dev 目录上挂载的文件系统为 devtmpfs。而 /dev 目录之所以命名为 dev，是因为该目录下所有放置的文件都是设备文件。    

但是你有没有想过，/dev 目录下的设备文件是怎么创建的呢？  

当然，这个问题并不是想刨根问底地要去深挖这个文件到底是如何在 devtmpfs 中创建并注册 inode 节点，而是需要弄清楚内核创建这个文件调用了什么接口。   

可能你马上会想到： proc_create() 接口用于在 /proc 目录下创建一个文件节点，sysfs_create_file()可以在 /sys 指定目录下创建一个文件节点。同样的，在 /dev 下创建节点很可能是 dev_create_file(),事实上却并非如此。

## /dev 文件的创建
在用户空间，我们可以使用 mknod 指令在 /dev 目录下创建一个文件节点，调用方式是这样的:

```
mknod /dev/NAME c major minor
```

/dev/NAME 表示文件节点名称
-c  表示创建字符设备节点 
major，minor 分别表示主次设备号

那么，在应用层创建文件是实现了，这样创建的文件有什么用呢？  

在前面的章节中，我们提到 register_char_region() 表示将设备号注册到系统，而 cdev_add() 则将 cdev 注册到系统中，其中 register_char_region() 函数确定设备名，而 cdev_add() 将 file_operation 结构体注册到系统中。

接下来，是不是就要调用 device_create() 来创建文件节点了？不，我们可以直接在应用层使用 mknod 指令来创建一个文件节点，只要设备号能够与内核注册的设备匹配即可。通过访问该文件节点同样可以调用内核中的相应驱动程序。  

由此可以了解到，在创建 /dev/* 文件的角度上，用户空间的 mknod 和内核API device_create()做了同样的事情，而 mknod 指令执行了系统调用 mknod。

但是，毕竟内核的运行不能使用用户空间的指令，要了解内核是如何创建设备节点的，还是需要深入源代码进行追踪。  


对于 devtmpfs，另一个问题是：从用户空间访问文件接口，是怎么调用到内核中对应的接口的呢？也就是说，当我们对 /dev/name 调用 read 时，是怎么调用到对应的 ops->read 函数的。

## device_create() 详解
device_create(),顾名思义，就是在内核中创建一个 device ，了解内核的都知道，device 是内核中最基本的构成，几乎每个硬件上的设备都对应一个 device 节点，实际上，device_create() 接口完全不限于创建设备节点，它更多地是负责整个设备在内核中的描述，包括 procfs、sysfs 中的文件节点创建。

```
struct device *device_create(struct class *class, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...)

```
函数的原型不难理解：

class：表示该设备对应的 class 设备，会在 /sys/class 下进行添加相应的节点，这个参数是必须的，当模块被加载到内核时，设备节点是由用户空间的应用程序 udev 创建的，而 udev 创建设备节点需要读取该设备相应的 class 信息。

parent：该设备的父节点，可以为 NULL，也可以是相应的父节点。

devt : 设备号。

drvdata：私有数据，可以通过这个参数将私有用户的数据与该 device 进行绑定，实际上就是设置：dev->drv_data = drv_data;

const char *fmt, ...:这两个参数用于决定设备名称。


接下来看函数的调用流程，

```
device_create
    -> device_create_vargs
        -> device_create_groups_vargs

static struct device *
device_create_groups_vargs(struct class *class, struct device *parent,
			   dev_t devt, void *drvdata,
			   const struct attribute_group **groups,
			   const char *fmt, va_list args)
{
	if (class == NULL || IS_ERR(class))
		goto error;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);

	device_initialize(dev);
	dev->devt = devt;
	dev->class = class;
	dev->parent = parent;
	dev->groups = groups;
	dev->release = device_create_release;
	dev_set_drvdata(dev, drvdata);

	retval = kobject_set_name_vargs(&dev->kobj, fmt, args);

	retval = device_add(dev);

	return dev;
}
            
```
在 device_create_groups_vargs() 中可以看到它的执行流程：
* 判断 class 是否为空，如果为空，错误返回
* 为 struct device dev 申请内存，并使用参数对其进行填充,其中 groups 为 NULL，release 注销函数，在该 device 被释放时调用。
* 使用传入的 fmt 和可变参数对 device 命名。
* 调用 device_add,由名称可以看出，该函数负责将 device 注册到系统中。

继续看 device_add(struct device* dev) 的实现。

```
int device_add(struct device *dev)
{
    ...
    device_create_file(dev, &dev_attr_uevent);
    device_add_class_symlinks(dev);
    device_add_attrs(dev);
    bus_add_device(dev);
    ...
    if (MAJOR(dev->devt)) {
		error = device_create_file(dev, &dev_attr_dev);
		if (error)
			goto DevAttrError;

		error = device_create_sys_dev_entry(dev);
		if (error)
			goto SysEntryError;

		devtmpfs_create_node(dev);
	}
    ...
}
```

在 device_add() 的前半部分，主要工作是一些文件接口的注册，这些接口主要针对 /sys 目录，毕竟，通常情况下，/sys 目录的主要工作就是从各个角度来描述一个驱动设备，所以需要创建一些文件和链接。  

重点需要关注的就是 devtmpfs_create_node() 接口，由该函数的命名可以看出，这正是我们要找的在 devtmpfs 下创建设备文件的接口。  

继续跟踪函数到了 drivers/base/devtmpfs.c 中，devtmpfs_create_node()源码如下：
```
static struct req {
	struct req *next;
	struct completion done;
	int err;
	const char *name;
	umode_t mode;	/* 0 => delete */
	kuid_t uid;
	kgid_t gid;
	struct device *dev;
} *requests;

int devtmpfs_create_node(struct device *dev)
{
	const char *tmp = NULL;
	struct req req;

	if (!thread)
		return 0;

	req.mode = 0;
	req.uid = GLOBAL_ROOT_UID;
	req.gid = GLOBAL_ROOT_GID;
	req.name = device_get_devnode(dev, &req.mode, &req.uid, &req.gid, &tmp);

	if (req.mode == 0)
		req.mode = 0600;
	if (is_blockdev(dev))
		req.mode |= S_IFBLK;
	else
		req.mode |= S_IFCHR;

	req.dev = dev;

	init_completion(&req.done);

	req.next = requests;
	requests = &req;

	wake_up_process(thread);
	wait_for_completion(&req.done);

	kfree(tmp);

	return req.err;
}
```

在该文件中，新引入了一个结构体，struct req，从结构体成员来看，该结构体描述的是该 device 一些权限、模式信息，同时对一个静态全局变量 struct req *request 进行赋值，该全局变量在后续的代码中将使用到。   

随着跟踪的深入，好像并没有看到哪里调用了创建设备文件的函数。  

只是在函数调用的最后调用了 wake_up_process(thread); 接口，唤醒了某个内核线程，那么设备节点的创建肯定就在当前的线程中。  

在当前文件下进行搜索，发现这个线程创建于 devtmpfs_init 函数中：

```
int __init devtmpfs_init(void)
{
	register_filesystem(&dev_fs_type);
	
	thread = kthread_run(devtmpfsd, &err, "kdevtmpfs");
	
    wait_for_completion(&setup_done);

	return 0;
}
```

随着代码的持续跟踪，无意中跟踪到了 devtmpfs 的根据地，register_filesystem() 函数接口用于注册 devtmpfs 文件系统，所有关于 devtmpfs 的内容都是以这里为起点，同时，我们也找到了上述提到的内核线程，自然继续跟踪进入到线程执行函数：devtmpfsd。

```
static int devtmpfsd(void *p)
{
...
	while (1) {
		while (requests) {
			struct req *req = requests;
			requests = NULL;
			while (req) {
				struct req *next = req->next;
				req->err = handle(req->name, req->mode,
						  req->uid, req->gid, req->dev);
				complete(&req->done);
				req = next;
			}
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock(&req_lock);
		schedule();
	}
	return 0;
...
}
```

线程函数中一个死循环，同时获取 requests 这个静态全局变量的值，以 req 的各项成员为参数，即传入 device 的模式、名称、uid、gid等信息，调用 handle() 函数。  

接着往下看，


```
static int handle(const char *name, umode_t mode, kuid_t uid, kgid_t gid,
		  struct device *dev)
{
    ...
	return handle_create(name, mode, uid, gid, dev);
    ...
}

static int handle_create(const char *nodename, umode_t mode, kuid_t uid,
			 kgid_t gid, struct device *dev)
{
	struct dentry *dentry;
	struct path path;
	int err;

	dentry = kern_path_create(AT_FDCWD, nodename, &path, 0);

	err = vfs_mknod(d_inode(path.dentry), dentry, mode, dev->devt);
	if (!err) {
		struct iattr newattrs;

		newattrs.ia_mode = mode;
		newattrs.ia_uid = uid;
		newattrs.ia_gid = gid;
		newattrs.ia_valid = ATTR_MODE|ATTR_UID|ATTR_GID;
		inode_lock(d_inode(dentry));
		notify_change(dentry, &newattrs, NULL);
		inode_unlock(d_inode(dentry));

		/* mark as kernel-created inode */
		d_inode(dentry)->i_private = &thread;
	}
	done_path_create(&path, dentry);
	return err;
}

```
功夫不负有心人，在 handle_create 中，我们终于看到了一丝曙光，vfs_mknod，这个函数看起来就是我们想要找到的 mknod函数，同时，在 handle_create 中，还看到了 struct dentry 的构建，这是创建一个目录缓存，说明该函数正在创建一个文件节点。  

继续往前，进入到 vfs_mknod() 中，查看它的源代码实现：

```
int vfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	int error = may_create(dir, dentry);
	...
	error = dir->i_op->mknod(dir, dentry, mode, dev);
	...
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}
```

在 vfs_mknode 中，重点需要关注的是 dir->i_op->mknod() 这个回调函数的执行，这个回调函数是在文件系统注册时被注册，几乎所有的文件系统注册都会实现这个回调函数,以jffs2为例：
```
void init_special_inode(struct inode *inode, umode_t mode, dev_t rdev)
{
	inode->i_mode = mode;
	if (S_ISCHR(mode)) {
		inode->i_fop = &def_chr_fops;
		inode->i_rdev = rdev;
	} 
	...
}

static int jffs2_mknod (struct inode *dir_i, struct dentry *dentry, umode_t mode, dev_t rdev)
{
	...
	init_special_inode(inode, inode->i_mode, rdev);
	...
}

.mknod = jffs2_mknod；
```
mknod 回调函数调用了 init_special_inode ，在该函数中，将创建的文件节点的 i_fops 赋值为 def_chr_fops；

而 def_chr_fops 被定义在 fs/char_dev.c 中，一切又回到了字符设备驱动文件中，接下来自然是查看 def_chr_fops 的定义：

```
static int chrdev_open(struct inode *inode, struct file *filp)
{
	const struct file_operations *fops;
	struct cdev *p;
	struct cdev *new = NULL;
	int ret = 0;

	spin_lock(&cdev_lock);
	p = inode->i_cdev;
	if (!p) {
		struct kobject *kobj;
		int idx;
		kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);

		new = container_of(kobj, struct cdev, kobj);

		p = inode->i_cdev;
		if (!p) {
			inode->i_cdev = p = new;
			list_add(&inode->i_devices, &p->list);
			new = NULL;
		} else if (!cdev_get(p))
			ret = -ENXIO;
	} 

	fops = fops_get(p->ops);

	replace_fops(filp, fops);
	if (filp->f_op->open) {
		ret = filp->f_op->open(inode, filp);
		if (ret)
			goto out_cdev_put;
	}

	return 0;

}

const struct file_operations def_chr_fops = {
	.open = chrdev_open,
	.llseek = noop_llseek,
};
```

在字符设备的通用 fops 中，就定义了一个 open 函数，而 open 函数的实现主要就是：
* 根据该字符设备的设备号，获取对应的 struct cdev 结构体
* 将通用 fops 替换为对应设备号的 cdev 的 fops ，所以，当我们 open 一个设备节点时，并非直接调用到了对应设备节点的 open，而是调用了 char_dev 的通用 open 接口，通过该 open 接口将真正的 fops 结构替换到设备节点上，之后调用 fops->read 和 fops->write 就是直接调用的关系了。整个调用链路还是非常长的。


## dev 的查找过程
在通用 char_dev 的 open 函数中，是通过设备号来查找对应的 cdev，那么就有必要深究以下到底是怎么查找的，其主要的函数就是 kobj_lookup():

```

```






