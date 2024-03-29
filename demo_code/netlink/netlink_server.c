#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <errno.h>

#define  MSGLEN 512 
#define  DST_PID  101

#define MY_NETLINK 28

unsigned long int src_pid;

int init_netlink(void)
{
	int ret = 0;
	int nlfd = 0;
	struct nlmsghdr *nlhdr = NULL;
	struct sockaddr_nl src_addr,dest_addr;
	struct msghdr msg;
	int state;
	
	nlfd = socket(AF_NETLINK, SOCK_RAW, MY_NETLINK);
	if(nlfd < 1) return -1;
	
	memset(&src_addr,0,sizeof(src_addr));
	memset(&dest_addr,0,sizeof(dest_addr));
	
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = src_pid;
	src_addr.nl_groups = 1;
		
	ret = bind(nlfd, (struct sockaddr *)&src_addr,
                sizeof(src_addr));
	if(ret < 0 ) {
		close(nlfd);
		return -2;
	}
	
	nlhdr = malloc(NLMSG_SPACE(MSGLEN));
	if(!nlhdr) {
		close(nlhdr);
		return -3;
	}
	
	struct iovec iov;
	int state_smg = 0;

	iov.iov_base = (void *)nlhdr;
	iov.iov_len = NLMSG_SPACE(MSGLEN);
	memset(&msg, 0, sizeof(msg));
	
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	memset(nlhdr,0,NLMSG_SPACE(MSGLEN));

	while(1){
		state = recvmsg(nlfd, &msg, 0);
		if(state < 0){
			printf("recv error!\n");
		}
		else{
			printf("/**********************************************************/\n");
			printf("recieve message:%s\n",(char *) NLMSG_DATA(nlhdr));
			printf("recv pid :%d\n",nlhdr->nlmsg_pid);
			printf("recv pid :%d\n",nlhdr->nlmsg_len);
			printf("recv pid :%d\n",nlhdr->nlmsg_type);
			printf("/**********************************************************/\n");
		}
	}
	return 0;
}



int main(int argc,char *argv[])
{
	int ret = 0;
	if(argc != 2){
		printf("Usage:\n ./server <PID_NUM>\n");
		return -1;
	}
	src_pid = strtoul(argv[1],NULL,0);
	ret = init_netlink();
	printf("ret = %d\n",ret);
	return 1;
}

