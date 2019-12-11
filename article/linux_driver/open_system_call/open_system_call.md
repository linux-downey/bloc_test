open 在应用层的接口：
	int open(const char *path, int oflag, ... );

O_APPEND ：追加
O_ASYNC：
O_CREATE:不存在就创建
O_NONBLOCK：不阻塞
O_TRUNC：将文件截断为0


对应的系统调用：

do_sys_open()
	build_open_flags(flags, mode, &op);  //创建打开flag并存放在 struct open_flags op; 中。
		get_unused_fd_flags()   //从 struct files_struct		\*files; 中获取未分配的fd，并返回，fdt[]中返回
		
		do_filp_open()
			path_openat()
				get_empty_filp(); //获取一个空的struct file结构，相当于创建一个struct file结构
					...
					最终会调用到 vsf_open() 在该文件中调用fops
	
	
	fsnotify_open()  //将文件添加到文件监测系统里面
	fd_install将fd 和 file 绑定



rlimit(RLIMIT_NOFILE)确定了打开文件最大值。

使用 get_name,将const char* filename 转换成 struct filename filename。

将pathname复制给nameidata，传入到path_openat()函数进行处理。

path_init() 对目录进行解析。


