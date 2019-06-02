# linux下deb包的制作(6) -- deb包gpg签名以及创建个人的deb仓库
在前面的章节中，我们介绍了如果使用gpg对文件进行签名，本章内容将讲述如何使用gpg对deb包进行签名。  


## deb包的安装以及安全性
在前面的章节中有提到，安装一个deb包有两种方式：
* 下载deb包到本地，然后使用指令进行安装：
```
sudo dpkg -i $PACKAGE
```
* 使用APT管理工具进行安装：
```
sudo apt-get install $PACKAGE
```
在第一种安装方式中，我们仅仅关注将deb包进行解压，然后安装到系统中，而没有进行安装包的校验工作，如果这时候，安装的deb包中包含非法软件，dpkg -i指令是检查不出的。  

在第二种安装方式中，由于使用官方提供的APT包管理工具，这个软件会对安装的软件包做一定的校验工作，以告知用户，这个软件包是否可信。   

同时，它并不会阻止用户安装不可信的包。  


## deb包的签名
在本系列博客的前面章节，一共介绍了三种deb包的制作方式：    
* 手动地创建所有软件包文件，包括debian目录下的所有控制文件。       
* 使用debmake+debuild自动创建软件包。   
* 使用debmake+sbuild自动创建软件包。  

第一种手动创建方式已经非常少用了，仅仅是作为简单的软件包或者是学习使用，通常使用第二种和第三种。   

当我们使用**debuild**或者**sbuild**创建deb包，最后会在debian目录的父目录下生成相应的.deb文件，这就是我们要的deb包。  

### 给deb包签名
除此之外，还会生成一个.dsc和.changes文件，这两个文件的内容大概是这样的(以之前章节中的debhello_1.0.0为例)：  
**debhello_1.0.0-1.dsc**   
```
-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA1
Format: 3.0 (quilt)
Source: debhello
Binary: debhello
Architecture: any
Version: 1.0.0-1
Maintainer: downeyboy <linux_downey@sina.com>
Homepage: https://github.com/
Standards-Version: 3.9.6
Build-Depends: debhelper (>= 9)
Package-List:
 debhello deb main optional arch=any
Checksums-Sha1:
 47f022d33fb20192754a9bedaad8ef76c4da9958 1766 debhello_1.0.0.orig.tar.gz
 a86f030f3c815b127d2d5d315b7ed736caeb665a 1356 debhello_1.0.0-1.debian.tar.xz
Checksums-Sha256:
 f47bc0d211b27a37fe39ee75b6b633d1eb62029ebf5cb8f265bcec7b7c9dfd0e 1766 debhello_1.0.0.orig.tar.gz
 e0164d3a169e972ddb28b5444ec5cd7b06e173ea2e2b9395079b71e2cbce4a61 1356 debhello_1.0.0-1.debian.tar.xz
Files:
 690fff674dede020485f9d515729cce8 1766 debhello_1.0.0.orig.tar.gz
 f8394154612c884dbd101e4daf2b385b 1356 debhello_1.0.0-1.debian.tar.xz
```

**debhello_1.0.0-1_amd64.changes**
```
Format: 1.8
Date: Mon, 29 Apr 2019 23:17:20 -0700
Source: debhello
Binary: debhello
Architecture: amd64
Version: 1.0.0-1
Distribution: xenial
Urgency: low
Maintainer: downeyboy <linux_downey@sina.com>
Changed-By: downeyboy <linux_downey@sina.com>
Description:
 debhello   - debhello package,just for test!
Changes:
 debhello (1.0.0-1) xenial; urgency=low
 .
   * just for test
Checksums-Sha1:
 1095b8507cbf4d0f2d648752ca4d7afd00cdffb8 3110 debhello_1.0.0-1_amd64.deb
Checksums-Sha256:
 fb48782d3147eef9929ce6654bc84266680bd60fcb04cb2050252fca2d8129ff 3110 debhello_1.0.0-1_amd64.deb
Files:
 6d105aa87c5173366c318a808b247e78 3110 main optional debhello_1.0.0-1_amd64.deb
```

可以明显地看到，.dsc文件中记录了源码包的相关信息，以及纯源码包、deb源码包的SHA1、SHA256和MD5的文件指纹(摘要)。   
.changes文件中记录了deb包的相关信息，保存了deb包的SHA1、SHA256和MD5的文件指纹(摘要)。   

如果你看了上一章节的gpg文件签名示例，就会觉得这个操作方式很熟悉，我们在对一系列文件签名的时候，对这些文件的指纹(摘要)进行签名即可保证它的来源以及检查是否被第三方修改。   

所以，在使用**debuild**或者**sbuild**创建生成了deb包之后，需要对这两个文件进行签名,在这里，我们使用debsign命令，这个命令同样是基于gpg key：
```
debsign -k FBB436FE debhello_1.0.0-1.dsc
debsign -k FBB436FE debhello_1.0.0-1_amd64.changes
```
-k选项表示指定对应的用作签名的gpg key，与直接使用gpg --sign指令不同的是，debsign并不会额外生成签名文件，而是直接将签名附在对应的文件末尾。   

至此，当我们直接以源码方式安装别人的安装包时，就可以使用对方的公钥对源码包的.dsc和.changes文件进行签名验证即可：
```
gpg --verify debhello_1.0.0-1.dsc
```

### 自动签名
在上面的方法中，我们手动地使用debsign对deb包中的.dsc和.changes文件进行签名，那么，前面我们使用debuild和sbuild自动构建工具有没有自动签名的功能呢？  

当然是有的，操作也是非常地简单，只需要在执行debuild或者sbuild的时候添加相应的参数即可，以debuild为例：
```
debuild -kFBB436FE
```
或者
```
debuild -k$(UID)
```
在讲解debuild的章节我们是用下面的指令来创建deb包：
```
debuild -us -uc
```
-us -uc表示不对deb包进行签名。   
而这里的-k 参数表示指定需要签名的gpg key，可以是key值，也可以是UID，-k参数与值之间不能有空格。  

这里的UID就是你在创建gpg key时填写的名称。   

执行结果就是：在deb包生成的最后阶段，将会提示你对.dsc文件和.changes文件签名需要输入密码：
```
....
Finished running lintian.
Now signing changes and any dsc files...
 signfile debhello_1.0.0-1.dsc huangdao

You need a passphrase to unlock the secret key for
user: "huangdao <1148256554@qq.com>"
4096-bit RSA key, ID FBB436FE, created 2019-05-30

                  
signfile debhello_1.0.0-1_amd64.changes huangdao

You need a passphrase to unlock the secret key for
user: "huangdao <1148256554@qq.com>"
4096-bit RSA key, ID FBB436FE, created 2019-05-30

          
Successfully signed dsc and changes files
```
输入两次密码，签名结束，再去查看deb包生成目录下的.dsc和.changes文件，就发现已经签名完成了。   


## 签名的验证
事实上，如果我们使用源码的方式安装，即使是源码包中提供了公钥文件、经过签名的.dsc和.changes文件，用户拿到之后也并不见得会去验证，或者是他们低估了软件安全的重要性，又或者是压根没有相关的意识。   

从这个角度来看，使用源码包的安装是不够安全的，因为不能假定最终的用户了解deb包签名的相关知识。  

但是在使用APT管理工具时，这个问题将不再存在，因为APT工具将主动验证deb包的签名。   

当我们使用指令对所有的源进行更新时：
```
sudo apt-get update
```
APT工具将检查当前系统中是否存在目标源deb包中的公钥，以及自动为其验证签名。  

如果验证不通过，将无法完成签名工作，也就无法完成对deb包的后续操作，包括安装。  







