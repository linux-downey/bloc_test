# linux下deb包的制作(5) -- gpg的加解密操作
在上一章节中，我们介绍了gpg的基本概念，以及与之对应的基本操作。  

在这一章节中，我们主要来看看gpg的加解密以及签名操作，毕竟这是我们使用gpg的目的所在。  

## gpg的加解密

### 加密
在上一章节中有提到，加密的操作是：使用公钥加密，然后再使用私钥解密，所以，在对文件进行加密时，需要指定对方的公钥和需要被加密的文件，比如：加密文件为test,加密的公钥为上一章节中的FBB436FE，加密的操作就是这样的：
```
gpg --recipient FBB436FE --output test.en --encrypt test
```
这时，生成的加密文件test.en打开就是一堆乱码，需要对方的私钥进行解密才能查看。  


## 解密
解密的过程和加密相反，是将被加密的文件还原到未加密的状态，操作是这样的：
```
gpg --output test --decrypt test.en 
```
在解密的过程中，命令行将提示你输入密码，这个密码也就是你在创建gpg秘钥对时输入的密码。  


## gpg的签名
### 签名
在验证数据的发送方时，通常我们不需要加解密，只需要对其进行签名。下面我们来示范一下签名操作：
* 创建一个文件，由于是示例，我们就创建一个简单的test文件：
```
echo hello_world > test
```
* 使用md5工具对test文件指纹：
```
md5sum test  > sign
```
sign文件的内容为：
```
6e933f3054f533c63dd59479ca9f4b6f  test
```
* 对test文件的指纹文件进行签名：
```
gpg --sign sign
```
将生成一个sign.gpg的文件，这个文件就是test文件的md5sum的签名文件,这个过程需要输入密码。  

通常的，为了方便起见，我们选择生成ASCII形式的签名：
```
gpg --clearsign sign
```
将生成一个，sign.asc文件，这个也是签名文件，由于它是ASCII形式的，我们可以查看这个asc文件：
```
-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA1

6e933f3054f533c63dd59479ca9f4b6f  test
-----BEGIN PGP SIGNATURE-----
Version: GnuPG v1

iQIcBAEBAgAGBQJc8flHAAoJEEH/7AopdX5l3JkP/RFyDRbVzJwH/M2pipS3g7kI
ZmydpGqRb9aGNscmr8kmVkb6QJPeITjO+SpIhIZVn3XKzg4uE6S/fbRiiHCqZTPx
RmBvfUwfOGqMook+B3VNn9SutJKYfJtZSB1P5/IttZke/zTt9DcC1pr1yI6o/jN0
qrJY4TfYurf/10YskbFKP+lY16qDqVSt6W21dl6OB9me6Xn+h8hSRoOOXT4Xq1qh
RmPhJklIK6kpPCHjEnVs0xfv9nYb73GvcdCXmflLTkyIiQ60rRih4XdZGRXayF9h
UHM76eE1rnT8CYDVWPNTyyzcD7vI4eqYeafY8U0pCRmkHVuu5OvHEffmMz5GngcQ
gGcj8Sq6Io9E+G2XiKMdpChof5zzD3kyW1gGda+t2kWcYV5/2z3iseEoig2nlUn+
/QvhNbXH02bct5yp+pQDGX6eZe/I6pJBqs4N4AboqozX3Z7vP56qFNcyw4078IHS
NRT7W1iICfZAY0I1rMt8GEZAP62MpCFjvnqqnxgcAsPkti/teBq88M3CraV0hg6Y
nm7SymBiEsshMUoUbH/sSXlFqxv4k52HB5aYIbghLau9CxYibaR15P1816uBnHdS
vhrd8i0sF+8j6vkvk22wxOpcACtOzoiYXkV4dNFadF5h+Z9dQRfWjGOSQrBvKtBV
wzkPGv8rjnOXOAKQq6Um
=iWQa
-----END PGP SIGNATURE-----
```  
可以看到，被签名的test文件的指纹被放在第一部分，第二部分就是对应的gpg 签名。  

### 验证签名
接收方收到test数据文件、和sign.asc文件。  

验证签名的做法是：
* 首先验证签名是否由目标作者发出，这一步需要使用目标作者的公钥对其签名进行验证：
```
gpg --verify sign.asc 
```
如果验证成功，将会输出以下信息：
```
gpg: Signature made Fri 31 May 2019 08:33:26 PM PDT using RSA key ID 29757E65
gpg: Good signature from "huangdao <1148256554@qq.com>"
  
```
表明文件由作者发出。
***  
如果验证失败，将输出以下信息：
```
gpg: Signature made Fri 31 May 2019 09:04:23 PM PDT using RSA key ID 29757E65
gpg: BAD signature from "huangdao <1148256554@qq.com>"
  
```
表明文件不是由作者发出，丢弃数据包。  
* 在第一步验证成功的前提下，接收者对test文件进行MD5(取决于对方使用的软件)取指纹操作：
```
md5sum test
```
* 对比生成的指纹与sign.asc中的指纹部分是否一致，如果一致，表明文件数据未被修改。  

