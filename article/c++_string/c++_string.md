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
从示例中不难看出，用一个外部的char型指针可以对string元素进行遍历，p指针的自增也是地址的自增。  
***需要注意的是，这里的指针p可以指向类内元素，但是不能将迭代器赋值给p，会报类型检查错误，示例：***

    string str("downey");
	char *p=str.begin();  
    编译报错：error: cannot convert ‘std::basic_string<char>::iterator {aka __gnu_cxx::__normal_iterator<char*, std::basic_string<char> >}’ to ‘char*’ in initialization
 

***
## string常用内置方法
***注1：全局替换：typedef basic_string string;***
***注2：所有的类型表示，例如size_type,value_type等全部替代为简化类型，平台不同会有细微差异***
***

### 构造函数
#### string()   //初始化一个空字符串
#### string(const string& __s) 
copy自一个string类
#### string(const char* __s) 
copy自一个字符串
#### string(const string& __s, int __pos,int __npos) 
copy __s从__pos开始的__npos个字符，示例：

    string str("downey");
	string str1(str,3);
    string str2(str,3，2);	
	cout<<str1<<endl;
    结果：
    ney
    ne
#### string(int __n, char __c) 
用__n个__c初始化类
***

### 操作符重载
#### string& operator=(const string& __s) 
将string类__s赋值给左值
#### string& operator=(const char* __s)   
将字符串__s赋值给左值
#### reference operator[](size_type __n) const
#### reference operator[](size_type __n)
提供下标访问
#### string& operator+=(const string& __s)
#### string& operator+=(const _CharT* __s)
#### string& operator+=(_CharT __c)
扩展字符串

***

### 迭代器方法
#### iterator begin()   

    { return _M_start; } 
返回初始位置迭代器
***
#### iterator end()     

    { return _M_finish; } 
返回结束迭代器，注意返回的end迭代器事实上是指向类内最后一个元素的下一个位置，这个迭代器并不指向具体元素，示例：

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
可以看出，end()方法返回的迭代器指向内容其实是不确定的，至于为什么c++给迭代器的实现标准为左开右闭区间而不是左开右开，个人的看法是如果设计成左开右开区间，就不能使用for(;it!=str.end();it++)这种形式，for循环中第二条件的迭代器需要+1，但是由于迭代器指向的对象是抽象化的，所以会给编程带来一些不必要的麻烦。
#### const_iterator begin() const //  {return _M_start;}
#### const_iterator end()   const //  { return _M_finish; }
const_iterator迭代器指向的类内元素是只读的，不可修改。示例：

    string str("downey");
	string::const_iterator it;
	it=str.begin();
	*it='s';
    结果：
    编译错误： error: assignment of read-only location ‘it.__gnu_cxx::__normal_iterator<_Iterator, _Container>::operator*<const char*, std::basic_string<char> >()’
***
#### reverse_iterator rbegin() 

    { return reverse_iterator(_M_finish); }
#### reverse_iterator rend()

    { return reverse_iterator(_M_start); }

#### const_reverse_iterator rbegin() const
#### const_reverse_iterator rend()   const 

这里的四个迭代器指的是反向地遍历整个类，从函数源码可以看出，原理与基本迭代器类似，就不一一列举了。
***

### 字符串操作方法
#### string& append(const string& __s)
在字符串最后连接一个字符串__s
#### string& append(const string& __s,int __pos, int __n)

#### string& append(const cahr* __s, size_type __n)
#### string& append(int __n, char __c);

#### void push_back(_CharT __c)
在最后插入字符
#### void pop_back()
删除最后一个字符，这个方法直接使用g++编译会报错，需要加上-std=c++11编译选项
#### string& insert(int __pos, const string& __s)
指定位置插入另一个string元素，被插入的为整个string
#### string& insert(int __pos, const string& __s,int __beg, int __n)
指定位置插入另一个string的元素，被插入的string从_beg开始
#### string& insert(int __pos, const char* __s, int __n)
在string的__pos位置插入字符串
#### string& insert(int __pos, int __n, char __c)
指定位置插入__n个__c字符
#### iterator insert(iterator __p, char __c)
以迭代器为索引插入
#### string& replace(int __pos, int __n, const string& __s)
在string的__pos位置将__n个字符替换成类__s,为了直观地理解，这里给出一个示例：

    string str("downey");
	string str1="1234567";
	str.replace(1,2,str1);
    结果：
    d1234567ney
