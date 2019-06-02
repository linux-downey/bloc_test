# linux下deb包的制作(1) -- deb包制作示例
在上一章节中，对deb包的使用、其内部结构做了简单的介绍，旨在建立一个基本的概念。  

在这一章节中，我们开始尝试制作一个 hello world deb包，以一个简单的示例入手来看看deb包是怎么被建立的。  

在这个示例中，我们使用二进制包的形式发布，事实上，大多数的deb包都是以这种形式发布的。  

## 准备可执行文件
源文件一共有三个：main.c share_lib.c share_lib.h
实现的功能为：
* share_lib.c中定义func(),函数执行结果为：打印"hello wolrd"。    
* main.c 调用share_lib.c中的func()函数  
* 将share_lib.c、编译成动态库，将要安装到系统目录。  

源码是这样的：  
main.c:
```
#include "share_lib.h"
void main(void)
{
    func();
}
```

share_lib.c:
```
#include "share_lib.h"
void func(void)
{
    printf("hello world!\r\n");
}
```

share_lib.h:
```
#include <stdio.h>
void func(void);
```

编译指令：
```
gcc -shared -fPIC share_lib.c -o libmk_deb_lib.so
gcc main.c -L. -lmk_deb_lib -o hello_world
```

编译得到的文件为两个：
```
hello_world    libmk_deb_lib.so
```

### 数据文件放置目录
在当前示例中，与程序相关的数据文件一共有两个：

    hello_world  libmk_deb_lib.so
当涉及到文件的安装时，我们将hello_world安装到/usr/bin目录下，而将libmk_deb_lib.so安装到/usr/lib目录下，遂创建两个目录:

    mkdir -p usr/lib
    mkdir -p usr/bin
然后将hello_world、libmk_deb_lib.so放置在对应的目录下。  

同样的，如果需要安装其他的配置文件到系统目录中，比如安装配置文件到/etc/中，就创建一个相应的etc/目录。  

## 创建并完善控制文件
在上一章节中提到，deb包主要包含控制文件和数据文件，数据文件已经准备好，我们就来完善控制文件。  

首先，创建DEBIAN目录：
```
mkdir DEBIAN
cd DEBIAN
```

在DEBIAN目录下创建在上一章节中提到的必要的文件：changelog control copyright.   

为了演示需要，再添加两个脚本文件：postinst 和 postrm，分别在安装时调用和卸载时调用。  
```
touch  changelog   control   copyright   postinst   postrm
```

然后对每个文件进行编辑：

### control
这个文件算是制作deb包过程中最重要的文件之一了，包含了很多供 dpkg、dselect、apt-get、apt-cache、aptitude 等包管理工具进行管理时所使用的许多变量，使实现deb包管理的主要文件。   

在当前示例中，control文件内容是这样的：  
```  
1 Source:mkdeb
2 Priority:optional
3 Maintainer:downey
4 Build-Depends:diodon
5 Standards-Version:4.0.0
6 Version:1.0.0
7 Package:mkdeb
8 Architecture:amd64
9 Description:This package just for test!
10     just print hello world!
11 
```  
#### 示例中字段
第一行：Source ： 软件包名，且仅是软件包名，不包含版本架构之类的信息。  
第二行：Priority ：描述了该软件包的优先级，软件包的优先级有：required(系统必须)、important(重要)、standard(标准，默认安装)或者optional(可选，默认不安装，需自行安装)，常规优先级(不与其他包冲突)的包可以改为optional，extra这个选项已经被弃用了，可以用optional来代替。
第三行：Maintainer： 维护者信息，可以添加姓名和电子邮件地址。  
第四行：Build-Depends ： 列出编译该软件包需要的其他软件包，这里的diodon只是一个演示，并不起实际作用，而对于某些归属于build-essential的软件，例如gcc 和 make等就不需要列出。  
第五行：Standards-Version ： 软件所依据的debian标准版本号
第六行： Version ：当前deb包的版本号。  
第七行：Package ：二进制包的包名
第八行： Architecture ：目标机器上的运行架构，由于当前软件包是在amd64平台上编译的二进制包，所以只支持amd64平台.除了指定对应的目标平台以外，该选项还可取值为：any、all。一般而言，源码、文本、图像或者解释型语言脚本独立与体系架构，可以使用all和any，如果值为all，那么将强制软件包只能编译平台无关的包。       
第九行： Description ：当前包的描述，短描述直接写在第一场，更详细的描述可以另起一行。  

