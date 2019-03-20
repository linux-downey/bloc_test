#include <sys/wait.h> 
#include <signal.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/mman.h> 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


int main(void)
{
	int status;
    int nfd = 0;
	//signal(SIGCHLD,SIG_IGN);
	
	int fd = open("mmap_device",O_RDWR|O_CREAT,0666);
	printf("fd = %d\r\n",fd);

    nfd = dup(fd);
    printf("nfd = %d\r\n",nfd);

    write(fd,"huangdao\n",8);
    write(nfd,"downey",6);
    close(fd);
    write(nfd,"ff",2);
	
	close(nfd);
	return 0;
}



dup的作用就是复制一个文件描述符，两个文件描述符都可以使用，且相互独立，当一个文件描述符关闭时，另一个文件描述符还可以对文件进行操作。  

