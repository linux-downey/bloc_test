# linux下deb包的制作(7) -- 使用私有仓库发布deb包
通常，我们使用apt-get install指令都是安装官方发布的包，如果我们要安装非官方的包，需要先添加软件源，即软件下载地址。  

然后使用更新指令下载软件源：
```
sudo apt-get update
```
这个命令的操作是：读取**sources.list**以及**sources.list.d**中的各个条目的内容，然后把文件索引下载到/var/lib/apt/lists/，当使用安装命令：
```
sudo apt-get install $PACKAGE
```
时，就会读取这一系列的索引文件，从指定的源下载软件包。  

同时，第三方的软件包安装一般放置在/usr/local目录下，与系统的目录/usr/分开，有利于管理。  


## 创建私有deb仓库
当我们想要发布自己的deb包时，是不能发布到官网的，所以我们得创建自己的deb仓库，让其他用户从我们的仓库中获取软件。  

### 公网服务器
这首当其冲的第一步，自然是需要一个公网服务器。  

对于一个组织(公司)而言，通常是购买一个服务器，然后自己进行维护，而对于个人而言，这未免有点麻烦，除了耗时耗力的维护工作，还需要一笔购买服务器的费用，显然是不划算的。    

在这里不得不再次感谢github，它提供的github pages允许用户搭建自己的个人静态网站，完全满足了我们创建deb仓库的需求，而且有几大优势：
* 免费且无限流量
* 由git进行版本管理，对于我们这些本来就习惯使用git的用户十分友好
* 免去了大部分自建服务器管理的工作

使用github pages的步骤也是非常简单的：  
* 使用github创建一个仓库，特别的是，这个仓库名必须遵循以下格式：
```
$USERNAME.github.io
```
这个USERNAME就是你的github用户名，博主的用户名为linux-downey，所以仓库名为**linux-downey.github.io** 。  

* 对新创建的仓库进行配置：点击仓库中的settings选项，找到github pages部分，然后选择相应的分支发布，选择None就是不使用github pages功能。  
TODO 
TODO
做完以上的工作，github pages功能就配置完成了。根据完成后的提示，我的github pages的访问公网地址就是：
```
https://linux-downey.github.io/
```

