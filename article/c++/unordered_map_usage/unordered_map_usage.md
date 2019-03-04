## C++ STL unordered_map用法
在C++11中，unordered_map作为一种关联容器，替代了hash_map，unordered_map的底层实现是hash表，所以被称为无序关联容器。  
不管是map还是unordered_map都是一种 key-map(value) 映射的容器，提供非常高的查找效率，下面我们来了解unordered_map的用法。

### 预备知识
在讲解unordered_map之前，我们先得了解一些预备知识：
#### 元素类型
除常用的语言内置类型以外，unordered_map的元素类型大致有以下几种：
* value_type : unordered_map元素类型，这种类型的形式为 key-map类型，key和map的类型都是模板类型。
* key_type : key，模板类型
* mapped_type ：map，即我们常说的value，模板类型
* pair类型 ：pair类型也是STL中的常用类型，原型为template <class T1, class T2> struct pair;由于unordered_map使用的就是Key-Map匹配对，所以在这里使用比较多。

#### 概念
* 插槽：英文为bucket，又可以翻译成桶。在hash表中，hash函数通常返回一个整型(或无符号整型)元素，对应hash表的数组下标，但是数组类型通常为指针指向一片内存或者是一个链表头，对应许多元素，就像一个桶可以装很多元素，这里称为插槽。

### 构造函数
    explicit unordered_map ( size_type n = N,const hasher& hf = hasher(),const key_equal& eql = key_equal(),const allocator_type& alloc = allocator_type() );  

这个构造函数接受无参数构造

* n：为hash表的最小插槽数，如果未指定，将会被自动确定(取决于特定的库实现，并不固定)
* hf:hash函数，因为底层实现是hash表，必然就有hash函数，STL提供了非常全面的不同类型的hash函数实现，也可以自己实现hash函数。
* key_equal:判断两个key对象的hash值相等以确定查找的命中，STL提供了大部分的不同类型的key_equal实现，同样也可以实现hash函数
* alloc：容易使用的内存构造器，可选择不同的内存构建方式

***  
    explicit unordered_map ( const allocator_type& alloc );
指定unordered_map的构造器

***  
    template <class InputIterator>
    unordered_map ( InputIterator first, InputIterator last,size_type n = N,const hasher& hf = hasher(),const key_equal& eql = key_equal(),const allocator_type& alloc = allocator_type() );  

接收输入迭代器构造方式，将迭代器指向的元素区间进行复制构造

***  
    unordered_map ( const unordered_map& ump );
    unordered_map ( const unordered_map& ump, const allocator_type& alloc );  

复制构造，第二个可指定构造器

***  
    unordered_map ( unordered_map&& ump );
    unordered_map ( unordered_map&& ump, const allocator_type& alloc );  

移动构造方式，这个C++11中新支持的特性，移动构造方式提供临时变量的引用，即右值引用的功能,&表示左值引用，&&表示右值引用。  

***  
    unordered_map ( initializer_list<value_type> il,size_type n = N,const hasher& hf = hasher(),const key_equal& eql = key_equal(),const allocator_type& alloc = allocator_type() );  

以传入列表的形式构造  

示例：
    std::unordered_map<std::string,std::string> strmap( {{"name","downey"},{"age","500"}} );  
    
***   

### 成员函数
####  at()

    mapped_type& at ( const key_type& k );
    const mapped_type& at ( const key_type& k ) const;  

根据Key值查找容器内元素，并返回map元素的引用。  

示例：

    std::unordered_map<std::string,int> mymap={"key",111};
    map.at("key")=123;
    map.at("key")+=123;
***  
#### begin()

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    local_iterator begin ( size_type n );
    const_local_iterator begin ( size_type n ) const;  

指向容器内第一个元素的迭代器。迭代器访问元素时，it->first对应key，it->second对应map(value).

#### end()

    iterator end() noexcept;
    const_iterator end() const noexcept;
    local_iterator end (size_type n);
    const_local_iterator end (size_type n) const;  

指向容器内最后一个元素的后一个位置的迭代器。

#### bucket()

    size_type bucket ( const key_type& k ) const;  

以key值寻找元素在容器中的位置。  

示例：

    str_map map1;
    map1.insert({"downey","hello"});   
    cout<<map1.bucket (it->first)<<endl;
    output:
    2  

从返回值可以看出，即使是插入的第一个元素，位置也不一定是1，这跟容器的hash实现相关。

#### bucket_count()

    size_type bucket_count() const noexcept;  

返回hash表的插槽值个数，这个函数的值对应构造函数中的n(最小插槽数)参数。

#### max_bucket_count()

    size_type max_bucket_count() const noexcept;  

返回容器所能支持的最大插槽数，根据平台不同而不同，一般是一个非常大的数字。
#### bucket_size()

    size_type bucket_size ( size_type n ) const;  

