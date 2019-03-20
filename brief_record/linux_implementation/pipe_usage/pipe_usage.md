#include <sys/wait.h> 
#include <signal.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/mman.h> 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


void process_child(int n)
{
    printf("signal,%d\r\n",n);
}

int main(void)
{
    int status;
    int pid = 0;
    int pid1= 0;
    //signal(SIGCHLD,SIG_IGN);

    int fd[2];
    int count = 0;

    if(pipe(fd)){
        printf("pipe operate failed\r\n");
        return -1;
    }

    pid = fork();
    if(pid == -1){
        printf("fork failed\r\n");
    }
    if(0 == pid)
    {
        close(fd[0]);
        
        while(1)
        {
            count++;
            printf("write count\r\n");
            write(fd[1],&count,sizeof(int));                                
            sleep(1);
        }
        exit(0);
    }
    else
    {
        int recv;
        close(fd[1]);
        while(1)
        {
            if(read(fd[0],&recv,sizeof(int)) < 0)
            {
                close(fd[0]);
                exit(0);
            }
            printf("recv data = %d\r\n",recv);
        }
        
    }
    
    return 0;
}



无名管道，一般只使用单向地传输，只能由父进程建立，然后子进程继承了父进程的管道。
子进程关闭管道的一端，父进程关闭管道的另一端，两者就可以进行单向通信了。  

无名管道就是在内存中建立一个管道文件，这个文件不属于任何文件系统，是管道独立的文件系统。  


注册pipe文件系统给的流程。
在内核fs/pipe.c中，

init_pipe_fs()中使用：
        register_filesystem()注册了pipe独立的文件系统，
        然后调用kern_mount()进行了挂载。返回pipe_mnt文件系统根节点


pip的调用流程：

pip触发系统调用：SYSCALL_DEFINE1(pipe, int __user *, fildes)
	pipe系统调用再调用sys_pipe2(fildes, 0),进入系统调用：
		do_pipe_flags()
			__do_pipe_flags(fd, files, flags);

				create_pipe_files();
					alloc_file(&path, FMODE_WRITE, &pipefifo_fops);  pipefifo_fops就是相应的file_operations结构体
						file->f_op = fop;  将参数中传入的pipefifo_fops赋值给file->f_op指针。  

				get_unused_fd_flags()调用两次，获取两个unuesed fds。	

		fd_install(fd[0], files[0]);fd_install(fd[1], files[1]);将调用获取的struct files和fd绑定

	调用成功，调用copy_to_user(fildes, fd, sizeof(fd))，将fd[2]拷贝回用户空间
创建了两个文件，两个fd。  


write调用：在file_operations结构体里查找，对应的调用函数为

pipe_read()
pipe_write()



# 命名管道
























