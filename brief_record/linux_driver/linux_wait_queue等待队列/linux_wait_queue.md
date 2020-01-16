# linux 等待队列
在 linux 中，进程的调度都是由内核来完成，而在内核中，控制进程调度使用得最频繁的接口莫过于等待队列了。  


## 等待资源的方式
当一个进程需要请求某项资源而无法满足要求时，通常需要等待该资源才能继续运行，而等待资源一般分两种方式：轮询和睡眠。  

在不同情况下使用这两种方式本质上都是出于效率的考量，在常规的印象中，睡眠等待肯定是比轮询效率更高，因为它的在等待的过程中将系统资源让给其他进程在运行。从宏观上来讲，这个说法是正确的，但是情况往往比单纯的想象要复杂。  

### 轮询
首先，中断中不能睡眠，它并不涉及到任何进程的上下文，进程的睡眠唤醒在中断中也就无从说起。  

同时，事实上并不是在所有情况下睡眠都比轮询要好，当等待的资源可以确认在很短的事件内返回时，可以使用轮询机制。通常，这个很短的事件通常要比一次系统的进程调度开销要小，在这种情况下，调度一次进程倒不如占着 CPU 就地等待，虽然这种实现看起来不够优美。这种实现的典型代表就是内核中的 spinlock，自旋锁。  

### 睡眠
进程向内核请求资源导致睡眠的情况也有多种，比较典型的场景是作用于同步和数据请求。  

信号量、互斥锁这一类的，专门用于进程之间的协调与同步。   

话说回来，Linux 内核是一个服务层，向用户空间提供系统服务，所以更多的需求通常是一些数据 IO 的交互，比如最常见的阻塞型 read、write 接口与内核的交互，通常这一类
接口的数据同步在内核中由等待队列来实现。  


## 等待队列  
等待队列的底层实现是链表，需要初始化一个链表头，然后将需要等待资源的进程挂在这个链表中。  

当我们排队买奶茶的时候，店家(生产者)的生产速度跟不上请求的速度，于是每一个想要买奶茶的人先到前台领号，找个地方坐下等。当店家做好一杯之后，广播中响起了叫号声，每个手持号码的人将自己的号和广播中的叫号对比，如果不是自己的，继续低头玩手机或者做其他事，如果是自己的号，就到柜台领奶茶，将小票扔到垃圾桶。  

有时候你会发现这个世界是多么的奇妙，等待队列的机制居然和买奶茶几乎一样，当新进来的请求资源进程过多导致当前的资源无法满足时，维护一个等待队列，所有进程在这个链表上注册并带着唤醒条件等待。当系统生产出一份等待队列中进程需要的资源时，就将整个队列中的进程临时唤醒，每个唤醒的进程查看这份资源是不是自己的，如果是，就从等待队列中退出，心满意足地带着获取到的资源进行下一步动作。如果不是，那就继续睡眠，接着等。  


## 进程的运行与休眠
每一个进程都被标记为多种状态，抛开那些繁杂的细节，我们真正关心的就是两种状态：
* 可运行状态
* 非可运行状态

内核维护一个就绪队列，所有处于可运行状态的进程都在该队列中等待进程的调度，系统会根据调度策略来选取下一个可以投入运行的进程，而一个进程独占CPU运行太长时间通常是不可能的，一个进程处于可运行状态表示它将在不久的将来(通常是接下来的数次调度中，ms级别)将会被投入运行。  

非可运行状态通常就是睡眠，或者退出。一个进程状态的切换只需要两个简单的接口：

```C
set_current_state(TASK_INTERRUPTIBLE);
schedule()
```
TASK_INTERRUPTIBLE 表示可接收信号的休眠状态，这个参数表示需要设置的进程状态，设置状态可以是其他值，这些值在 include/linux/sched.h中定义，有 TASK_RUNNING/TASK_INTERRUPTIBLE/TASK_UNINTERRUPTIBLE 等，分别代表着进程不同的状态。  

而 schedule 函数将启动系统调度器，由系统根据具体的调度算法在就绪队列中选择合适的进程运行。  

但是，如果直接这样进行进程的调度，并不利于进程的管理，等待队列本质上就是在其基础上进行封装，加上一个由链表操作为核心的管理层。  

## 等待队列接口

### 等待队列头
等待队列头的结构体为 struct wait_queue_head：

