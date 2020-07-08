# linux 内核 uevent 机制实现
用户空间中 udev 的实现依赖于内核提供的设备信息,所有的硬件设备都属于硬件资源,哪怕是一个 GPIO,在操作系统的设计中,所有的硬件资源都应该由内核进行托管,当内核中的设备信息发生更新时,比如发生热插拔事件,内核需要通过某种通信方式来通知 udevd 守护进程,设备的状态已经被改变,需要及时地更新设备信息,增加/删除/修改某个硬件设备对应的设备节点,将这种修改实时地反映到用户空间的设备节点上,以免用户在使用时因为更新不及时而产生一些错误.   

针对设备的动态管理,内核通过 uevent 机制实现,这种动态管理设备节点的机制底层原理是通过 kobject 对所有的设备进行组织,一旦发生设备的变动,同时也会反映在 kobject 结构中,驱动开发者可以选择将相应的变动信息通过内核提供的接口发送到用户空间,由 udevd 接收并处理,所以整个 uevent 机制的实现其实是内核与用户进程 udevd 之间的配合,而内核中的 uevent 实现和 kobject 是强相关的. 

## uevent 机制初始化
uevent 机制中,内核与用户空间的交互通过两种方式实现:
* 使用 netlink 实现内核空间与用户空间的数据发送
* 使用 call userhelper 的机制,内核直接调用用户空间的脚本,处理用户空间的设备节点

netlink 通信的方式相对于 call userhelper 的方式来说,看起来是要麻烦一些,由内核直接调用用户空间脚本非常直观且容易实现,但是这种方式有一个非常大的问题就是开销大,调用用户脚本意味着创建一个新的进程,从这个角度来看,通过 netlink 发送数据到用户空间的效率要远远优于直接调用用户程序,尤其是当系统启动的时候,会产生大量的 uevent 调用.  

在内核的实现中,netlink 机制要更受欢迎,而对应的用户空间的接收程序为 udevd.  

uevent 的初始化在 lib/kobject_uevent.c 中:

```c++
static int uevent_net_init(struct net *net)
{
	struct uevent_sock *ue_sk;
	struct netlink_kernel_cfg cfg = {
        /* 配置 netlink 组 */
		.groups	= 1,
		.flags	= NL_CFG_F_NONROOT_RECV,
	};

	ue_sk = kzalloc(sizeof(*ue_sk), GFP_KERNEL);
    /* 创建并返回一个 netlink 套接字 */
	ue_sk->sk = netlink_kernel_create(net, NETLINK_KOBJECT_UEVENT, &cfg);
    /* 链接到全局链表 */
    list_add_tail(&ue_sk->list, &uevent_sock_list);
	return 0;
}
```
netlink 的初始化部分主要完成两个部分:
* 申请并创建一个 netlink 套接字
* 将返回的套接字句柄链接到全局链表 uevent_sock_list 中.  

内核中创建 netlink 套接字直接调用内核的接口即可,和用户空间的创建方式不一样,毕竟内核中并没有实现 glibc.  

## uevent 接口

### 相关数据结构
在了解 uevent 接口之前,有必要先来看看 uevent 相关的数据结构:

#### kobj_uevent_env

struct kobj_uevent_env 用于描述 uevent 相关的信息,包括需要发送到用户空间的数据,参数等.  

```c++
struct kobj_uevent_env {
    /* 使用 call userhelper 方式的时候使用,用于保存执行脚本名以及参数 */
	char *argv[3];
    /* 使用 netlink 机制时发送到用户空间的环境数据指针,指向 buf */
	char *envp[UEVENT_NUM_ENVP];
    /* 添加 envp item 时的索引值 */
	int envp_idx;
    /* 使用 netlink 机制时发送到用户空间的环境数据 */ 
	char buf[UEVENT_BUFFER_SIZE];
    /* buf 长度 */
	int buflen;
};
```

#### enum kobject_action 
enum kobject_action 是一系列的枚举值,用于描述设备的变动类型:

```c++
enum kobject_action {
	KOBJ_ADD,     //添加
	KOBJ_REMOVE,  //移除
	KOBJ_CHANGE,  //修改
	KOBJ_MOVE,    //移动
	KOBJ_ONLINE,  //上线
	KOBJ_OFFLINE, //离线
	KOBJ_BIND,    //绑定
	KOBJ_UNBIND,  //解绑
	KOBJ_MAX      //结束
};
```
#### struct kset_uevent_ops

