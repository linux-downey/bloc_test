//调用kobject_internal,然后指定一个ktype。
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)
{
	kobject_init_internal(kobj);
	kobj->ktype = ktype;
}



static struct kobj_type dynamic_kobj_ktype = {
	.release	= dynamic_kobj_release,
	.sysfs_ops	= &kobj_sysfs_ops,
};

//申请内存空间，同时调用kobject_init，ktype使用默认的dynamic_kobj_ktype
struct kobject *kobject_create(void)
{
	kobj = kzalloc(sizeof(*kobj), GFP_KERNEL);
	kobject_init(kobj, &dynamic_kobj_ktype);
}


//kobject_create和kobject_add集合体，与kobject_init的区别是传入地址和返回指针。
struct kobject *kobject_create_and_add(const char *name, struct kobject *parent)
{
	kobj = kobject_create();
	kobject_add(kobj, parent, "%s", name);
}


//kobject_add_internal,需要parent节点，需要设置当前kobject_name。
int kobject_add(struct kobject *kobj, struct kobject *parent,
		const char *fmt, ...)
{
	va_start(args, fmt);
	retval = kobject_add_varg(kobj, parent, fmt, args);
	va_end(args);
}



kobject_rename()
kobject_move()


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



