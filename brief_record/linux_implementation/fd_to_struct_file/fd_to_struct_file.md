current->files->fdt->fd[].  


调用read()->sys_read->fd转换到struct files-


使用creat创建文件，调用SYSCALL_DEFINE2(creat, const char __user *, pathname, umode_t, mode) 
	sys_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
		do_sys_open(AT_FDCWD, filename, flags, mode);
			tmp = getname(filename);
			get_unused_fd_flags(flags);
			struct file *f = do_filp_open(dfd, tmp, &op);
			fsnotify_open(f);
			fd_install(fd, f);
				__fd_install(current->files, fd, file);
					rcu_assign_pointer(fdt->fd[fd], file);
						
						
						

在linux内核中，inode对应文件本身，即ls -l出来的信息都属于inode。  




struct file主要是描述一个打开的文件，文件偏移地址fpos，以及fops(文件相应的读写操作)。
当进程对同一文件打开两次时，将会创建两个struct file，这两个struct file指向同一个inode。
需要验证。


打开的文件总是返回当前可用的最小的文件描述符。  


示例：先关闭标准输入，然后再打开一个文件，返回的fd就是标准输入的fd。  
使用gets()从标准输入读就直接从文件中读取数据了。

	#include <sys/wait.h> 
	#include <signal.h>
	#include <sys/stat.h> 
	#include <fcntl.h> 
	#include <sys/mman.h> 
	#include <linux/sched.h> 

	#include <stdio.h>
	#include <unistd.h>
	#include <stdlib.h>

	int main(void)
	{
		int status;
		int new_fd = 0;
		int pid = 0;
		int fd = 0;
		int c;
		char str[50] = {0};

		close(STDIN_FILENO);
		fd = open("mmap_device",O_RDWR);
		printf("fd = %d\n",fd);
		/*
		while(c = getchar())
		{
			printf("read : %c\n",(char)c);
		}*/

		while(gets(str))
		{
			printf("read : %s\n",str);
		}

		return 0;
	}
















