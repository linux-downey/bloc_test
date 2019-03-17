僵尸进程的起因：父进程没有终止，子进程已经终止，
僵尸进程只是一些结构信息，并不太占内存。
如果父进程同时终止，子进程将由系统管理，被回收

父进程可以调用wait()等待子进程结束
wait()等到一个子进程返回，或者收集到一个子进程的信息就返回。  
但是wait()如果没有子进程，wait()直接返回-1；

对于waitpid(),waitpid(-1,&status,0) = wait()
waitpid第三个参数是用来设置指定接收的子进程终止状态的。  

测试代码如下:
    #include <sys/wait.h> 
    #include <stdio.h>
    #include <unistd.h>
    #include <stdlib.h>

    int main(void)
    {
        int status;
        int pid = 0;
        int pid1= 0;
        pid = fork();
        if(pid == -1){
            printf("fork failed\r\n");
        }
        if(0 == pid)
        {
            printf("This is child,pid = %d\r\n",getpid());
            exit(-1);
        }
        else
        {
            printf("This is father, child pid = %d,my pid = %d\r\n",pid,getpid());
            pid1 = fork();
            if(-1 == pid1)
            {
                
            }
            else if(0 == pid1)
            {
                printf("second child,pid1 = %d\r\n",getpid());
                exit(1);
            }
            else
            {
                wait(&status);
                wait(&status);
                int ret = wait(&status);
                printf("ret = %d\r\n",ret);
                //sleep(1);
                //while(1);
            }
            
            exit(0);
        }
        
        return 0;
    }



防止僵尸进程，还可以使用signal函数，

安装一个signal函数，signal(SIGCHLD,SIG_IGN);需要使用SIG_IGN,如果使用其他处理函数，还得调用wait()来为子进程收尸，否则还是僵尸进程。  

处理僵尸进程，除了使用signal()函数以外，原则上可以让僵尸进程的父进程终止，由系统接管子进程即可。

调用fork产生一个子进程，再由子进程调用fork产生一个子进程，结束子进程，孙子进程就由系统接管，就成为了独立进程。  

在bash终端中，当父进程终止时，子进程也会被终止，可能是shell的特殊环境所致。 


    #include <sys/wait.h> 
    #include <signal.h>
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
        signal(SIGCHLD,SIG_IGN);
        pid = fork();
        if(pid == -1){
            printf("fork failed\r\n");
        }
        if(0 == pid)
        {
            printf("This is child,pid = %d\r\n",getpid());
            pid1 = fork();
            if(-1 == pid1)
            {
                printf("fork error\r\n");
            }
            else if(0 == pid1)
            {
                printf("grandson process,pid1 = %d\r\n",getpid());
                while(1);
                exit(1);
            }
            else
            {
                printf("son process\r\n");
                //while(1);
                exit(0);
            }
        }
        else
        {
            printf("This is father, child pid = %d,my pid = %d\r\n",pid,getpid());
            
            while(1);
            exit(0);
        }
        
        return 0;
    }






