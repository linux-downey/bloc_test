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
	
	
	
	fsnotify_open()  //将文件添加到文件监测系统里面
	fd_install将fd 和 file 绑定



rlimit(RLIMIT_NOFILE)确定了打开文件最大值。