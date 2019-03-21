current->files->fdt->fd[].  


调用read()->sys_read->fd转换到struct files-


使用creat创建文件，调用SYSCALL_DEFINE2(creat, const char __user *, pathname, umode_t, mode)，未完。  