```C
struct wait_queue_head {
	spinlock_t		lock;
	struct list_head	head;
};
```
和普通链表节点不一样的是，等待队列头和节点是两个不同的结构，等待队列头结点包含两个成员：自旋锁和链表节点。其中，自旋锁主要是防止多进程同时操作该链表时产生竞态而导致数据混乱。  

### 等待队列节点
等待队列节点的结构体由 struct wait_queue_entry 来描述：

```C
struct wait_queue_entry {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head	entry;
};
```
节点由四个成员组成
* flags：表示当前节点的状态，也就是需要设置的进程状态，也可能是状态组合。
* private：私有数据成员，通常用来保存进程的进程控制块，也就是一个进程的 task_struct 结构。
* func：对应的唤醒函数，回调函数，在系统唤醒该节点对应的进程时被调用。
* entry：链表节点，用于挂在链表中。  

### 加入等待队列接口
常用的将进程加入等待队列的函数调用实现主要有以下几个函数：
* wait_event(wq_head, condition):将当前运行进程加入到等待队列 wq_head 中，并设置 condition 条件，当系统唤醒该进程时，必须同时满足 condition 为 true，进程才会继续执行。
* wait_event_timeout(wq_head, condition, timeout)：在 wait_event 的基础上设置超时时间，当超过 timeout 而 condition 不为 true 时也会唤醒该进程，并继续运行。  
* wait_event_interruptible(wq_head, condition)：在 wait_event 的基础上响应信号的处理，即使在睡眠状态，当该进程有信号传来时，响应信号并处理。
* wait_event_interruptible_timeout(wq_head, condition, timeout)：在 wait_event 的基础上增加超时和信号处理响应，也就是在接收到信号之后，唤醒进程并去处理。  

### 唤醒等待队列接口
常见的唤醒等待队列接口实现主要有以下几个函数：
wake_up(x)：传入的参数 x 为当前等待队列头节点，唤醒队列上因为调用 wait_event 接口陷入休眠的进程。
wake_up_all(x):唤醒等待队列上所有的进程。
wake_up_interruptible(x):传入的参数 x 为当前等待队列头节点，唤醒队列上因为调用 wait_event_interruptible 接口陷入休眠的进程。  


### 等待队列示例
讲解完接口的使用，自然是需要编写一个对应的驱动程序才能加深对接口的理解，下面是一个等待队列的示例：

