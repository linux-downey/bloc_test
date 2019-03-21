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
