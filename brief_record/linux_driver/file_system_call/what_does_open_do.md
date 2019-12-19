# open 做了什么
接触 Linux 的朋友几乎都知道它的设计哲学：一切皆文件。

所以，无论是对应用还是驱动开发者来说，一直在和文件打交道，对于文件的的常用操作接口也是信手拈来。  

不过，博主在这里想问一个比较扫兴的问题：你真的知道文件接口做了什么吗？  

当然，有些朋友会很快地回答：open() 打开或者创建文件，read/write 对文件进行读写，如果这是一个应用程序开发者的回答，还尚可接受，但是如果你是一个驱动工程师，是不应该仅限于知道这些的。  

因为，对内部实现原理的探究才是底层工程师该做的事。


## 从用户空间到内核
在经典 32 位操作系统中，Linux 虚拟内存的 0~3G 为用户空间，3~4G 的内存空间为内核空间，用户空间只负责运行用户程序，而内核空间掌控所有的硬件资源，当用户程序需要使用到硬件资源，比如访问磁盘的时候，就需要向内核提交申请，内核将用户进程需要的资源返回给用户。这是大部分开发者都知道的。   

再往深一点想，用户空间的进程和内核之间到底是怎么运作的？看起来，用户程序在不断地运行、内核接收用户程序的请求运行，还有内核线程和中断系统，整个系统上这么多看起来在同时运行的程序到底是怎么协调工作的。  

其实如果你了解实时操作系统的实现原理，这也并没有什么难理解的。是的，这些程序看起来都是同时在系统上运行，实际上是任务切换的速度够快，即使一个进程是间隔运行也让人感觉不到它的停顿，毕竟这个间隔很可能是 30ms 甚至更短，在系统任务非常多的时候，你会感觉到系统很卡，就是由于任务的增多而让你感受到了这个间隔，这时这个间隔可能是 200ms 。  

用户程序、中断系统、内核线程的"同时执行"可以理解，但是进程和内核是怎么交互的呢？  

当一个进程运行时，一个全局指针 current 将指向当前进程的进程控制块，也就是 struct task_struct 结构，该结构中包含了一个进程的所有私有数据。 而用户进程到内核是通过系统调用接口进入的，内核对于用户进程而言，其实是一种"代替执行"的模式，所以，尽管在内核空间内，同样可以访问当前进程的所有进程数据，也就是通过 current 指针访问当前进程进程控制块。而此时，用户进程是处于睡眠状态的，直到内核处理返回。    

如果你看过盗梦空间，就会发现这两者之间的相似之处，电影中，人进入梦境执行任务，梦境中的"人"拥有本体的所有思想和记忆，而本体则陷入睡眠，这种情况直到梦境的退出，人也就醒了。

同时，这和中国古代神话中的灵魂出窍也是如出一辙。  

## 说明
在本章中，我们只把精力放在 open 函数上，事实上，open的实现也是很复杂的，我们只针对设备驱动程序部分。

## open 函数
这一章的讨论我们由外至内，确切地说是从用户空间到内核，在用户空间，open 调用是这样的：

```
int open(const char *pathname, int oflag, ... );
```
open 函数除了打开文件的功能之外，还可以创建文件，需要在flag 处设置 O_CREAT 参数，通过传入一个路径来打开或者创建一个文件。  

## file_oprations 中的 open
在写第一个设备驱动程序时，我们就被告知：需要在驱动程序中实现一个 file_oprations 结构体，填充结构体中的 .open、.write 等函数注册到系统中，当应用程序在用户空间调用 open、write 函数时，就会调用到我们的驱动程序中对应的函数。  
但是，只要仔细一看，就会发现，内核驱动中的 open 函数指针的定义是：

```
int (*open) (struct inode *, struct file *);
```
它的参数为 struct inode *,struct file *，和用户空间中的 open 完全不一致，难免会有疑惑，从用户空间到内核中，open 到底经历了一个怎样的调用流程？


### 系统调用
用户空间的 open 函数将会触发系统调用，系统调用是用户控件到内核的接口，如果说用户空间和内核之间有一堵墙，那么系统调用就是墙上开的一道门，系统调用和用户 API 接口名称都是一一对应的，比如 open 对应 sys_open,read 对应 sys_read.  

open 的系统调用是这样的：
```
SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	...
	return do_sys_open(AT_FDCWD, filename, flags, mode);
}
```
oepn系统调用传入的参数和用户空间的 open 是对应的，唯一的区别是，filename 指针被 __user 修饰，这是因为，open 函数传入的指针是用户空间的指针，而用户空间和内核空间是隔离的，尽管内核对用户空间有自由的访问权，还是需要进行区分，在必要时将用户空间的数据复制到内核空间中，而不是直接使用用户空间的数据，这可能造成混乱。  


接下来就是 do_sys_open 的调用：

```
long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
{
	struct open_flags op;
	int fd = build_open_flags(flags, mode, &op);
	struct filename *tmp;

	if (fd)
		return fd;

	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	fd = get_unused_fd_flags(flags);
	if (fd >= 0) {
		struct file *f = do_filp_open(dfd, tmp, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
}
```

从 do_sys_open 这个函数名可以猜到，这是文件打开的主要操作文件，它完成了整个文件打开的流程：
* 将用户传入的 filename 即路径名转换成内核可识别的路径信息。
* 使用 get_unused_fd_flags() 从系统中获取一个空闲的 fd。
* do_filp_open() 接口打开文件，并返回一个 struct file 结构体，这个结构体描述一个打开的文件
* 调用 fd_install() 将 fd 和返回的 struct file 绑定。
* 返回 fd。

至此，open 就完成了从传入路径到返回一个 fd 的调用流程，之后就可以使用这个 fd 进行读写操作。   

