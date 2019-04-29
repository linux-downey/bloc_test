## sbuild的使用
### 创建
首先，创建一个新的sbuild环境，换句话说，就是在当前主机上创建一个独立的debian环境以编译deb包，可以创建与主机不同的架构环境，比如在X86上创建arm32或者arm64的。

sbuild-createchroot 调用了debootstrap，debootstrap是一个工具，主要功能是在一个debian系统的某一个目录下安装一个纯净的deb系统(可以是不同架构)，主要用来编译deb包，在纯净系统下编译出的deb包将不会存在依赖问题。  

sbuild-createchroot同时会做一些基础的设置工作。

第一步：
sbuild-createchroot --help

sbuild-createchroot [option\] SUITE TARGET-DIRECTORY DEBIAN-MIRROR-URI  

参数： 
--arch=arch                 创建的目标架构
--chroot-suffix             添加创建包的后缀名，默认是-sbuild，一般使用默认值。
--resolve-deps              自动处理依赖问题，这是默认使能的选项
--no-resolve-deps           与上一个参数相反的作用
--keep-debootstrap-dir      在安装完成之后不删除/debootstrap目录
--debootstrap=debootstrap   生成deb系统工具默认使用debootstrap，一般不用修改
--include=packages          用逗号分隔的package名称，被指定的package将会被安装到新生成的系统中
--exclude=packages          移除本来应该安装在系统中的package，package之间用逗号分隔
--keyring=keyring-file      指定keyring文件，这里指gpg文件，默认使用/etc/apt/trusted.gpg，这个gpg文件主要用与检查下载文件的签名


## SUITE                       
目标平台的发行版，不是版本号，而是发行版的代号，在ubuntu中有jessie、sid等等，使用

        lsb-release -a

查看，在16.04.3 LES上的显示是这样的：

        Distributor ID:	Ubuntu
        Description:	Ubuntu 16.04.3 LTS
        Release:	16.04
        Codename:	xenial
这里的SUITE就是相应的Codename。

        uname -a
可以查看目标架构

## TARGET-DIRECTORY  
生成的镜像放置位置，一般是放置在

        /srv/chroot/
目录下，这一项一般是这么填写：

        /srv/chroot/SUITE-ARCH-SUFFIX
例如，如果要生成一个x86_64架构，xenial的纯净镜像，一般这么写：

        /srv/chroot/xenial-x86_64-sbuild(最后的后缀可以修改，但是不建议)
        
## DEBIAN-MIRROR-URI
目标发行版的源URL。可以在目标主机上查看：

        cat /etc/apt/sources.list



所以最后如果要在ubuntu下生成一个全新的deb环境可以使用以下指令：

        sudo sbuild-createchroot --include=eatmydata,ccache,gnupg xenial /srv/chroot/xenial-x86_64-sbuild http://security.ubuntu.com/ubuntu

emptydata:如果用户存储一些并不是很重要的数据时可以使用emptydata以加快编译速度，在编译时通常就是这种情况
ccache可以帮助编译时进行缓存，加快编译时间
gnupg(gpg),是一个加密&签名软件

这个生成过程需要一段时间，我们可以查看相应的目录下是否生成了文件：

        ls /srv/chroot/xenial-x86_64-sbuild

删除一个chroot也可以直接删除这个文件即可.


然后使用
        sudo  sbuild-shell  xenial-amd64
就可以进到相应系统的shell，可以在shell中对文件进行操作，也可以直接操作/srv/chroot/xenial-x86_64-sbuild这个目录中的文件。


退出可以使用exit()


## 配置及配置文件

配置文件被放置在~/.sbuildrc中。 

$build_arch_all = 1;
$distribution = '$distribution';

使能build_arch_all会默认构建一个依赖于架构的包
指定distribution，也可以不指定

### 使用lintian
使能lintian将提高软件包的生成质量

$run_lintian = 1;
$lintian_opts = ['-i', '-I'];

### 使用piuparts
使能piuparts能提高软件包质量

$run_autopkgtest = 1;
$autopkgtest_root_args = [];
$autopkgtest_opts = [ '--', 'schroot', '%r-%a-sbuild' ];

### 使用autopkgtest
使能autopkgtest，在编译完成之后就会自动测试生成的包

$run_autopkgtest = 1;
$autopkgtest_root_args = [];
$autopkgtest_opts = [ '--', 'schroot', '%r-%a-sbuild' ];



## 编译包
编译一个debian包，运行命令：

        sbuild
在主机中调用sbuild，并且必须在已经格式化的deb包的目录下，它将启动上述创建的纯净系统来进行编译。

交叉编译：
        sbuild --host=mips




x86_64和amd64是同一个东西

sbuild文档地址：https://wiki.debian.org/sbuild
















