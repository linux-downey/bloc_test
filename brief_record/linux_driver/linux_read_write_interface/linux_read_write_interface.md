用户空间经过系统调用进入到内核空间，x86 上是通过 0x80 软中断，进入到内核中。  


以 read 为例，应用层上的函数原型为：

ssize_t read(int fd, void *buf, size_t count)
fffffff
进入到系统调用 fs/read_write() 中：

在文件的最开始，处理 fd 与 struct fd 结构的影射关系，先通过 fd 来找到对应的 struct fd，所以就可以找到对应的 struct file，struct file中
保存了内核设置的 file_operations.
struct fd {
    struct file *file;
    unsigned int flags;
};

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
	ksys_read(fd, buf, count);
		file_pos_read
		vfs_read
			__vfs_read
				if(file->f_op->read)
					file->f_op->read(file, buf, count, pos)
		file_pos_write



SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
	do_sys_open(AT_FDCWD, filename, flags, mode)
		get_unused_fd_flags(flags);
			fd_install
			
			
对于每一个进程而言，current 指针指向当前进程的 task_struct ，所有打开的文件，都会在 task_struct->files->fdt->struct file **fd
记录下来，fd是一个结构体数组，返回给用户的 fd 就是该结构体数组的下标，根据这个下标，就可以找到对应的 struct file 结构体，struct file
中保存了 ops。  

每一次打开文件都会分配一个 struct file，即使是打开同一个文件，也会分配不同的struct file结构，因为通常文件偏移位置是特定的。  

struct dentry 目录项，struct inode 对应真实物理设备上的inode节点，struct dentry 中保存了指向 inode 节点的指针，一个 inode 可以对应多个
dentry，但是一个 dentry 只能对应一个 inode。  

inode 对应一个真实文件的属性，而 block 则对应了一个文件的数据部分。








