# linux 进程加载  - execve 系统调用  

在前面的文章我们一直在讨论 elf 文件和链接相关的知识点，所有的这些知识都是服务于程序的运行，而这一章我们要讨论的就是程序运行之前的最后一步：程序的加载。  

程序编译和链接负责生成可执行的程序代码，怎么将这些程序代码放到对应的位置并执行就是加载器的工作，我们所讨论的加载是基于 elf 文件格式的可执行文件，对于机器而言，它只认识并执行二进制的代码，而 elf 格式可执行文件并不像 bin 文件那样是纯粹的二进制数据，直接整个文件 copy 到内存中是运行不了的，它有着相应的格式，因此，程序如果要运行，就需要一段代码对 elf 文件进行解析，把其中的代码和数据部分放到链接时指定的内存位置，然后从指定的开始地址执行代码，这就是整个加载过程。  

了解程序的加载过程需要对 elf 文件格式以及链接过程有一点的了解，可以参考[elf文件格式](https://zhuanlan.zhihu.com/p/363487856)。  

本章所涉及的程序加载是基于 linux 平台的 elf 文件，鉴于动态链接的复杂性，博主不会深入地讨论共享库的加载的源码实现，关于共享库加载的概念可以参考[linux共享库](https://zhuanlan.zhihu.com/p/363489654)，同时，execve 调用将涉及到很多用户权限、安全相关的部分，比如 process credential 进程信任状，主要负责用户权限检查与生成，在内核中由 struct cred 描述，这一部分不作过多讨论。  



## 进程的产生与程序的加载
在 linux 中，进程的产生通常是通过 fork 函数，由父进程衍生出一个子进程，这种进程产生的方式并不伴随着程序的加载，因为子进程独立运行所需要的所有资源都是从父进程拷贝的，包括代码段、数据段、页表等一系列东西，只是在后续的运行中与父进程脱离，数据部分不再共享。  

在实际的执行环境中，大部分的进程并不是直接在父进程的基础上运行，产生一个新进程的目的在于重新运行一个完全不同的程序，这时候就需要用到 exec 函数族，exec 函数族提供了一个在进程中启动另一个程序执行的方法。它可以根据指定的文件名或目录名找到可执行文件，并用它来取代原调用进程的数据段、代码段和堆栈段，在执行完之后，原调用进程的内容除了进程号以及一些系统配置外，其他全部被新的进程替换了。  

exec 函数族有多个变种的形式：execl、execle、execv 等 6 种，这 6 种 exec 函数只是在调用的形式上有所区别，实际上都是调用了 glibc 中的 \_\_execve 函数，而 glibc 中的 \_\_execve 函数向内核发起 execve 系统调用，传递的参数为：



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



## 注册可执行文件加载程序

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

```c++
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

	current->fs->in_exec = 0;
	current->in_execve = 0;
	membarrier_execve(current);
	acct_update_integrals(current);
	task_numa_free(current);
	free_bprm(bprm);
	kfree(pathbuf);
	putname(filename);
	if (displaced)
		put_files_struct(displaced);
	return retval;

	...

}
```

整个 do_execveat_common 的实现流程在上面的源代码中展现得比较清楚,总共分为三部分:



* 文件的打开,读写以及参数的处理,构建一个 bprm 结构
* 以构建的 bprm 为参数执行程序加载的主要函数:exec_binprm
* 加载的收尾工作,释放资源等. 

接下来自然是进入到 exec_binprm 中一探究竟:

```C++
static int exec_binprm(struct linux_binprm *bprm)
{
	int ret;
	/* PID 相关操作 */

	ret = search_binary_handler(bprm);
	
	/* 错误处理 */
}
```

exec_binprm 主要调用了 search_binary_handler 函数:

```c++
int search_binary_handler(struct linux_binprm *bprm)
{
	bool need_retry = IS_ENABLED(CONFIG_MODULES);
	struct linux_binfmt *fmt;
	int retval;

	/* 错误检查部分 */

 retry:
	/* 遍历加载器列表 */
	list_for_each_entry(fmt, &formats, lh) {
		if (!try_module_get(fmt->module))
			continue;
		bprm->recursion_depth++;
		/* 尝试调用加载器的 load_binary 回调函数 */
		retval = fmt->load_binary(bprm);
		put_binfmt(fmt);
		bprm->recursion_depth--;
		if (retval < 0 && !bprm->mm) {
			/* we got to flush_old_exec() and failed after it */
			force_sigsegv(SIGSEGV, current);
			return retval;
		}
		if (retval != -ENOEXEC || !bprm->file) {
			return retval;
		}
	}

	/* 再尝试一次,跳转到 retry */

	return retval;
}
```
search_binary_handler 函数的工作就是遍历系统中已注册的加载器,尝试对当前可执行文件进行解析并加载.  

formats 为全局链表,在上文注册可执行文件加载程序小节中提到,每一种可执行文件格式都会在系统启动时向内核注册对应的加载程序,而这些加载程序由 struct linux_binfmt 结构描述,以回调函数的形式提供,所有类型的 linux_binfmt 都会链接到一个全局链表中,这个链表头就是 formats.  

fmt 是全局链表 formats 中链表节点,struct linux_binfmt 类型,每一个 fmt 对应一种加载器.  

list_for_each_entry(fmt, &formats, lh) 指令的作用就是遍历所有加载器,并调用 load_binary 回调函数,看是否能找到一种加载器可以处理保存在 bprm 中的文件.也就是执行 execve 系统调用传入的文件.在 do_execveat_common->prepare_binprm 函数中,读取了当前可执行文件的前 128 字节到 bprm->buf 中,通过该 128 字节的文件头可以快速地判断当前文件是哪种类型,从而快速地找到对应的加载器.   

本章节基于 elf 格式的可执行文件讨论,因此,当我们调用 execve 并传入 elf 格式的文件时,对应的 elf 加载器自然是可以对其进行处理的,于是我们将目光转移到 elf 加载器的 load_binary 回调函数:

```c++
static struct linux_binfmt elf_format = {
	...
	.load_binary	= load_elf_binary,      
	...
};
```

因此,执行 elf 格式可执行文件的真正加载操作的就是 load_elf_binary 函数.这个函数被定义在 fs/binfmt_elf.c 中.  



## load_elf_binary 函数解析

load_elf_binary 函数直接操作 elf 文件，对文件中的段进行加载，分析 load_elf_binary 函数之前，需要对 elf 可执行文件格式有一定的了解，对于 elf 文件的加载格式，这里做一个简要的介绍：

首先，编译过程中产生的目标文件是以段(section)的形式组织的，所有的段大致可以分为三种类型：



* 数据段，程序数据，典型的比如 .data、.bss
* 代码段，指令数据，典型的比如 .text、.init
* 针对链接阶段的辅助信息，典型的比如重定位表、符号表等。  

文件附带的还有一个段表(section header table)，分别描述各个段的信息，相当于一个目录。  

目标文件并不是最终的形式，它经过链接过程产生可执行文件，可以说，目标文件一定程度上为程序的链接服务，而可执行文件为程序的加载服务，加载是加载到内存中，对于内存而言，并不关心程序有多少个段，它所关心的是可执行文件中的内存访问属性。  

所以，在可执行文件中，数据不再以段为划分，而是以内存访问权限划分，比如所有的指令数据段，拥有 RX(读执行)权限，就合并成一个部分，这种合并的结果称为 segment，可执行文件中存在多个 segment，一个 segment 由多个段合成，而段信息默认情况下依旧保存。  

可执行文件用一个 segment header table 描述所有的 segment，程序的加载就是针对 segment 的操作，所有对于加载过程而言，最核心的操作分为两步：一是读取 elf header 获取整个 elf 文件的相关信息，二是读取 segment header table，然后根据其给出的信息将各个 segment 放置到不同的内存区域中。   

segment header table 对应的数据结构为：

```
typedef struct elf32_phdr{
  Elf32_Word	p_type;            //program header 类型，比如可加载的 segment、动态加载器信息 segment、加载辅助信息 segment 等
  Elf32_Off	p_offset;              // segment 在文件中的偏移
  Elf32_Addr	p_vaddr;           // 虚拟地址
  Elf32_Addr	p_paddr;           // 加载地址
  Elf32_Word	p_filesz;          // segment 的 size
  Elf32_Word	p_memsz;           // 对应虚拟内存的 size
  Elf32_Word	p_flags;           // 标志位
  Elf32_Word	p_align;           // 对齐参数
} Elf32_Phdr;
```

先熟悉 elf 文件相关概念有利于接下来的文件加载分析，也算是磨刀不误砍柴工。更详细的 elf 格式信息可以参考[elf文件格式](https://zhuanlan.zhihu.com/p/363487856)。   

接下来我们进入正文：分析 load_elf_binary 函数，与往常一样，省去一些错误检查和一些与主逻辑关联不大的分支代码，专注于加载过程的剖析。  

```C++
static int load_elf_binary(struct linux_binprm *bprm)
{
	struct elf_phdr *elf_ppnt, *elf_phdata, *interp_elf_phdata = NULL;
	char * elf_interpreter = NULL;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long elf_bss, elf_brk,elf_entry;
	struct pt_regs *regs = current_pt_regs();

	struct {
		/* 保存可执行文件的 program header */
		struct elfhdr elf_ex;
		/* 保存动态加载器的 program header */
		struct elfhdr interp_elf_ex;
	} *loc;

	/* 为两种 program header 结构申请内存空间 */
	loc = kmalloc(sizeof(*loc), GFP_KERNEL);

	/* bprm->buf 中保存的是 128 字节的文件头，将 bprm->buf 的前 52 字节数据赋值给 loc->elf_ex ，gcc 中结构体是可以直接使用 = 赋值的。*/
	loc->elf_ex = *((struct elfhdr *)bprm->buf);

	/* 对 elf 头部字段作各种检查 */
	...

	/* 根据 elf 文件头的 e_phoff 字段(即program header table 偏移地址)，从可执行文件中读取 program header table，返回其首地址 */
	/* program header table 的初始地址保存在 struct elf_phdr 指针类型的 elf_phdata 中 */
	elf_phdata = load_elf_phdrs(&loc->elf_ex, bprm->file);

	/* 第一次遍历 program header table */
	elf_ppnt = elf_phdata;
	for (i = 0; i < loc->elf_ex.e_phnum; i++) {
		/* 第一次遍历只针对 PT_INTERP 类型的 segment 做处理，这个 segment 中保存着动态加载器在文件系统中的路径信息,通常为 ld-linux.so.x,动态加载器也是一个共享库 */
		if (elf_ppnt->p_type == PT_INTERP) {
			/* 从该 segment 中读出动态加载器的路径名，并保存在 char 类型的 elf_interpreter 指针处 */
			elf_interpreter = kmalloc(elf_ppnt->p_filesz,GFP_KERNEL);
			retval = kernel_read(bprm->file, elf_interpreter,elf_ppnt->p_filesz, &pos);

			/* 打开动态加载器文件,将返回的 struct file 结构赋值给 interpreter, */
			interpreter = open_exec(elf_interpreter);

			/* 从动态链接程序中读取处 elf 文件头保存在 loc->interp_elf_ex 中. */
			retval = kernel_read(interpreter, &loc->interp_elf_ex,sizeof(loc->interp_elf_ex), &pos);
		}
		elf_ppnt++;
	}

	// 第二次遍历 program header table.做一些特殊处理.  
	elf_ppnt = elf_phdata;
	for (i = 0; i < loc->elf_ex.e_phnum; i++, elf_ppnt++)
		switch (elf_ppnt->p_type) {
		/* 描述程序栈的 segment,正常情况下不使用该类型的 segment*/
		case PT_GNU_STACK:
			if (elf_ppnt->p_flags & PF_X)
				executable_stack = EXSTACK_ENABLE_X;
			else
				executable_stack = EXSTACK_DISABLE_X;
			break;
		/* 为特定的处理器保留,在 arm 中不使用该类型的 segment */
		case PT_LOPROC ... PT_HIPROC:
			retval = arch_elf_pt_proc(&loc->elf_ex, elf_ppnt,
						  bprm->file, false,
						  &arch_state);
			if (retval)
				goto out_free_dentry;
			break;
		}

	/* 如果程序中指定了动态链接器,就将动态链接器程序读出来,需要一并加载 */
	if (elf_interpreter) {
		/* 再次检查文件 */
		...
		
		/* 读取动态链接器文件的,根据上面读取到的动态链接器 elf 文件头提供的 elf 文件系统,定位到 Program header tables 地址并读出到内存,并使用指针  interp_elf_phdata 指向该片内存以备后续访问. */
		interp_elf_phdata = load_elf_phdrs(&loc->interp_elf_ex,
						   interpreter);
		
		/* 遍历动态链接器的 Program header table,对一些特殊字段做处理,处理内容和上一段代码 第二次遍历可执行文件的 Program header table 一致.*/
		...
	}

	/* 上文中所贴出的代码都是针对可执行文件和加载器文件的读取,是整个加载过程的第一个准备阶段,接下来进入加载过程的第二个准备阶段:进程环境的准备. */

	/* 所有新进程(除第一个进程)的创建都是通过 fork 由父进程复制而来，因此继承了父进程的很多资源，既然要重新加载一个完全不同的程序，就需要释放从父进程继承而来的所有资源，创造一个全新的进程(进程号不变)，包括但不限于：
	** 1、重新初始化进程的信号表
	** 2、将 (struct mm_struct)mm->exe_file 替换成新的可执行文件
	** 3、将 curent->mm 结构替换成 bprm->mm 结构，bprm->mm 是新创建的 mm 结构
	** 4、发送信号退出所有的子线程，初始化 tls
	** 5、关闭原进程所有的文件，并释放资源
	** 6、...
	*/
	retval = flush_old_exec(bprm);
	
	/* 设置新的可执行文件环境，初始化部分进程资源 */
	setup_new_exec(bprm);

	/* 为当前进程安装进程信任状，进程信任状保存在 bprm->cred，为 execve 系统调用开始时根据当前进程环境新创建 */
	install_exec_creds(bprm);

	/* 设置栈空间,栈的设置支持加上一个随机偏移地址作为一种安全措施 
	** 设置参数的起始地址,参数包括命令行参数和环境变量
	** 
	*/
	retval = setup_arg_pages(bprm, randomize_stack_top(STACK_TOP),executable_stack);
	current->mm->start_stack = bprm->p;

	/* 设置完整个进程的内存空间，意味着准备工作已经完成，接下来就进入了程序加载的正题：执行程序的加载，这是一个复杂而繁琐的工作 */
	/*******************************************************************************************************/

	/* 遍历可执行文件的 Program header */
	for(i = 0, elf_ppnt = elf_phdata;
	    i < loc->elf_ex.e_phnum; i++, elf_ppnt++) {

		/* 该 segment 如果不是 PT_LOAD 类型,就不作加载处理,转到下一个 Program header
		** segment 类型有:PT_LOAD(可加载),PT_DYNAMIC(动态链接相关信息),PT_INTERP(动态链接器信息),PT_NOTE(辅助加载信息)等
		** 只需要将 PT_LOAD 类型的数据加载到对应的内存空间
		*/
		if (elf_ppnt->p_type != PT_LOAD)
				continue;

		/* elf_bss 和 elf_brk 对应加载数据部分和bss部分的游标,即记录上一次加载完之后 bss 和 brk 的值,作为下一次加载的开始地址, 
		** 一般情况下,链接器会将所有的代码,只读数据,读写数据,bss段划分为两个 segment,一个是只读代码和只读数据部分,一个是读写数据部分(包括bss)
		** 在加载完 bss 段之后,会出现 elf_brk > elf_bss 的情况.
		** 但是在实际的链接过程中,只读的 segment 作为第一个 PT_LOAD 属性的 segment,而读写数据对应的 segment 作为第二个 PT_LOAD 属性的 segment
		** 通常可执行文件中也只存在这两个 PT_LOAD 类型的 segment,而数据 segment(包含bss) 放在最后加载,因为找不到第三个 PT_LOAD 类型的 segment 而退出整个循环,并不会执行到这里.
		** 这也是为什么使用 unlikely 修饰这段指令的原因,unlikely 在内核中表示进入该分之的情况很少.  
		** 对于某些特殊的平台,或者用户自定义链接脚本的情况下,才会出现 bss 所属的 segment 加载完成之后还有其它的 segment 需要加载.   
		*/
		if (unlikely (elf_brk > elf_bss)) {
			/* 如果执行到这里,映射匿名页面,设置brk,并清除bss数据区域 */
		}

		/* 设置标志位以及 flag */

		/* 可执行文件的类型为 ET_EXEC,共享库的类型为 ET_DYN,因此进入 if 分之而不会进入 else if 分之进行处理 */
		if (loc->elf_ex.e_type == ET_EXEC || load_addr_set) {
			elf_flags |= MAP_FIXED;
		}else if (loc->elf_ex.e_type == ET_DYN) {
			/* 针对动态库的操作 */
		}

		/* 为文件中对应的 segment 建立内存的 mmap 
		** elf_map 调用了 vm_mmap,将文件中的内容映射到虚拟内存空间中,传入可执行文件 file 和该 segment 对应的 Program header.
		** program header 中有几个字段会使用到:
		** 1-p_vaddr,内存虚拟地址
		** 2-p_filesz,该 segment 的 size 
		** 3-p_offset,当前 segment 在可执行文件中的位置 
		** 根据这三个参数,就可以定位到可执行文件中该 segment 对应的起始地址和长度,以及需要映射到的虚拟地址和长度
		** 需要注意的是,vm_mmap 这个接口仅仅是将文件映射到内存中,并没有真正地将文件中的数据拷贝到内存映射区,真正的拷贝在发生内存访问时产生缺页中断,由缺页中断将物理页拷贝到对应的虚拟内存地址处. 
		*/
		error = elf_map(bprm->file, load_bias + vaddr, elf_ppnt,
				elf_prot, elf_flags, total_size);

		/* 在加载第一个类型为 PT_LOAD 的 segment 时,需要确定加载地址,这个加载地址并不是该 segment 对应的虚拟地址,而是针对整个文件的虚拟地址 
		** 计算方式为:load_addr = 当前 segment 虚拟地址 - 该 segment 在文件中的偏移地址.
		** 在 load_elf_binary 函数中,只会加载可执行文件中类型为 PT_LOAD 的 segment 以及加载动态链接器到指定内存中
		** 而可执行文件的其它 segment 由动态链接器进行处理,在动态链接的处理过程中需要为整个文件留出空间而不仅仅是 PT_LOAD 类型的 segment.
		*/
		if (!load_addr_set) {
			load_addr_set = 1;
			//虚拟地址减去文件内偏移?
			load_addr = (elf_ppnt->p_vaddr - elf_ppnt->p_offset);
			//不是 ET_DYN,不执行
			if (loc->elf_ex.e_type == ET_DYN) {
				...
			}
		}
		
		k = elf_ppnt->p_vaddr;
		/* 更新 start_code 的地址,也就是代码段开始地址,更准确的说法是包含代码段的 读/执行 属性的 segment 而不单单是 .text 段
		** 加载中使用 segment 的概念,一个 segment 通常是多个段的组合.
		*/
		if (k < start_code)
			start_code = k;
		//更新 start_data 的地址,也就是数据部分(包含 .data,.bss)开始地址
		if (start_data < k)
			start_data = k;

		/* bss 地址等于数据 segment 虚拟地址+filesz,因为在可执行文件中,bss 并不占实际的空间,长度为 0 
		** 而且 .bss 段被放置在数据 segment 的最后,因此 bss 地址 = 虚拟地址 + segment 占用文件长度
		*/
		k = elf_ppnt->p_vaddr + elf_ppnt->p_filesz;
		if (k > elf_bss)
			elf_bss = k;
		/* 更新 end_code */
		if ((elf_ppnt->p_flags & PF_X) && end_code < k)
			end_code = k;

		/* 更新 end_data */
		if (end_data < k)
			end_data = k;

		/* 更新 brk , brk 是整个数据 segment 的结束虚拟地址,尽管 bss 不占可执行文件的空间,但是在链接时会为其分配虚拟内存地址空间 
		** p_memsz 是当前 segment 所占用的内存空间, p_memsz - p_filesz = sizeof(.bss)
		*/
		k = elf_ppnt->p_vaddr + elf_ppnt->p_memsz;
		if (k > elf_brk) {
			bss_prot = elf_prot;
			elf_brk = k;
		}
		/* 当前 segment 处理结束,回到 for 循环,找到下一个 segment 对应的 Program header,查看类型是否为 PT_LOAD,如果是,继续解析,如果不是,跳过 segment 继续找,直到遍历过所有的 PT_LOAD.通常 arm 中一个可执行文件只包含两个 segment. */
	}

	/* 到这里,所有 PT_LOAD 类型的 segment 已经被加载完成 */

	/* load_bias 表示偏移地址,对于 arm32 下的可执行文件加载,默认并没有设置偏移地址,对于 64 系统或者有针对性的安全策略系统,会在加载时添加一个随机的偏移地址,攻击者无法通过可执行文件的信息定位到程序在内存中的运行位置.
	** 在加载动态库时,load_bias 也是有意义的,动态库的加载位置有一个固定的偏移地址.  
	*/
	loc->elf_ex.e_entry += load_bias;
	elf_bss += load_bias;
	elf_brk += load_bias;
	start_code += load_bias;
	end_code += load_bias;
	start_data += load_bias;
	end_data += load_bias;

	/* 设置 current->mm->start_brk =  elf_brk */
	retval = set_brk(elf_bss, elf_brk, bss_prot);

	/* 可执行文件的 PT_LOAD segment 加载完成,接下来需要加载动态加载器到内存中 */
	/*****************************************************************/

	/* elf_interpreter 在前文中被赋值为动态链接器路径+文件名.
	** 默认情况下,可执行文件至少会依赖 glibc 动态库,因此都是需要动态加载器的.
	** 如果在编译时指定 -static 选项,表示编译静态可执行文件,就不需要使用到动态链接器.
	*/
	if (elf_interpreter) {
		/* load_elf_interp 函数负责加载动态加载器到内核中,动态加载器也是个动态库,这看起来有点奇怪:动态库由动态加载器加载,而动态加载器本身就是个动态库,它被谁加载呢?  
		** 解决方案是:动态加载器有一段自举代码,也就是自己加载自己,前提是它被 elf 加载器加载到内存中并执行.  
		** 总的来说, load_elf_interp 对动态加载器的加载和加载可执行文件是差不多的过程,可以参考上面可执行文件的加载,加载流程如下:
		** 1,在上面的准备工作中,已经把动态加载器的 elf 头部和 Program header table 基地址读取到了.
		** 2,遍历每个 Program header(Program header 用于描述 segment),检查 segment 类型是不是 PT_LOAD,同样只操作 PT_LOAD 类型的 segment.
		** 3,调用 elf_map 对动态加载器文件中 PT_LOAD 类型的 segment 进行映射
		** 4,与可执行文件不同的是,动态加载器的加载不需要记录 start_code,start_data,bss 等内存参数
		** 5,返回动态链接器的加载地址,这个地址也是动态链接器代码段的开始地址,当整个 execve 系统调用执行完成之后,并不是马上执行可执行文件中的代码,而是执行动态链接器的启动代码,也就是这个返回的地址.
		*/
		elf_entry = load_elf_interp(&loc->interp_elf_ex,
					    interpreter,
					    &interp_map_addr,
					    load_bias, interp_elf_phdata);
		/* 保存返回地址/偏移地址 */
		interp_load_addr = elf_entry;
		elf_entry += loc->interp_elf_ex.e_entry;
		reloc_func_desc = interp_load_addr;

		/* 设置写权限 */
		allow_write_access(interpreter);
	}
	/* 如果可执行文件依赖于动态库,存在动态加载器,程序的入口地址就是动态加载器的入口地址,否则就是可执行文件的可执行地址 */
	else {
		elf_entry = loc->elf_ex.e_entry;
		if (BAD_ADDR(elf_entry)) {
			retval = -EINVAL;
			goto out_free_dentry;
	}
	/************************************* 动态链接器加载完成 ******************************/
	/* 设置 mm->binfmt 为当前的 binfmt,binfmt 用于描述一个加载器以及对应的解析函数 */
	set_binfmt(&elf_format);

	/* 该函数主要负责处理用户空间参数的传递
	** 构建一个 elf_info 结构
	** 将保存在 bprm 中的用户空间参数保存到用户空间栈中
	** 将保存在 bprm 中的用户空间环境变量保存到用户空间栈中
	** 将 elf_info 结构数据保存到用户空间栈中 
	** 用户空间栈地址保存在 bprm->p 中.
	** 程序员并不会实际接触到保存栈上的参数,这些参数被 glibc 处理过后作为参数传递给 main 函数,所以这些参数对于应用程序员来说是无感的. 
	*/
	retval = create_elf_tables(bprm, &loc->elf_ex,load_addr, interp_load_addr);

	/* 将加载过程中生成的内存信息赋值到 current->mm 中. */
	current->mm->end_code = end_code;
	current->mm->start_code = start_code;
	current->mm->start_data = start_data;
	current->mm->end_data = end_data;
	current->mm->start_stack = bprm->p;

	/* 这是一个宏定义,接受三个参数:寄存器列表基地址,pc寄存器,sp寄存器,该宏的功能在于重新设置用户空间的运行信息.
	*/
	start_thread(regs, elf_entry, bprm->p);

	/* 异常处理和资源释放 */
	...
	/* ending */
}

```

对于 start_thread 这个接口的实现,完全有必要详细解释一下,它涉及当新程序加载完成之后是如何跳转到新的程序执行的,同时涉及必要多的底层知识,有些难理解.   

首先,我们需要了解系统调用的基本知识:用户空间发起 execve 系统调用陷入内核,内核所做的第一件事就是保存用户空间的断点信息到内核栈上,这些断点信息为 18 个寄存器值,然后内核在做完一系列检查和设置后,就会根据用户空间传入的系统调用号执行对应的系统调用,系统调用完成之后,再根据原来保存在内核栈上的断点信息恢复用户空间程序的执行.

关于系统调用的详细流程可以参考[arm linux系统调用流程](https://zhuanlan.zhihu.com/p/363290974).  

而 execve 系统调用是一个例外,execve系统调用的作用就是替换掉用户空间程序并执行新的程序,自然不再需要回到原来的程序中执行,理解了这个概念之后再来看 start_thread 的实现就比较简单了,这个函数就是修改保存在内核栈上的断点信息,当系统调用完成之后就会执行我们指定的用户程序中,从而完成程序的完整替换并重新执行.    


```c++
/* 传入三个参数:regs,elf_entry,bprm->p 对应 regs,pc,sp
** regs 是在 load_elf_binary 开始部分定义并赋值的:struct pt_regs *regs = current_pt_regs();这个函数返回内核栈上保存断点参数的基地址
** elf_entry 赋值给用户空间的 pc 寄存器,也就是返回用户空间时,程序从 elf_entry 开始执行,在可执行文件依赖于动态库时,elf_entry 为动态库入口地址,否则为可执行文件入口地址.
** bprm->p 赋值给用户空间的 sp,确定了用户空间的栈地址.
*/
#define start_thread(regs,pc,sp)					\
({									\
	/* 清除所有寄存器值 */
	memset(regs->uregs, 0, sizeof(regs->uregs));			\
	/* 初始化用户空间的 APSR 寄存器*/
	if (current->personality & ADDR_LIMIT_32BIT)			\
		regs->ARM_cpsr = USR_MODE;				\
	else								\
		regs->ARM_cpsr = USR26_MODE;				\
	if (elf_hwcap & HWCAP_THUMB && pc & 1)				\
		regs->ARM_cpsr |= PSR_T_BIT;				\
	regs->ARM_cpsr |= PSR_ENDSTATE;					\

	/* 设置用户空间 pc 指针 */
	regs->ARM_pc = pc & ~1;		/* pc */			\
	/* 设置用户空间栈指针 */
	regs->ARM_sp = sp;		/* sp */			\

	/* 在带有 MMU 的 linux 下为空指令 */
	nommu_start_thread(regs);					\
})
```



当加载过程完成之后，程序将返回到用户空间，实际上这时候并不会立马执行应用程序，而是执行链接器的代码执行动态链接工作，至于链接器的时候博主就没有继续深究了，有兴趣的朋友可以去看看。



## 小结

对于整个加载的过程，可以总结为这几个关键的阶段：

* 用户空间发起 exec* 系统调用，传入需要执行的应用程序以及参数，触发内核 execve 系统调用。
* 内核中系统调用的常规检查，执行内存拷贝，比如文件名、命令行参数、环境参数等，内核不直接访问用户空间的内存。
* 读取可执行文件的前 128 字节数据，根据文件的前 128 字节可以判断可执行文件属于哪种文件类型，对于 elf 而言，文件头为 52 字节。
* 确定了可执行文件类型之后，就调用对应加载器的 load_binary 回调函数，对于 elf 加载器而言，该回调函数为 load_elf_binary。



load_elf_binary 是核心的加载函数，其执行流程为：

* 读取 52 字节的 elf 头部，elf header 中包含整个 elf 的关键信息，包括 program header table 位置、数量等。
* 读取类型为 PT_INTERP 的 segment，实际上就是 .interp section 中包含的内容，该section 中指定当前可执行文件需要使用的链接器，在加载当前可执行文件之前，需要先加载链接器，链接器是必须的
* 释放原进程的资源，包括打开的文件、信号等。
* 扫描所有的 program headers，也就是逐个获取 segment，segment 有不同的属性，有些属于指令数据(PT_LOAD类型)需要直接 copy 到内存中，而有一些属于加载时辅助信息，比如重定位信息，这部分不需要拷贝到内存中。
  加载器为指令和数据部分创建一个新的 vm_struct_area 结构，建立映射，但是并不分配实际的物理内存，只有在访问时才会触发缺页异常分配对应的物理页面。
* 在整个加载期间创建并初始化一个新的 mm_struct 结构。
* 修改系统调用的返回地址，加载完成之后已经不需要返回到原来的程序断点，而是返回到动态链接器程序执行地址，执行链接程序。



因为这篇博客的完成时间比较久远，直接将代码和代码逻辑分析放到一起，看起来比较乱，阅读体验不好，后续有时间的话再回过头来整理吧。



### 参考

内核 4.14 源码

man exec 指令

---

[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

