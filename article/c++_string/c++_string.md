## C++ STL剖析-string类型详解
***
## 序言
C++中的string是C++标准库中的一个针对字符串操作的类，支持相当多的字符串操作方式，相对于C而言，它将程序员从繁琐的字符、指针、字符串的内存分配释放中解脱出来，但是同时需要付出的代价是，程序员们需要花额外的时间去了解这些特性，记这些接口，或者是额外花出时间来Google这些接口。孰好孰坏，我们暂且不讨论，今天我们就来看看string类都有哪些神奇的东西
***

## 读者须知  
注1：由于版本问题，源码可能有出入，仅供参考
注2：以下所有的示例代码，因为篇幅原因，省去头文件以及命名空间部分

    #include <iostrem>
    #include <string>
    using namespace std;//正式项目中并不建议这样做，这里为了方便起见
注3：编译示例代码的编译器为gcc 5.4.0
***
## 在源代码面前，一切都无所遁形
***注1：由于版本问题，源码可能部分不一致***
***注2：***
***参考源码地址：[stl_string 源代码](https://github.com/HelloWorldYanGang/STL-source-code) ***
对于程序员而言，分析成熟且标准库的源代码是学习中最高效的方式之一，不仅能看到库实现的底层细节，了解功能特性，同时能学习到大神的代码风格。博主在写下这篇博文之前，下载并阅读了SGI STL的string部分源码，特意在此分享。  
本文章将全方位，无死角地解析string的使用以及部分特性。同时推荐侯捷的《STL源码剖析》，但是需要一定的c/c++语言功底。
***
## 源码摘要  
string类出自于std::basic_string模板类，事实上在源码当中所体现的是std::basic_string类。  
std::basic_string类继承std::_String_base基类，为了理解方便，在这里我们只讨论重点的几个变量和方法

    变量：  
    
    1. _Tp* _M_start;   
    
    2. _Tp* _M_finish;  
    
    3. _Tp* _M_end_of_storage;   
    
    

### 变量解析  
#####  _Tp* _M_start;   
//指针，同时可以理解为迭代器，类型为string类的元素类型，即字符型，初始时指向第一个元素  
#####  _Tp* _M_finish;  
//同上，初始时指向最后一个元素，如果用户使用默认构造函数，即string str(),则等于_M_finish=_M_start 

##### typedef const value_type        const_iterator;  
##### typedef value_type*            iterator;  
//以上两个就是我们陌生而又熟悉的迭代器，很多朋友被迭代器这个晦涩的名字给吓到了，其实你就可以理解迭代器为这个类的元素指针，在string中，为char类型，光说不练假把式，这里给出个示例：

    string str("downey");
	char *p=&str[0];
	cout<<*p<<endl;

    输出：  
    d
##### _Tp* _M_end_of_storage; 
//C++中规定，string类在内存中存储方式为连续存储，这个指针代表内存边界，示例如下：

    string str("downey");
	for(int i=0;i<str.size();i++)
		cout<<*(p+i);
    输出：  
    downey
从示例中不难看出，用一个外部的char型指针可以对string元素进行遍历。  
***需要注意的是，这里的指针p可以指向类内元素，但是不能将迭代器赋值给p，会报类型检查错误，示例：***

    string str("downey");
	char *p=str.begin();  
    编译报错：error: cannot convert ‘std::basic_string<char>::iterator {aka __gnu_cxx::__normal_iterator<char*, std::basic_string<char> >}’ to ‘char*’ in initialization
 

***
## string内置方法
***注1：全局替换：typedef basic_string string;***
***注2：所有的类型表示，例如size_type,value_type等全部替代为简化类型，平台不同会有细微差异***
### 构造函数
#### string()   //初始化一个空字符串
#### string(const string& __s) //copy自一个string类
#### string(const char* __s) //copy自一个字符串
#### string(const string& __s, int __pos,int __npos) //copy __s从__pos开始的__npos个字符，示例：

    string str("downey");
	string str1(str,3);
    string str2(str,3，2);	
	cout<<str1<<endl;
    结果：
    ney
    ne
#### string(int __n, char __c) //用__n个__c初始化类
***

### 操作符重载
#### string& operator=(const string& __s) //将string类__s赋值给左值
#### string& operator=(const char* __s)   //将字符串__s赋值给左值

***
### 迭代器方法
#### iterator begin()   //{ return _M_start; } 返回初始位置迭代器
#### iterator end()     //{ return _M_finish; } ,返回结束迭代器，注意返回的end迭代器事实上是指向类内最后一个元素的下一个位置，这个迭代器并不指向具体元素，示例：

    string str("downey");
	string::iterator it;
	it=str.end();
	cout<<*it<<endl;
	it=str.begin();
	cout<<*it<<endl;
	for(;it!=str.end();it++)
		cout<<*it;
	cout<<endl;
    结果：

    d
    downey
可以看出，end()方法返回的迭代器指向内容其实是不确定的，至于为什么c++给迭代器的实现标准为左开右闭区间而不是左开右开，个人的看法是如果设计成左开右开区间，就不能使用for(;it!=str.end();it++)这种形式，中间的迭代器需要+1，但是由于迭代器指向的对象时抽象化的，所以会给编程带来一些不必要的麻烦。
#### const_iterator begin() const //  {return _M_start;}
#### const_iterator end()   const //  { return _M_finish; }
const_iterator迭代器指向的类内元素是只读的，不可修改。示例：

    string str("downey");
	string::const_iterator it;
	it=str.begin();
	*it='s';
    结果：
    编译错误： error: assignment of read-only location ‘it.__gnu_cxx::__normal_iterator<_Iterator, _Container>::operator*<const char*, std::basic_string<char> >()’
#### reverse_iterator rbegin() //  { return reverse_iterator(_M_finish); }