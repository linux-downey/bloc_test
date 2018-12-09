# python调用C语言接口
***
***注：本文所有示例介绍基于linux平台***
***
在底层开发中，一般是使用C或者C++，但是有时候为了开发效率或者在写测试脚本的时候，会经常使用到python，所以这就涉及到一个问题，用C/C++写的底层库，怎么样直接被python来调用？  
python作为一门胶水语言，当然有办法来处理这个问题，python提供的方案就是ctypes库。

## ctypes
ctypes是python的外部函数库，它提供了C语言的兼容类型，而且可以直接调用用C语言封装的动态库。  
如果各位有较好的英语水平，可以参考![ctypes官方文档](https://docs.python.org/3/library/ctypes.html?highlight=ctypes#module-ctypes)，但是我会给出更详细的示例，以便各位更好地理解。

## 库的封装
C代码如果要能够被python调用，首先我们先得把被调用C接口封装成库，一般是封装成动态库。编译动态库的指令是这样的：

    gcc --shared -fPIC -o target.c libtarget.so
在这里，  
--shared -fPIC 是编译动态库的选项。
-o 是指定生成动态库的名称
在linux下，一般的命名规则是：静态库为lib*.a，动态库为lib*.so  
target.c为目标文件，在编译时常有更复杂的调用关系和依赖，这里就不详说，有兴趣的朋友可以去了解了解gcc编译规则。  

## 在python中导入库
既然库已经封装好了，那肯定是就想把它用起来。我们可以在python中导入这个库，以导入libtarget.so为例：

    import ctypes
    target = cdll.LoadLibrary("./libtarget.so")
顺带提一下，如果在windows环境下，动态库文件是.dll文件，例如导入libtarget.dll:

    import ctypes
    target = windll.LoadLibrary("./libtarget.dll")

在这里，可以将target看成是动态库的示例，直接可以以变量target来访问动态库中的内容。    
LoadLibrary("./libtarget.so")表示导入同目录下的libtarget.so文件。  
细心的朋友已经发现了，在导入时，linux环境下使用的是cdll，而windows环境下使用的是windll。这里涉及到C语言的调用约定，gcc使用的调用约定是cdecl，windows动态库一般使用stdcall调用约定，既然是调用约定，就肯定是关于调用时的规则，他们之间的主要区别就是cdecl调用时由调用者清除被调用函数栈，而stdcall规定由被调用者清除被调用函数栈。关于这个就不在这里赘述了，有兴趣的朋友可以看看我另外一篇博客。

## hello world！
学会了封装动态库，学会了导入库，接下来我们就要动手写一个hello_world,毕竟学会了hello_world就算是入门了。  
代码如下：  
target.c:

    #include <stdio.h>
    void hello_world(void)
    {
        printf("hello downey!!\r\n");
    }
编译动态库：

    gcc -fPIC --shared target.c -o libtarget.so

test.py:

    from ctypes import *
    test = cdll.LoadLibrary("./libtarget.so")
    test.hello_world()

执行python脚本：

    python test.py
输出结果：

    hello downey!!

虽然这些代码都是非常简单，但是我还是准备梳理一下流程：
在target.c中我们定义了函数hello_world(),然后将其封装成动态库。  
在test.py中导入libtarget.so动态库，然后调用动态库中的hello_world()函数，结果显而易见，执行了hello_world().  

是不是非常简单，是的，python调用C程序就是这么简单，但是可别忘了，入门简单可并不代表真正使用起来简单！ 
我们可以想一想，上面的hello_world()函数没有参数和返回值，如果是一个带参数或者带返回值的C函数呢,python该怎么调用？  
python的内建类型中可没有C语言那么多花里胡哨的类型，在python中怎么去区分int，short，char这些类型呢？ 

## 类型转换
针对上面的问题，python定义了一系列兼容C语言的类型

如图所示，这个图算是很清晰地将python与C类型对应关系展现了出来。我们将要使用的就是最左边一列的ctypes type，以替代C库中的各种类型。


## 函数带参示例
对于程序员而言，看图片看文档永远没有看代码来得直接，所以在这里先上一段演示代码，看看在C库中的类型是怎么被替换的,但是凡事讲究个循序渐进，我们先来一个简单的，普通变量版的,代码如下：
### 较为简单的示例  
target.c:

    #include <stdio.h>
    char hello_world(int num)
    {
        printf("hello %d!!\r\n",num);
        return (char)num+1;
    }
test.py:

    1 from ctypes import *
    2 test = cdll.LoadLibrary("./libtarget.so")
    3 test.hello_world.restype = c_char
    4 c = test.hello_world(48)
    5 print(type(c))
    6 print(c)
输出：

    hello 48!!
    <type 'str'>
    1
C语言代码我就不多解释，我们主要来关注python部分：

* 第1、2行不用解释了吧
* 第3行：这条指令的作用是指定函数的返回值，python解释器并不能自动识别C函数的返回值,所以我们需要人为地指定，如果不指定，将会是默认的int型。
* 第4行调用函数，并传入参数48，第五行打印返回值的类型，第六行打印返回值。
我们再来看输出部分：
* 第一行是hello_world()函数的输出。
* 第二行打印出来的返回值类型明显是不对的，明明指定了返回值类型为c_char，为什么在这里变成了str(字符串)类型，而且在第三行的输出中输出了1，而不是49。原因有以下几点：
    1. 在python中，内置的类型有int, float，list, tuple等等，但并不包含char类型，既然程序中c是python中的变量，必然将会被转换，而且与C不一样的是，所有变量都是对象。
    2. 如果是需要转换，那会遵循什么规则呢？我们只好从官方文档中找答案，原文是这样的：

        Represents the C char datatype, and interprets the value as a single character. The constructor accepts an optional string initializer, the length of the string must be exactly one character.
        翻译就是，c_char代表C中的char，在python中被视为单个字符，构造函数接受可选的字符串初始值设定项，字符串的长度必须恰好是一个字符。通俗地说，就是一个字符的字符串。
    3. 为什么输出1而不是49，这个就很简单了，十进制的49就是字符1，既然是被视为字符，当然以字符显示

其实在这里，博主选取了一个比较特殊的例子，就是char在python中转换的特殊性，各位朋友可以思考下面两个问题：  
1. 如果在hello_world函数中，将返回值从char改成short，输出是什么?(当然test.py中的第三行也要将c_char改为c_short)
2. 接上题，如果将返回值从char改为float，输出将是什么？
3. 自己动手试试，如果在test.py中不指定函数返回值类型，输出将会是什么？


## 进阶版
如果你看完了上面那个简单版的函数参数转换，我们进入进阶版的。在这个进阶版的示例中，将引入数组，指针，结构体。不说了，直接上码：
target.c:

    #include <stdio.h>
    #include <string.h>
    typedef struct{
        char   *ptr;
        float f;
        char array[10];
    }target_struct;

    target_struct* hello_world(target_struct* target)
    {
        // printf("hello %s.%d!!\r\n",name,num[0]);
        static char temp = 0x30;
        target->ptr = &temp;
        target->f = 3.1;
        memset(target->array,1,sizeof(target->array));
        return target;
    }
test.py:
    1 from ctypes import *
    2 test = cdll.LoadLibrary("./libtarget.so")

    3 class test_struct(Structure):
    4 _fields_ = [('ptr',c_char_p),
    5              ('c',c_float),
    6             ('array',c_char*10)]
    7 struct = test_struct(c = 0.5)
    8 test.hello_world.restype =POINTER(test_struct)

    9 ret_struct = test.hello_world(pointer(struct))
    10 print ret_struct.contents.ptr
    11 print ret_struct.contents.c
输出：

    0
    3.09999990463
对于target.c不多说，大家肯定看得懂，我们还是主要来对照分析一下test.py的内容：
* 第1、2行不用解释，大家都懂
* 第3-6行才是重头戏，这就是python中对结构体的支持，新建一个类，继承Structure，将C中结构体内容一一对应地在类中进行声明，你可以将这个类看成是对应C库中的结构体，_fields_是字典类型，key要与C库中结构体相对应，value则是指定相应类型，在示例中大家应该能看懂了。
* 第7行，构造一个对应C中结构体的类，可以传入对应参数进行构造。
* 第8行，指定返回值类型为test_struct指针类型，这里的类型由POINTER()修饰，表示是指针类型。
* 第九行，调用hello_world()函数，传入struct类，pointer(struct)就是将struct转为指针类型实例。因为在C中的接口就是传入target_struct*类型，返回target_struct*类型，所以ret_struct也是target_struct*类型
* 第10、11行，打印函数返回值，查看执行结果。对于一个指针类型的变量，如果我们要获取它的值，可以使用.contents方法，例如ret_struct.contents返回结构体类示例，然后再访问结构体类中的元素。
* 输出结果，因为在hello_world中元素ptr指向变量的值为0x30，所以输出1，而float类型c被赋值为3.1，但是输出3.09999990463，这其实并不是bug，只能算是python中对浮点数的取值精度问题，这里就不展开讨论了。

## 小结
经过这两个示例，我相信大家对ctypes的使用有了一个大概的认识，但是我建议大家看过之后自己多尝试尝试，这样才有更深的体会，这里再做一个总结：
1. python中ctypes模块支持python类型到C类型的转换，具体对应关系参考上文的图表。
2. 一般情况下，如果导入的目标动态库为linux下的.so类型库，使用cdll.LoadLibrary()导入，如果是windows下的dll动态库，使用windll.LoadLibrary()导入，两种库的区别在于函数调用约定
3. python中需要$LIB.$FUNCTION.restype指定函数返回类型，如果不指定，返回值类型默认为int，同时也可以使用$LIB.$FUNCTION.argtypes指定传入参数类型，$LIB.$FUNCTION.argtypes的类型为列表，大家可以自行试试
4. 在python中c_char类型会被转换成str类型，被视为只有一个字符的字符串
5. 对于指针，不能直接访问，如果直接使用print(ptr)，将会打印出一个地址，需要使用ptr.contents来访问其实例
6. 对于C中的结构体的支持，python中需要新定义一个结构体类，继承Structure类，然后在_fields_字段中一一对应地定义结构体中的元素，在使用时，可视为结构体类等于结构体
7. POINTER()和pointer()，这两个方法，一个大写一个小写，大家在上面的例子中有看到，博主刚接触的时候也是一脸懵逼，后来查了一下官方文档，然后自己尝试了一遍，终于理解了它们之间的区别，这里贴上官方说明：

    POINTER():This factory function creates and returns a new ctypes pointer type. Pointer types are cached and reused internally, so calling this function repeatedly is cheap. type must be a ctypes type.
    pointer():This function creates a new pointer instance, pointing to obj. The returned object is of the type POINTER(type(obj)).
    Note: If you just want to pass a pointer to an object to a foreign function call, you should use byref(obj) which is much faster.

    简单翻译一下就是：POINTER()创建并返回一个新的指针类型，pointer()创建一个新的指针实例，一个是针对类型，一个是针对实际对象，这里还提到了byref()，上面有说到，如果你仅仅是想讲一个外部对象作为参数传递到函数，byref()可以替代pointer()。如果你还没有明白这一部分，你可以参考参考上面我的例子，并且自己试一试，这个并不难。
8. 对于数组，其实也挺简单，大家可以参考上面示例，我相信大家能看懂。


## 扩展内容 —— 回调函数
在参数类型中，还有一种非常特殊的存在——函数指针，在C语言中，我们经常将函数指针作为参数来实现回调函数，这种做法在各种标准化框架中经常见到，那么问题来了，C库中的函数实现了回调函数，python调用时该怎么做？按照我们对C语言的理解，其实函数指针也是指针的一种，我们可以将一个指针强制转换成函数指针类型然后执行，然后博主就在python中尝试了一下，结果不管是我试图将什么类型的指针转换成函数执行，结果都是这样的：

    TypeError: XXXX object is not callable
好吧，我还是老老实实地使用官方提供的接口，还是直接上码：
target.c:

    #include <stdio.h>
    typedef void (*callback)(int);
    void func(callback c1,callback c2,int p1,int p2)
    {
        c1(p1);
        c2(p2);
    }
test.py:
    1 from ctypes import *
    2 test = cdll.LoadLibrary("./libtarget.so")
 
    3 def test_callback1(val):
    4     print "I'm callback1"
    5     print val

    6 def test_callback2(val):
    7     print "I'm callback1"
    8     print val

    9 CMPFUNC = CFUNCTYPE(None, c_int)
    10 cbk1 = CMPFUNC(test_callback1)
    11 cbk2 = CMPFUNC(test_callback2)
    12 test.func(cbk1,cbk2,1,2)
输出：

    I'm callback1
    1
    I'm callback1
    2
可以看到，在target.c中func函数传入了两个函数指针参数，然后在函数中调用这两个函数。  
我们再来仔细分析python中的调用：
* 第1、2行，请参考上面两个示例。
* 3-6行，6-8行，定义两个回调函数，类型。
* 第9行，站在C语言角度来说，相当于创建一个函数类型，指定函数的返回值和参数，第一个元素为返回值，后面依次放参数，因为返回值为void，所以这里是None
* 10-11行，用上面创建的函数类型修饰两个函数，返回一个函数实例，这个函数实例就可以作为函数参数，以函数指针(再次声明，python中没有函数指针这回事，这里为了理解方便将这个概念引入)的形式传递到函数中。
* 12行，调用func()函数,而func()函数的内容就是直接执行传进来的两个函数，传入的参数是test_callback1和test_callback2，所以执行了test_callback1和test_callback2，打印了相应内容。  

好了，关于python ctypes调用C代码的问题就到此为止了，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***个人邮箱：linux_downey@sina.com***
***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.
（完）

