# apt-get简介
在Ubuntu系统中，经常要用到apt-get install指令来安装软件，由于常常需要root权限来操作，所以搭配sudo食用口感更佳，apt-get指令对于安装、卸载、升级软件提供一条龙服务，对比于源码安装，实在是业界良心。
### 源码安装
源码安装的流程一般是三部曲：

    ./configure
    make
    make install
* ./configure是为了检测目标安装平台的特征，并且检查依赖的软件包是否可用或者是否缺少依赖软件包，configure事实上是个脚本，最终的目的是生成Makefile。
* 如果第一条指令没有报错，会生成一个Makefile，make指令就是编译这个源码包
* 正常编译完之后如果没有报错，就生成了可执行文件，make install指令就是将可执行文件放到指定目录并配置环境变量，允许我们在任何目录下使用这个软件。
#### 源码安装的优点
对于普通用户而言，实在是想不到什么优点...  
对于软件开发者而言，可以拿到源码阅读学习并修改，geek一个软件简直比找女朋友还好玩！同时也可以在一定程度上防止黑客的攻击(你知道这个版本的漏洞，但是老夫已经把它修复了！！！)
#### 源码安装的缺点
其实三部曲的安装还是不那么麻烦的，前提是不报错！一旦报错，对于linux初学者而言，那也真是丈二摸不着头脑，然后各种百度各种google，按照各种江湖术士的方法来整，结果把系统整崩的情况数不胜数，即使当时能用，但是也有可能留下在以后的使用中经常出现莫名其妙问题的隐患，让我们来看看这些问题都是啥样的：
* ./configure报错： 如果检查到缺少依赖或者依赖文件的版本不匹配呢？一般出现这种情况，就自己解决吧，一般的做法是，升级软件包或者安装缺少的依赖软件包，运气好的话，解决报错的依赖问题就行了，运气不好的话，A软件包依赖B，B又依赖C.....这是比较常见的linux劝退方式，从入门到放弃！
* make报错，由于源码包的形式多是个人用户更新维护的，所以可能出现一些平台没测试到位或者在特定平台上程序出现bug的情况，这种情况就没办法了，如果你有能力debug那当然另说
* make install 报错，这个指令报错的形式一般仅仅是没有权限，加上sudo就行。但是同时因为源码包多由个人维护，也经常可能出现造成系统垃圾的情况，又或者你需要卸载的时候 make uninstall指令仅仅卸载可执行文件，其他配置文件和依赖文件不作处理。

### apt-get指令管理安装包
在上面说了那么多源码安装的缺点，聪明的盆友就要猜我我将要引出今天的主角：apt-get，由apt-get管理的软件包可以轻松做到一键安装卸载。废话不多说，我们先看看它的常用用法：

    sudo apt-get install XXX
    sudo apt-get install -y XXX
    sudo apt-get install -q XXX
    sudo apt-get remove XXX
    sudo apt-get purge XXX
    sudo apt-get autoremove
    sudo apt-get update
    sudo apt-get upgrade
    
#### apt-get install
一键安装软件包，与源码安装不同的是，这个指令会自动检测并安装依赖，而且用apt-get安装的包都是成熟的软件包，基本不存在安装包有严重bug或者文件缺失的情况。
#### sudo apt-get install -y
这里主要将的就是-y选项，添加这个选项就相当于不需要重复地确认安装
#### sudo apt-get install -q
即-quiet，静默安装，当然也不是完全静默，会将低等级的log信息屏蔽。
#### sudo apt-get remove
既然有安装就会有卸载，remove指令就是卸载，值得注意的是，remove仅仅卸载软件，但是并不卸载配置文件
#### sudo apt-get purge
卸载指令，同时卸载相应的配置文件
#### sudo apt-get autoremove
关于这条指令，官方解释是这样的：

    autoremove is used to remove packages that were automatically installed to satisfy dependencies for other packages and are now no longer needed
在卸载软件的时候同时卸载那些当初作为依赖但是现在并不需要的包。  
看起来非常完美的指令，但是博主建议慎用！！这条指令很可能将你要用的依赖包同时卸载，有时候你的安装包并没有通过apt-get指令来管理，apt-get管理工具不会加入这些包的信息，所以在检索包的依赖关系时可能出问题，甚至导致宕机。

#### apt-get update
将所有包的来源更新，也就是提取最新的包信息，这一条我们经常使用到。

#### apt-get upgrade
这条指令一般执行在apt-get update之后，它的作用是将系统中旧版本的包升级成最新的，慎用！  
因为在linux下，由于大部分为非商业软件，所以稳定性并没有得到很好的验证，升级到最新版本需要十分慎重！

##apt-get执行原理
