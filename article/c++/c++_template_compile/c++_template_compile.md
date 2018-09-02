# C++模板不支持分离编译的深度解析
## 前言
在我初入程序员这行时，因为学生阶段只写一些简单的考试题，所以经常是将声明和实现统一写到一个文件中，导致同事在用我的代码时一脸懵逼，因此还有一段悲惨的往事。
### 为什么代码需要分离编译
通常来说，在写C/C++代码是，一般是将函数的声明放在.h文件中，而函数的定义放在.c/.cpp文件中，然后将.h文件包含在.cpp文件中，首先，什么是文件包含。
#### 文件包含
在写第一个程序的时候，我们都会写下诸如这种包含：

    #include <stdio.h>
这就是一种文件包含，意为将stdio.h中的所有声明在预处理阶段全部拷贝到本文件中。
***
#### 分离编译的原理
从编译的角度来说，声明文件在编译的时候不会为其分配空间，而在编译定义文件，即.c/.cpp文件时，编译器为每个函数，全局变量分配内存空间。  
在程序的预编译、编译和汇编阶段，如果程序中调用了某个函数，需要这个函数的声明或者实现，如果仅有函数的声明而没有实现，编译器就会在目标文件中做一个标记，告诉链接器在链接的时候需要去找这个函数的声明，把问题移交给链接器，如果链接器找不到，就会报错：undefined reference。  
从这个原理出发，一个.h文件可以被多个文件包含，前提是这个.h文件没有定义行为，如果有类似于

    int val=0；
    或者
    void func(){}
这种定义行为，这个.h文件在被两个及以上文件包含时，在编译时就会出现val或者func()被两个文件同时定义，链接器在对目标文件进行链接的时候就会发现同时有两个val或func()，就是重复定义行为，而声明不会分配内存空间，即不会存在于目标文件中，多个声明并不会影响编译过程。  
同时，将声明和定义分离编译有利于程序的移植和复用。  

***
## C++模板的分离编译问题
如果说C++实现泛型编程的方法是什么，熟悉C++的人都会脱口而出：模板。  
是的，利用C++模板的特性，可以轻松写出高移植性，高复用性、好维护的代码，但是在这里，我要说的是，C++的模板并不支持分离编译。  
首先，给出一个示例：
定义三个文件：test.cpp,test.h,main.cpp:
test.h:

    #include <iostream>
    using namespace std;
    template<class T1,class T2>
    void func(T1 t1,T2 t2);
test.cpp:

    #include "test.h"
    using namespace std;
    template<class T1,class T2>
    void func(T1 t1,T2 t2)
    {
        cout<<t1<<t2<<endl;
    }
main.cpp:

    #include "test.h"
    int main()
    {
        string str("downey");
        int i=5;
        func(str,5);
    }
标准的分离编译模式：在test.h中声明func(),在test.cpp中定义func()实现，在main.cpp中包含test.h头文件并应用func().  
键入编译命令：

    g++ test.cpp test.h main.cpp -o test
结果却是：

    /tmp/ccqLWRwf.o: In function `main':
    main.cpp:(.text+0x6c): undefined reference to `void func<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, int>(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, int)'
    collect2: error: ld returned 1 exit status
非常熟悉的undefined reference to错误。  
那么，既然我已经定义了func()函数，为什么还会报错呢。

### 模板的编译模式
让我们从编译器的角度出发来观察模板编译的问题，做一些假设：
#### 假设1：如果我只用test.cpp和test.h文件来编译出目标文件会发生什么？
首先，这种方式是可行的，用g++提供的-C参数对目标文件只编译不链接：

    g++ -C test.cpp test.h -o test.o
生成的test.o文件，用linux下的 nm 命令查看目标文件符号表：
（关于nm命令的应用可以查看我的另一篇博文）

    nm -C -n test.o
结果是：

    U __cxa_atexit
    U __dso_handle
    U std::ios_base::Init::Init()
    U std::ios_base::Init::~Init()
    0000000000000000 t __static_initialization_and_destruction_0(int, int)
    0000000000000000 b std::__ioinit
    000000000000003e t _GLOBAL__sub_I_test.cpp
