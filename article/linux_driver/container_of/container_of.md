# linux 驱动程序中 container_of宏解析
众所周知，linux内核的主要开发语言是C，但是现在内核的框架使用了非常多的面向对象的思想，这就面临了一个用C语言来实现面向对象编程的问题，今天我们就来讲讲其中一个例子。  
## 利用结构体中元素指针获取结构体指针  
Kobject是linux设备驱动模型的基础，也是设备模型中抽象的一部分。linux内核为了兼容各种形形色色的设备，就需要对各种设备的共性进行抽象，抽象出一个基类，其余的设备只需要继承此基类就可以了。而此基类就是kobject(暂且把它看成是一个类)，但是C语言没有面向对象语法。  
在C++中这样的操作非常简单，继承基类就可以了，而在C语言中需要将基类的结构体指针嵌入到派生的类中，那么为什么将基类指针嵌入就可以得到派生类的指针呢？  
这个实现是一个宏：container_of。 

## container_of
首先，它的原型是这样的：

    #define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	BUILD_BUG_ON_MSG(!__same_type(*(ptr), ((type *)0)->member) &&	\
			 !__same_type(*(ptr), void),			\
			 "pointer type mismatch in container_of()");	\
	((type *)(__mptr - offsetof(type, member))); })

被定义于/linux/kernel.h中。
***为了阅读和理解的方便，我们在这里先假设基类为Base,而派生类为Derived，显然，派生类Derived中是包含基类Base的定义的，即已知类为Base，未知类为Derived，我们要求解Derived的地址***，由此我们来逐一分析这个宏的实现。

### 参数

    ptr：ptr是已知基类Base的地址
    type: type是派生类的的类型
    member：基类Base在派生类Derived中定义的名字

### void *__mptr = (void *)(ptr);
这一条使用一个临时变量__mptr赋值，仅仅是为了避免对ptr的误操作导致修改了ptr的值，这可不是我们的初衷。思考一下，为什么在这里要使用void类型，保持原类型可不可以？    

### BUILD_BUG_ON_MSG(...);
这一条包含两个操作，首先是使用__same_type宏，其实这个宏名就已经解释了这个宏的作用，即判断传入的两个类型是否一致：

    # define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
__builtin_types_compatible_p是一个內建功能函数，typeof是gcc关键字，这里的作用是判断传入的两个参数类型是否匹配。
然后是BUILD_BUG_ON_MSG宏：

    #define BUILD_BUG_ON_MSG(cond, msg) compiletime_assert(!(cond), msg)
这个宏检测cond(condition)，如果条件不为零，输出msg。
这一部分总体的作用就是检测container_of(ptr, type, member)中，ptr的类型是否和member类型一致，如果不一致，则报错。
### ((type *)(__mptr - offsetof(type, member)))
这一部分就是核心所在。  
首先，使用offsetof找到基类Base在派生类中的偏移地址，原型是这样的：

    #define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)
&((TYPE *)0)->MEMBER：这一个表达式将0地址强制转换成TYPE类型(TYPE是用户传入的派生类Derived的类型)，然后访问member成员，用&获取member成员地址。对C语言不那么熟悉的小伙伴会非常疑惑：
* 不是说0地址是非法地址么，为什么这里可以用？0地址是非法地址没错，但是仅仅是限于不能进行访问操作。
* 为什么经过强制转换的0地址可以访问member成员，显然0地址处并不存在TYPE的实例？这个问题其实就是属于指针以及指针类型的经典难题了，这里简要解释一下：

    首先我们要弄清楚指针的类型以及指针指向数据的类型。
    指针是变量，指针的值为一个地址，指针本身的类型是int型，而指针指向数据的类型则有多种，常见的int*，char*等等
    当我们对一个指针变量进行加减运算时，事实上是以指针指向数据类型的size进行操作，
    比如ptr指向一个int类型，ptr值为0x1000，ptr++是将指针的值+4(32位)，即0x1004,若是指向char型，ptr++就是+1,ptr值为0x1001
    而我们对目标类型进行强制转换，比如ptr本来是指向int型，我们这样操作：((char*)prt)++,这样ptr就临时成为了char*型指针，ptr++的操作时ptr的值只+1，即0x1001.
    即强制转换提供一种临时的类型变换，在强制转换过程中目标地址保持为被转换类型。
    对于结构体而言，结构体指针可以直接通过->来访问某个成员。
    而我们对0地址进行强制转换，就可以看成是0地址处放置了一个目标结构体的实例，然后通过->成员来访问，当然这里并没有访问，如果访问肯定会出错，这里只是取出了member的相对于结构体地址的偏移地址。

刚刚只是介绍了offsetof(type, member)，然后我们结合来看((type *)(__mptr - offsetof(type, member)))：
    __mptr-offsetof(type, member)：即基类ptr在结构体中的地址减去基类ptr在结构体中的偏移地址，就得到了派生类结构体的地址(这一部分涉及到结构体的存储方式)。  
    刚刚我们提到了一个问：为什么在__mptr要使用void类型，保持原类型可不可以？如果你理解了我上面提到的指针部分，这个问题就很好解释了：
如果__mptr为void*类型，__mptr - offsetof(type, member)这个表达式就是__mptr的值减去offsetof(type, member)的值，得到__mptr。  
如果保持原类型，__mptr - offsetof(type, member)这个表达式成了__mptr的值减去 offsetof(type, member)*sizeof(type),得到__mptr,显然，我们要的是第一种。
（参考上面的指针类型简析的第四行）


## 小结
container_of(ptr, type, member)。
ptr为基类在结构体中的地址，
type为派生类类型，
member为基类在派生类中定义时的名字，其实就是为了在宏中直接以member名来访问。
总的来说，就是我们可以通过结构体内元素指针的值获取结构体的值。看到这里，很多朋友基本上已经懂了container_of的用法和原理，其实container_of的用法并不局限于kobject系统中，而是适用于所有基类派生类的模式。


### 示例
上面讲了那么多概念，还是得用实例来说话，下面是博主写的一个小demo：

    1 #include <stdio.h>
    2 
    3 struct Base{                    //定义一个Base类
    4     int var;
    5     char *string;
    6 };
    7 
    8 struct Derived{                 //定义一个派生类
    9     int var;
    10     struct Base base;           //派生类中包含了struct Base类
    11 };

    12 #define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)       //offset_of宏
    13 #define container_of(ptr, type, member) ({				\            //阉割版container_of，省去了类型检查
    14     void *__mptr = (void *)(ptr);                     \
    15     ((type *)(__mptr - offsetof(type, member))); })
    16 
    17 struct Base *base_p;                  
    18 struct Derived test_derived;       
    19 
    20 
    21 struct Derived *get;
    22 int main()
    23 {
    24    base_p = &test_derived.base;                                     //赋值给指针
    25    printf("Derived addr = %x\n",(unsigned int)&test_derived);
    26    printf("=================================\n");
    27    get = container_of(base_p,struct Derived,base);                 //使用base_p指针获取派生类的首地址
    28    printf("get Derived addr = %x\n",(unsigned int)get);
    29
    30    return 0;
    31 }
输出：

    Derived addr = 601050
    =================================
    get Derived addr = 601050


### 你是否真的懂了？
为了考验你是否真的懂了，博主准备了一个问题，欢迎留言：

    第10行的定义中能否换成指针类型？如果换成指针类型，container_of()还能否实现，为什么？
    博主非常建议你动手试试！！！


好了，关于linux驱动中container_of()宏的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
