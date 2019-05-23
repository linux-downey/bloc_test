# linux下deb包的制作(0) -- deb包制作的简单示例
deb是unix系统下的安装包，它基于tar包，因此本身会记录文件的权限以及所有者/用户组，同时，由于Unix类系统对权限、所有者、组的严格要求，而deb格式安装包又经常会涉及到系统比较底层的操作，所以权限的控制显得尤为重要。(----摘自百度百科，如有侵权，请联系我删除)      

deb包通常配合APT软件管理系统来使用，它是Debian系统(包含debian和ubuntu)下专属的安装包格式，称为当前在linux下非常主流的一种包管理方式。  

deb包的使用是非常方便的，当我们想发布自己的软件包时，就不得不去了解deb包的制作了。  

在本系列博客中，博主将一步一步带你们了解deb包的整个制作流程。  

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
dpkg -i package 或者 dpkg --install package：表示安装该软件包，deb包并非平台无关的包，deb包与主机架构需要对应，才能安装使用，在安装的过程中终端将打印相应的步骤信息，在出错时也方便调试。 

事实上，deb包的安装分为了两个阶段：先解压压缩包，然后进行配置。这两个步骤可以分开处理。  

### --unpack 解压



### -r -P 移除
dpkg -r packge 或者 dpkg --remove packge：表示从系统中移除一个deb包，但是保留其配置文件、维护文件以及用户数据，仅相当于停用程序，下次安装可继续使用。  

dpkg -P packge 或者 dpkg --purge ： 表示从系统汇总移除一个deb包，同时，删除所有包相关的文件。  

### 