```
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/timer.h>

MODULE_AUTHOR("Downey");
MODULE_LICENSE("GPL");

int majorNumber;
dev_t dev_id;
struct cdev basic_cdev;
struct class *basic_class;
struct device *basic_device;

struct wait_queue_head wq;

struct basic_struct{
        struct timer_list timer;
        int flag;
};
int flag = 0;
static const char *CLASS_NAME = "basic_class";
static const char *DEVICE_NAME = "basic_demo";


void timer_cb(unsigned long data)
{

    printk("timer_callback\n");
    struct basic_struct *basic = (struct basic_struct*)data;
    if(NULL == basic) {
        printk("basic = null!\n");

        return;
    }
	//flag 被设置为唤醒的 condition，在这里置位1。
    flag = 1;
	//定时回调函数，唤醒队列上所有进程
    wake_up(&wq);
}

static int basic_open(struct inode *node, struct file *file)
{
    printk("open!\n");
    struct basic_struct *basic = kmalloc(sizeof(struct basic_struct),GFP_KERNEL);
    if(IS_ERR(basic))
        return -ENOMEM;
    flag = 0;

    init_timer(&basic->timer);
    basic->timer.function = timer_cb;
    basic->timer.expires = jiffies + 3 * HZ;
    basic->timer.data = (unsigned long)basic;
    add_timer(&basic->timer);


    file->private_data = basic;

    return 0;
}

static ssize_t basic_read(struct file *file,char *buf, size_t len,loff_t *offset)
{

    struct basic_struct *basic = (struct basic_struct*)file->private_data;
    if(flag != 1){
            printk("wait for flag!\n");
			//调用 wait_event 等待 flag 被置1。
            wait_event(wq,flag);
            if(1 == flag){
                flag = 0;
                printk("wait for flag over!\n");
            }
    }
    return 0;
}

static ssize_t basic_write(struct file *file,const char *buf,size_t len,loff_t *offset)
{
    return len;
}

static int basic_release(struct inode *node,struct file *file)
{
    printk("close!\n");
    struct basic_struct *basic = (struct basic_struct*)file->private_data;

    del_timer(&basic->timer);
    kfree(basic);
    return 0;
}

static struct file_operations file_oprts =
{
    .open = basic_open,
    .read = basic_read,
    .write = basic_write,
    .release = basic_release,
};


static int __init basic_init(void)
{
    int ret;
    printk(KERN_ALERT "Driver init\r\n");
    ret = alloc_chrdev_region(&dev_id,0,2,DEVICE_NAME);
    if(ret){

        printk(KERN_ALERT "Alloc chrdev region failed!!\r\n");
        return -ENOMEM;
    }
    majorNumber = MAJOR(dev_id);
    cdev_init(&basic_cdev,&file_oprts);
    ret = cdev_add(&basic_cdev,dev_id,2);

    basic_class = class_create(THIS_MODULE,CLASS_NAME);
    basic_device = device_create(basic_class,NULL,MKDEV(majorNumber,0),NULL,DEVICE_NAME);
    printk(KERN_INFO "major = %d,mino = %d\n",MAJOR(dev_id),MINOR(dev_id));
    printk(KERN_ALERT "Basic device init success!!\r\n");

    init_waitqueue_head(&wq);
    return 0;
}

static void __exit basic_exit(void)
{
    device_destroy(basic_class,MKDEV(majorNumber,0));
    class_unregister(basic_class);
    class_destroy(basic_class);
    cdev_del(&basic_cdev);
    unregister_chrdev_region(dev_id,2);
}

module_init(basic_init);
module_exit(basic_exit);

```
在该示例中，演示了下列的流程：
* 在 /dev/ 目录下创建一个设备节点，名为 /dev/basic_demo
* 在 open 函数中创建一个定时器，在启动后 3s 到时，并执行回调函数，该回调函数将 flag 置1，并唤醒队列.
* 在 read 函数中调用 wait_event() condition 为 flag ，即如果要唤醒该进程，就需要调用 wake_up() 系列函数同时 flag 为 1。

有一个细节部分需要注意的是，对每一个进程，对应一个单独的 open 函数，也就是每个进程在访问时都会使用一个独立的定时器。   

而 flag 和等待队列头是全局变量，事实上，通常情况下，除了等待队列头，每个进程都应该单独有一份数据，包括它的condition，而这里把 condition(flag)设置为全局变量只是为了提供示例的说明效果，继续见下文。  

### 实验现象和方法
编译并加载该内核，输出调试信息：

```
Jan 15 15:15:48 : [ 2630.572425] Driver init
Jan 15 15:15:48 : [ 2630.580785] major = 241,mino = 0
Jan 15 15:15:48 : [ 2630.580801] Basic device init success!
```

使用 cat 指令读取文件, cat /dev/basic_demo ,输出为:

```
Jan 15 15:18:24 : [ 2786.343255] open!
Jan 15 15:18:24 : [ 2786.343355] wait for flag!

Jan 15 15:18:27 : [ 2789.536545] timer_callback
Jan 15 15:18:27 : [ 2789.536687] wait for flag over!
Jan 15 15:18:27 : [ 2789.536853] close!
```
可以看到，open 到 close 的过程间隔了 3s，也就是定时回调被调用时，用户进程才得以返回，否则终端显示的 cat /dev/basic_demo 就一直处于阻塞状态。   


编写内核驱动的时候，我们一定要注意一个问题，就是你编写的代码是服务程序，通常会存在多个用户进程访问该驱动，所以，我们就同时打开两个终端，先后每个终端先后调用 cat /dev/basic_demo，它的输出是这样的：


```
//终端1打开
Jan 15 15:24:37 : [ 3159.672634] open!
Jan 15 15:24:37 : [ 3159.672736] wait for flag!
//终端2打开
Jan 15 15:24:40 : [ 3162.072924] open!
Jan 15 15:24:40 : [ 3162.073026] wait for flag!

//终端1唤醒
Jan 15 15:24:40 : [ 3162.814158] timer_callback
Jan 15 15:24:40 : [ 3162.814297] wait for flag over!
Jan 15 15:24:40 : [ 3162.814469] close!
//终端2唤醒
Jan 15 15:24:43 : [ 3165.118344] timer_callback
Jan 15 15:24:43 : [ 3165.118489] wait for flag over!
Jan 15 15:24:43 : [ 3165.118646] close!

```
结果也在预料之内，因为打开了两个终端，所以会创建两个定时器，执行两次定时回调函数，但是这里有个问题：两个进程都在同一个等待队列上进行睡眠，而且 condition 也是同一个(flag)，为什么每一次唤醒都只唤醒了一个？  