签名验证的流程有两个部分：
* 验证是否有作者发出
* 是否由第三方对包进行修改

第一点不用讨论，我们主要来讨论第二点：确认包是否被第三方修改。   

首先，如果数据文件test被修改，它的MD5指纹肯定会被改变，那么修改者必须重新生成一份MD5指纹，不然接收者通过asc文件中的指纹对比可以发现test的MD5指纹有问题。   
修改者必须将新的test文件和MD5指针替换掉原来的test和MD5，但是签名是针对MD5指纹的，如果MD5指纹被修改而签名不变，在验证签名时必然会失败。  
同时，第三方没有原作者的私钥，所以也无法生成对应的签名，在这种情况下就保证了安全性。  

至于为什么不直接对所有元数据进行签名，是因为使用类似MD5指纹算法能起到同样的效果，而且能够大大地节省计算量，因为非对称算法是非常耗时的。   



## 子秘钥
在gpg中，还有一个十分重要的特性：子秘钥。  

在上一章节的介绍中有提到，gpg在网络上是可以作为个人信用凭证的，很多业界大牛都会有一个固定的gpg秘钥，以方便他人使用其公钥来与其通信。   

这就决定了我们必须对它的私钥进行非常严密的保护，甚至将它锁在保险箱里(这么说并不为过，放电脑上并不是安全的)，因为，如果你的私钥一旦被窃取，窃取者就可以用你的私钥做一些违法的事。  

那么唯一的做法是废除你之前gpg key，然后重新创建一个gpg key，在自己的个人网站和gpg服务器上重新发布，相当于重新建立自己的信用。
 
所以，你在需要用到gpg私钥的时候，必须从保险箱冲中取出U盘，操作完删除电脑上的记录，再放回保险箱。   

当然，你也可以将它放到电脑上，使用一些加密算法来保存，只要你可以接收私钥丢失的后果。  

可以想到，这是非常不方便的，为了安全性少了便利性，为了便利性而少了安全性，好像是个两难的问题。  

### 子秘钥机制
针对上述的情况，gpg推出了子秘钥的机制,即：在创建gpg秘钥时，默认生成一份主秘钥对和一份子秘钥对，主秘钥对与子秘钥对是严格绑定的。  

子秘钥对同样有加解密以及签名的功能，秘钥作者直接使用子秘钥进行对外的数据传输加解密、签名功能，一旦子秘钥私钥被窃取，使用主秘钥将子秘钥撤销，重新生成一份子秘钥对并使用新的子秘钥，官方机构将对外发布撤销结果，同时也可以发布在个人网站上。  

即使是出现子私钥被窃取，因为主秘钥私钥还在，所以长期以来建立的个人信用并不会丢失，当然，只要是私钥被窃取，总会产生一些负面影响，只是在使用子秘钥的情况下，可以快速响应，而且保全自己的个人信用。    

这样，就可以将主秘钥copy到U盘然后锁到保险箱了。   

既然加解密和签名工作都让子秘钥来做，那么主秘钥主要负责做什么呢？它主要在以下几种情况下会被请出来：  
* 当你签署别人的秘钥或者撤销现有的签名时
* 当你需要创建一个新的UID或者将切换某个UID为主UID时(UID为User ID，即用户名)
* 创建一个新子秘钥对时
* 撤销一个存在的子秘钥对或者一个UID时
* 更改UID时
* 更改主秘钥或者任何其他子秘钥的到期时间时
* 生成一个撤销证书时

除了主秘钥和子秘钥，在创建gpg时需要填写一个密码，这个密码需要在用到私钥时进行验证。  


### 创建子秘钥
默认情况下，gpg秘钥中包含一个主秘钥和子秘钥，如果我们想再创建一个子秘钥，可以遵循以下的操作：
```
gpg --edit-key FBB436FE
```
将会出现以下提示：
```
gpg (GnuPG) 1.4.20; Copyright (C) 2015 Free Software Foundation, Inc.
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

Secret key is available.

pub  4096R/FBB436FE  created: 2019-05-30  expires: never       usage: SC  
                     trust: ultimate      validity: ultimate
sub  4096R/229B2D28  created: 2019-05-30  expires: never       usage: E   
[ultimate] (1). huangdao <1148256554@qq.com>

gpg> 
```

键入：
```
add key
```
输入相应的操作密码，然后就是子秘钥生成的选择界面，这个操作流程与上一章节中创建秘钥过程差不多。  
需要注意的是，在创建完成之后的gpg提示栏中键入：
```
save
```
然后键入q,就退出了gpg key的编辑界面。  

gpg子秘钥创建完成，我们可以使用以下指令查看：
```
gpg --list-key FBB436FE
```
就可以看到在当前gpg key的列表中多了一个sub key。  

紧接着就可以使用sub key进行相应的操作。   




更多详细资料可以参考[官方RFC4880文档：OpenGPG](https://tools.ietf.org/html/rfc4880)  
更多详细资料可以参考[官方文档：Subkeys](https://wiki.debian.org/Subkeys)  



