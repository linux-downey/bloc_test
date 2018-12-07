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
首先我们先得把被调用C接口封装成库，一般是封装成动态库。编译动态库的指令是这样的：

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
这里稍微提一下，如果在windows环境下，动态库文件是.dll文件，例如导入libtarget.dll:

    import ctypes
    target = windll.LoadLibrary("./libtarget.dll")

在这里，可以将target看成是动态库的示例，直接可以以变量target来访问动态库中的内容。    
LoadLibrary("./libtarget.so")表示导入同目录下的libtarget.so文件。  
细心的朋友已经发现了，在导入时，linux环境下使用的是cdll，而windows环境下使用的是windll。这里设计到C语言的调用约定，gcc使用的调用约定是cdecl，windows动态库一般使用stdcall调用约定，既然是调用约定，就肯定是关于调用时的规则，他们之间的主要区别就是cdecl调用时由调用者清除被调用函数栈，而stdcall规定由被调用者清除被调用函数栈。关于这个就不在这里赘述了，有兴趣的朋友可以看看我另外一篇博客。

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

