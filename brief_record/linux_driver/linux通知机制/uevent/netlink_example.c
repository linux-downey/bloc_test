#include <stdio.h>
#include <string.h>
#include <linux/netlink.h>

#define  MSGLEN 512 
#define  SRC_PID  100
#define  DST_PID  101

int init_netlink(void)
{
	int ret = 0;
	int nlfd = 0;
	struct nlmsghdr *nlhdr = NULL;
	struct sockaddr_nl src_addr,dest_addr;
	
	nlfd = socket(AF_NETLINK, SOCK_DGRAM, 30);
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
	
	dest_addr.nl_family = AF_NETLINK;

	dest_addr.nl_pid = DST_PID; // 0 - kernel

	dest_addr.nl_groups = 0;

	nlhdr->nlmsg_len = NLMSG_SPACE(MSGLEN);

	nlhdr->nlmsg_pid = SRC_PID; 

	nlhdr->nlmsg_flags = 0;
	
	strcpy(NLMSG_DATA(nlhdr),"Hello World!\n"); 
	
	struct iovec iov;

	iov.iov_base = (void *)nlhdr;
	iov.iov_len = NLMSG_SPACE(MSGLEN);
	
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	state_smg = sendmsg(nlfd,&msg,0);
	if(state_smg < 0) {
		return -4;
	}
	
	return 0;
}



int main(void)
{
	
	return 1;
}

