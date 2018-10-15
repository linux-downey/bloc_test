## 用数字签名工具生成驱动的签名
下面我们来介绍怎么用数字签名工具生成一个windows下驱动的签名。
### 需提前准备的工具
"数字签名工具"安装包
数字签名证书，这个证书一般是从官方机构处购买，费用根据实际情况而言
windows下驱动的描述文件，例如：如果你想要安装USB转TTL工具的驱动，就需要先配置一份驱动程序相对应的.inf描述文件(配置方法需要查看网络教程，Google即可)。
Inf2Cat.exe工具，这个工具作用为通过inf文件生成cat文件，然后对cat文件进行数字签名。cat文件其实是个文本文件，是微软公司发布的对于驱动程序的数字签名文件， 其作用是对通过了微软运行测试的、合格的驱动程序进行标识。
### 安装数字签名工具
首先，我们需要在windows下安装数字签名工具，网站上即可搜到，安装完成之后选择在桌面生成快捷方式即可看到这么一个软件图标：  

![sign_tool](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/Sign_tool.png)  
### 使用数字签名工具
#### 打开工具
***注意，我们需要以管理员身份运行打开这个软件，不然在部分系统上会出现使用问题***
#### 导入证书
打开软件后，跳过第一栏出现的欢迎页，打开证书管理栏，默认情况下，这一栏内容为空：![manage](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/Certificate%20management.png)

我们需要点击导入，将会弹出这么一个导入证书界面：![import certificate](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/import_certificate.png)

证书的格式有两种：
* PFX格式：PFX格式对应证书的PFX后缀文件，直接添加然后输入证书密码就可以
* PVK_SPC格式：这个格式需要两个文件，一个PVK文件，一个SPC文件，后缀为SPC的文件为证书，需要输入相应密码，然后私钥那一栏填入以PVK为后缀名的文件
***(注：这几个文件为证书文件，是向证书机构购买证书时获取的文件)***

证书导入成功之后出现这个界面:![import success](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/import_success.png)

#### 指定签名规则
切换到签名规则栏：![sign rule](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/Sign_rule.png)
点击添加签名，将会出现这样一个界面：![add rule](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/add_rule.png)
这个界面有这些项目：
1. 规则名：规则名为签名的一个标识，可根据具体项目来填写，或者填写公司名都好，最好是填写英文
2. 证书：如果在上一步证书管理中有添加证书，在下拉菜单中选择想要添加的证书
3. sha2证书：如果在上一步证书管理中有添加证书，在下拉菜单中选择想要添加的证书
4. 文件扩展名：使用默认值
5. 包含子目录：需要勾选
6. 跳过有效签名文件：忽略

#### 通过Inf2Cat.exe工具生成.cat文件
在存在Inf2Cat.exe工具的目录下运行如下脚本，将inf文件放置在指定目录，即/drivers目录下：
Inf2Cat.exe /v /driver:.  /os:XP_X86,XP_X64,Vista_X86,Vista_X64,7_X86,7_X64,10_X86,10_X64
命令执行完之后，会在/drivers目录下生成cat文件

#### 数字签名
在数字签名工具中切换到数字签名栏：![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/digital_signature.png)

将上一步中生成的cat文件拖入数字签名栏，或者点击添加文件，选中相应的cat文件
点击右下角的数字签名，弹出模式选择，点击应用模式，然后点击驱动模式(证书同时支持应用模式和驱动模式，根据证书支持选择)，若无意外，在签名状态处会提示签名成功

进行签名验证：![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/Signature_verification.png)
根据签名的方式验证相应的签名结果。
结果是这样：![](https://raw.githubusercontent.com/linux-downey/bloc_test/master/article/generate_sign/Signature_certificate_result.png)

#### 驱动安装
至此，inf文件即可作为驱动文件被安装在电脑中正常使用。