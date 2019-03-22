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



dup的作用就是复制一个文件描述符，两个文件描述符(fd)对应同一个struct file，所以有相同的文件操作函数和文件偏移地址，往一个文件中写入时，文件偏移地址的变化会反应到另一个fd上。  




dup不能实现重定向是因为无法指定fd

dup2可以实现重定向
fcntl也可以做到修改文件fd


当文件打开时，内核分配一个fd，对应一个struct file结构体，

文件描述符  fd到struct的对应表：

    struct task_struct *current  ->
        struct files_struct	*files ->
            struct fdtable *fdt  ->
                struct file __rcu **fd;
这里的fd是一个指针数组，每一个指针都对应一个被打开的文件，即一个struct file结构体，fd就是文件的下标。  

对应一个fd，直接可以使用current->files->fdt->fd[fd]获取struct file结构体指针。  

struct file主要是对打开文件的描述信息。 
struct file {
    ...
	union {
		struct llist_node	fu_llist;
		struct rcu_head 	fu_rcuhead;
	} f_u;
	struct path		f_path;                             //对应的路径
	struct inode		*f_inode;	                    //对应的i_node
	const struct file_operations	*f_op;              //文件操作函数 
	loff_t			f_pos;                              //文件偏移地址
    ...
} __randomize_layout __attribute__((aligned(4)));	/* lest something weird decides that 2 is OK */



在调用open打开文件的时候，在内核中就会构建一个struct dentry结构体。  
struct dentry直接由传入的文件路径获取。从而从struct dentry获取相应的struct  inode。

struct dentry的作用是索引文件在内核文件树中的位置。  
struct dentry {
	/* RCU lookup touched fields */
	unsigned int d_flags;		/* protected by d_lock */
	seqcount_t d_seq;		/* per dentry seqlock */
	struct hlist_bl_node d_hash;	/* lookup hash list */
	struct dentry *d_parent;	/* parent directory */
	struct qstr d_name;
	struct inode *d_inode;		/* Where the name belongs to - NULL is
					 * negative */
	struct list_head d_child;	/* child of parent list */
	struct list_head d_subdirs;	/* our children */
	
} __randomize_layout;

struct dentry 由链表组织或者以树状结构组织。  




一个struct inode可能对应多个struct dentry，因为一个文件很可能在多个目录中存在引用。  

struct inode结构体对应具体的文件，根据struct inode可以查看到文件的内部信息，ls-l查看到的信息基本上就是属于struct inode。  

包括文件权限、修改时间、属主、大小、占用的block等等。  

struct inode {
	umode_t			i_mode;
	unsigned short		i_opflags;
	kuid_t			i_uid;
	kgid_t			i_gid;
	unsigned int		i_flags;

	const struct inode_operations	*i_op;
	struct super_block	*i_sb;
	struct address_space	*i_mapping;

	/* Stat data, not accessed from path walking */
	unsigned long		i_ino;

	union {
		const unsigned int i_nlink;
		unsigned int __i_nlink;
	};
	dev_t			i_rdev;
	loff_t			i_size;
	struct timespec		i_atime;
	struct timespec		i_mtime;
	struct timespec		i_ctime;
	spinlock_t		i_lock;	/* i_blocks, i_bytes, maybe i_size */
	unsigned short          i_bytes;
	unsigned int		i_blkbits;
	enum rw_hint		i_write_hint;
	blkcnt_t		i_blocks;

	unsigned long		i_state;
	struct rw_semaphore	i_rwsem;

	unsigned long		dirtied_when;	/* jiffies of first dirtying */
	unsigned long		dirtied_time_when;

	struct hlist_node	i_hash;
	struct list_head	i_io_list;	/* backing dev IO list */
    
    struct list_head	i_lru;		/* inode LRU list */
	struct list_head	i_sb_list;
	struct list_head	i_wb_list;	/* backing dev writeback list */
	union {
		struct hlist_head	i_dentry;
		struct rcu_head		i_rcu;
	};
	u64			i_version;
	atomic_t		i_count;
	atomic_t		i_dio_count;
	atomic_t		i_writecount;

	const struct file_operations	*i_fop;	/* former ->i_op->default_file_ops */
	struct file_lock_context	*i_flctx;
	struct address_space	i_data;
	struct list_head	i_devices;
	union {
		struct pipe_inode_info	*i_pipe;
		struct block_device	*i_bdev;
		struct cdev		*i_cdev;
		char			*i_link;
		unsigned		i_dir_seq;
	};

	__u32			i_generation;

	void			*i_private; /* fs or device private pointer */
} __randomize_layout;



进程打开文件描述符的各种方式的区别：
1、使用dup、dup2：分配不同的描述符，但是对应同一个struct file，最直观的现象就是两个文件描述符共享同一个文件偏移。  
2、同一个进程中使用两次open打开同一个文件，返回两个fd分别对应两个struct file，所以通过两个fd对文件的读写是独立的，但是两个文件对应同一个struct inode。  

3、 在不同的进程中，分两次打开文件描述符







同一个进程创建两个文件的代码。
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

	
	 nfd = open("mmap_device",O_RDWR|O_CREAT,0666);
    printf("nfd = %d\r\n",nfd);

    write(nfd,"downey",6);
    write(fd,"huangdao1234\n",12);
    close(fd);
    write(nfd,"ff",2);
	
	close(nfd);
	return 0;
}