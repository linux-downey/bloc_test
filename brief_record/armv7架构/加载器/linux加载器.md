# linux 加载器 0 - execve 系统调用  

在前面的文章我们一直在讨论 elf 文件和链接相关的知识点，所有的这些知识都是服务于程序的运行，而这一章我们要讨论的就是程序运行之前的最后一步：程序的加载。  

程序编译和链接负责生成可执行的程序代码，怎么将这些程序代码放到对应的位置并执行就是加载器的工作，我们所讨论的加载是基于 elf 文件格式的可执行文件，对于机器而言，它只认识并执行二进制的代码，而 elf 格式可执行文件并不像 bin 文件那样是纯粹的二进制数据，直接整个文件 copy 到内存中是运行不了的，它有着相应的格式，因此，程序如果要运行，就需要一段代码对 elf 文件进行解析，把其中的代码和数据部分放到链接时指定的内存位置，然后从指定的开始地址执行代码，这就是整个加载过程。  

了解程序的加载过程需要对 elf 文件格式以及链接过程有一点的了解，可以参考这些博客：TODO。  

本章所涉及的程序加载是基于 linux 平台的 elf 文件，鉴于动态链接的复杂性，博主不会深入地讨论共享库的加载，同时，execve 调用将涉及到很多用户权限、安全相关的部分，比如 process credential 进程信任状，主要负责用户权限检查与生成，在内核中由 struct cred 描述，这一部分不作过多讨论。  

## 进程的产生与程序的加载
在 linux 中，进程的产生通常是通过 fork 函数，由父进程衍生出一个子进程，这种进程产生的方式并不伴随着程序的加载，因为子进程独立运行所需要的所有资源都是从父进程拷贝的，包括代码段、数据段、页表等一系列东西，只是在后续的运行中与父进程脱离，数据部分不再共享。  

在实际的执行环境中，大部分的进程并不是直接在父进程的基础上运行，产生一个新进程的目的在于重新运行一个完全不同的程序，这时候就需要用到 exec 函数族，exec 函数族提供了一个在进程中启动另一个程序执行的方法。它可以根据指定的文件名或目录名找到可执行文件，并用它来取代原调用进程的数据段、代码段和堆栈段，在执行完之后，原调用进程的内容除了进程号外，其他全部被新的进程替换了。  

exec 函数族有多个变种的形式：execl、execle、execv 等 6 种，这 6 种 exec 函数只是在调用的形式上有所区别，实际上都是调用了 glibc 中的 __execve 函数，而 glibc 中的 __execve 函数向内核发起 execve 系统调用，传递的参数为：
* filename：指向可执行文件名的用户空间指针。 
* argv：参数列表，指向用户空间的参数列表起始地址
* envp：环境变量表，环境变量是一系列键值对，字符串类型


execve 系统调用被定义在内核文件 fs/exec.c 中：

```
SYSCALL_DEFINE3(execve,const char __user *, filename,const char __user *const __user *, argv,const char __user *const __user *, envp)
{
	return do_execve(getname(filename), argv, envp);
}

int do_execve(struct filename *filename,
	const char __user *const __user *__argv,
	const char __user *const __user *__envp)
{
	struct user_arg_ptr argv = { .ptr.native = __argv };
	struct user_arg_ptr envp = { .ptr.native = __envp };
	return do_execveat_common(AT_FDCWD, filename, argv, envp, 0);
}
```
getname() 函数主要工作就是将用户空间的文件名指针拷贝到内核中，返回一个 struct filename 的结构，该结构中保存了内核中文件名地址、用户空间文件名地址，而且该结构对于同一个文件还可以进行复用以节省操作时间以及内存空间，总之内核空间与用户空间的数据需要进行严格的划分。  

execve 系统调用紧接着就是调用了 do_execve 函数,这两个函数都是做一些针对参数的处理。紧接着调用  do_execveat_common 函数， do_execveat_common 函数中开始了真正的文件处理。不过在介绍源码细节之前，我们需要先来了解一下重要的数据结构，在分析内核代码的过程中，主要数据结构之间的联系基本上反映了主体框架的逻辑。    

## struct linux_binprm
struct linux_binprm 这个数据结构是整个 execve 系统调用的核心所在，囊括了在 execve 系统调用过程中需要的几乎所有资源，结构成员如下：

