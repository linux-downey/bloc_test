# C语言强、弱符号，强、弱引用
## 符号定义
在编程中我们经常碰到符号重复定义的情况，当我们在同一个作用域内重复定义同一个变量时，有时是因为误写，有时是文件之间的冲突，编译器的处理方式就是报错：

    redefinition of 'xxx'
注意，这里针对于同一作用域才会有冲突，如果是不同作用域，比如全局和局部，即使是相同变量名，也是不会报错，编译器会默认根据一定的优先级处理，总是更小作用域的变量覆盖更大作用域的变量，前提是这两个变量的作用域是包含与被包含的关系。  
初学C语言的朋友对于作用域的划分有一定的误解，认为函数是划分作用域的界限，函数外的就是全局，函数内的就是局部。  
事实上，在C语言中，作用域的分类方式是代码块作用域和文件作用域，文件作用域即定义在函数之外的的变量可以跨文件访问，代码块作用域指的是由花括号限定的作用域，它不仅仅限于函数体，在C99中将其扩展到了for,while,if语句所控制的代码，甚至可以在函数内以单独的花括号存在，我们不妨看看以下的示例：
示例1：

    int main()
    {
        {
            int x=1;
        }
        x = 2;
    }
编译输出的结果是：

    error: ‘x’ undeclared (first use in this function)
示例2：

    for(int i=0;i<4;i++){}
    i = 1
输出的结果是：

    error: ‘x’ undeclared (first use in this function)
在C语言中，我们可以简单地认为花括号是作用域的分隔符。  

## 强符号和弱符号
在同一作用域下不能定义同一个变量或函数，很多C语言学习者都理所当然地这么认为。  
这个概念其实是错误的，或者说至少是有所偏颇的，在GCC中，对于符号而言，存在着强符号和弱符号之分。  

对于C/C++而言，编译器默认函数和已初始化的全局变量为强符号，而未初始化的全局变量为弱符号，在编程者没有显示指定时，编译器对强弱符号的定义会有一些默认行为，同时开发者也可以对符号进行指定，使用"__attribute__((weak))"来声明一个符号为弱符号。  

定义一个相同的变量，当两者不全是强符号时，gcc在编译时并不会报错，而是遵循一定的规则进行取舍：
* 当两者都为强符号时，报错：redefinition of 'xxx'
* 当两者为一强一弱时，选取强符号的值
* 当两者同时为弱时，选择其中占用空间较大的符号，这个其实很好理解，编译器不知道编程者的用意，选择占用空间大的符号至少不会造成诸如溢出、越界等严重后果。

在默认的符号类型情况下，强符号和弱符号是可以共存的，类似于这样：  

    int x;
    int x = 1;
编译不会报错，在编译时x的取值将会是1.  
但是使用__attribute__((weak))将强符号转换为弱符号，却不能与一个强符号共存，类似于这样：

    int __attribute__((weak)) x = 0;
    int x = 1;
编译器将报重复定义错误。   


## 强引用和弱引用
除了强符号和弱符号的区别之外，GNUC还有一个特性就是强引用和弱引用，我们知道的是，编译器在编译阶段只负责将源文件编译成目标文件(即二进制文件)，然后由链接器对所有二进制文件进行链接操作，在分离式编译中，当编译器检查到当前使用的函数或者变量在本模块中仅有声明而没有定义时，编译器直接使用这个符号，将工作转交给链接器，链接器则负责根据这些信息找到这些函数或者变量的实体地址，因为在程序执行时，程序必须确切地知道每个函数和全局变量的地址。如果没有找到该符号的实体，就会报undefined reference错误，这种符号之间的引用被称为强引用.  
编译器默认所有的变量和函数为强引用，同时编程者可以使用__attribute__((weakref))来声明一个函数，注意这里是声明而不是定义，既然是引用，那么就是使用其他模块中定义的实体，对于函数而言，我们可以使用这样的写法：

    __attribute__((weakref)) void func(void);
,然后在函数中调用func()，如果func()没有被定义，则func的值为0，如果func被定义，则调用相应func，在《程序员的自我修养》这本书中有介绍，它是这样写的：

    __attribute__((weakref)) void func(void);
    void main(void)
    {
        if(func) {func();}
    }
但是在现代的编译系统中，这种写法却是错误的，编译虽然通过(有警告信息)，但是却不正确:

    warning: ‘weakref’ attribute should be accompanied with an ‘alias’ attribute [-Wattributes]
