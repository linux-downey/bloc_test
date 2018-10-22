## C++ STL unordered_map用法
在C++11中，unordered_map作为一种关联容器，替代了hash_map，unordered_map的底层实现是hash表，所以被称为无序关联容器。  
不管是map还是unordered_map都是一种 key-value 映射的容器，提供非常高的查找效率，下面我们来了解unordered_map的用法。

#### 构造函数
    explicit unordered_map ( size_type n = N,const hasher& hf = hasher(),const key_equal& eql = key_equal(),const allocator_type& alloc = allocator_type() );
这个构造函数接受无参数构造
* n：为hash表的最小插槽数，如果未指定，将会被自动确定(取决于特定的库实现，并不固定)
* hf:hash函数，因为底层实现是hash表，必然就有hash函数，STL提供了非常全面的不同类型的hash函数实现，也可以自己实现hash函数。
* key_equal:判断两个key对象的hash值相等以确定查找的命中，STL提供了大部分的不同类型的key_equal实现，同样也可以实现hash函数
* alloc：容易使用的内存构造器，可选择不同的内存构建方式

    explicit unordered_map ( const allocator_type& alloc );
指定unordered_map的构造器

    template <class InputIterator>
    unordered_map ( InputIterator first, InputIterator last,size_type n = N,const hasher& hf = hasher(),const key_equal& eql = key_equal(),const allocator_type& alloc = allocator_type() );
接收输入迭代器构造方式，将迭代器指向的元素区间进行复制构造

    unordered_map ( const unordered_map& ump );
    unordered_map ( const unordered_map& ump, const allocator_type& alloc );
复制构造，第二个可指定构造器

    unordered_map ( unordered_map&& ump );
    unordered_map ( unordered_map&& ump, const allocator_type& alloc );

### hash函数
既然底层实现是hash表，那么就必然涉及到hash函数，C++ STL提供一些内建的hash函数：
#### 基本类型hash函数：

    template<> struct hash<bool>;
    template<> struct hash<char>;
    template<> struct hash<signed char>;
    template<> struct hash<unsigned char>;
    template<> struct hash<char16_t>;
    template<> struct hash<char32_t>;
    template<> struct hash<wchar_t>;
    template<> struct hash<short>;
    template<> struct hash<unsigned short>;
    template<> struct hash<int>;
    template<> struct hash<unsigned int>;
    template<> struct hash<long>;
    template<> struct hash<long long>;
    template<> struct hash<unsigned long>;
    template<> struct hash<unsigned long long>;
    template<> struct hash<float>;
    template<> struct hash<double>;
    template<> struct hash<long double>;
    template< class T > struct hash<T*>;
#### 库类型hash函数：

    std::hash<std::string>
    std::hash<std::u16string>
    std::hash<std::u32string>
    std::hash<std::wstring>
    std::hash<std::pmr::string>
    std::hash<std::pmr::u16string>
    std::hash<std::pmr::u32string>
    std::hash<std::pmr::wstring>
    std::hash<std::error_code>
    std::hash<std::bitset>
    std::hash<std::unique_ptr>
    std::hash<std::shared_ptr>
    std::hash<std::type_index>
    std::hash<std::vector<bool>>
    std::hash<std::thread::id>
    std::hash<std::optional>
    std::hash<std::variant>
    std::hash<std::string_view>
    std::hash<std::wstring_view>
    std::hash<std::u16string_view>
    std::hash<std::u32string_view>
    std::hash<std::error_condition>



