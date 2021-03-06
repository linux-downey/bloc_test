# 内核Kbuild详解系列-开篇

## 前言  
对于linux的学习而言，我想大部分人遇到的第一个坎就是linux的内核编译了吧，想当初我从windows系统刚开始转而学习linux开发的时候，编译linux的内核就是特别让我兴奋的一件事，觉得自己进入了一个自由的世界：连操作系统源代码都是自己编译的，以后我想怎么玩就怎么玩了。  

随着学习的深入，我逐渐发现了一个linux开发中一个非常残酷的事实，它叫做自由的代价：如果你只是跟着教程敲指令而不知道自己究竟在做什么，linux总是会百般地刁难你，直到你弄清楚原理或者是放弃。   

原来，自由只是对强者而言，而灵活，也意味着复杂。   

既然无路可退，就只能勇往直前。  

## 写在前面的话
本系列博客为内核 Kbuild 系统，包含两个部分：
* Kbuild 系统的使用，通俗来说也就是 linux 内核源码的编译，先了解其基本的使用
* 探究 Kbuild 编译内核源码背后的原理，旨在深入到 Kbuild 系统中，了解 linux 内核编译的背后细节与实现

对于 Kbuild 系统的学习，一方面是为了解惑，知其然而不知其所以然总是一件痛苦的事情，我们知道如何敲一个命令，然后等待返回结果，却不知道这中间发生了什么，长此以往，我们只会成为电脑旁的流水线工人，工作中为了效率尚且可以理解，但是我们应该更多地对事情背后的东西保持强烈的好奇心。  

话说回来，如果你了解了 Kbuild 系统，那么对 linux 的裁剪和移植将会有非常大的帮助。  

另一方面，也是更值得关注的一方面：就是了解系统设计背后的思想，软件的发展日新月异，虽然底层软件框架的更新速度较应用层框架的更新速度慢了很多，但是，我们不应该止步于了解应该怎么做，而是更多地去了解为什么这么做，只有学到内功心法，才能灵活地运用各种招式。  

甚至，到某一天，我们可以为国产操作系统设计它的框架，毕竟，梦也是需要做的。      

## 本系列博客规范

阅读本系列博客之前需要有一定的阅读 Makefile 的基础，了解 Makefile 可以参考博主的另一系列博客：
[Makefile详解系列](http://www.downeyboy.com/2019/05/14/makefile_series_sumary/)

涉及到平台与内核版本的部分，统一使用 arm 平台，与内核版本 5.2.rc4.

同时，本博客研究使用主线内核，即发布在 github 上的内核版本，对于嵌入式开发而言，通常各厂商都会在主线内核版本上有些许修改，相对饮的，Makefile 也会有一些小变化，不过万变不离其宗，了解了主线内核再切换到各发行版，也是非常简单的一件事。  

Makefile 中的一些概念：

**模块**：指内核中的功能模块，通常对应内核源码下的文件夹，比如 网络模块、声卡模块 就对应源码中的 net/ sound/，不一定是根目录下的文件夹才是模块，drivers/ 下也有非常多的驱动模块。  

**外部模块、可加载模块**：主要指以 .ko 结尾的模块，支持在内核运行时动态加载的模块。  

**內建模块**：编译进镜像的模块

**top makefile**:即顶层 Makefile，特指源码根目录下的 Makefile。 

****

## 分篇博客索引

为了各位的查找方便，以下是整个系列博客的链接与描述：



***  



