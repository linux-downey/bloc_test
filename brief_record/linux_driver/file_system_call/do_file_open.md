# do_file_open 函数解析
在上一章节中，我们讲解了 open 函数从用户空间到内核空间的交互过程，并且留下了一个关键的问题：内核是怎么根据用户传入的 filename 来找到一个文件的。  

由于我们只讨论设备驱动中的文件操作方式，而设备驱动程序中的文件都是基于 RAM 的 fs，情况相对要简单一些，我们的目标就是找到在设备驱动程序中 file_operations->fops 的调用路径，而不用深入了解物理磁盘文件的加载。  


## 预备知识

### VFS
众所周知，Linux 支持非常多的文件系统，而每一种文件系统都有其自身的特点，比如针对性能、针对大文件、针对安全性等等。对于每一种文件系统，其底层的读写实现都是不一样的，如果每一次文件的读写都要针对每一种文件系统做相应的驱动适配，那将会是一种灾难。  

计算机世界有一句经典的名言："Any problem in computer science can be solved by another layer of indirection"，即计算机科学中的任何问题都可以通过增加一个中间层来解决。  

就像 MMU 通过引入虚拟地址解决了进程地址映射的问题，针对各类繁杂的文件系统类型，Linux 在很早就提出了 VFS 这个概念，即虚拟文件系统。  

其实也不难理解，本来用户的读写需要直接面对具体的文件系统，面对的是磁盘或者 ram 上的文件节点，现在在中间加了一层，用户空间的读写面对 VFS ，VFS 再根据访问目标的文件系统类型调用相应的接口进行指定的读写操作。即：
    
    用户空间调用 open 接口-> VFS -> 文件系统的 open 接口

VFS 算是中间的一个转换层，将用户的文件操作转换成对具体文件系统的操作，这样的好处是对用户而言，不用再关心操作的文件系统类型，只需要欢快地调用文件读写接口就可以了。  

### struct inode
除了文件系统本身的分层之外，文件系统中的元数据也是以分层的形式保存的。  

元数据：原文是 meta data，是从英文直译过来的，这个名称并没有自解释性，在这里我更倾向于将元数据解释成描述信息。比如你买了一台车，车上有座椅、方向盘、发动机、轮胎等，这些是车的组件(component),而描述信息指的是：车重、价格、外观这一类对整体的描述，如果套用计算机术语这些就被称为元数据。  

对于一个文件系统而言，其元数据表示对整个文件系统的描述，对应一个结构体 struct super_block；该结构体描述了该文件系统的类型、属性、ID、块大小等信息。  

文件系统中，自然都是一些文件，而这些文件的元数据和文件本身的数据也是分开存放的，文件的元数据由 struct inode 结构体进行描述，它描述了该文件的 类型、大小、修改时间等等一系列的属性。  

struct inode 的定义是这样的：

```C
struct inode {
    ...
    const struct file_operations	*i_fop;
    struct hlist_head	i_dentry;
    union {
		struct block_device	*i_bdev;
		struct cdev		*i_cdev;
	};
    ...
}
```
来看这几个关键部分：
i_fop ： 文件操作回调函数集，其中包括 open、read、write等，那它是不是就是我们在字符设备驱动中填充的 struct file_operations 结构呢？对于字符设备和块设备而言，事实上它对应字符设备顶层的 struct file_operations 结构，在字符设备的 open 回调函数中，再将我们填充的 struct file_operations 结构替换，在调用 read write 等接口时就直接调用到我们在字符设备驱动中注册的函数了，关于这一点可以参考我的另一篇博客。

i_dentry：dentry 是内核中的目录项，将整个文件系统以树状结构映射到内核中，方便文件的查找，而这里的 i_dentry 是链表结构，这是因为，在文件系统中，一个 dentry 只对应一个物理文件，即一个 inode 结构，而一个物理文件可能对应多个目录项，即一个物理文件可能在系统中存在多个文件节点，比如文件的软连接。这并不难理解。  

### struct dentry
struct dentry 是构建在 RAM 中的目录项，在 Linux 中，一切皆文件，文件中存储着对应的数据。对于目录而言，它也是文件，只是目录文件的文件内容是该目录下的各文件名。   

想象这么一个路径：/home/user/test.txt,如果我们需要对它进行读写操作，先在 / 目录下找到 home 目录，接着通过 home 目录依次往下找到 test.txt 文件，每一次都要通过读取文件以确定下一级目录，在效率上这是行不通的。  

文件的组织也完全符合树的形式，所以解决方案是在内核中构建一棵树，根为 / ，树的中间节点就是目录，通常最后一级子节点就是普通文件，而每一个节点都由 struct dentry 来描述。所有的 struct dentry 节点都是系统启动之后构建在 RAM 中的，它缓存了文件的路径信息与文件节点的位置。struct dentry 的定义如下：