事实上，在上面的示例中，理论上进入到定时回调函数中时，wake_up 将会先后唤醒两个进程，同时将 flag 置1，但是在 basic_read 函数中，唤醒第一个进程之后又将 flag 置为 0，所以导致第二个进程被唤醒之后又因为 condition 为 false 又进入睡眠。   

如果你将 basic_read 中的 flag = 0 这条语句去掉，会发现两个进程将会在第一个定时回调中都被唤醒。  


同时，你也可以修改上面的驱动程序，将上述的 wait_event 修改为 wait_event_interruptible ,通过重新编译加载之后，同样在应用层调用 cat /dev/basic_demo，终端显示如下：
```
Jan 15 15:24:37 : [ 3159.672634] open!
Jan 15 15:24:37 : [ 3159.672736] wait for flag!

Jan 15 15:24:40 : [ 3162.814158] timer_callback
``` 
等待 3s 之后，进入了定时回调函数，但是进程并没有被唤醒，因为 wake_up 并不能唤醒 wait_event_interruptible 睡眠的进程，此时只能在终端键入 CTRL+C 来结束这个进程。  

但是如果你使用 wait_event 将进程放在等待队列，而使用 wake_up_interruptible 来唤醒，你会发现你不仅不能唤醒进程，连 CTRL+C 也没反应，因为 wait_event 设置的睡眠是无法接受信号的。   

wake_up_all 则可以唤醒等待队列上所有的进程。  



```C
//××××××××××××××××××××××××××××××××唤醒函数所做的事情×××××××××××××××
//唤醒每个等待队列上的进程，并从等待队列中删除
int default_wake_function(wait_queue_entry_t *curr, unsigned mode, int wake_flags,
			  void *key)
{
	return try_to_wake_up(curr->private, mode, wake_flags);
}


int autoremove_wake_function(struct wait_queue_entry *wq_entry, unsigned mode, int sync, void *key)
{
	int ret = default_wake_function(wq_entry, mode, sync, key);

	if (ret)
		list_del_init(&wq_entry->entry);
	return ret;
}

```

```C

static inline int signal_pending_state(long state, struct task_struct *p)
{
	if (!(state & (TASK_INTERRUPTIBLE | TASK_WAKEKILL)))
		return 0;
	//没有信号处理的时候走这个分支。  
	if (!signal_pending(p))
		return 0;
	//有信号处理的时候走这个分支。  
	return (state & TASK_INTERRUPTIBLE) || __fatal_signal_pending(p);
}

long prepare_to_wait_event(struct wait_queue_head *wq_head, struct wait_queue_entry *wq_entry, int state)
{
	unsigned long flags;
	long ret = 0;
	//轮询检查是否有信号要处理，如果没有，返回0，如果有，返回1
	if (unlikely(signal_pending_state(state, current))) {
		
		list_del_init(&wq_entry->entry);
		//进而返回错误码，通知上层跳出进程睡眠，被唤醒
		ret = -ERESTARTSYS;
		
	}
	//否则，将当前节点添加到等待队列中，并设置进程状态为用户传入的设置状态。 
	else {
		if (list_empty(&wq_entry->entry)) {
			if (wq_entry->flags & WQ_FLAG_EXCLUSIVE)
				__add_wait_queue_entry_tail(wq_head, wq_entry);
			else
				__add_wait_queue(wq_head, wq_entry);
		}
		set_current_state(state);
	}

	return ret;
}
```

