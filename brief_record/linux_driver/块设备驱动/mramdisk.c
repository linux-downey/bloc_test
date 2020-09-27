#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/cpumask.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/kdev_t.h>
#include <linux/fs_struct.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/file.h>

#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/hdreg.h>



typedef struct {
    unsigned char *data;
    struct request_queue *queue;
    struct gendisk *gd;
    spinlock_t lock; // globol data opertion need to be protected.ignore in this example.
}priv_data_t;

#define RAM_SIZE (1024 * 1024 * 10)   //10M
#define MINORS        4               //How many partitions does this disk support.for alloc_disk
#define RAMDIS_NAME  "ramdisk-test"
#define SECTOR_SIZE 512

static DEFINE_SPINLOCK(ramdisk_lock);

static unsigned char *ram_start_addr;
static priv_data_t *dev_data;
static dev_t blk_major;

static int space_init(void)
{
    ram_start_addr = (unsigned char*)vmalloc(RAM_SIZE);
		if(!ram_start_addr){
            return -ENOMEM;
        }
    return 0;
}

static int priv_data_init(void)
{
    dev_data = kmalloc(sizeof(*dev_data),GFP_KERNEL);
    if(!dev_data){
        return -ENOMEM;
    }
    return 0;
}

static void priv_data_exit(void)
{
    kfree(dev_data);
}

static void space_exit(void)
{
    vfree(ram_start_addr);    
}

int ramdisk_open(struct block_device *bdev,fmode_t mode)
{
    return 0;
}

void ramdisk_release(struct gendisk *gd,fmode_t mode)
{
}

int ramdisk_ioctl(struct block_device *bdev,fmode_t mode,unsigned int cmd,unsigned long arg)
{
    // int err;
    // struct hd_geometry geo;
    
    // switch(cmd){
    //     case HDIO_GETGEO:
    //         err = !access_ok(VERIFY_WRITE,arg,sizeof(geo));
    //         if(err) return -EFAULT;
    //         geo.cylinders = 256;

    //     return 0;
    // }
    return 0;
}
static int ramdisk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct hd_geometry *p = geo;
	bdev = bdev;
	p->cylinders = 1;
	p->heads = 1;
	p->sectors = RAM_SIZE/SECTOR_SIZE;
	return 0;
}
/*
static int ramdisk_getgeo(struct block_device *dev, struct hd_geometry *geo)
{
	geo->cylinders=1;

	geo->heads=1;
	
	geo->sectors = RAM_SIZE/SECTOR_SIZE;

	return 0;
}*/

static struct block_device_operations ramdisk_fops = {
    .owner = THIS_MODULE,
    .open = ramdisk_open,
    .release = ramdisk_release,
    .ioctl = ramdisk_ioctl,
    .getgeo = ramdisk_getgeo,
};

void ramdisk_queue_func(struct request_queue *q)
{
    struct request *req;
    priv_data_t *pdata;
    unsigned long addr_of_blkram,start,size;
    char *data;
    
    char *ram_addr;
    struct bio_vec bv;
	struct req_iterator iter;
    char *base;

    req = blk_fetch_request(q);
    printk("write!!!\n");
    while(req){
        //spin_unlock_irq(q->queue_lock);
        // start position of sector;
        start = blk_rq_pos(req);

        pdata = (priv_data_t*)req->rq_disk->private_data;
        // ram start addr
        data = pdata->data;
        // addr of target addr.
        
	    base = bio_data(req->bio);
        
        addr_of_blkram = (unsigned long)data + start * SECTOR_SIZE;

        size = blk_rq_cur_bytes(req);

        rq_for_each_segment(bv,req,iter){
            ram_addr = (char*)page_address(bv.bv_page) + bv.bv_offset;
            if(rq_data_dir(req) == READ){
                memcpy(ram_addr,(char*)addr_of_blkram,bv.bv_len);
            }else{
                memcpy((char*)addr_of_blkram,ram_addr,bv.bv_len);
            }
        }

        if(!__blk_end_request_cur(req,0))
        {
            req = blk_fetch_request(q);
        }
    }
}

static int __init ramdisk_init(void)
{
    if(space_init()){
        return -ENOMEM;
    }
    if(priv_data_init()){
        return -ENOMEM;
    }

    blk_major = register_blkdev(0,RAMDIS_NAME);
    dev_data->data = ram_start_addr;
    dev_data->gd = alloc_disk(1);
    dev_data->queue = blk_init_queue(ramdisk_queue_func,&ramdisk_lock);
    dev_data->gd->major = blk_major;
    dev_data->gd->first_minor = 1;
    dev_data->gd->fops = &ramdisk_fops;
    dev_data->gd->queue = dev_data->queue;
    dev_data->gd->private_data = dev_data;
    sprintf(dev_data->gd->disk_name,"ramsd%c",'a');
    set_capacity(dev_data->gd,RAM_SIZE/SECTOR_SIZE);
    add_disk(dev_data->gd);
    printk("ramdisk init\n");
    return 0;
}

static void __exit ramdisk_exit(void)
{
    
    printk("ramdisk exit!!!\n");
    del_gendisk(dev_data->gd);
    put_disk(dev_data->gd);
    blk_cleanup_queue(dev_data->queue);
    unregister_blkdev(blk_major,RAMDIS_NAME);
    priv_data_exit();
    space_exit();
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);

MODULE_LICENSE("GPL");
