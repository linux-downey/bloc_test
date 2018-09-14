## 不同语言的负数取余问题
### 问题的出现
作为一个程序员，地铁上的时光总是比较难熬的，所以就养成了在地铁上心算一些小题目的习惯，这一天我一如既往地选择了一道leetcode中非常简单的题目：

    Given a 32-bit signed integer, reverse digits of an integer.
翻译成中文就是：

    给定一个32位有符号整数，将整数由低位到高位反向输出
题目很简单，有很多种实现方式，大概十分钟左右就在脑海中想到了一个自认为最好的解法：

    int reverse_num(int x,int res)
    {
        if(!x) return res;
        return reverse_num(x/10,(res*10+x%10));
    }
    int main()
    {
        int val=0;
        val=reverse_num(-9870,val);
        cout<<val<<endl;
    }
输出结果：

    -789
解决！！其实用循环也可以高效地实现，为什么要用递归？因为递归总是能写出简洁优美的代码(其实是为了装X...)。  
作为习惯，我再用python实现一遍，同样的代码结构：

    def reverse(x,res):
        if x==0:
            return res
        return reverse(x/10,(res*10+x%10))


    def main():
        num=-987
        res=0
        val=reverse(num,res)
        print val
输出结果：

    RuntimeError: maximum recursion depth exceeded
What the ****！！？？  
我抬起我颤抖的小手移动着屏幕一行一行地检查代码，发现并没有错。  
本以为大局已定，结果被极限反杀，当然不能就这么算了，于是我开启了debug模式！
很快就发现了问题：

    >>> print -987/10
    -99
    >>> -987%10
    3
从上述运行结果来看，-987/10的结果居然是-99而不是-98，而-987%10答案是3而不是7，作为一个C/C++开发者，这样的结论自然无法接受。  
### 问题的解决
根据资料显示，目前使用的主流除法处理有两种：Truncated Division和Floored Division，这两 种除法在对正数除法的处理上是一致的，而在对负数处理上则略有不同。首先，需要了解的是，对于整数的除法，都满足这个公式：

    m=q*n+r
    m为被除数
    n为除数
    r为余
    q为商
    m,n,q,r都为整数
即：

    14=3*4+2
    14除以4得3余2
这不是很标准了么，那为什么还会有分歧呢？  
正整数的除法当然是没有问题的
#### Truncated Division
在C99的标准中，明确规定了"truncation toward zero",即向0取整。在这个模式下，负整数除法的取余是往靠近0的方向进行取整