更多github pages搭建细节可以参考[github pages getting started](https://guides.github.com/features/pages/)


## 上传deb包到公网服务器
有了自己的服务器，下一步就需要将我们生成的deb包上传到服务器中了。   

先把在上文中创建的github pages对应的git仓库clone到本地，然后直接对本地的仓库进行操作即可，假设clone下来的仓库名为：debhello，即本地clone下来的目录名为debhello。  

事实上，上传deb包并不是简单地将其上传到服务器即可，它必须按照一定的格式和布局才能被APT管理工具识别。  

在deb包的上传过程中，我们需要使用**reprepro**这个命令来生成相应的配置文件，以使得APT工具可以识别并下载它。   


## deb仓库的初始化
在上传deb包之前，我们先要做一件事，初始化我们新创建的deb仓库，当前目录为git仓库的根目录：
```
mkdir conf
touch conf/distributions
```
在本地仓库下创建一个conf目录，和一个distributions文件，这个distributions是关于当前软件仓库的控制文件，描述当前仓库的信息，我们需要编辑它：
```
vim conf/distributions
```

并写入以下内容：
```
Origin: linux-downey.github.io
Label: linux-downey.github.io
Codename: xenial
Architectures: i386 amd64 source
Components: main
Description: debhello
SignWith: 229B2D28
```
APT工具根据这些描述信息的内容来识别一个deb仓库，下面来分析其中相应的字段：
* Origin ： 软件包来源，一般使用URL
* Label ： 软件包标签
* Codename ： 目标明天的发行版代号，对于ubuntu有"xenial"，"trusty"，"jessie"等等，根据目标机器的codename而设置。     
可以在机器上使用以下命令查看codename：
```
lsb_release -c
```
* Architectures ： 目标机器的架构。  
* Components ： 软件分类的标识符，标准情况下使用main，我们可以看到，在上传deb包时，就会将deb包放置在main目录下，我们可以使用其他Components，以将不同的deb包区分开来，在安装时选择性地安装不同版本。
我们查看/etc/apt/sources.list中的条目可以看到：
```
deb https://linux-downey.github.io/ xenial main
```
最后的main就是我们指定的components，APT将根据这个去服务器相应main目录下寻找软件包。  
* Description ： 软件包的描述信息
* SignWith ：表示对该软件包签名的 gpg key。


### 使用reprepro

初始化完成之后，就可以使用reprepro指令上传deb包了。   

依照博主之前介绍的方法，当遇到一个陌生的命令时，我们应该先键入：
```
reprepro --help
```
我们就可以看到它的大概用法。  


选项比较多，这里就不一一列举了，列出几个主要需要用到的选项：  
* -b, --basedir \<dir\>:                指定操作基准目录，基准目录应该为仓库的根目录。  
* include \<distribution\> \<.changes-file\>   上传.changes文件，.changes文件为签名文件
* includedeb \<distribution\> \<.deb-file\>    上传deb包
* remove \<distribution\> \<packagename\>      从仓库中删除资源

当前示例中，我们需要上传的是之前使用debuild自动构建生成的debhello_1.0.0-1_amd64.deb和经过签名的debhello_1.0.0-1_amd64.changes文件。    
```
reprepro include xenial debhello_1.0.0-1_amd64.changes
reprepro includedeb xenial debhello_1.0.0-1_amd64.deb
```
其中xenial为目标机器发行版代号。  
如果你不在当前仓库根目录下操作，就需要使用-b参数来指定仓库根目录。   

操作完成之后，会发现仓库目录下多出了很多文件，使用tree查看：
```
.
├── conf
│   └── distributions
├── db
│   ├── checksums.db
│   ├── contents.cache.db
│   ├── packages.db
│   ├── references.db
│   ├── release.caches.db
│   └── version
├── dists
│   └── xenial
│       ├── InRelease
│       ├── main
│       │   ├── binary-amd64
│       │   │   ├── Packages
│       │   │   ├── Packages.gz
│       │   │   └── Release
│       │   ├── binary-i386
│       │   │   ├── Packages
│       │   │   ├── Packages.gz
│       │   │   └── Release
│       │   └── source
│       │       ├── Release
│       │       └── Sources.gz
│       ├── Release
│       └── Release.gpg
├── pool
│   └── main
│       └── d
│           └── debhello
│               ├── debhello_1.0.0-1_amd64.deb
│               ├── debhello_1.0.0-1.debian.tar.xz
│               ├── debhello_1.0.0-1.dsc
│               └── debhello_1.0.0.orig.tar.gz
```
这些文件都是reprepro自动生成一些配置文件，我们暂时不用理会。   

到这里，基本上deb仓库就制作完成了。   

但是，既然我们的deb包是经过gpg key签名的，那么使用者在验证签名的时候需要用到我们的公钥，我们还得把对应的公钥也公布在deb仓库中,将公钥导出到仓库根目录下：
```
gpg --export FBB436FE > public.key
```

本地仓库算是大功告成了，最后一步就是将本地仓库推送到服务器：
```
git push -u origin master
```
等待传输完成即可。   


## deb包的安装
既然个人的deb仓库制作完成，接下来我们就来测试一下上文中配置好的deb服务器。  

### 添加公钥
首先，我们需要将公钥导入到当前APT管理系统中,我们可以先下载服务器中public.key，然后导入到系统：
```
sudo apt-key add public.key
```
或者直接通过网络的方式直接进行安装：
```
curl https://raw.githubusercontent.com/linux-downey/linux-downey.github.io/master/public.key | sudo apt-key add -
```
导入完成之后，deb仓库的公钥将被添加到trusted.gpg中。  

### 添加sources.list
然后添加软件源，这样APT软件管理系统才能由此找到我们的软件包。   

我们可以在/etc/apt/sources.list.d/目录下新建一个文件：test.list,然后编辑test.list为以下内容:
```
deb https://linux-downey.github.io/ xenial main
```
指定软件源地址为**https://linux-downey.github.io/**，目标发型版为xenial，deb包目录为main。  

然后更新软件源：
```
sudo apt-get update
```
需要注意的是，如果没有添加deb仓库的公钥，将报以下错误：
```
GPG error: https://linux-downey.github.io xenial InRelease: The following signatures couldn't be verified because the public key is not available: NO_PUBKEY 41FFEC0A29757E65
```

当然，如果你自己在创建deb包时没有对deb包进行签名，仅仅是上传了deb包而没有上传.changes文件到deb仓库，用户在安装时不添加gpg key的公钥也是可以安装的，只是会提示警告。  

### 安装
等待update完成之后，我们就可以安装了：
```
sudo apt-get install debhello
```


参考资料：[HOWTO: Create debian repositories with reprepro](https://blog.packagecloud.io/eng/2017/03/23/create-debian-repository-reprepro/)