接下来就是深入到各个步骤中，了解各个操作的细节。  

## 前期操作
do_sys_open() 的准备部分主要是两个函数：
* build_open_flags()
* getname()

build_open_flags 的定义为：

```
static inline int build_open_flags(int flags, umode_t mode, struct open_flags *op)
{
    int lookup_flags = 0;
	int acc_mode = ACC_MODE(flags);

	flags &= VALID_OPEN_FLAGS;

	if (flags & (O_CREAT | __O_TMPFILE))
		op->mode = (mode & S_IALLUGO) | S_IFREG;
	else
		op->mode = 0;

	flags &= ~FMODE_NONOTIFY & ~O_CLOEXEC;
	if (flags & __O_SYNC)
		flags |= O_DSYNC;

	if (flags & __O_TMPFILE) {
		if ((flags & O_TMPFILE_MASK) != O_TMPFILE)
			return -EINVAL;
		if (!(acc_mode & MAY_WRITE))
			return -EINVAL;
	} else if (flags & O_PATH) {
		flags &= O_DIRECTORY | O_NOFOLLOW | O_PATH;
		acc_mode = 0;
	}

	op->open_flag = flags;

	if (flags & O_TRUNC)
		acc_mode |= MAY_WRITE;

	if (flags & O_APPEND)
		acc_mode |= MAY_APPEND;

	op->acc_mode = acc_mode;

	op->intent = flags & O_PATH ? 0 : LOOKUP_OPEN;

	if (flags & O_CREAT) {
		op->intent |= LOOKUP_CREATE;
		if (flags & O_EXCL)
			op->intent |= LOOKUP_EXCL;
	}

	if (flags & O_DIRECTORY)
		lookup_flags |= LOOKUP_DIRECTORY;
	if (!(flags & O_NOFOLLOW))
		lookup_flags |= LOOKUP_FOLLOW;
	op->lookup_flags = lookup_flags;
	return 0;
}
```
这个函数主要负责标志位处理，还记得用户空间的 open 函数的第二个参数 flag 吧，我们可以通过该 flag 传入各种各样的文件打开参数，比如最常用的 O_RDONLY、O_CREAT 等。  

在该函数中，就负责各种参数的处理，结果记录在 struct open_flags *op 中返回到调用者，而函数的返回值指示是否存在错误。   

为什么 open 的调用参数还需要单独处理，而不是直接使用用户传入的 flag 呢？  

你大可以使用 'man 2 open'指令查看 open 的详细用法，虽然我们平常使用 open 仅用于读写，并不代表 open 是简单的，你可以看看 open 函数对应的那一长串参数列表，就知道它的实现是很灵活的。  

与常用的读写相比，一些特殊性打开方式比如：

O_PATH:该 open 操作并不真正打开文件，而是作为某些测试目的或者仅对 fd 进行操作，这个 flag 对应的文件是不能进行读写的。
O_TMPFILE：创建一个临时文件，临时文件的处理方式和普通文件由非常大的区别。  

可以看出，build_open_flags 就像一个过滤器，针对一些特殊的操作做相应处理，然后将处理结果交给下一级进行操作。

前期准备除了对于 flag 的处理，还有一个就是调用 getname 对传入的 filename 即路径名进行处理。  

```
struct filename *result;
	char *kname;
	int len;

	result = audit_reusename(filename);
	if (result)
		return result;

	result = __getname();
	if (unlikely(!result))
		return ERR_PTR(-ENOMEM);

	kname = (char *)result->iname;
	result->name = kname;

	len = strncpy_from_user(kname, filename, EMBEDDED_NAME_MAX);
	if (unlikely(len < 0)) {
		__putname(result);
		return ERR_PTR(len);
	}

	if (unlikely(len == EMBEDDED_NAME_MAX)) {
		const size_t size = offsetof(struct filename, iname[1]);
		kname = (char *)result;

		result = kzalloc(size, GFP_KERNEL);
		if (unlikely(!result)) {
			__putname(kname);
			return ERR_PTR(-ENOMEM);
		}
		result->name = kname;
		len = strncpy_from_user(kname, filename, PATH_MAX);
		if (unlikely(len < 0)) {
			__putname(kname);
			kfree(result);
			return ERR_PTR(len);
		}
		if (unlikely(len == PATH_MAX)) {
			__putname(kname);
			kfree(result);
			return ERR_PTR(-ENAMETOOLONG);
		}
	}

	result->refcnt = 1;

	if (unlikely(!len)) {
		if (empty)
			*empty = 1;
		if (!(flags & LOOKUP_EMPTY)) {
			putname(result);
			return ERR_PTR(-ENOENT);
		}
	}

	result->uptr = filename;
	result->aname = NULL;
	audit_getname(result);
	return result;
}

struct filename *
getname(const char __user * filename)
{
	return getname_flags(filename, 0, NULL);
}
```

对于 getname，一方面，对用户传入的 filename 做一系列的检查，比如是否为空，比如重复打开，针对这些特殊情况做一些特殊处理。

所有打开的文件路径都会被记录到系统中，如果是新打开一个文件路径，也需要将其记录到系统中，同时，将存储在用户空间的 filename 复制到内核空间中，并填充一个 struct filename 结构体返回给调用者，这个结构体中有以下内容：
```
struct filename {
    //内核中的字符串保存地址
	const char		*name;	
    //用户空间的字符串地址
	const __user char	*uptr;	
    //该路径被引用次数
	int			refcnt;
	...
};
```

在文件打开操作的预处理中，主要是针对标志位和文件路径做一些过滤处理再传入到内核，对于需要特殊处理的部分做相应操作。  