```C
struct dentry {

    ...
	struct dentry *d_parent;	/* parent directory */
	struct inode *d_inode;		

	const struct dentry_operations *d_op;
	struct super_block *d_sb;	

	struct list_head d_child;	/* child of parent list */
	struct list_head d_subdirs;	/* our children */
    ...

} ;
```
为了方便理解，对该结构体定义进行了一些裁剪。  

在 struct dentry 的定义中，d_parent 指向父目录的 struct dentry 项。

而 d_subdirs 链表中保存了所有孩子节点，当前节点通过 d_child 链表节点将当前节点添加到父目录的孩子节点链表 d_subdirs 中，这就组成了一个完整的树结构，寻找一个目录项就可以直接通过 struct dentry 树找到目标文件对应的 struct dentry 结构。  

因为 struct dentry 结构中保存了 d_inode 文件节点，找到对应的 struct dentry 结构之后就相当于找到了具体的文件节点 inode。

其中，struct dentry_operations *d_op; 表示操作当前目录项的一系列回调函数，包括 初始化、释放、hash对比等等。通常，对于 dentry 中的目录项，都是以 hash 值的形式表示的。    


## do_file_open 函数
关于文件系统的预备知识介绍完了，接下来就深入到 do_file_open 函数中，看看文件的打开是怎么做的：

```
struct file *do_filp_open(int dfd, struct filename *pathname,
		const struct open_flags *op)
{
	struct nameidata nd;
	int flags = op->lookup_flags;
	struct file *filp;

	set_nameidata(&nd, dfd, pathname);
	filp = path_openat(&nd, op, flags | LOOKUP_RCU);
	...
	restore_nameidata();
	return filp;
}
```

整个 do_filep_open 函数做了两部分的工作：
* 设置 struct nameidata 结构
* 调用 path_openat 接口，创建并返回一个 struct file 结构。

其中，struct nameidata 更像是一个数据大杂烩，将用户传入的信息以及在操作过程中需要用到的数据结构放到这个结构体中，然后将该结构体传递给 path_openat 函数以协助 struct file 结构的创建。  

它的定义是：

```C
struct nameidata {
	...
	struct path	path;
	struct qstr	last;
	struct path	root;
	struct inode	*inode; 

	struct filename	*name;
	...
} ;
```
包括 inode，path(dentry)、filename、root path等等。

接下来，就是 path_openat 的调用：

```C
static struct file *path_openat(struct nameidata *nd,
			const struct open_flags *op, unsigned flags)
{
	const char *s;
	struct file *file;
	int opened = 0;
	int error;

	file = get_empty_filp();

	file->f_flags = op->open_flag;

	s = path_init(nd, flags);

	while (!(error = link_path_walk(s, nd)) &&
		(error = do_last(nd, file, op, &opened)) > 0) {
		nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
		s = trailing_symlink(nd);
	}
	terminate_walk(nd);

	return file;
}
```
该函数传入三个参数：
第一个参数 struct nameidata *nd ，表示上文中提到的文件节点相关信息，
第二个参数 struct open_flags *op 表示文件的打开参数，也就是用户传入的文件打开标志，比如 O_RDWR、O_CREAT.
第三个参数 flags 表示查找文件方式的标志，这些标志为定义在 include/linux/namei.h 中，表示查找的方式，比如 LOOKUP_RCU 表示使用 RCU 方式查询，LOOKUP_ROOT 表示从 ROOT 开始。  

同时，该函数调用了由三部分组成：
* get_empty_filp() ：顾名思义，这是从系统中申请一个空的 struct file 结构，在后续的操作中进行填充，与内核中的 alloc_xxx 系列函数作用一致。
* path_init() ： 找到用户传入路径的形式,包括相对路径、绝对路径和其他形式(从进程中已打开的文件开始,该函数调用方式也可以是传入已打开文件的fd)。
	\
	查询结果是获取到起始节点的 dentry、inode 信息，存放到 struct nameidata *nd中，有了起始节点，就可以开始查询工作了。
* link_path_walk()、do_last()：根据路径信息搜索到对应的文件节点，并做相关的操作。  

link_path_walk 就是目录跟踪实现函数，从起点路径开始，通过内存中创建的 dentry 树，层层向下找到对应的文件。  