#### 更多字段
下面是上述示例中未列出但较为重要的选项：
* Section：描述该软件包要进入发行版中的分类，你可能曾经有注意到，debian仓库被分为几个类别：main(自由软件)、non-free (非自由软件)以及 contrib (依赖于非自由软件的自由软件)，在这些大的分类下还有多个逻辑上的子分类，通常这个选项并非必须，但是有助于deb软件的分发。  
* Package interrelationship fields ：这不是一个选项字段，这一部分要讲的是一系列的软件字段，其中包含Depends、Recommends、 Suggests、Pre-Depends、Conflicts、Breaks、Provides、Replaces。不难理解的是，系统中的软件包不都是独立存在的，一个包与另一个包会存在或多或少的关系，这一系列软件包展示了debian软件系统中最重要的特性，它指示了每个软件包与其他软件包之间的关系，下面逐一讲解。  
    * Depends：此软件包仅当它依赖的软件包均已安装后才可以安装。如果你的软件包有必须依赖软件包，写在这个列表之后，如果没有要求的软件包该软件便不能正常运行。
    * Recommends ： 这一项并不是严格意义上必须安装才能保证运行。但是推荐安装以获得当前安装包更完整的功能。  
    * Suggests ： 此项中的软件包可以和本程序更好地协同工作，但不是必须的
    * Pre-Depends ： 在此项列出的安装包中，仅当这些安装包已经安装并且正确配置的情况下，才能正常安装。与Depends不同的是，Depends针对运行，而此项针对安装。  
    * Conflicts ： 表示有冲突的安装包，仅当所有冲突的软件包都已经删除后此软件包才可以安装。
    * Breaks ： 同样带有冲突性质，此项中列出的软件包将会损坏当前软件包。  
    * Replaces ： 当你的程序要替换其他软件包的某些文件，或是完全地替换另一个软件包(与 Conflicts 一起使用)。列出的软件包中的某些文件会被你软件包中的文件所覆盖。
* Homepage ： 放置上游项目首页的URL，该URL提供安装信息。  
* Essential ： 这个字段是一个bool值，如果值为yes，这个软件包将只能升级替换，而不能被移除，当不指定该选项时，默认为no。  
* Installed-Size ： 提供一个预估的包大小，提示安装需要的大概磁盘空间，这个大小可能根据文件系统的不同而不同。  
* Files ： 该字段提供所有文件的大小以及md5值，它的格式是这样的：
```
Files:
 4c31ab7bfc40d3cf49d7811987390357 1428 text extra example_1.2-1.dsc
 c6f698f19f2a2aa07dbb9bbda90a2754 571925 text extra example_1.2.orig.tar.gz
 938512f08422f3509ff36f125f5873ba 6220 text extra example_1.2-1.diff.gz
 7c98fe853b3bbb47a00e5cd129b6cb56 703542 text extra example_1.2-1_i386.deb
```
* Checksums-Sha1和Checksums-Sha256：提供包内所有文件的sh1和sha256签名。格式是这样的：
```
Checksums-Sha1:
 1f418afaa01464e63cc1ee8a66a05f0848bd155c 1276 example_1.0-1.dsc
 a0ed1456fad61116f868b1855530dbe948e20f06 171602 example_1.0.orig.tar.gz
 5e86ecf0671e113b63388dac81dd8d00e00ef298 6137 example_1.0-1.debian.tar.gz
 71a0ff7da0faaf608481195f9cf30974b142c183 548402 example_1.0-1_i386.deb
Checksums-Sha256:
 ac9d57254f7e835bed299926fd51bf6f534597cc3fcc52db01c4bffedae81272 1276 example_1.0-1.dsc
 0d123be7f51e61c4bf15e5c492b484054be7e90f3081608a5517007bfb1fd128 171602 example_1.0.orig.tar.gz
 f54ae966a5f580571ae7d9ef5e1df0bd42d63e27cb505b27957351a495bc6288 6137 example_1.0-1.debian.tar.gz
 3bec05c03974fdecd11d020fc2e8250de8404867a8a2ce865160c250eb723664 548402 example_1.0-1_i386.deb
```