看到 ops 就知道这是一个回调函数集合,这个结构体被包含在 kset 中,因为 uevent 的上报由 kset 进行统一管理,在后文中我们继续讨论.  
```c++
struct kset_uevent_ops {
    /* 过滤函数,决定当前 uevent 是否上报,该回调函数将在 uevent 上报时调用,如果返回 0 则不上报 */
	int (* const filter)(struct kset *kset, struct kobject *kobj);
	/* name 的返回决定上报数据中的 subsystem 字段 */
    const char *(* const name)(struct kset *kset, struct kobject *kobj);
    /* 通过自定义的 uevent 回调函数可以添加一些自定义的上报数据 */
	int (* const uevent)(struct kset *kset, struct kobject *kobj,
		      struct kobj_uevent_env *env);
};
```

### 函数接口
uevent 的接口比较简单,通常被用到的也就是两个接口:
* int kobject_uevent(struct kobject *kobj, enum kobject_action action)
* int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,char *envp_ext[])

实际上第一个接口直接调用第二个接口,将第 kobject_uevent_env 的第三个参数默认设置为 NULL.  

kobject_uevent_env 接受三个参数:
* kobj:发生变动设备对应的 kobject.
* action:修改的类型,ADD/REMOVE/CHANGE 等
* envp_ext:这是一个指针数据,也可以说是字符串数组,这个数组中所指向的所有字符串将会添加到发送内容中.  

了解了相关的数据结构,接下来自然就是来看看源代码实现,同样为了简洁起见,不重要的部分被删除:

```c++

int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,char *envp_ext[])
{
    struct kobj_uevent_env *env;
    struct kobject *top_kobj;
	struct kset *kset;
	const struct kset_uevent_ops *uevent_ops;

    /* 如果当前 kobj->kset 没有设置,就向上找到 kobj->parent,再次检查是否 kset 被设置,直到找到一个可用的 kset
    ** 如果向上找不到所属的 kset,退出,函数执行终止并返回失败值. 
    */
    top_kobj = kobj;
	while (!top_kobj->kset && top_kobj->parent)
		top_kobj = top_kobj->parent;
    if (!top_kobj->kset) {
        return -EINVAL; 
    }
    /* 取出 kset->uevent_ops 回调函数结构. */
    kset = top_kobj->kset;
	uevent_ops = kset->uevent_ops;

    /* 如果当前 kobj 设置 kobj->uevent_suppress 为 true,表示不上传 ueven,返回. */
    if (kobj->uevent_suppress) {
        return 0;
    }
    /* 执行 uevent->filter 回调函数,该回调函数由用户实现,用于过滤一些不需要上传的 uevent,当 filter 返回 0 时,表示不上传,返回 */
    if (uevent_ops && uevent_ops->filter)
		if (!uevent_ops->filter(kset, kobj)) {
            return 0;
        }
    
    /* 获取 subsystem 名,这个 subsystem 名称正是需要传递到用户空间的 subsystem 的值 
    ** 如果设置了 name 回调函数,将 uevent->name 的返回值作为 subsystem 的值.
    ** 如果每设置 name 回调函数,使用 kset->kobj->name 作为 subsystem 的值.  
    */
    if (uevent_ops && uevent_ops->name)
		subsystem = uevent_ops->name(kset, kobj);
	else
		subsystem = kobject_name(&kset->kobj);

    /* 获取当前 kobject 在 sysfs 中的路径 */
    devpath = kobject_get_path(kobj, GFP_KERNEL);

    /* 将键值对填充到 env->buf 中,并使用 env->envp 指向对应的数据
    ** 并更新 env->buflen,env->envp_idx
    */
    retval = add_uevent_var(env, "ACTION=%s", action_string);
	retval = add_uevent_var(env, "DEVPATH=%s", devpath);
	retval = add_uevent_var(env, "SUBSYSTEM=%s", subsystem);

    /* 将传递进来的第三个参数 envp_ext 添加到 env->buf 中,用户可以自定义字符串消息内容 */
    if (envp_ext) {
		for (i = 0; envp_ext[i]; i++) {
			retval = add_uevent_var(env, "%s", envp_ext[i]);
		}
	}

    /* 调用开发者传入的 uevent_ops->uevent 回调函数,允许开发者对 env 中的字符串消息进行修改 */
    if (uevent_ops && uevent_ops->uevent) {
		retval = uevent_ops->uevent(kset, kobj, env);
	}
    /* 添加一个上报序列号 */
    add_uevent_var(env, "SEQNUM=%llu", (unsigned long long)++uevent_seqnum);

    ...

}
```
kobject_uevent_env 的前半部分主要是一些准备工作,分为两个部分:
* 找到当前 kobject 对应的 kset,获取 uevent_ops 结构.
* 检查 kobject->uevent_suppress 和 uevent_ops->filter,确定当前 uevent 是否被屏蔽,如果是,则返回.  
* 将需要发送到用户空间的字符串以键值对的方式组织起来,保存在 struct kobj_uevent_env 类型的 env 结构中.  

