#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <net/netlink.h>
#include <net/sock.h>

#define debug  true
MODULE_LICENSE("GPL");              

#define MY_NETLINK 28
struct sock *sock = NULL;

static void my_netlink_rcv(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	int ret = 0;
	char *data = NULL;
	char *back_str = "How are you,downey!";

	nlh = nlmsg_hdr(skb);
	data = NLMSG_DATA(nlh);
	pr_info("Recv data = %s\n",data);

	int len = strlen(back_str);	

	struct sk_buff *send_skb;
	
	send_skb = nlmsg_new(len,GFP_KERNEL);
	if(send_skb){
		char *scratch;
		struct nlmsghdr *send_nlh;
		send_nlh = nlmsg_put(send_skb,100,0,NLMSG_DONE,len,0);
		if(!send_nlh){
			kfree_skb(send_skb);
			return ;
		}
		scratch = nlmsg_data(send_nlh);
		memcpy(scratch,back_str,len);		
		NETLINK_CB(send_skb).dst_group = 1;
		ret = netlink_broadcast(sock,send_skb,0,1,MSG_DONTWAIT);
//		ret = netlink_unicast(sock,send_skb,100,MSG_DONTWAIT);
		if(ret == -ENOBUFS || ret == -ESRCH){
			pr_err("Failed to broadcast,ret = %d\n",ret);
		}
	}

}

static int __init mnetlink_init(void)
{
	int ret = 0;
	struct netlink_kernel_cfg cfg = {
		.groups = 1,
		.input	= my_netlink_rcv,
		/* whether the non-root user can recv/send broadcast */
		.flags  = NL_CFG_F_NONROOT_RECV | NL_CFG_F_NONROOT_SEND,
	};
	pr_info("Create kernel netlink\n");
	sock = netlink_kernel_create(&init_net, MY_NETLINK, &cfg);
	if(NULL == sock){
		pr_err("Failed to create socket\n");
	}
	return 0;
}

static void __exit mnetlink_exit(void)
{
	pr_info("Release kernel netlink\n");
	netlink_kernel_release(sock);
}

module_init(mnetlink_init);
module_exit(mnetlink_exit);