```
static int link_path_walk(const char *name, struct nameidata *nd)
{
	int err;

	for(;;) {
		u64 hash_len;
		int type;

		err = may_lookup(nd);

		hash_len = hash_name(nd->path.dentry, name);

		type = LAST_NORM;
		if (name[0] == '.') switch (hashlen_len(hash_len)) {
			case 2:
				if (name[1] == '.') {
					type = LAST_DOTDOT;
					nd->flags |= LOOKUP_JUMPED;
				}
				break;
			case 1:
				type = LAST_DOT;
		}
		if (likely(type == LAST_NORM)) {
			struct dentry *parent = nd->path.dentry;
			nd->flags &= ~LOOKUP_JUMPED;
			if (unlikely(parent->d_flags & DCACHE_OP_HASH)) {
				struct qstr this = { { .hash_len = hash_len }, .name = name };
				err = parent->d_op->d_hash(parent, &this);
				if (err < 0)
					return err;
				hash_len = this.hash_len;
				name = this.name;
			}
		}

		nd->last.hash_len = hash_len;
		nd->last.name = name;
		nd->last_type = type;

		name += hashlen_len(hash_len);

		do {
			name++;
		} while (unlikely(*name == '/'));
		if (unlikely(!*name)) {
OK:
			/* pathname body, done */
			if (!nd->depth)
				return 0;
			name = nd->stack[nd->depth - 1].name;

			if (!name)
				return 0;

			err = walk_component(nd, WALK_FOLLOW);
		} else {

			err = walk_component(nd, WALK_FOLLOW | WALK_MORE);
		}

		if (err) {
			const char *s = get_link(nd);

			if (IS_ERR(s))
				return PTR_ERR(s);
			err = 0;
			if (unlikely(!s)) {
				/* jumped */
				put_link(nd);
			} else {
				nd->stack[nd->depth - 1].name = name;
				name = s;
				continue;
			}
		}
		if (unlikely(!d_can_lookup(nd->path.dentry))) {
			if (nd->flags & LOOKUP_RCU) {
				if (unlazy_walk(nd))
					return -ECHILD;
			}
			return -ENOTDIR;
		}
	}
}
```
该函数的实现传入两个参数，第一个参数 name 表示文件路径，第二个参数 struct nameidata *nd 也就是描述该文件信息的参数集。  

该函数的实现如下：
调用 may_lookup() 检查打开权限，只有操作权限通过才能进行下一步动作。

进入 for(;;)死循环中，以路径中的 / 为分隔，每次取出一个部分，即一个目录名，对于每个中间目录，调用  walk_component(nd, WALK_FOLLOW)，其中，WALK_FOLLOW 持续跟踪路径到最后的文件。  



比如，对于 /home/user/test.txt ,取出 / ，对其调用 walk_component(nd, WALK_FOLLOW)，该函数将匹配并找到下一级目录的 dentry，然后对下一级目录同样调用 walk_component(nd, WALK_FOLLOW)，直到找到目标文件的 dentry。  

下面是 walk_component 函数的源码实现：
```C
static int walk_component(struct nameidata *nd, int flags)
{
	struct path path;
	struct inode *inode;
	unsigned seq;
	int err;

	if (unlikely(nd->last_type != LAST_NORM)) {
		err = handle_dots(nd, nd->last_type);
		if (!(flags & WALK_MORE) && nd->depth)
			put_link(nd);
		return err;
	}
	err = lookup_fast(nd, &path, &inode, &seq);
	if (unlikely(err <= 0)) {
		if (err < 0)
			return err;
		path.dentry = lookup_slow(&nd->last, nd->path.dentry,
					  nd->flags);
		if (IS_ERR(path.dentry))
			return PTR_ERR(path.dentry);

		path.mnt = nd->path.mnt;
		err = follow_managed(&path, nd);
		if (unlikely(err < 0))
			return err;

		if (unlikely(d_is_negative(path.dentry))) {
			path_to_nameidata(&path, nd);
			return -ENOENT;
		}

		seq = 0;	/* we are already out of RCU mode */
		inode = d_backing_inode(path.dentry);
	}

	return step_into(nd, &path, flags, inode, seq);
}
```
其中，nd->last_type 为下级文件的标志位，这是一个枚举值，LAST_NORM 表示正常文件或目录，还有 LAST_DOT LAST_DOTDOT ，表示为 . 和 ..，linux 中，. 表示当前目录，而 .. 表示上级目录。  

调用 lookup_fast，这个函数尝试从 dentry 缓存中找到下一级 dentry 目录项，找到的目录项给 nd->path.dentry 赋值。  

但是，并非所有的文件在系统启动时都生成了对应的 dentry，通常是在需要的时候生成，然后缓存在系统中以便后续使用。  

当系统内没有对应的 dentry 时，lookup_fast 将执行失败，转而执行 lookup_slow 和 follow_managed，其中 lookup_slow 负责创建新的 dentry，而 follow_managed 负责设置 dentry。调用这两个函数，对应的 dentry 就创建好了，并保存在内存中，下次使用时无需再次创建，可以直接使用。  


找到了 dentry 之后，可以直接通过 dentry->d_inode 获取对应的 inode 节点。  



