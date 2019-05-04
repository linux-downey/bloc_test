apt-get install原理：

/etc/apt/sources.list  一般是官方源地址
添加自己的源可以添加到/etc/apt/sources.list.d目录中。  

sudo apt-get update 原理：读取source list中的各个条目
把文件索引下载到/var/lib/apt/lists/

在apt-get update的时候，读取/var/lib/apt/lists/中的文件


/var/cache/apt/archives，在安装的时候，软件包存放的临时路径



一般安装在usr/local/目录下，而不是直接放在usr/bin,usr/lib中。与系统的目录分开