丝毫看不到func()的影子，也就是说编译出的二进制文件中根本没有为func()分配内存空间，这是为什么呢？
答案是：模板只有在使用的时候才进行实例化  
通俗地说，用户写一个模板定义，模板本身提供任何数据类型的支持，而编译器在编译出的目标文件中只支持确定的类型例如func(int,int),func(string char)，而不能支持func(模板类，模板类...)（***注意模板是C++的特性而非编译器的特性***）,那么编译器在编译的时候根本不知道用户要传入什么样的参数，所以无法确定模板的实例，所以编译器只能等到用户使用此模板的时候才能进行实例化，才能确定模板的具体类型，从而为其分配内存空间，像上述例子中，模板没有实例化，编译器也就不会为模板分配内存空间。
#### 那为什么在同时编译test.cpp test.h main.cpp的例子中，模板在main函数中有进行实例化，还是会提示无定义行为呢？
事实上编译器是对所有的.cpp文件分开编译的：  
main.cpp依赖test.h,就会将main.cpp和test.h编译成main.o目标文件
test.cpp依赖test.h，将test.cpp和test.h编译成test.o目标文件
然后链接器将main.o和test.o以及一些标准库链接成可执行文件。
但是由上的分析得知，在编译test.o文件过程中编译器并没有将func()实例化，也就是没有func()的定义，所以在链接的时候出现未定义行为。
#### 那如果我在.cpp文件中将模板实例化呢？
在以上的示例中，我在test.cpp中添加一个func()的调用：
    void func1(string str,int val)
    {
	    func(str,val)	
    }
即test.cpp变成：

    #include "test.h"

    template<class T1,class T2>
    void func(T1 t1,T2 t2)
    {
        cout<<t1<<t2<<endl;
    }

    void func1(string str,int val)
    {
        func(str,val);	
    }
main.cpp保持不变：

    #include "test.h"
    int main()
    {
        string str("downey");
        int i=5;
        func(str,5);
    }
这时候再进行编译：

    g++ test.cpp test.h main.cpp -o test
竟然可以编译通过，而且键入命令

    ./test
可以正常运行，这个例子说明只要我在定义文件中进行了实例化，编译器就会为这个模板分配内存空间
#### 编译器是否能完全地支持这个模板函数呢？
在上述例子中需要注意到的是，在模板定义文件中使用的是func(str,int)，在main.cpp中使用的也是func(str,int),那在main.cpp中是否也能支持模板的其他重载函数呢？例如func(string,char)或者func(string,string)
对此，需要修改main函数为：

      #include "test.h"
    int main()
    {
        string str("downey");
        string str_tmp("1234567");
        func(str,str_tmp);
    }
结果是：

    /tmp/ccBe2hDd.o: In function `main':
    main.cpp:(.text+0xc2): undefined reference to `void func<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >)'
    collect2: error: ld returned 1 exit status
结果很显然，无定义行为。  
对此，我们再用nm指令查看目标文件test.o中的符号表：

    0000000000000000 W void func<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, int>(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, int)
    0000000000000000 T func1(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, int)
    0000000000000000 b std::__ioinit
    0000000000000087 t __static_initialization_and_destruction_0(int, int)
    00000000000000c5 t _GLOBAL__sub_I__Z5func1NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEi

发现符号表中有func(string,int)和func1()函数，却并没有func(string,string)的函数存在，所以，调用func(string,string)将会出现未定义行为，如果需要用到func(string,string)，就得在test.cpp文件中将func(string,string)实例化一次，也就是使用一次。
##  模板编译问题的解决方案
***注：虽然这里以函数模板为例，但应用于类模板是一样的***
### 解决方案1：提前实例化模板
看到这里，各位观众们应该大概能想到一种分离编译的解决方案了，***在模板定义文件中将所有要用到的模板函数都进行实例化***，不过说实话，这很扯蛋，而且完全不符合程序的美学，设计泛型接口本身就是为了多态，并不需要知道调用者以什么方式调用，这样实现的话接口设计者就得知道调用者的所有调用方式！但事实上是确实可以这么做。
### 解决方案2：定义实现全部放在同一个.h文件中
第二种解决方案就是将模板的定义和实现全部放到一个.h文件中，为什么这样又可以呢？  
当某个.cpp文件用到某个模板时，包含了相应的头文件，就对这个模板进行了实例化，这样就可以使用模板了。这也是常见的做法，STL就是这样实现的，遗憾的是，这违背了分离编译的思想。   


好了，关于C++模板分离编译的问题就到此为止了，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
（完）