更多control文件字段细节可以参考[官方文档-control](https://www.debian.org/doc/debian-policy/ch-controlfields.html)   

### changelog
对于changelog字段，也有其固定的格式，这可能跟我们的日常习惯不太一致。   

对于我们平常的使用，changelog所起到的作用主要是记录历史变更，只要保证这个文件中的内容清晰易懂即可，但debian包管理系统它提供的自动管理机制能更方便地对历史变更进行操作，要付出的代价就是我们要遵循它的语法。  

在当前示例中，使用的changelog文件是这样的：
```
mkdeb (1.0.0) unstable; urgency=low

  * Initial release

 downey <linux_downey@sina.com>  Sun,14 Apr 2019 00:00:00 +100

```

下面就系统地讲一下debian包中changelog中的语法：  
```
package (version) distribution(s); urgency=urgency
  [optional blank line(s), stripped]
  * change details
  more change details
  [blank line(s), included in output of dpkg-parsechangelog]
  * even more change details
  [optional blank line(s), stripped]
-- maintainer name <email address>[two spaces]  date
```
***
首先，第一行的内容为：
```
包名 (版本号) 目标发行版(可以是列表);当前修改的重要性=重要性   
```
格式严格按照官方提供的模板，包括分号，空格等。  

***
从第二行开始，除了最后一行，这一部分被视为changelog的真正**描述部分**，以空格或者tab缩进。  

**描述部分**的开头和最后可选地添加一个空行，建议添加，这样可以使真个文本更加清晰，毕竟changelog的目的就在于清晰地表达变更历史。    

描述正文部分需要以* 开头，再添加相应的变更描述，更详细的描述可以另起一行。  

***
最后一行的内容为：
```
维护者名称 <邮箱地址>  日期
```
需要注意的是，邮箱地址需要以<>包含，而日期格式为：
```
day-of-week, dd month yyyy hh:mm:ss +zzzz
```
这一部分参考上面的示例就可以理解，唯一需要解释的就是+zzzz，这一部分表示时区信息。  

更多changelog文件字段细节可以参考[官方文档-changelog](https://www.debian.org/doc/debian-policy/ch-source.html#s-dpkgchangelog)







### copyright
每个包都必须伴随着copyright文件，copyright文件是没有具体的语法要求的，作者仅需要把License声明放在里面即可。  



## 使用dpkg-deb指令创建deb包
到这里，整个deb包的准备部分已经完成，现在整个目录结构是这样的：

    make_deb_test/
    ├── bin
    │   └── mk_deb
    ├── DEBIAN
    │   ├── changelog
    │   ├── control
    │   ├── copyright
    │   ├── postinst
    │   └── postrm
    └── usr
        └── lib
            └── libmk_deb_lib.so

然后我们回到编译deb包的root目录(也就是make_deb_test存在的目录)，使用下面的指令编译deb包：
```
dpkg-deb -b make_deb_test/ debhello_1.0.0-1.0_amd64.deb
```
dpkg-deb -b指令会根据提供的源码目录创建一个deb包，当然前提是该源码目录下存在上述提到的必要的文件。  

接下来我们可以使用dpkg命令对该deb包进行操作了。  



### 源码地址
[点击这里](https://github.com/linux-downey/mk_binary_deb_demo) 获取本示例中源代码

参考:[官方手册](https://www.debian.org/doc/manuals/maint-guide/dreq.zh-cn.html#changelog) 



