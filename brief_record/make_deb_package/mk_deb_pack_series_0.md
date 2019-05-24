# linux下deb包的制作(0) -- deb包制作的简单示例
deb是unix系统下的安装包，它基于tar包，因此本身会记录文件的权限以及所有者/用户组，同时，由于Unix类系统对权限、所有者、组的严格要求，而deb格式安装包又经常会涉及到系统比较底层的操作，所以权限的控制显得尤为重要。(----摘自百度百科，如有侵权，请联系我删除)      

deb包通常配合APT软件管理系统来使用，它是Debian系统(包含debian和ubuntu)下专属的安装包格式，称为当前在linux下非常主流的一种包管理方式。  

deb包的使用是非常方便的，当我们想发布自己的软件包时，就不得不去了解deb包的制作了。  

在本系列博客中，博主将一步一步带你们了解deb包的整个制作流程，包括软件包制作、签名与加密、上传服务器、使用APT管理工具进行管理。  

## deb包的安装方式
首先，我们有必要了解deb包的安装方式，它的安装方式有两种：
* 通过APT包管理系统安装，也就是我们最常用的方式：
```
sudo apt-get install $PACKAGE_NAME
```
一般通过这种方式安装的都是官方推出的，安装过程不会出现什么问题。  
但是，个人或组织同样可以发布以这种方式安装的deb包，这一类非官方的deb包可能出现一些问题，之后的教程也会教大家怎么制作以APT来管理的deb包。  

* 第二种方式就是直接下载deb包，通常是以.deb结尾的包，需要自己手动安装，而手动安装的指令也非常简单：
```
dpkg -i $PACKAGE_NAME
```

值得一提的是，deb包的标准命名格式为：
```
<name>_<VersionNumber>-<DebianRevisionNumber>_<DebianArchitecture>.deb
```  
无论如何，遵照标准都是一种好的习惯，同时，非规范的命名很可能在安装时导致一些问题。  


## 对于deb包的操作
dpkg命令除了使用-i指令安装deb包之后，还提供一系列的选项对deb包进行操作。  

我们以libtool为例来演示dpkg的各个选项设置。  

首先，下载libtool的deb包：

    sudo apt-get download libtool  
该指令表示下载对应的deb包而不安装，指令执行完成将在当前目录下出现一个libtool的软件包，它的命名方式为：

    <name>_<VersionNumber>-<DebianRevisionNumber>_<DebianArchitecture>.deb
在当前主机上出现的软件包名为：

    libtool_2.4.6-0.1_all.deb
表示包名为libtool，版本号为2.4.6,次版本号为0.1，all表示适用于所有架构。  


## dpkg选项
如果要在linux下查看一个指令的使用方法，最好的办法就是使用--help选项：

    dpkg --help
基本上所有成熟软件都会支持这个选项，它将列出关于这个命令的详细信息。  

### -i 安装
dpkg -i package_full_name 或者 dpkg --install package：表示安装该软件包，deb包并非平台无关的包，deb包与主机架构需要对应，才能安装使用，在安装的过程中终端将打印相应的步骤信息，在出错时也方便调试。 

事实上，deb包的安装分为了两个阶段：先解压压缩包，然后进行配置。这两个步骤可以分开处理。  

### -r -P 移除
dpkg -r package_name 或者 dpkg --remove packge：表示从系统中移除一个deb包，但是保留其配置文件、维护文件以及用户数据，仅相当于停用程序，下次安装可继续使用。  

dpkg -P package_name 或者 dpkg --purge ： 表示从系统汇总移除一个deb包，同时，删除所有包相关的文件。  

### -L 列出已安装包信息
dpkg -L packge_name ： 列出某个已安装deb包的文件清单，包含所有安装的软件，但是不包含控制信息。需要注意的是，这一选项需要目标是已安装软件，packge只需要包名而不需要带.deb后缀的全名。  

### -c 显示安装文件的详细信息
dpkg -c package_full_name 或者 dpkg --content package_full_name ，显示目标包安装文件的详细信息，包括权限，大小等等，与-L选项不同的是不需要安装即可查看，对于查看其它架构下的deb很方便。  

### deb包的解压
在dpkg命令的使用说明中，--unpack选项表示解压deb包，但是我尝试使用
```
sudo dpkg --unpack package_full_name
```
尽管整个过程没有报错，但是并没有在当前目录下找到任何解压后的相应的包文件。   

几番尝试无果之下，我使用了另一个命令来对deb包进行解压：
```
sudo dpkg-deb -R package_full_name target_dir
```
这条命令会将deb包的信息解压到target_dir目录下。  

与dpkg命令性质相同，dpkg-deb命令也是用于deb包的各项操作命令，可以使用
```
dpkg-deb --help 
```
来查看相应的功能。  

### deb包的结构
首先，我们选取下载一个deb包到当前目录下，这里以code_1.33.1-1554971066_amd64.deb(vs code，一款十分受欢迎的开源编译器)为例，来看一下deb包的目录框架。  

首先，创建一个存放解压后的deb包的目录：

    mkdir code
然后将deb包解压到上述目录中：

    sudo dpkg-deb -R code_1.33.1-1554971066_amd64.deb  code

查看code目录下的文件，主要是两个文件夹：

    DEBIAN  usr

#### DEBIAN目录
DEBIAN目录下一般都是一些控制文件，也可以以debian命名，这个目录下通常是这些文件：
* control ： deb包的描述文件，包含了很多供dpkg、apt-get、apt-cache等包管理工具进行管理时所使用的许多变量。  
* copyright ： 顾名思义，这个文件包含了上游软件的版权以及许可证信息，如果你常年混迹于开源社区，那么开源协议和版权是需要牢记在心的。  
* changeLog ： 版本变更信息，对于一个需要长期维护的软件，版本变更信息的跟踪是非常有必要的。  
* rules : 每一个rules文件，就像其他的Makefile一样，包含若干的规则，每一列规则都定义了一些对于deb包的操作，主要作用是控制源码包的编译安装等行为。  
* 其他脚本 ： 常见的脚本文件有preinst、prerm、postinst、postrm，分别代表脚本的执行时机为：解压包之前执行、卸载之前执行、安装之后执行、卸载之后执行，这些脚本可以很方便地对目标环境进行一些配置，以方便安装的执行。  


#### usr目录
usr目录下都是当前软件包的数据文件，主要包含：可执行文件、共享库。   
通常的，usr目录下的目录在安装时，会默认被安装到/usr/目录对应的目录下。   

比如，在当前code目录下usr目录下的文件夹有这些：  

    usr/share/appdata   usr/share/applications  usr/share/code  usr/share/pixmaps  

在安装时，这些文件夹就被安装到系统/usr对应的目录下。   

同样的，如果创建一个bin目录，则会将bin/下的所有数据安装到系统的/bin/目录下对应的文件夹中。   


***  
*** 


当我们需要制作软件包时，主要关注deb包的这两个部分：控制文件和数据，其中，控制文件是这系列文章的重点讲解部分。  

在下一章节中，我们来讨论怎样制作一个deb包。  
