需要发送到用户空间的字符串消息已经准备完毕,接下来就是发送到用户空间的工作.  


分为两种方式:netlink 发送,和 call_userhelper.由内核宏进行控制,先看 netlink 方式的源码实现:

```c++
/* netlink 上报方式是否支持由内核宏 CONFIG_NET 控制. */
#if defined(CONFIG_NET)
    /* 遍历全局链表 uevent_sock_list, 其中保存的是 netlink 初始化时返回的 sock. */
	list_for_each_entry(ue_sk, &uevent_sock_list, list) {
		struct sock *uevent_sock = ue_sk->sk;
		struct sk_buff *skb;
		size_t len;
        /* 检查当前的 sock 是否有监听者,如果没有,就不使用这个 sock */
		if (!netlink_has_listeners(uevent_sock, 1))
			continue;

        /* 申请并填充一个 skb 结构,在内核中, netlink 的数据交互是通过 skb 结构完成的. */
		len = strlen(action_string) + strlen(devpath) + 2;
		skb = alloc_skb(len + env->buflen, GFP_KERNEL);
		if (skb) {
			char *scratch;

			scratch = skb_put(skb, len);
			sprintf(scratch, "%s@%s", action_string, devpath);
            /* 将上述准备好的数据一一拷贝到 skb 中 */
			for (i = 0; i < env->envp_idx; i++) {
				len = strlen(env->envp[i]) + 1;
				scratch = skb_put(skb, len);
				strcpy(scratch, env->envp[i]);
			}

			NETLINK_CB(skb).dst_group = 1;
            /* 将 skb 中的数据通过 netlink 进行广播 */
			retval = netlink_broadcast_filtered(uevent_sock, skb,0, 1, GFP_KERNEL,kobj_bcast_filter,kobj);
	}
#endif
```

对于 netlink 的发送,基本上就是调用内核的接口,使用比较简单,本章不对 netlink 的实现机制作详细讨论,关于 netlink 的使用可以参考我的另一篇博客:TODO.   


接下来我们来看看 call_userhelper 的实现方式:

```c++
/* call userhelper 的方式是否支持由内核宏 CONFIG_UEVENT_HELPER 控制 */
#ifdef CONFIG_UEVENT_HELPER
	/* uevent_helper 中保存的是用户空间的脚本路径名,为空表示没有设置用户空间脚本,自然不会执行.   
    */
	if (uevent_helper[0] && !kobj_usermode_filter(kobj)) {
		struct subprocess_info *info;
        /* 添加一系列环境参数 */
		retval = add_uevent_var(env, "HOME=/");
		retval = add_uevent_var(env,
					"PATH=/sbin:/bin:/usr/sbin:/usr/bin");

        /* 初始化 env->argv 参数:
        ** env->argv[0] = uevent_helper  //保存带路径的脚本名
        ** env->argv[1] = &env->buf[env->buflen];  //指向 env 参数
        ** env->argv[1] = NULL
        */
		retval = init_uevent_argv(env, subsystem);

        // 初始化 call userhelper
		info = call_usermodehelper_setup(env->argv[0], env->argv,env->envp, GFP_KERNEL,NULL, cleanup_uevent_env, env);

        /* 执行调用 */
        retval = call_usermodehelper_exec(info, UMH_NO_WAIT);
	}
#endif

```

关键的处理在于 call userhelper 的初始化和执行调用中,  

