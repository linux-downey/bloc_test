# linux下deb包的制作(4) -- 使用gpg进行签名
随着计算机的飞速发展，除了享受它的各种便利，随之而来的也有各种网络安全问题，虽然linux下的安全问题没有windows下的那么严重，也总是不能被忽视的。  

在这一系列文章中，我们一直讨论的是deb包的相关操作，那么，在我们下载安装软件包时，怎么确定这个安装包是安全的呢？  

这就需要我们对常用的加密策略有一定的了解，了解基本的非对称加密方式可以参考我的另一篇博客：[SCP免密传输详解](https://www.cnblogs.com/downey-blog/p/10477670.html)。  

在deb包的下载安装时，我们往往考虑的并不是加密，更多的是一种验证方式：即怎么确认我下载的安装包确实由官方发布，又或者是确认即使安装包由官方发布、但是是否经过第三方的篡改。  

对此，debian官方选择的加密软件为gpg。  

## gpg简介
gpg全称为GNU Privacy Guard，是一个密码学软件，用于加密、签名通信内容以及管理非对称密码学的密钥。  
gpg基于非对称加密算法，也就是使用非对称的密钥对，公钥和私钥，通常，私钥是受到严密保护的，只有密钥对拥有者持有，而公钥的公开的，所有人都可以获取。   

## 加密与签名的使用场景
在加密传输数据的场景中，其他用户将数据使用公钥加密，私钥持有者将接收的数据使用私钥进行解密，以达到加密的作用。事实证明，非对称加密算法是非常难以破解的，除非由于其他因素而导致私钥的泄漏，否则这种方式的加密效果在目前来说安全性是很高的。  

在使用签名的场景中，私钥持有者将数据使用私钥进行加密，其他用户使用公钥对其进行解密，由于私钥通常特定的人或组织持有，其他用户就可以通过公钥解密的方式确认这个数据是否是由私钥拥有者发出来的，具有不可抵赖性。  

通常签名的使用是这样的：  
* 私钥签名者将需要发送的数据提取摘要，提取摘要的方法有多种，比如MD5、SHA算法。  
* 将摘要用私钥进行加密，加密生成的数据附在数据之后与数据一起发出。  
* 接收者使用公钥对摘要数据进行解密，得到数据部分的摘要。  
* 接收者将数据部分使用同样的摘要算法，得到摘要数据，与解密出来的摘要数据做对比，如果比对一致，就表明数据包由私钥拥有者发出，且中途未被篡改。   
使用摘要算法验证的作用就是确认数据包没有被第三方修改过。  

## gpg的使用
由于版本的演化，gpg软件出现了gpg版本和gpg2版本，我们主要讨论gpg2，gpg2与gpg1都是基于OpenGPG 协议，所以原理上没有太大差别，只有使用上的差别，在一定程度上是保持兼容的。  

### 安装gpg2
gpg2的安装和其他软件的安装并没有什么区别：
```
sudo apt-get install gpgv2
```

### 生成gpg2密钥对
在使用gpg时，我们需要拥有自己的gpg密钥，当然，如果你仅仅是使用别人的公钥进行数据验证也是可以的，但是在大多数情况下，我们都需要生成自己的gpg密钥。  

国际惯例，在使用一个命令前，先使用以下命令查看命令的用法：
```
gpg --help
```
得到使用方法：
```
Syntax: gpg [options] [files]
Sign, check, encrypt or decrypt
Default operation depends on the input data
....
(限于篇幅原因，选项内容请自行查看)
```
***
然后在选项中找到对应的创建密钥的选项命令：
```
gpg --gen-key
```
***
依据提示分别输入这些内容：
```
Please select what kind of key you want:
   (1) RSA and RSA (default)
   (2) DSA and Elgamal
   (3) DSA (sign only)
   (4) RSA (sign only)

```
输入1。使用RSA加密。  
***  
```
What keysize do you want? (2048)
```
输入4096，在不考虑效率的情况下，推荐使用4096 bit的加密安全级别，而不使用默认的2048.  
***  
```
Please specify how long the key should be valid.
    0 = key does not expire
    <n>  = key expires in n days
    <n>w = key expires in n weeks
    <n>m = key expires in n months
    <n>y = key expires in n years
```
输入0，这个选项是指当前密钥的过期时间，分别可以以天、周、月、年为单位，选择0表示永不过期，这个示情况而定。  
***
```
Real name: 
Email address: 
Comment: 
```
填写你的名称、邮箱地址、以及添加一些注释。  
需要注意的是，gpg生成的key是绑定名字和邮箱的，你需要在生成gpg key的时候输入名称和邮箱，当然它不会真的去检查你的名称和邮箱，如果仅仅是测试使用，你可以随便填写。  
如果作为你的专用gpg密钥对，那么最好是填写真实信息。   
***
```
Enter passphrase:
```
最后，需要输入相应的密码，这个密码可以保护生成的key，其他人在没有密码的情况下无法使用你的key。  

***
然后，系统将会有以下提示：
```
We need to generate a lot of random bytes. It is a good idea to perform
some other action (type on the keyboard, move the mouse, utilize the
disks) during the prime generation; this gives the random number
generator a better chance to gain enough entropy.
```
这是表示它在收集一些随机信息，以生成密钥对，这是你大可以去干一些其他事，或者打出一些随机字符。这个过程可能需要几分钟。    

***  
最后生成的gpg结果是这样的：
```
gpg: key FBB436FE marked as ultimately trusted
public and secret key created and signed.

gpg: checking the trustdb
gpg: 3 marginal(s) needed, 1 complete(s) needed, PGP trust model
gpg: depth: 0  valid:   2  signed:   0  trust: 0-, 0q, 0n, 0m, 0f, 2u
pub   4096R/FBB436FE 2019-05-30
      Key fingerprint = A830 1E45 6DA3 3029 B977  72B3 1173 B69D FBB4 36FE
uid                  huangdao (nothing) <1148256554@qq.com>
sub   4096R/DF4343C8 2019-05-30
```

### 对gpg的选择
gpg命令有丰富的选项，通常，在使用gpg时需要指定要操作对象，因为很可能你的当前用户下存在多个gpg key。   
gpg key 可以通过名称、邮箱、key值来指定，比如，如果我们要对gpg进行操作，可以这样指定一个gpg key：
```
gpg -opt huangdao
gpg -opt 1148256554@qq.com
gpg -opt FBB436FE
```
通常，这三个信息都会指向同一个gpg key。  
但是有时候可能一个用户名或者邮箱对应多个gpg key，它就会将对应的都列出来，因为，我们可以灵活地对它们进行操作，如果你确定只想指定一个gpg key(通常在脚本中希望如此)，就直接使用key值，这个一定是唯一的。   

### 查看key
gpg key在生成之后被保存在~.gnupg/目录下，我们可以直接操作里面的文件，但是，如果你不是非常熟悉gpg，最好还是使用命令来进行操作，毕竟这才是相对安全的做法。  

首先，我们可以使用下面的指令查看对应的gpg key：
```
gpg --list-keys
gpg --list-key $name
```
命令选项 **--list-keys**表示列出当前用户下的所有gpg key。  
而 **--list-key $name**则列出绑定了对应用户名的gpg key，这个name是在生成gpg key的时候输入的Real name。  

在当前用户下只存在一个gpg key时，这两个指令都是一样的，在我的电脑上是这样的结果：
```
pub   4096R/FBB436FE 2019-05-30
uid                  huangdao (nothing) <1148256554@qq.com>
sub   4096R/DF4343C8 2019-05-30
```
其中，可以看到，刚生成的gpg key是有两个的，一个pub一个sub。  
事实上，sub为密钥对的字密钥对，是的，这个gpg key在生成时就有两份密钥对，这是gpg key的一种保护机制，在后文中我们会继续谈到。  
***  
如果需要查看key的详细信息，我们可以用以下指令进行编辑状态查看：
```
gpg --edit-key $name
```
它的输出是这样的：
```
pub  4096R/FBB436FE  created: 2019-05-30  expires: never       usage: SC  
                     trust: ultimate      validity: ultimate
sub  4096R/DF4343C8  created: 2019-05-30  expires: never       usage: E   
[ultimate] (1). huangdao (nothing) <1148256554@qq.com>
```
其中显示了大部分gpg的创建信息。   
这里需要特别关注的一点就是它的usage字段，这个字段表示当前key的作用：
```
S - sign                           表示签名 
E - encryption                     表示加密
C - creating certificate           表示创建证书
A - authentication purposes        表示用作认证
```
可以明显看到，默认情况下主密钥对是不负责加密工作的，而子密钥对才负责加密，这又是为什么？见后文。  

### 导入key
 


更多细节可以参考[官方文档](https://gnupg.org/documentation/manuals/gnupg/#toc-A-short-installation-guide)    










