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
#include <sys/types.h>
#include <arpa/inet.h>

#define  MSGLEN 512 
#define  SRC_PID  101
#define  DST_PID  100

int init_netlink(void)
{
	int ret = 0;
	int nlfd = 0;
	struct nlmsghdr *nlhdr = NULL;
	struct sockaddr_nl src_addr,dest_addr;
	
	printf("sizeof(struct sockaddr_nl) = %d\n",sizeof(struct sockaddr_in));
	//while(1);

	nlfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
	if(nlfd < 1) return -1;
	
	memset(&src_addr,0,sizeof(src_addr));
	memset(&dest_addr,0,sizeof(dest_addr));
	
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = SRC_PID;
	src_addr.nl_groups = 0;
	
	
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
	struct msghdr msg;
	int state;
	int state_smg = 0;

	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = DST_PID; // 0 - kernel
	dest_addr.nl_groups = 0;
	
	nlhdr->nlmsg_len = NLMSG_SPACE(MSGLEN);
	nlhdr->nlmsg_pid = SRC_PID; 
	nlhdr->nlmsg_flags = 0;
	strcpy(NLMSG_DATA(nlhdr),"Hello World!\n"); 

	iov.iov_base = (void *)nlhdr;
	iov.iov_len = NLMSG_SPACE(MSGLEN);
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	while(1){
		state_smg = sendmsg(nlfd,&msg,0);

		if(state_smg < 0) {
			printf("send failed\n");
		}
	}
	
	//memset(nlhdr,0,NLMSG_SPACE(MSGLEN));
	/*while(1){
		state = recvmsg(nlfd, &msg, 0);
		if(state < 0){

		}
		else{
			printf("recieve message:%s\n",(char *) NLMSG_DATA(nlhdr));
		}
	}*/
	return 0;
}



int main(void)
{
	int ret = 0;
	ret = init_netlink();
	printf("ret = %d\n",ret);
	return 1;
}