从结果不难看出，在str[1]的位置将两个字符替换成了str1.
#### string& replace(size_type __pos1, size_type __n1,const string& __s,size_type __pos2, size_type __n2)
#### basic_string& replace(size_type __pos, size_type __n1,const _CharT* __s, size_type __n2)
#### basic_string& replace(size_type __pos, size_type __n1,const _CharT* __s)
#### basic_string& replace(size_type __pos, size_type __n1,size_type __n2, _CharT __c)
#### basic_string& replace(iterator __first, iterator __last, const basic_string __s)
#### basic_string& replace(iterator __first, iterator __last,const _CharT* __s, size_type __n)
#### basic_string& replace(iterator __first, iterator __last,const _CharT* __s)
#### basic_string& replace(iterator __first, iterator __last, size_type __n, _CharT __c)

***
#### void swap(basic_string& __s)
交换两个类的元素内容
***

### 只读方法
#### const _CharT* c_str() const;
转换成C形式的字符串
#### const _CharT* data()  const;
获取字符串内容
#### size_type find(const basic_string& __s, size_type __pos = 0) const
#### size_type find(const _CharT* __s, size_type __pos = 0) const
#### size_type find(const _CharT* __s, size_type __pos, size_type __n) const;
#### size_type find(_CharT __c, size_type __pos = 0) const;
#### size_type rfind(const basic_string& __s, size_type __pos = npos) const 
#### size_type rfind(const _CharT* __s, size_type __pos = npos) const 
#### size_type rfind(const _CharT* __s, size_type __pos, size_type __n) const;
#### size_type rfind(_CharT __c, size_type __pos = npos) const;
#### size_type find_first_of(const basic_string& __s, size_type __pos = 0) const
#### size_type find_first_of(const _CharT* __s, size_type __pos = 0) const
#### size_type find_first_of(const _CharT* __s, size_type __pos, size_type __n) const;
#### size_type find_first_of(_CharT __c, size_type __pos = 0) const
#### size_type find_last_of(const basic_string& __s,size_type __pos = npos) const
#### size_type find_last_of(const basic_string& __s,size_type __pos = npos) const
#### size_type find_last_of(const _CharT* __s, size_type __pos = npos) const
#### size_type find_last_of(const _CharT* __s, size_type __pos, size_type __n) const;
#### size_type find_last_of(_CharT __c, size_type __pos = npos) const
#### size_type find_first_not_of(const basic_string& __s, size_type __pos = 0) const
#### size_type find_first_not_of(const _CharT* __s, size_type __pos = 0) const
#### size_type find_first_not_of(const _CharT* __s, size_type __pos,size_type __n) const;
#### size_type find_first_not_of(_CharT __c, size_type __pos = 0) const;
#### size_type find_last_not_of(const basic_string& __s, size_type __pos = npos) const
#### size_type find_last_not_of(const _CharT* __s, size_type __pos = npos) const
#### size_type find_last_not_of(const _CharT* __s, size_type __pos,size_type __n) const;
#### size_type find_last_not_of(_CharT __c, size_type __pos = npos) const;
#### 

#### basic_string substr(size_type __pos = 0, size_type __n = npos) const
获取子字符串，从__pos开始，__n个字符的字符串，_npos为0则截取到字符串结尾。
#### int compare(const basic_string& __s) const 
#### int compare(size_type __pos1, size_type __n1,const basic_string& __s) const
#### int compare(size_type __pos1, size_type __n1,const basic_string& __s,size_type __pos2, size_type __n2) const
#### int compare(const _CharT* __s) const
#### int compare(size_type __pos1, size_type __n1, const _CharT* __s) const
#### int compare(size_type __pos1, size_type __n1, const _CharT* __s,size_type __n2) const
#### static int _M_compare(const _CharT* __f1, const _CharT* __l1,const _CharT* __f2, const _CharT* __l2)
***
### 流操作
#### getline(istream& __is, basic_string<_CharT,_Traits,_Alloc>& __s)
从输入流中读取一行字符，默认遇到'\n'字符结束
#### istream& getline(istream& __is,basic_string<_CharT,_Traits,_Alloc>& __s,_CharT __delim)
从输入流中读取一行字符，遇到_delim字符结束读取，结果字符串中不包含__delim，示例：
    string str("downey");
	string str1;
	getline(cin,str1,'n');
	cout<<str1<<endl;
    结果：
    输入->downey
    dow


### 其他方法
#### int size() const ;

    return _M_finish - _M_start;
#### int length() const ;
与size方法其实是完全一样的，length()调用了size()
#### void clear()
删除类内所有元素并释放内存，但是不删除类
#### bool empty() const
判断是否为空类，为空返回true.

好了，到这里，大部分常用的string方法已经罗列完成，有个这个就可以开发大部分的应用程序了，如果你以为STL的string类就讲完了，STL的设计者会毫不犹豫地对你竖起中指以表达关爱。  
事实上，效率永远应该是程序员追逐的目标，而STL的底层实现才是一个合格程序员应该掌握的东西，下一章节我们将会讨论STL 的string底层实现为效率的提升做了哪一些工作。
（完）
