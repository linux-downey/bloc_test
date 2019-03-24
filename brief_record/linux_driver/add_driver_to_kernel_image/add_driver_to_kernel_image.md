# 将模块编译进内核


将源代码添加到源代码目录中，一般的字符设备驱动就添加到drivers/char中，可以自建一个目录，也可以直接放在drivers/char目录下。  

如果是自建一个目录，需要新建一个Kconfig文件

建完Kconfig文件，然后在drivers的Kconfig文件中将当前的Kconfig文件路径添加上。  

在drivers的Makefile中需要将新建的目录添加进去，表示在编译drivers时，同时需要进入这个目录进行编译

添加的原理，Kbuild文件决定了在make menuconfig的时候是否出现在选项中。  
填写正确的Kbuild就可以在make menuconfig的菜单中选中，配置结果出现在.config中
顶层makefile根据.config再逐一地递归目录进行编译。  




https://blog.csdn.net/a746742897/article/details/52329916
https://ocelot1985-163-com.iteye.com/blog/980711