## 制作deb包的流程

## 可以使用dh-make

暂时只试了编译完成的包，制作deb的过程没有编译。  

一个简单的deb包的目录结构

make_deb_test/
├── bin
│   └── mk_deb
├── DEBIAN
│   ├── changelog
│   ├── control
│   ├── copyright
│   ├── postinst
│   └── postrm
├── etc
│   └── just_for_test
└── usr
    └── lib
        └── libmk_deb_lib.so

bin、etc、usr分别是对应要安装在系统中对应目录的文件

DEBIAN目录下的文件为控制文件，描述整个文件包属性
其中changelog、control、copyright是必须的。


deb包网址：https://www.debian.org/doc/manuals/maint-guide/dreq.zh-cn.html


## sbuild部分 

sbuild网址：https://wiki.debian.org/sbuild

创建sbuild环境：sudo sbuild-createchroot --include=eatmydata,ccache,gnupg unstable /srv/chroot/unstable-amd64-sbuild http://deb.debian.org/debian

其中--include 表示需要在创建时安装的包，以逗号结尾

unstable是当前发行版的名称，使用sudo lsb_release -a可以查看，codename那一栏就是发行版名称。  

/srv/chroot/unstable-amd64-sbuild 表示创建的压缩包放置的目录位置

http://deb.debian.org/debian 表示镜像地址


## gpg部分
生成gpg：
gpg --gen-key

	Please select what kind of key you want:
   (1) RSA and RSA (default)
   (2) DSA and Elgamal
   (3) DSA (sign only)
   (4) RSA (sign only)

	RSA keys may be between 1024 and 4096 bits long.
	What keysize do you want? (2048) 4096

	Please specify how long the key should be valid.
    0 = key does not expire
      <n>  = key expires in n days
      <n>w = key expires in n weeks
      <n>m = key expires in n months
      <n>y = key expires in n years

gpg --list-keys downey-boy

gpg --edit-key $PUB-KEY
	pub  4096R/C0B56915  created: 2019-04-15  expires: never       usage: SC  
						trust: ultimate      validity: ultimate
	sub  4096R/174F007E  created: 2019-04-15  expires: never       usage: E
显示当前gpg key，一个pub，作用为SC，S表示signing，C表示creating certificate，创建一个证书，A 表示 PUBKEY_USAGE_AUTH 表示authentication purposes，用作认证。
E 表示ecyption，加密。

在新生成的gpg key中，master不能用作加密，用作加密的sub key。  

在gpg --edit-key的提示下，可以添加sub key，但是需要输入主秘钥的密码。  
最后需要使用save进行保存。  

gpg --export-secret-key 10E6133F > private.key
将master的private key导出到文件
将private key导出到文件之后，将gpg文件中的private key删除即可。
gpg --delete-secret-keys 10E6133F
这样，整个gpg文件中就没有private key了，注意要保存好private key。
可以用gpg --list-secret-keys查看。

需要注意的是，当将secrete key删除时，gpg的某些操作就不能做了：比如：
生成sub key
生成撤销证书等需要master完成的操作。
如果要操作的话必须重新导入secret key。


在导出公钥时可以使用-a参数以字符串的形式导出。  

生成撤销证书，当用户想要撤销服务器上的公钥时，就可以进行撤销。
gpg --output revoke.asc --gen-revoke 10E6133F
撤销证书被保存在revoke.asc中，将证书导入到秘钥环中，就会在本地将证书撤销。

        pub   4096R/C0B56915 2019-04-15 [revoked: 2019-04-16]
        uid                  downey-boy (downey boy's gpg key) <linux_downey@sina.com>
最后，再将本地撤销的秘钥发送到server，即可撤销远程的key。
gpg --keyserver keyserver.ubuntu.com --send-key C0B56915

gpg --list-keys downey-boy  >> private.key
将master的pub key导出到文件



http://keyserver.ubuntu.com/

gpg --keyserver keyserver.ubuntu.com --send-key C0B56915将gpg key发送到官方机构网站。



fakeroot  和 root 的不同
fakeroot将文件权限模拟为root，在编译deb包时使用。