```C
#define ___wait_event(wq_head, condition, state, exclusive, ret, cmd)		\
({										\
	__label__ __out;							\
	struct wait_queue_entry __wq_entry;					\
	long __ret = ret;	/* explicit shadow */				\
										\
    //初始化 wait_entry 节点，故名思义，就是 wait_queue 中的节点
	//设置flag、设置当前进程task_struct 为private，设置唤醒的回调函数，在需要唤醒时调用，wq_entry->func = autoremove_wake_function;

	init_wait_entry(&__wq_entry, exclusive ? WQ_FLAG_EXCLUSIVE : 0);	\
	for (;;) {	
		//准备进入等待队列							\
		long __int = prepare_to_wait_event(&wq_head, &__wq_entry, state);\
										\
		if (condition)							\
			break;							\
										\
		if (___wait_is_interruptible(state) && __int) {			\
			__ret = __int;						\
			goto __out;						\
		}								\
										\
		cmd;								\
	}									\
	finish_wait(&wq_head, &__wq_entry);					\
__out:	__ret;									\
})
```




```C
#define __wait_event_interruptible(wq_head, condition)				\
	___wait_event(wq_head, condition, TASK_INTERRUPTIBLE, 0, 0,		\
		      schedule())

#define wait_event_interruptible(wq_head, condition)				\
({										\
	int __ret = 0;								\
	might_sleep();								\
	if (!(condition))							\
		__ret = __wait_event_interruptible(wq_head, condition);		\
	__ret;									\
})

```



```C

// 如果 condition 为 1 ，或者 __ret 等于 0 ，返回 1
// 如果 condition 为 1，同时 __ret 等于 0，将 __ret 置为 1，表示 timeout 为 1.
#define ___wait_cond_timeout(condition)						\
({										\
	bool __cond = (condition);						\
	if (__cond && !__ret)							\
		__ret = 1;							\
	__cond || !__ret;							\
})



//调用 ___wait_event
//传入的参数中 TASK_UNINTERRUPTIBLE、schedule_timeout(__ret) 需要关注
//

#define __wait_event_timeout(wq_head, condition, timeout)			\
	___wait_event(wq_head, ___wait_cond_timeout(condition),			\
		      TASK_UNINTERRUPTIBLE, 0, timeout,				\
		      __ret = schedule_timeout(__ret))


//如果 __wait_cond_timeout 不为0，直接返回 timeout
// 否则调用 __wait_event_timeout。
#define wait_event_timeout(wq_head, condition, timeout)				\
({										\
	long __ret = timeout;							\
	might_sleep();								\
	if (!___wait_cond_timeout(condition))					\
		__ret = __wait_event_timeout(wq_head, condition, timeout);	\
	__ret;									\
})

```







```C

signed long __sched schedule_timeout(signed long timeout)
{
	struct timer_list timer;
	unsigned long expire;

	switch (timeout)
	{
	case MAX_SCHEDULE_TIMEOUT:
		/*
		 * These two special cases are useful to be comfortable
		 * in the caller. Nothing more. We could take
		 * MAX_SCHEDULE_TIMEOUT from one of the negative value
		 * but I' d like to return a valid offset (>=0) to allow
		 * the caller to do everything it want with the retval.
		 */
		schedule();
		goto out;
	default:
		/*
		 * Another bit of PARANOID. Note that the retval will be
		 * 0 since no piece of kernel is supposed to do a check
		 * for a negative retval of schedule_timeout() (since it
		 * should never happens anyway). You just have the printk()
		 * that will tell you if something is gone wrong and where.
		 */
		if (timeout < 0) {
			printk(KERN_ERR "schedule_timeout: wrong timeout "
				"value %lx\n", timeout);
			dump_stack();
			current->state = TASK_RUNNING;
			goto out;
		}
	}

	expire = timeout + jiffies;

	setup_timer_on_stack(&timer, process_timeout, (unsigned long)current);
	__mod_timer(&timer, expire, false);
	schedule();
	del_singleshot_timer_sync(&timer);

	/* Remove the timer from the object tracker */
	destroy_timer_on_stack(&timer);

	timeout = expire - jiffies;

 out:
	return timeout < 0 ? 0 : timeout;
}


#define __wait_event_timeout(wq_head, condition, timeout)			\
	___wait_event(wq_head, ___wait_cond_timeout(condition),			\
		      TASK_UNINTERRUPTIBLE, 0, timeout,				\
		      __ret = schedule_timeout(__ret))

#define wait_event_timeout(wq_head, condition, timeout)				\
({										\
	long __ret = timeout;							\
	might_sleep();								\
	if (!___wait_cond_timeout(condition))					\
		__ret = __wait_event_timeout(wq_head, condition, timeout);	\
	__ret;									\
})
```


