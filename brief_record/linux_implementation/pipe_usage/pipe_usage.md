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

无名管道在内存中存在。 

无名管道就是在内存中建立一个管道文件，这个文件不属于任何文件系统，是管道独立的文件系统。  
值得注意的是，无名管道如果关闭一端，另一端不会因此而关闭，还是可以读写。  

如果管道的读端已经被关闭，再往管道里面写就会产生信号SIGPIPE，如果不处理这个信号，则写失败。  

但是写端如果先关闭，读端依旧可以读取管道数据。  
代码如下：
void sigpipe_hd(int sig)
{
	printf("oooooooomg,failed\r\n");
}

int main(void)
{
	int status;
	int new_fd = 0;
	int pid = 0;
	int fd[2] = {0};

	signal(SIGPIPE,sigpipe_hd);
	pipe(fd);
	close(fd[0]);
	if(write(fd[1],"huangdao\n",9) == -1)
	{
		printf("write failed\r\n");
		return 0;
	}
	printf("xyz\n");

	return 0;
}





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
与无名管道不同的是，命名管道的创建需要在文件系统中创建一个fifo文件，其他进程可以直接访问。  
不再局限于父子进程或者兄弟进程。  

使用mkfifo来创建命名管道。  

int mkfifo(const char *path, mode_t mode); 

创建管道文件，然后进行通信。

在命名管道文件中，使用ls查看文件长度始终为0，当文件中存在数据时，另一个进程将数据读出时，数据将减少。  
对管道文件的写，当数据量小于某个数值时，写文件是原子操作。原子操作必须包含在同一个函数调用中。  

当写文件时，如果没有打开相应的读端，且写文件的mode默认为block形式，写进程将会被阻塞。

对于读文件时同样的道理。  

不能存在多读多写的情况，只能存在一读多写的情况。且基本上单个进程对于一个管道只进行单向的数据通信。只读不写或者只写不读。    


#include <sys/wait.h> 
#include <signal.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/mman.h> 
#include <linux/sched.h> 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


void sigpipe_hd(int sig)
{
	printf("oooooooomg,failed\r\n");
}

int main(void)
{
	char buf[200]={0};
	int pid = 0;

	signal(SIGPIPE,sigpipe_hd);
	if(!access("fifo",F_OK))
	{
		printf("file exist,remove file\n");
		remove("fifo");
	}
	if(mkfifo("fifo",0666))
	{
		printf("failed to create fifo\n");
		return 0;
	}
	
	pid = fork();
	if(pid<0){

	}
	else if(0 == pid){
		char buf[100]= {0};
		int fd = open("fifo",O_RDONLY);
		printf("son's fd = %d\n",fd);
		

		while(1)
		{
			read(fd,buf,100);
			printf("read buf = %s\n",buf);
			sleep(2);
		}
		exit(0);
	}
	else{
		int new_pid = 0;
		char buf[100] = {0};
		
		new_pid = fork();
		if(new_pid < 0){

		}
		else if(0 == new_pid){
			int fd = open("fifo",O_WRONLY);
			printf("silbling's fd = %d\n",fd);
			while(1)
			{
				//write(fd,"huangdao\n",9);
				sleep(1);
			}
		}
		else
		{
			int fd = open("fifo",O_WRONLY);
			printf("father's fd = %d\n",fd);
			while(1)
			{
				//write(fd,"downey\n",7);
				sleep(1);
			}
		}
	}


	return 0;
}



无名管道适合父子、兄弟进程之间的通信。  

命名管道适合一读多写，不同进程中的一读一写。

多读多写的模型适合使用mmap()配合posix信号量。 


信号量的操作：

int main(void)
{
	

	char buf[200]={0};
	int pid = 0;

	signal(SIGPIPE,sigpipe_hd);
	// if(!access("fifo",F_OK))
	// {
	// 	printf("file exist,remove file\n");
	// 	remove("fifo");
	// }
	// if(mkfifo("fifo",0666))
	// {
	// 	printf("failed to create fifo\n");
	// 	return 0;
	// }

	
	pid = fork();
	if(pid<0){

	}
	else if(0 == pid){
		sem_t *my_sem = sem_open("/mysem",O_CREAT, 0666,1);
		char buf[100]= {0};
		int count = 0;
		while(1)
		{
			count++;
			printf("%d\n",count);

			sem_wait(my_sem);
			count++;
			printf("%d\n",count);
			sem_wait(my_sem);
			count++;
			printf("%d\n",count);
			sem_wait(my_sem);
			
		}
		sem_close(my_sem);
		exit(0);
	}
	else{
		sleep(1);
		sem_t* _sem = sem_open("/mysem",O_EXCL);
		while(1)
		{

			sem_post(_sem);
			sleep(1);
		}
		sem_close(_sem);


	}


	return 0;
}


















