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

formats 为全局链表,在上文 注册可执行文件加载程序 小节中提到,每一种可执行文件格式都会在系统启动时向内核注册对应的加载程序,而这些加载程序由 struct linux_binfmt 结构描述,以回调函数的形式提供,所有类型的 linux_binfmt 都会链接到一个全局链表中,这个链表头就是 formats.  

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
typedef struct elf32_hdr{
  unsigned char	e_ident[EI_NIDENT];   // ELF 魔数以及其他用于识别的字段
  Elf32_Half	e_type;               // ELF 类型
  Elf32_Half	e_machine;            // 机器类型
  Elf32_Word	e_version;            // 版本
  Elf32_Addr	e_entry;              // 入口地址
  Elf32_Off	e_phoff;                  // program header table 偏移地址
  Elf32_Off	e_shoff;                  // section header table 偏移地址
  Elf32_Word	e_flags;              // 标志位
  Elf32_Half	e_ehsize;             // elf 头部 size
  Elf32_Half	e_phentsize;          // program header table 的 size
  Elf32_Half	e_phnum;              // program header table 的数量
  Elf32_Half	e_shentsize;          // section header table 的 size
  Elf32_Half	e_shnum;              // section header table 的数量
  Elf32_Half	e_shstrndx;           // section header table 中 string table 的位置，该段用于保存字符串
} Elf32_Ehdr;
```

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

先熟悉 elf 文件相关概念有利于接下来的文件加载分析，也算是磨刀不误砍柴工。更详细的 elf 格式信息可以参考这一些博客：TODO。   

接下来我们进入正文：分析 load_elf_binary 函数，与往常一样，省去一些错误检查和一些与主逻辑关联不大的分支代码，专注于加载过程的剖析。  

```C++
static int load_elf_binary(struct linux_binprm *bprm)
{
	struct elf_phdr *elf_ppnt, *elf_phdata, *interp_elf_phdata = NULL;
	char * elf_interpreter = NULL;

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

	/* 设置栈空间 TODO */
	retval = setup_arg_pages(bprm, randomize_stack_top(STACK_TOP),executable_stack);
	current->mm->start_stack = bprm->p;

	/* 设置完整个进程的内存空间，意味着准备工作已经完成，接下来就进入了程序加载的正题：执行程序的加载，这是一个复杂而繁琐的工作 */

	
}

```





