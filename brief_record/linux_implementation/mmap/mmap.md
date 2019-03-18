mmap的基本使用：
在不同的进程里面，对同一个文件调用mmap，获取一个地址，这个地址就可以像操作内存一样操作文件，多进程中进行通信。

源码实现：

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
        pid = fork();
        if(pid == -1){
            printf("fork failed\r\n");
        }
        if(0 == pid)
        {
            int count=0;
            printf("This is child,pid = %d\r\n",getpid());
            int fd = open("/home/downey/test_dir/test_c/mmap_file",O_RDWR);
            printf("fd = %d\r\n",fd);
            char* ptr = (char*)mmap(NULL,4096,PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
            if(ptr == NULL) printf("fuck\r\n");
            printf("0x%x\r\n",ptr);
            while(1)
            {
                count++;
                sprintf(ptr,"value = %d\r\n",count);
                sleep(1);
                //write(fd,"huangdao\n",10);
                
            }
            exit(1);
        }
        else
        {
            int fd = open("/home/downey/test_dir/test_c/mmap_file",O_RDWR);
            printf("fd = %d\r\n",fd);
            printf("This is father, child pid = %d,my pid = %d\r\n",pid,getpid());
            char* ptr = (char*)mmap(NULL,4096,PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
            if(ptr == NULL) printf("fuck1\r\n");
            printf("0x%x\r\n",ptr);
            while(1)
            {
                //write(fd,"hemengting\n",12);
                printf("ptr = %s\r\n",ptr);
                sleep(1);
            }
            exit(0);
        }
        
        return 0;
    }

一个进程读，一个进程写
需要注意的是，mmap()的操作并不会扩展文件的大小，如果是新创建的文件，文件长度为0，往映射内存的部分写东西将会到值Bus_error.
创建文件时，需要先把文件扩大。
使用ftruncate(fd,length);来对文件进行扩展，成功返回0，失败返回-1。  


使用msync()函数来将当前写入的数据更新到物理空间中。 

mmap调用流程在syscalls.c中，
    
    SYSCALL_DEFINE6(mmap, unsigned long, addr, size_t, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, off_t, offset)




