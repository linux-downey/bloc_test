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
            char* ptr = (char*)mmap(NULL,4096,PROT_WRITE, MAP_SHARED, fd, 0);
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
            char* ptr = (char*)mmap(NULL,4096,PROT_WRITE, MAP_SHARED, fd, 0);
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
