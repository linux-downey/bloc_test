# 历史

    Markdown是一种轻量级标记语言，创始人为约翰·格鲁伯（英语：John Gruber）。它允许人们“使用易读易写的纯文本格式编写文档，然后转换成有效的XHTML（或者HTML）文档”
    自从GitHub流行以来，Markdown作为一种轻量级标记语言就深受程序员的喜爱，上手也是非常简单，接下来我们就来分析一下为什么markdown为什么能被一向挑剔的程序员们爱不释手呢？


# 语法
***(注：Markdown发展至今衍生出一些细节上有些许差异的版本，这里以GitHub 的Flavored Markdown（同样在标准Markdown语法上有一些修改） 语法为标准讨论，谁叫咱是程序员呢..)***
***
## 1、head
***
###Markdown支持多级标题对应不同的字体大小，例如：

    # Markdown
    ## Markdown
    ### Markdown
    #### Markdown
    ##### Markdown
    ###### Markdown
所对应的显示内容为：
# Markdown  
## Markdown
### Markdown
#### Markdown
##### Markdown
###### Markdown
最多可以支持6个，而且需要注意的是，这里的#开头的文字或标题必须另起一行，#和文字中间必须有空格，不然GitHub的README无法识别。同时有些爱思考的同学就会说，要是我用七个#，会不会将前六个#作为字体符号，最后一个在文本中显示呢。例如：

    ####### Markdown
而结果是：
####### Markdown
显然，Markdown并没有对其进行转化。

***
###除了用#标识字体(标题)大小，还有一种方法：

    Markdown
    =========
    Markdown
    ---------

所对应的显示内容为：

Markdown
========
Markdown
--------
这里的字体只分为两级，大号的字体下一行用"="号隔开，略小一号的字体用"-"（短横线）隔开，"="或"-"的数量最好是三个以上，但是有些版本可以为一个，通常的习惯为与字体等长。
***

## 2、段落和换行
***
### 分隔线 
在实际的文本显示中，为了使文本更加清晰明了，可以适当地加入一些分隔线

    ***
    ---
分隔线由***或者---表示，一般是三个连续的符号，单行开头，也可以多于三个
***
### 换行
换行可以使用一个或者多个空行来另起一个段落，***注意是一个空行，而不是简单地用回车来另换一行***，示例：

    第一行

    第二行
    第三行
所对应的显示内容为：  
第一行

第二行
第三行

很明显，第二行和第三行连到了一起，那如果我就是不喜欢隔一个空行，非要以回车来换行呢呢，考虑到部分强迫症晚期患者，Markdown有另一种换行的方法:  
***在上一行的行尾添加两个空格，然后以回车来换行，建议用空行换行***  
***
### 插入文本引用
如果我需要将一段文本与正文文本相区分，比如示例，引用文章之类的，怎么做呢？  
答案是先换行(隔一个或者多个空行)，然后新行以tab键开头，键入文本，示例：

    正文

        引用文本示例
***
所对应的显示内容为：  
正文

    引用文本示例。
***

### 列表
#### 无序列表
无序列表k而已使用* + -来标识，注意在字符之后要添加一个空格，示例：  

    * Markdown1
    + Markdown2
    - Markdown3
所对应的显示内容为：  
* Markdown1
+ Markdown2
- Markdown3
结果显示，这三种符号是可以交叉使用的  
***
#### 有序列表
有序列表需要添加一个相应的'数字'+'.'+'空格'来标识，示例：  

    1. Markdown1   
    2. Markdown2  
    3. Markdown3  
所对应的显示内容为：  
1. Markdown1
2. Markdown2
3. Markdown3
****

### 代码
如何把代码贴上去呢？很简单，将代码用两个`包含起来，这个符号不是单引号，而是反引号，键盘左上角那个

    `print "hello world" `  
所对应的显示内容为  
`print "hello world"`

同时也可以用上面提到的引用文本的方法来贴代码

***
### 链接  
添加链接的方法为：
    
    [博客](https://www.cnblogs.com/downey-blog/)  
所对应的显示内容为：  
[博客](https://www.cnblogs.com/downey-blog/)，点击高亮字体即可进入相应链接  

### 插入图片
插入图片的方法与插入链接相比仅仅在前面多了一个 "!",即：  

    ![pic](https://github.com/linux-downey/bloc_test/blob/master/picture/Makedown/Makedown.png)  

所对应的显示内容为：  

![pic](https://github.com/linux-downey/bloc_test/blob/master/picture/Makedown/Makedown.png)  

***值得注意的是，目前还并没有一种兼容性比较好的方法将图片内嵌到文本中，一般的做法是在文本内引用图片的链接，但是在这种做法下，如果图片资源一旦迁移，将会额外增加修改成本***
***  
#### 修改图片大小  

    <img src="https://github.com/linux-downey/bloc_test/blob/master/picture/Makedown/Makedown.png" width=256 height=256 />

所对应的显示内容为：  
<img src="https://github.com/linux-downey/bloc_test/blob/master/picture/Makedown/Makedown.png" width=256 height=256 />