## linux内核在用户空间注册接口
在linux中，内核空间和用户空间是相互隔离的，
## /dev


## sysfs系统
### kobject_init() 和 kobject_create()
kobject_init()应用于初始化一个已经分配好内存的kobject，而kobject_create()负责创建并初始化一个kobject()并返回此kobject的指针。  

kobject_init()原型是这样的：

	void kobject_init(struct kobject *kobj, struct kobj_type *ktype){
		kobject_init_internal(kobj);
		kobj->ktype = ktype;
	}
kobject_init()的调用需要传入一个struct kobj_type结构体参数，这个struct kobj_type指定了当前kobject的属性和访问行为，struct kobj_type的原型是这样的：

	struct kobj_type {
		void (*release)(struct kobject *kobj);  //删除当前kobj节点的函数实现
		const struct sysfs_ops *sysfs_ops;      //包含show()和store()两个函数指针，分别对应用户对文件的读和写的回调函数。  
		struct attribute **default_attrs;       
		***
	};

同时kobject_create负责创建并初始化一个kobject，并不需要传入任何参数，kobject_create()的原型是这样的：

	struct kobject *kobject_create(void)
	{
		kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);
		kobject_init(kobj, &dynamic_kobj_ktype);
	}
它首先申请了一片struct kobject的内存，然后调用kobject_init()来进行初始化，传入了默认的kobj_type:dynamic_kobj_ktype,它的定义是这样的：


	static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
	{
		struct kobj_attribute *kattr;
		kattr = container_of(attr, struct kobj_attribute, attr);
		if (kattr->show)
			ret = kattr->show(kobj, kattr, buf);
	}
	static ssize_t kobj_attr_store(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
	{
		struct kobj_attribute *kattr;
		kattr = container_of(attr, struct kobj_attribute, attr);
		if (kattr->store)
			ret = kattr->store(kobj, kattr, buf, count);
		...
	}
	const struct sysfs_ops kobj_sysfs_ops = {
		.show	= kobj_attr_show,
		.store	= kobj_attr_store,
	};
	static struct kobj_type dynamic_kobj_ktype = {
		.release	= dynamic_kobj_release,
		.sysfs_ops	= &kobj_sysfs_ops,
	};
默认的kobj_type很简单，就是检测当前kobj_type的attr是否由struct kobj_attribute提供，如果是，就调用struct kobj_attribute结构体中相应的show()和strore()函数。  

在这两个函数之外，linux内核还提供了kobject_create_and_add()，算是上述两个函数的综合版本，传入


### kobject_add()
kobject_add()将当前kobject加入到/sys系统中，它的原型是这样的：

	int kobject_add(struct kobject *kobj, struct kobject *parent,const char *fmt, ...)
	{
		...
		retval = kobject_add_varg(kobj, parent, fmt, args);
		...
	}
	int kobject_add_varg(struct kobject *kobj,struct kobject *parent,const char *fmt, va_list vargs)
	{
		retval = kobject_set_name_vargs(kobj, fmt, vargs);
		kobj->parent = parent;
		return kobject_add_internal(kobj);
	}
	static int kobject_add_internal(struct kobject *kobj)
	{
		...
		parent = kobject_get(kobj->parent);
		if (kobj->kset) {
			if (!parent)
				parent = kobject_get(&kobj->kset->kobj);
			kobj_kset_join(kobj);
			kobj->parent = parent;
		}
		create_dir(kobj);
		...
	}
由于所有的kobject的组成一种树结构，对于kobject_add()，传入当前需要加入到/sys的kobj节点，其父节点，名称。  
名称的传入并不是以字符串的方式进行传递，而是传入可变参数，这样的方式更加灵活，最后名称将会在kobject_set_name_vargs()函数中被赋值给kobj->name成员。  

最终，调用kobject_add_internal()函数将kobj插入到系统kobject树结构中，同时为其在/sys目录下生成相应文件。  
了解kobject系统的都知道，kset是一系列kobject的集合，而parent节点直接决定了kobject在树结构中的位置，从源码中可以看出：当没有父节点时，其kobject属于的kset将成为当前kobject的父节点，那么在/sys文件系统中，kobject所属的kset和parent节点，到底是怎么决定当前kobject的文件位置的。  
* 由子节点创建的目录是父节点的子目录
* 当没有定义一个kobj的父节点时，kset自动成为当前节点的父节点
* 当没有定义一个节点的父节点且没有kset时，将无法进行注册


## kset_init()和kset_register()
kset是kobject的集合，本身也包含一个kobject，在/sys目录下通常表示为一个子集，而非单一的一个对象。  




## procfs




list成员list_head list，整个kset包含的kobject都包含在这个链表中。
void kset_init(struct kset *k)
{
	kobject_init_internal(&k->kobj);
	INIT_LIST_HEAD(&k->list);
	spin_lock_init(&k->list_lock);
}


//先调用kset_init,然后调用kobject_uevent.
int kset_register(struct kset *k)
{
	kset_init(k);
	err = kobject_add_internal(&k->kobj);
	kobject_uevent(&k->kobj, KOBJ_ADD);
}

//根据名称，在kset链表中逐个查找。
struct kobject *kset_find_obj(struct kset *kset, const char *name)
{
	list_for_each_entry(k, &kset->list, entry) {
		if (kobject_name(k) && !strcmp(kobject_name(k), name)) {
			ret = kobject_get_unless_zero(k);
			break;
		}
	}
}

//申请一个kset，使用提供的的uevent_ops,使用默认提供的kset_ktype。  
static struct kset *kset_create(const char *name,
				const struct kset_uevent_ops *uevent_ops,
				struct kobject *parent_kobj)
{
	kset = kzalloc(sizeof(*kset), GFP_KERNEL);
	
	retval = kobject_set_name(&kset->kobj, "%s", name);	}
	kset->uevent_ops = uevent_ops;
	kset->kobj.parent = parent_kobj;

	kset->kobj.ktype = &kset_ktype;
	kset->kobj.kset = NULL;

}


struct kset *kset_create_and_add(const char *name,
				 const struct kset_uevent_ops *uevent_ops,
				 struct kobject *parent_kobj)
{
	kset = kset_create(name, uevent_ops, parent_kobj);

	error = kset_register(kset);
}



device_register()
{
	device_initialize(dev);
	return device_add(dev);
}

device_add()    算是device_register的第二个版本。

dev_set_name();主要是设置device->kobj的名称。

device_create() 最后调用device_add(dev)

device_create_file()最后还是调用device_create_file()，


kobj->name表示在/sys下的名字。  
device_create传入的名称字符串被设置成了dev->kobj的名称。


在/dev下创建文件由device_add中的devtmpfs_create_node(dev);
spinlock
rcu

radix tree
list




验证一下kset和parent同时存在的问题。  
如果parent是一个文件，那么字节点怎么生成文件。
