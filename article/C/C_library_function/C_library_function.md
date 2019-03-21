# C语言常用库函数的实现
在本篇博客中，将讲解常用C语言库函数的实现，这些库函数都是由C语言专家编写，虽说看起来函数的实现非常简单，但是其中蕴含着C语言的精粹。同时，手写C库函数是面试中常用的笔试方法。  

**(注：本文参考glibc，可点击(glibc官方源码)[http://ftp.gnu.org/gnu/glibc/]进行下载。)**

## 编程的流程
在开始编程时，首先我们得先理清楚编程思路，然后再动笔，这个道理每个人都知道，但是却经常会被忽略。为了说明这个，博主举一个自己的反面教材。  

### 惨痛的教训
曾经在一次比较重要的面试中，作为两年工作经验的我犯了甚至新手都不会犯的一系列错误，记得当时面试官让我实现strcmp()函数，我的做法是这样的：

* 接过笔直接写，将函数定义写错，事实上，当时的我还不完全知道strcmp()是做什么用的。  
* 由于有一些紧张，完全没有清晰的思路，想到哪里写到哪里。  
* 经过面试官的部分提醒，最终实现出来一个初级版本，写完之后面试官告诉我strcmp()并非只返回-1和0，会根据两个字符串的对比返回-1,0,1。

结果可想而知，面试仅仅花了15分钟，我就收拾东西回家了。  

其实，在整个过程中，我丝毫没有做提前将实现的思路整理出来的规划，犯了一些错：
* 第一次犯的错是拿过笔就写
* 第二次犯的错是经过面试官提醒，先理清思路再写，由于紧张思路完全不清晰
* 第三次犯的错是直到最后，经过面试官的多次提醒，还是没有把最基本的东西确认下来：这个函数实现了什么？

当时的我难道不懂要先规划再写吗？不是的，甚至我经常庄重地告诫我带的新员工：一定要先整理思路再写。  

但是在面试中还是会有极其差的表现，这就是知道和做到的区别，我们要做的就是通过一些训练让一些基本的东西成为习惯，而不是我觉得我知道了。  

这样即使在高压环境下，习惯的力量也不会让你把事情弄砸。  

### 事后总结
做错了事情，总结是必须的，那么如果时光倒流，我能怎么做呢？  
* 首先，我对strcmp()的完整实现有点模糊，先清楚地告诉面试官对这个函数定义不清晰，请其为之讲解一下。  
* 其次，即使之前没有写过，先整理思路，整理出第一步第二步，这样写出来即使拿不到满分，每一步做出来也能收获一个好印象。  
* 关于面试有点紧张的问题，事实上在所有高压环境下都会紧张，紧张大多是因为自身实力不够，这种调节心态需要长期的实战锻炼来克服，同时练就扎实的基本功也可以无所畏惧。  

希望大家引以为戒，不要犯博主这种低级错误。  

接下来步入正题，讲解glibc中各个库函数的实现。  
## 各个库函数的实现
### strlen()
strlen()是非常常用的一个库函数了，它的作用是计算并返回一个字符串的长度，函数实现的原理非常简单：遍历整个字符串并计数，返回计数。  

**函数原型**：

    size_t strlen(const char *s); 
传入一个字符串，返回字符串的长度。  

**思路**：  

* 判断传入参数是否合法
* 遍历整个字符串，返回长度

### 代码实现

    size_t strlen(const char *s)
    {
        int len = 0;
        if(NULL == s)
            return 0;
        while(*s++)
            len++;
        return len;    
    }
函数实现很简单，基本上没有什么坑，唯一值得注意的就是传入的参数需要以const修饰，表示源字符串不可修改。  

对于开头的指针检测，不同平台上的实现有些许区别，在VC6.0(VC编译器)的平台上，strlen(NULL)导致程序崩溃，而ubuntu上(gcc编译器)strlen(NULL)返回0.   
如果要编写可移植性代码，最好提前做指针检查，不要试图依赖strlen()提供的检查。  

***
***
## strcat()
strcat()函数连接两个字符串，返回连接后的字符串。  

**函数原型**：

    char *strcat(char *dest, const char *src);
传入两个字符串dest和src，即将src添加到dest的结尾，返回dest。
  

**思路**：  

* 判断传入参数是否合法
* 递归到dest结尾，将src字符串逐一连接到dest结尾处。 
* 返回dest

**代码实现**

    char *strcat(char *dest, const char *src)
    {
        char *pdest = dest;
        if(NULL==dest || NULL==src)
            return NULL;
        while(*pdest)
            pdest++;
        while(*src)
            *pdest++ = *src++;
        *pdest = '\0';
        return dest;
    }

**易错点**
将src字符串连接到dest字符串结尾，主要要注意以下几点：
* 删除dest字符串中的\0字符，然后拼接src字符串，最后需要再添加上\0字符。  
* 在使用时，需要注意dest字符串buf要足够容纳两个字符串。  
* 并没有处理两个指针指向同一个字符串的情况，所以不能传入两个相同指针或者是指向同一个字符串的不同指针。对于这一点，VC和gcc表现一致，传入指向同一字符串的指针将导致程序崩溃，你也可以从上述函数中分析为什么将导致崩溃。  

还有一个在字符串操作中经常出现的错误，很多人会把代码写成这样：

    char *p1 = "hello";
    char *p2 = "world";
    strcat(p1,p2);
这个代码可能可以编译通过，但是是无法执行的。在C语言中，所有定义的字符串字面值被放在只读数据区，这些字符串是只读不可写的，所以上述示例中应该把p1定义成数组。  


***
***
## memcpy()
memcpy()实现内存的拷贝。

**函数原型**：

    void *memcpy(void *dst, const void *src, size_t n); 
传入两个void*型指针，并传入需要copy的数据长度，返回copy的内容。  

**思路**：  

* 判断传入参数是否合法
* 考虑两种情况：内存重叠和内存不重叠的情况。  
* 当src地址在dst地址后且出现地址重叠，或者不存在地址重叠时，可以从前向后拷贝。
* 当src地址在dst地址前且出现地址重叠，需要从后向前拷贝。  



**代码实现**：  

    void *memcpy(void *dst, const void *src, size_t n)
    {
        char *pdst = (char*)dst;
        char *psrc = (char*)src;
        if(NULL==dst || NULL==src)
            return NULL;
        //src地址在dst后，无论是地址重叠还是不重叠都可以从前向后拷贝
        //或者当src地址在dst前，但是不发生地址重叠
        if(src > dst || (char*)src+n < dst)   
        {
            while(n--)
                *pdst++ = *psrc++;
        }
        //或者当src地址在dst前，发生地址重叠，需要从后向前拷贝。
        else
        {
            pdst = (char*)dst + n - 1;
            psrc = (char*)src + n - 1;
            while(n--)
                *pdst-- = *psrc--;
        }
        return dst;
    }

**易错点**
* 地址重叠部分的分支需要重点去理解
* 对void*型指针，如果需要指针运算，需要进行强制转换

***
***
## strcpy()
strcpy()函数的作用是复制一个字符串，返回复制后的字符串。  

**函数原型**：

    char *strcpy(char *dst, const char *src); 

**思路**：  

* 判断传入参数是否合法
* 遍历字符串中每一个字符，将src赋值给dst

**代码实现**：

    char *strcpy(char *dst, const char *src)
    {
        char* pdst = dst;
        if(NULL== dst || NULL==src)
            return NULL;
        while((*pdst++ = *src++) != '\0');
        return dst;
    }

**易错点**：
1、在上述代码实现中，并没有在dst字符串结尾添加\0字符，是因为

    while((*pdst++ = *src++) != '\0');
这条语句将src中的\0字符同时复制了，不理解的朋友们可以尝试自己试试。  


2、当dst和src指向的内容有重叠时，举个例子:

    char p[20] = "hello\r\n";
    strcpy(&p[2],p);
在这种情况下，VC编译器中将会程序崩溃，而在gcc中运行良好，明显的，VC编译器中并没有考虑到地址重叠的情况，上述代码和VC编译器实现一致。  

究其原因是：在gcc的glibc中，strcpy()直接调用memcpy()，而在memcpy()中考虑到了地址重叠的问题。  

如果在面试中，写出上述代码即可。  

***
***
## strcmp()
strcmp()函数实现了比较两个字符串是否一致，如果一致，返回0。如果不一致，比较第一个不同的字符大小，并根据比较结果返回正数和负数。  

**函数原型**：

    int strcmp(const char *s1, const char *s2); 
传入两个需要比较的字符串，返回int型参数。  

**思路**：  

* 判断传入参数是否合法
* 遍历字符串中每一个字符，比较每一个字符是否相等，同时判断字符串是否结束，如果字符串结束，返回0
* 如果出现其中一个字符不相等，比较大小并返回1或者-1；

**代码实现**：

    int strcmp(const char *s1, const char *s2)
    {
        int ret;
        if(NULL==s1 || NULL==s1)
            return NULL;
        while(!(ret = *(unsigned char*)s1 - *(unsigned char*)s2) && *s1)
            s1++,s2++;
        if(ret > 0)
            return 1;
        if(ret < 0)
            return -1;
        return 0;
    }
**易错点**：
* 首先在while中判断s1和s2中字符大小，为什么要将字符转换成unsigned char类型呢？值得注意的是，并非所有字符编码都在0~127区间，像键盘上的'￥'字符的ASCII码值为164,如果在sign char类型下就是-92，当对比'￥'与'a'时，就会出现相反的结果，除了相减，用'>'或者'<'在不转换的情况下'￥'将被判定为负数，从而小于'a';  
* 判断对比是否结束，只需要判断其中一个字符串s1是否到结束符即可，因为如果只有其中一个字符串到了结尾，第一个判断条件将不会被满足从而跳出循环。  



好了，关于C库中经典函数实现的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.