```C
struct linux_binprm {
	char buf[BINPRM_BUF_SIZE];         // BINPRM_BUF_SIZE 的值为 128，用于保存可执行文件的前 128 个字节

	struct vm_area_struct *vma;        //保存参数用的 vma 结构
	unsigned long vma_pages;           //对应的 vma pages

	struct mm_struct *mm;              //当前进程的 mm_struct 结构，该结构描述 linux 下进程的地址空间的所有的内存信息
	unsigned long p;                   //
	unsigned int
		called_set_creds:1,
		cap_elevated:1,
		secureexec:1;
                                       //这三个成员涉及到用户与访问权限

	unsigned int recursion_depth;      //记录 search_binary_handler 调用时的递归深度，在 4.14 版本内核中设置不能大于 5
	struct file * file;                //每个打开的文件都对应一个 struct file 结构，该 file 对应需要执行的可执行文件
	struct cred *cred;	               //用户访问权限相关
	int unsafe;		                   //安全性描述字段
	unsigned int per_clear;	/* bits to clear in current->personality */
	int argc, envc;
	const char * filename;	            //要执行的文件名称
	const char * interp;	            //要执行的真实文件名称，对于 elf 文件格式而言，等于 filename。    
	unsigned interp_flags;
	unsigned interp_data;
	unsigned long loader, exec;
} __randomize_layout;
```

## struct linux_binfmt
可执行文件的格式有很多种，比如 a.out、coff 或者是我们要分析的 elf 文件，每一种可执行文件都有不同的加载方式以及附带的参数，struct linux_binfmt 用于描述一种可执行文件的加载数据：

```C
struct linux_binfmt {
	struct list_head lh;                             //链表节点，所有类型的可执行文件对应的 struct linux_binfmt 结构会被链接到一个全局链表中，正是通过该节点链入
	struct module *module;                           //对应的 module
	int (*load_binary)(struct linux_binprm *);       // 该回调函数实现可执行文件的加载
	int (*load_shlib)(struct file *);                // 该回调函数实现动态库的加载
	int (*core_dump)(struct coredump_params *cprm);  // core_dump 回调函数,用于核心转储文件
	unsigned long min_coredump;	                     // 最小的转储文件大小
} __randomize_layout;
```

在 linux 的初始化阶段，对应每一种可执行文件格式，都会注册相应的 struct linux_binfmt 结构到内核中，当程序真正加载可执行文件时，调用对应的 load_binary 回调函数即可实现可执行文件的加载，对于 elf 格式的可加载文件而言，对应的注册程序在 fs/binfmt_elf.c 中：

```C
static struct linux_binfmt elf_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_elf_binary,      
	.load_shlib	= load_elf_library,
	.core_dump	= elf_core_dump,
	.min_coredump	= ELF_EXEC_PAGESIZE,
};

static int __init init_elf_binfmt(void)
{
	register_binfmt(&elf_format);
	return 0;
}
```
elf 可执行文件的加载对应 load_elf_binary 函数，对应的动态库加载函数为 load_elf_library，在后续的加载过程中将会调用到这两个函数实现程序的加载。  


## do_execveat_common
介绍完了主要的结构体，再回到 do_execveat_common 函数中，继续分析程序的执行：

```
static int do_execveat_common(int fd, struct filename *filename,struct user_arg_ptr argv,struct user_arg_ptr envp,int flags)
{
    char *pathbuf = NULL;
	struct linux_binprm *bprm;
	struct file *file;
	struct files_struct *displaced;
	int retval;

    ...
    /* 为 bprm 结构分配内存空间 */
    bprm = kzalloc(sizeof(*bprm), GFP_KERNEL);

    /* 生成进程信任状 bprm->cred  */
    retval = prepare_bprm_creds(bprm);

    /* 通过 filename 结构指定的文件路径打开文件，并返回 struct file 结构体指针 */
    file = do_open_execat(fd, filename, flags);

    /* 在创建进程的时候，是一个很好的机会做进程的负载均衡，在多核 CPU 中，如果当前 CPU 上执行任务过于繁忙，就会将当前需要启动的进程切换到另一个 CPU 的 runqueue 上，因为这时候进程所占用的资源是最少的，迁移成本最小。 */
    sched_exec();

    /* 将返回的内核文件描述符赋值给 bprm->file */
    bprm->file = file;

    ...  /* bprm 结构体成员的赋值 */

    /* 向内核申请一个新的 mm_struct 结构，并赋值给 bprm->mm，mm_struct 结构用于描述整个进程的内存布局，为构建一个全新的内存布局做准备 */
    retval = bprm_mm_init(bprm);

    /* 计算命令行参数个数 */
    bprm->argc = count(argv, MAX_ARG_STRINGS);

    /* 计算环境变量参数个数 */
    bprm->envc = count(envp, MAX_ARG_STRINGS);

    /* 设置进程信任状相关标志位，同时读取文件中的前 128 个字节的数据到 bprm->buf 中，这 128 字节的文件头用于判断可执行文件的类型，对于 elf 文件而言，文件头的长度为 52 字节(32 位下) */
    retval = prepare_binprm(bprm);

    /* 将用户空间的字符串拷贝到内核中 */
    retval = copy_strings_kernel(1, &bprm->filename, bprm);
    retval = copy_strings(bprm->argc, argv, bprm);
    retval = copy_strings(bprm->argc, argv, bprm);

    would_dump(bprm, bprm->file);
    
    /* 执行程序加载的主要函数 */
    retval = exec_binprm(bprm);


}
```


系统调用返回到了哪里？


