# union 共用体浅析
对于结构体，大家应该是非常熟悉了，那可是模块化编程的基础，但是对于共用体，在C语言的编程中可谓是被遗忘在黑暗的角落，很少被用到。 

## union的内存模型
union的语法和结构体是非常像的，从直观上来说union和struct唯一的区别是union所有成员共用一片内存，而struct中为每个成员分配内存空间。下面举个例子：

    union test{
        char c;
        short s;
        int i;
    };

    








