# armv7-A系列-总览
回头看看工作也三年多了，接触 linux 一年多，时间倒是过得飞快，技术却没有达到理想的状态，这也跟之前走了不少弯路有关，也算是个遗憾吧。    

在接触 linux 之前一直做单片机相关，也做一些应用软件，想想之前对于底层的态度，总觉得造轮子是没有意义的事，我只要能用会用接口不就可以了吗，后面随着技术的深入，才发现，底层永远是绕不过的坎儿，这玩意确实就是地基，尤其是在越来越深入到linux内核的过程中，没有扎实的底层知识根本寸步难行，所以只好又回过头来学习芯片架构指令集的知识，哎，又算是走了一次弯路吧，回头填以前埋下的坑。   

幸运的是，这一次的底层之旅确实是解决了之前遗留的非常多的问题，同时弄清楚了以前觉得懂但实际上不懂的知识点。   

而且，最让我很开心的一件事是发现我可以在这件事中找到一些满足感和成就感，发现自己开始喜欢这份工作了，这样说起来很奇怪，难道自己喜欢什么不喜欢什么自己都不知道吗？至少对于之前的我确实是这样的，其实对于现在的我也未必能看透这些事。虽然在之前的面试或者交谈中我总是说我喜欢这个行业，我喜欢搞技术，但是那多多少少有点欺骗面试官、甚至欺骗自己的行为，在真正了解一个行业之前，说喜欢和不喜欢其实多多少少带一些臆测。  

这可能就像谈恋爱吧，刚谈的时候自然是喜欢的，过了几年再回头看，才清楚当时到底是见色起意还是真正灵魂相投，成长除了更清晰地认识世界，同时也是认识自己的过程。  

蛋扯得有点长了，可能经常写东西的人潜移默化地就会带着更多的反思，啰嗦了一点希望见谅。  

## 内容概览
整个系列其实包含几个部分：
* armv7-A 架构的介绍，主要介绍指令集，当然，指令集涉及到很多其它东西也是要详述的，比如内部寄存器、比如指令编码。相对于 armv8 来说，armv7 要简单很多。  
* 程序的编译链接，包括链接脚本、aapcs、elf 格式，主要是要弄清楚程序是怎么运行的，这一部分知识需要依赖于对 CPU 架构的理解。
* linux 的整个中断系统实现，其实当时决定要研究底层就是因为在研究中断和启动的时候发现绕不过底层这道坎。  
* U-boot 实现，主要针对 Uboot 重定位之前的 boot-up，这部分涉及到的底层知识非常多，至于后面的应用实现部分，有时间就研究研究。  

这几个部分可能需要花上几个月的时间，也是非常值得的。  





