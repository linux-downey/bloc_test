# 整数的编码：反码
众所周知，计算机中所有的数据在内存中都是以二进制的形式存在的，之所以我们看到的数据形式不一致，是因为对二进制数据的解析规则不同导致的。  
比如，同样的一个32位的数据，根据不同的解析规则，可以被解释成：正(负)整数、字符串、浮点数。  
这一章，我们就来讨论一下整数的解析规则。  

## 源码、补码、反码
学过C语言的都知道，对于整数类型，有有符号类型和无符号类型两种，有符号类型可以表示正整数和负整数，而无符号类型数据的所有位都用来表示正整数。  
假设一个整数数据类型有16位，对于无符号类型而言，所有的位都用来表示正整数部分，数据取值范围就是 0 ~ 2^16-1(65535),至于为什么不是 0 ~ 2^16呢？试一试就知道，2^16二进制表示需要17位，所以最多是0xffff。  

对于无符号类型的解析而言，向来没有什么争议，仅仅将二进制数据展开成十进制数据就行。  

但是对于有符号整数而言，就涉及到一个问题：负数怎么表示？



## 二进制原码
二进制原码表示整数的方式非常简单，它的解析方式可以看成这样:

        i = 最高位为1?(-1):1  * 其他数据位的总和
通俗地讲，可以这么理解：

        最高位单纯只作为符号位，而除去最高位的其他位则作为数据部分
我们来举几个示例：
                16进制             符号位              低位总和             最终数据
                0xffff               -                 32767               -32767
                0x8000               -                   0                   -0 
                0x7fff               +                 32767               32767
                  0                  +                   0                   +0
可以发现一个很奇怪的问题，用原码表示的整数会有两个0值，一个是符号位为1，其余位为0，一个是符号位为0，其余位也为0.   

这种方式可谓是非常直观且方便理解了，至少站在人的角度是这样的，如果站在机器的角度上来说呢？  
机器可不这么想，在运算的时候，以原码表示的数据需要考虑进位问题，也就是符号位的转换问题，假如计算8位符号数5+(-5)，二进制就是00000101与10000101相加，首先将会需要根据运算符来对操作数进行转换。需要注意的是，加法是计算器最基本的运算，对二进制数据直接进行加法运算才是最理想最高效的做法。
很明显如果使用原码进行操作，5+(-5)如果直接按位相加，结果是00000101+10000101=10001010，结果为-10，明显是不对的。  
由此引出反码。

## 二进制反码
在上面的5+(-5)的例子中，可以理解成当有负数在操作数中，因为符号位的特性，原码表示的加法操作是不能直接进行相对应的二进制加法操作的。反码就是为了解决这个问题。  
反码的解析规则是这样的：

        i = 最高位为1?(-1):1  * 其他数据位取反的总和
就是在其原码的基础上，将其余的各个位取反。  
示例是这样的：
                16进制             符号位              低位总和             最终数据
                0xffff               -                ~0x7fff=0               -0
                0x8000               -                ~0=0x7fff             -32767 
                0x7fff               +                ~0x7fff=0                0
                  0                  +                ~0=0x7fff              +32767
再来计算5+(-5)就变成了00000101+11111010=1 00000000,因为数据只有8位，不算溢出部分，结果就是0。  
加法运算中带有负号(可以理解成减法运算)的问题算是解决了，但是0的问题还是没有解决。  
为了解决这个问题，又有人提出使用补码。  


## 二进制补码
补码的解析方式是这样的：

        i = 最高位的值取负的值 + 其他位数据的总和
跟原码和反码从理解上最大的区别就是：最高位不仅仅代表符号位，它开始作为一个值出现，一个带负号的值。  

以一个16位数据i为例，i的值以反码表示是多少呢？  
最高位的值为 i&0x8000，所有低位的值为 i&0x7fff,所以最后的结果就是-(i&0x8000)+(i&0x7fff)。  
上面的结果太过抽象，我们来举几个示例：

                16进制             符号位              低位总和             最终数据
                0xffff        0x8000=-32768         0x7fff=32767       -32768 + 32767 = -1
                0x8000        0x8000=-32768               0                 -32768
                0x7fff              0               0x7fff=32767             32767
                  0                 0                    0                   0

在补码解析的方式中，一个位数为n的数据，最小值为-2^(n-1)，而最大值是2^(n-1)-1，这并不是对称的.  
最小值=最大值+1，即0x8000 = 0x7fff +1
这种符号解析的特性可以为我们所用，同时也可能造成一些程序上的问题，比如，在16位有符号数据运算中，20000*2并不会等于期望中的40000，而是一个负数值，if(a>b)和if(a-b)>0有时候也会得到不同的结果。  
在进行数字运算的时候，需要时时刻刻注意数字溢出与正负号的关系。  
值得注意的是，C语言的标准并没有规定要用二进制补码形式来表示符号整数，但是几乎所有的机器都是这么做的。所以出于移植性考虑，我们需要使用补码方式来解析整数。  



在有符号整数中，对于正数而言，原码反码补码都是一样的结果，而对于负数的存储方式，就体现出它们三个之间的区别。  
对于无符号整数而言，数据的值就是简单的二进制到十(十六)进制的转换。    

## 有符号数与无符号数的强制转换
C语言支持不同符号数字之间的强制转换，按照人类的惯有思维，从有符号到无符号转换时，正数的转换就是本身，而负数的转换应该是绝对值，毕竟只是符号的问题。  
但是，计算机可不是这么做的，它仅仅遵循一个非常简单的原则：在内存中的二进制数据不变，仅仅是改变数据解析方式。  
这样可以完全保证计算机的执行效率，同时也意味着有符号和无符号之间的转换并非只是简单地切换符号。  
鉴于目前的计算机都以反码存储方式，不妨以反码举几个例子：
* 有符号8位整数-5的二进制反码表示为11111011,转换成无符号整数就变成了251(直接将11111011转换为十进制)。  
* 无符号8位整数160，二进制反码表示为10100000(直接将十进制160转换为二进制10100000)，转换成有符号整数,按照有符号反码解析就变成了-96(-128+32).  


好了，关于整型数据的解析存储方式的讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.