初始化部分:
```c++
struct subprocess_info *call_usermodehelper_setup(const char *path, char **argv,
		char **envp, gfp_t gfp_mask,
		int (*init)(struct subprocess_info *info, struct cred *new),
		void (*cleanup)(struct subprocess_info *info),
		void *data)
{
	struct subprocess_info *sub_info;
	sub_info = kzalloc(sizeof(struct subprocess_info), gfp_mask);
	if (!sub_info)
		goto out;

    /* 初始化一个 work 结构,用于工作队列 */
	INIT_WORK(&sub_info->work, call_usermodehelper_exec_work);

/* 如果内核配置了 CONFIG_STATIC_USERMODEHELPER,就使用内核配置中 CONFIG_STATIC_USERMODEHELPER_PATH 的路径
** 不可自行配置
*/
#ifdef CONFIG_STATIC_USERMODEHELPER
	sub_info->path = CONFIG_STATIC_USERMODEHELPER_PATH;
#else
	sub_info->path = path;
#endif
	sub_info->argv = argv;
	sub_info->envp = envp;

	sub_info->cleanup = cleanup;
	sub_info->init = init;
	sub_info->data = data;
  out:
	return sub_info;
}

```
初始化部分比较简单,主要有两个部分:
* 初始化一个 work,回调函数为 call_usermodehelper_exec_work,不难猜到,后续执行该 call userhelper时,就是执行该 work 实现相应的功能
* 构建一个 sub_info 结构,并通过传递进来的参数填充它,包括 环境参数 envp,执行路径 path 等.  

执行调用为 call_usermodehelper_exec:

```c++
int call_usermodehelper_exec(struct subprocess_info *sub_info, int wait)
{
    /* 初始化一个 wait 事件 */
    sub_info->complete = (wait == UMH_NO_WAIT) ? NULL : &done;
	sub_info->wait = wait;

    /* 将工作推入到工作队列, */
	queue_work(system_unbound_wq, &sub_info->work);
    /* 当前进程进入睡眠,等待执行完成 */
    retval = wait_for_completion_killable(&done);
    wait_for_completion(&done);
    ...
}
```

执行调用主要包含两个操作:
* 将初始化部分定义的工作加入到待执行工作队列,该工作将在不久的将来被执行,工作的回调函数 call_usermodehelper_exec_work 将被调用
* 进程进入睡眠,等待执行完成.

在工作的回调函数 call_usermodehelper_exec_work 中, 调用了 do_fork 和 do_execve,这和用户空间调用 fork 和 exec 函数族类似,只是用户空间调用还需要触发系统调用间接地调用到 do_fork 和 do_execve,关于进程的创建与执行这里就不再过多讨论了.  

call userhelper 的方式执行 uevent 是比较少见的,尽管在很多平台上,call userhelper 机制是默认使能的,但是它是否被执行取决于一些内核配置:
* CONFIG_UEVENT_HELPER:如果这个宏没有被使能,整个 call userhelper 机制被禁止.   
* CONFIG_UEVENT_HELPER_PATH:这个宏是一个带路径的文件名,表示默认的用户空间脚本.通常是 /sbin/hotplug 或者为空.  
* CONFIG_STATIC_USERMODEHELPER:如果这个内核选项被配置,表示使用内核编译时指定的脚本,同时需要配置 CONFIG_STATIC_USERMODEHELPER_PATH ,这个宏就是带路径的脚本名,这种静态配置不再接受用户的配置,除非特殊情况才会使用,因为在系统的启动阶段,会产生大量的 uevent,如果每个 uevent 都要创建一个进程执行,会导致启动过程非常缓慢,甚至有系统资源耗尽的风险.一般来说,即使使用 call userhelper 的机制处理 uevent,在系统启动阶段不会使能,而是在系统起来之后动态地进行配置.  
* 可以通过读写 /sys/kernel/uevent_helper 的方式动态地指定用户空间的 call userhelper 脚本.该文件接口的内核实现在 kernel/ksysfs.c 中.   


如果你对系统调用过程和程序加载过程有兴趣的话,可以参考这一些博客:TODO TODO.  

内核中 uevent 的处理就到此结束了,内核通过 netlink 将信息发到用户空间,那么用户空间是怎么接收并处理的呢?在后面的文章中我们将继续讨论udev 的实现.  

