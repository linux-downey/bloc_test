# 在编译的时候，linux的版本号被修改的解决办法

当源码是由git托管的时候，在修改了代码而没有提交的情况下进行编译，编译出来的镜像很可能版本号被改变。  
比如在beagle bone上面，原本版本号为4.14.79-ti-r89+,编译过后版本号变成了4.14.79+。

linux 控制版本号的变量由makefile和.config决定

makefile的顶部确定了主版本号：

VERSION = 4
PATCHLEVEL = 14
SUBLEVEL = 79
EXTRAVERSION =

CONFIG_LOCALVERSION 决定了版本号的后缀名


CONFIG_LOCALVERSION_AUTO  如果这个变量被设置为y，那么就会在版本号后面添加上git仓库的信息(tag名称等)

去除版本号后的+，可以将后缀版本号写在CONFIG_LOCALVERSION部分，然后

	make LOCALVERSION="" 
	
同时可以用make kernelrelease 查看编译出来的版本号是否正确。