警告显示：weakref需要伴随着一个别名才能正常使用。  
既然书籍有版本问题，那么唯一的办法就是去查![官方文档](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes)，在官方文档中是这样指出的：

    The weakref attribute marks a declaration as a weak reference. Without arguments, it should be accompanied by an alias attribute naming the target symbol. Optionally, the target may be given as an argument to weakref itself. In either case, weakref implicitly marks the declaration as weak. Without a target, given as an argument to weakref or to alias, weakref is equivalent to weak.
    At present, a declaration to which weakref is attached can only be static.
贴出稍为重要的部分，通俗地解释就是：
* weakref需要伴随着一个别名，别名不需要带函数参数，如果对象函数没有定义，我们可以使用别名来实现函数的定义工作，如果不指定别名,weakref作用等于weak。在后面我们会给出相应的示例以助理解。  
* weakref的声明必须为静态。


## 强/弱符号和强/弱引用的作用
这种弱符号、弱引用的扩展机制在库的实现中非常有用。我们在库中可以使用弱符号和弱引用机制，这样对于一个弱符号函数而言，用户可以自定义扩展功能的函数来覆盖这个弱符号函数。  
同时我们可以将某些扩展功能函数定义为弱引用，当用户需要使用扩展功能时，就对其进行定义，链接到程序当中，如果用户不进行定义，则链接也不会报错，这使得库的功能可以很方便地进行裁剪和组合。


## 应用示例
在示例中，我们使用一个静态库作为示例，来展现弱符号和弱引用的用法。  
### 弱符号的使用
test.c:

    #include <stdio.h>
    void __attribute__((weak)) weak_func(void)
    {
        printf("defualt weak func is running!\n");
    }

    void test_func(void)
    {
        weak_func();
    }
test.h:

    void test_func(void);
main.c:

    #include <stdio.h>
    #include "test.h"

    void weak_func(void)
    {
        printf("custom strong func override!\n");
    }

    int main()
    {
        test_func();
        return 0;
    }

将test.c编译成静态库;

    gcc -c test.c
    ar -rsc libtest.a test.o
编译main.c:

    gcc main.c test.h -L. -ltest -o test
-L表示指定静态库的目录，如果不指定目录，编译器会去系统目录查找，如果系统目录没有将报错。
-l表示链接对应的静态库，在linux下静态库的名称统一为libxxx.a，在链接时只需要使用-lxxx即可。  
执行程序：

    ./test
输出结果：

    custom strong func override!
很明显，在test库中，我们定义了weak_func函数，且weak_func是一个弱符号，且在test_func中被调用，然后在main.c中，我们重新定义了test_func函数，且是个强符号，所以在链接时，链接器选择链接main.c中的test_func函数。  
如果我们将main.c中weak_func函数定义去掉，它的执行结果将是：

    defualt weak func is running!
喜欢动手的朋友可以试试。   

## 弱引用的使用
test.c:

    #include <stdio.h>
    static __attribute__((weakref("test"))) void weak_ref(void);
    void test_func(void)
    {
        if(weak_ref){
            weak_ref();
        }
        else{
            printf("weak ref function is null\n");
        }
    }
test.h:

    void test_func(void);
main.c:

    #include <stdio.h>
    #include <stdarg.h>
    #include "test.h"
    void test(void)
    {
        printf("running custom weak ref function!\n");
    }

    int main()
    {
        test_func();
        return 0;
    }
将test.c编译成静态库;

<<<<<<< HEAD
    gcc -c test.c
    ar -rsc libtest.a test.o
编译main.c:

    gcc main.c test.h -L. -ltest -o test
=======
    
>>>>>>> e922194277029fe7a741695475a13e9569f6e557

    ./test
输出结果：

    running custom weak ref function!\n
在test静态库中，我们仅仅声明了静态的weak_ref函数，且声明时使用了函数别名test，因为是静态声明，这个函数名的作用域被限定在本模块内，所以即使是我们在其他模块中定义了weak_ref函数，也无法影响到当前模块中的weak_ref函数，官方提供的方法是我们可以定义它的别名函数来代替，如上所示weak_ref的别名为test，所以在main.c中定义了test函数就相当于定义了weak_ref函数，所以在test_func的判断分支中，test_func中不为null，执行if(test_func)分支，如果在main.c中去除weak_ref的定义，函数的执行结果是这样的:

    weak ref function is null\n
喜欢的动手的朋友的试试，同时试试带参数的函数，这样可以加深理解。   
至于为什么要使用别名来实现，将目标函数声明为静态，而不是直接使用当前名称实现，博主尚未找到答案，如果有路过的大神希望指点一二。  

好了，关于C语言中强弱引用和强弱符号的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言