这个函数返回每个插槽中的元素数量。

#### cbegin()

    const_iterator cbegin() const noexcept;
    const_local_iterator cbegin ( size_type n ) const;  

返回const类型的第一位置迭代器
#### cend()
返回const类型的最后一个位置的下一位置的迭代器。

#### clear()

    void clear() noexcept;
删除容器内所有元素。

#### count()
    size_type count ( const key_type& k ) const;  

某个key值对应的map(value)值的数量，因为unordered_map不允许重复元素，所以返回值总是0或1

#### emplace()

    template <class... Args>
    pair<iterator, bool> emplace ( Args&&... args );  

如果key元素是唯一的，在unordered_map中插入新元素，使用Args作为元素构造函数的参数来构造这个新元素。参数为右值引用。
示例：
    mymap.emplace ("NCC-1701", "J.T. Kirk");
    即可插入相应的map元素

#### emplace_hint()

    template <class... Args>
    iterator emplace_hint ( const_iterator position, Args&&... args );  

与emplace()操作一致，position参数则是提供一个建议搜索位置的起点的提示，可以优化执行时间。

#### empty()

    bool empty() const noexcept;  

判断容器是否为空，返回bool值

#### erase()
    iterator erase ( const_iterator position );
    size_type erase ( const key_type& k );
    iterator erase ( const_iterator first, const_iterator last );  

根据不同的索引擦除插槽中的元素.

#### find()

    iterator find ( const key_type& k );
    const_iterator find ( const key_type& k ) const;  

查找函数，通过key查找一个元素，返回迭代器类型。

#### get_allocator()

    allocator_type get_allocator() const noexcept;  

返回容器目前使用的内存构造器。

#### hash_function()
    hasher hash_function() const;  

获取hash容器当前使用的hash函数

#### insert()
    pair<iterator,bool> insert ( const value_type& val );  

直接插入元素类型，返回pair类型，返回值pair第一元素是插入元素迭代器，第二元素表示操作是否成功  

    template <class P>
    pair<iterator,bool> insert ( P&& val );  

移动插入方式，可以传入右值插入
    iterator insert ( const_iterator hint, const value_type& val );  

用户给出一个插入起点以优化查找时间
    template <class P>
    iterator insert ( const_iterator hint, P&& val );	
    template <class InputIterator>
    void insert ( InputIterator first, InputIterator last );  

复制型插入，将(first,last]所包含的内容全部复制插入	
    void insert ( initializer_list<value_type> il );
插入一个列表形式的元素

#### key_eq()

    key_equal key_eq() const;  

获取key equal函数，key_equal函数为判断key值是否匹配，在一般情况下，hash函数并不能保证每一个输入对应一个独一无二的输出，可能多个输入会对应同一个输出，这就是hash冲突。可能一个槽内同时由多个元素，这时候就需要使用key_equal来进行进一步判断。

#### load_factor()

    float load_factor() const noexcept;  

load factor在中文中被翻译成负载因子，负载因子是容器中元素数量与插槽数量之间的比例。即：

    load_factor = size / bucket_count 

#### max_load_factor()
    float max_load_factor() const noexcept;
    void max_load_factor ( float z );  

第一个函数是查询目前容器最大的负载因子，默认为1。  

第二个函数是进行最大的负载因子的设置。

#### max_size()

    size_type max_size() const noexcept;  

容器可支持的元素最大数量，linux平台下，使用4.8.5的STL库中这个值是：268435455

#### '='运算符重载

    unordered_map& operator= ( const unordered_map& ump );
    unordered_map& operator= ( unordered_map&& ump );
    unordered_map& operator= ( intitializer_list<value_type> il );  

以不同方式对容器进行赋值。

#### '[]'操作符重载
    mapped_type& operator[] ( const key_type& k );
    mapped_type& operator[] ( key_type&& k );
[]操作符重载，使得容易可以通过map[Key]的方式进行索引。

#### rehash()

    void rehash( size_type n );  

重建hash表，将插槽的数量扩展的n，如果n小于目前插槽数量，这个函数并不起作用。
#### reserve()

    void reserve ( size_type n );
将容器的插槽数设置成最适合n个元素的情况，这样可以避免多次rehash和直接rehash空间的浪费。  

与rehash相比，这个函数由用户给一个插槽数量建议值，由系统去分配空间，而rehash则是指定容器的插槽值

#### size()

    size_type size() const noexcept;  

返回当前容器中元素的个数

#### swap()

    void swap ( unordered_map& ump )  
    
交换两个容器的内容，两个容器的类型必须一致，但大小可以不同。




好了，关于C++ STL unordered_map 的API的用法讨论就到此为止啦，如果朋友们对于这个有什么疑问或者发现有文章中有什么错误，欢迎留言

***原创博客，转载请注明出处！***

祝各位早日实现项目丛中过，bug不沾身.