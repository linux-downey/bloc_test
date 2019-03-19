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
            char* ptr = (char*)mmap(NULL,4096,PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
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
            char* ptr = (char*)mmap(NULL,4096,PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
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
需要注意的是，mmap()的操作并不会扩展文件的大小，如果是新创建的文件，文件长度为0，往映射内存的部分写东西将会到值Bus_error.
创建文件时，需要先把文件扩大。
使用ftruncate(fd,length);来对文件进行扩展，成功返回0，失败返回-1。  


使用msync()函数来将当前写入的数据更新到物理空间中。 

mmap调用流程在syscalls.c中，
    
    SYSCALL_DEFINE6(mmap, unsigned long, addr, size_t, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, off_t, offset)





    #include <linux/init.h>  
    #include <linux/module.h>
    #include <linux/device.h>  
    #include <linux/kernel.h>  
    #include <linux/fs.h>
    #include <linux/uaccess.h>
    #include <linux/string.h>
    #include <linux/mm_types.h>
    #include <linux/mm.h>
    #include <linux/slab.h>
    #include <linux/string.h>


    MODULE_AUTHOR("Downey");
    MODULE_LICENSE("GPL");

    static int majorNumber = 0;
    static const char *CLASS_NAME = "mmap_class";
    static const char *DEVICE_NAME = "mmap_device";

    static int mmap_open(struct inode *node, struct file *file);
    static int my_mmap(struct file *filp, struct vm_area_struct *vma);
    static int mmap_release(struct inode *node,struct file *file);

    static struct class *mmap_class = NULL;
    static struct device *mmap_device = NULL;

    static struct file_operations file_oprts = 
    {
        .open = mmap_open,
        .mmap = my_mmap,
        .release = mmap_release,
    };

    static char* buffer = NULL;
    #define BUF_SIZE 4096



    static int mmap_open(struct inode *node, struct file *file)
    {
        struct mm_struct *mm = current->mm;
        
        printk("client: %s (%d)\n", current->comm, current->pid);
        printk("code  section: [0x%lx   0x%lx]\n", mm->start_code, mm->end_code);
        printk("data  section: [0x%lx   0x%lx]\n", mm->start_data, mm->end_data);
        printk("brk   section: s: 0x%lx, c: 0x%lx\n", mm->start_brk, mm->brk);
        printk("mmap  section: s: 0x%lx\n", mm->mmap_base);
        printk("stack section: s: 0x%lx\n", mm->start_stack);
        printk("arg   section: [0x%lx   0x%lx]\n", mm->arg_start, mm->arg_end);
        printk("env   section: [0x%lx   0x%lx]\n", mm->env_start, mm->env_end);

        printk(KERN_ALERT "GPIO init \n");
        return 0;
    }


    static int my_mmap(struct file *filp, struct vm_area_struct *vma)
    {
        unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;
        unsigned long pfn_start = (virt_to_phys(buffer) >> PAGE_SHIFT + vma->vm_pgoff);
        unsigned long virt_start = (unsigned long)buffer + offset;
        unsigned long size = vma->vm_end - vma->vm_start;
        char *ptr = "huangdao!\n";

        printk(KERN_INFO "VM page off = 0x%x\r\n",vma->vm_pgoff);
        printk(KERN_INFO "PAGE_SHIFT = 0x%x\r\n",PAGE_SHIFT);
        printk(KERN_INFO "virt_start = 0x%x\r\n",virt_start);
        printk(KERN_INFO "size = 0x%x\r\n",size);

        int ret = 0;

        printk("phy: 0x%lx, offset: 0x%lx, size: 0x%lx\n", pfn_start << PAGE_SHIFT, offset, size);
        ret = remap_pfn_range(vma, vma->vm_start, pfn_start, size, vma->vm_page_prot);
        if (ret)
            printk("%s: remap_pfn_range failed at [0x%lx  0x%lx]\n",__func__, vma->vm_start, vma->vm_end);
        else
            printk("%s: map 0x%lx to 0x%lx, size: 0x%lx\n", __func__, virt_start,vma->vm_start, size);

        memcpy(buffer,ptr,strlen(ptr));

        return ret;
    }

    /*当用户打开设备文件时，调用这个函数*/
    static int mmap_release(struct inode *node,struct file *file)
    {
        printk(KERN_INFO "Release!!\n");
        return 0;
    }



    static int mymmap_init(void)
    {
        printk(KERN_ALERT "Driver init\r\n");
        /*注册一个新的字符设备，返回主设备号*/

        buffer = kzalloc(BUF_SIZE,GFP_KERNEL);
        printk(KERN_INFO "Alloc buffer virtual address = 0x%x\r\n",(unsigned int)buffer);
        printk(KERN_INFO "Alloc buffer pysical address = 0x%x\r\n",(unsigned int)virt_to_phys(buffer));
        printk(KERN_INFO "Length of int =  %d\r\n",sizeof(int));
        if(NULL == buffer){
            printk(KERN_INFO "Alloc memory failed\r\n");
            return -ENOMEM;
        }

        majorNumber = register_chrdev(0,DEVICE_NAME,&file_oprts);
        if(majorNumber < 0 ){
            printk(KERN_ALERT "Register failed!!\r\n");
            return majorNumber;
        }
        printk(KERN_ALERT "Registe success,major number is %d\r\n",majorNumber);

        /*以CLASS_NAME创建一个class结构，这个动作将会在/sys/class目录创建一个名为CLASS_NAME的目录*/
        mmap_class = class_create(THIS_MODULE,CLASS_NAME);
        if(IS_ERR(mmap_class))
        {
            unregister_chrdev(majorNumber,DEVICE_NAME);
            return PTR_ERR(mmap_class);
        }

        mmap_device = device_create(mmap_class,NULL,MKDEV(majorNumber,0),NULL,DEVICE_NAME);
        if(IS_ERR(mmap_device))
        {
            class_destroy(mmap_class);
            unregister_chrdev(majorNumber,DEVICE_NAME);
            return PTR_ERR(mmap_device);
        }
        printk(KERN_ALERT "mmap device init success!!\r\n");

        return 0;
    }


    /*销毁注册的所有资源，卸载模块，这是保持linux内核稳定的重要一步*/
    static void  mymmap_exit(void)
    {
        device_destroy(mmap_class,MKDEV(majorNumber,0));
        class_unregister(mmap_class);
        class_destroy(mmap_class);
        unregister_chrdev(majorNumber,DEVICE_NAME);
    }


    module_init(mymmap_init);
    module_exit(mymmap_exit);



在内存映射中，最关键的一个函数就是remap_pfn_range()，
int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
		    unsigned long pfn, unsigned long size, pgprot_t prot)。  

第一个参数 vma是映射到用户空间的虚拟地址，vma->vm_start虚拟地址的映射开始处。   
第二个参数，addr，就是虚拟地址映射的开始处，等于vma->vm_start.  
第三个参数，pfn，是被映射到用户空间的物理地址开始页。
第四个参数，size，大小
第五个参数，prot，映射内存的保护方式。

计算映射地址对应内核的虚拟空间：被映射的内核虚拟地址起始+offset>>page_size，以页为单位映射的。  


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
        int pid = 0;
        int pid1= 0;
        //signal(SIGCHLD,SIG_IGN);
        
        int fd = open("/dev/mmap_device",O_RDWR);
        printf("fd = %d\r\n",fd);
        char* ptr = (char*)mmap(NULL,4096,PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
        if(ptr == NULL) printf("fuck\r\n");
        printf("0x%x\r\n",ptr);
        while(1)
        {
            printf("Buffer data = %s\r\n",ptr);
            sleep(1);
            //write(fd,"huangdao\n",10);
            
        }
        close(fd);
        return 0;
    }


