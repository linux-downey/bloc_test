# /usr 目录

### /usr/bin

### /usr/include
系统或者用户的头文件将会放置在该目录下. 

### /usr/lib
和 /lib 类似,用于放置目标文件或者动态库文件,通常被用户级别的软件所依赖.对于需要使用多个库文件的软件,可以在 /usr/lib 下创建子目录.  

### /usr/ilbexec
/usr/ilbexec 包含一些内部的可执行文件,可以直接被用户执行或者被脚本调用

### /usr/share
顾名思义,share目录下的文件都是 shareable 的,架构无关的.这些文件通常不建议用户修改,从而可以在不同架构的主机中进行移植,但是不保证可以在不同的 OS 之间进行移植.   

每个应用程序所附带的不需要修改的文件数据通常就放置在该目录下,比如静态的配置文件,辅助数据,帮助文档(常用的 man 手册)等,FHS 建议在该目录下创建子目录来组织单个软件的所有 share 文件.  

相对的,需要修改的数据通常被放在 /var/share 中. 

#### /usr/share/color
linux 中使用的 ICC profile(色彩特性文件) 保存在该目录下,ICC Profile简单来说就是某一彩色设备的色彩特性描述的文件，他表示了这一特定设备的色彩描述方式与标准色彩空间的对应关系。

#### /usr/share/dict
该目录是系统上单词列表的主目录。传统上，此目录仅包含英文单词文件，该文件由look(1)和各种拼写程序使用。单词可以使用美国或英国拼写.  

#### /usr/share/man
linux 的 man 是非常有用的一个指令,它输出系统中软件和接口的详细信息,基本上查询接口使用的时候直接使用 man 手册是比较高效且全面的,而 man 命令就是通过查找当前目录下的 man 文件来显示帮助信息的.  

man 一共分为八种类型的帮助信息,以数字为区分,分别为 1~8,在执行 man 命令时也是直接带上数字,比如 man 2 read,这些不同类型的帮助信息被分类为:
* man1:用户命令相关的手册,大部分用户使用的程序都放在当前目录下,比如 man 1 ls.  
* man2:系统调用相关的手册,用于查询直接调用系统调用接口的详细信息,比如 man 2 read.
* man3:不直接调用系统调用接口的库文件接口,比如 man 3 printf.
* man4:特殊文件,包括在 /dev 目录下的设备文件,比如 man 4 rtc,以了解 rtc 的相关信息. 
* man5:文件类型
* man6:游戏相关的手册
* man7:杂项类,一些不方便归类的帮助手册放置处. 
* man8:一些系统管理员才有权限使用的用于系统管理的指令,比如 man 8 shutdown.  

当使用 man 时,并不一定需要指定分类数字,man 会自动进行匹配,只有在出现冲突时,比如 man write 时,显示的是 man 1 write 的结果(用户程序),实际上也存在 man 2 write 指令(系统调用相关),这时候就需要通过分类数字进行区分.  

在 /usr/share/man/ 中包含了 gzip 压缩形式的各个 man 手册，这些手册支持多种语言，同时也可以通过包管理工具安装各种语言的官方 man 手册，通过设置 /etc/manpath.config 文件来切换执行 man 命令时的显示语言，对于系统的帮助信息(标准库接口、系统命令等)直接包含在 man 的官方软件包中，而其它的安装软件则由对应的软件提供。  


### /usr/local
本地软件安装目录,当前目录下的软件不会被系统的软件更新所影响,这里的本地针对的不是用户,而是主机,当用户手动编译安装某些软件时,建议安装在当前目录中.  

经过前文的介绍,用户软件的安装可以分别在 /usr /opt 和 /usr/local 下,/usr 通常是针对包管理工具安装的软件,且这些软件通常被系统管理员或者普通用户使用(比如 vim,zip等),而 /opt 更倾向于安装一些临时软件,测试版等额外的,特定情况下使用的软件,/usr/local 是相对于 /usr 的另一种解决方案,/usr 下安装的软件通常是自动管理的,会随着系统软件的升级而变化,因此最好不要人为地去直接干预 /usr 下的安装软件,将手动安装的软件包安装到 /usr/local 下是一种不错的选择.  

另一方面,手动编译安装的软件包通常不具备良好的可移植性,在手动编译时通常会指定一些特定的编译或者安装选项,在其它平台上无法很好地适配,对于不同架构,自然是无法兼容的,这也是 local 这个目录名所指代的.    

/usr/local/ 和 /usr/ 下的目录结构比较相似, 包含以下目录结构:

```
Directory Description
bin Local binaries
etc Host-specific system configuration for local
binaries
games Local game binaries
include Local C header files
lib Local libraries
man Local online manuals
sbin Local system binaries
share Local architecture-independent hierarchy
src Local source code
```

### /usr/local/share
作用和 /usr/share 一致,针对于安装在 /usr/locla 中的软件.  


### /usr/src
src 是 source 的缩写，表示源代码的存放地址，这里可以存放各种源代码文件，通常，如果需要在用户之间共享源代码，就可以将源代码放在 /usr/src 目录下，通常是一些标准库的源码，比如 busybox、libc。  

需要注意的是，在 ubuntu 和 debian 上，内核源代码一般并不存放在该目录下，而用于编译内核源代码的头文件和脚本默认被安装在这里而不是 /usr/include 目录(使用包管理工具安装头文件时)。  



## /var 目录
相对于 /usr 的 static 属性，/var 目录中则是为 variable 数据而存在，这包括管理文件和日志记录数据以及临时文件。同时，/var/ 下的大部分文件都是 unshareable 的，比如/var/log, /var/lock, and /var/run，而有些则具有一定的移植性，比如 /var/mail, /var/cache/man, /var/cache/fonts, and /var/spool/news，总的来说，/var/ 目录更偏向于 unshareable 的属性。  

/usr 可以被挂载为只读，而 /var 必须挂载为可读写。  

放置在 /var 下的文件应该保存在单独的子目录中，而不是直接放置在 /var 目录下。  
 
