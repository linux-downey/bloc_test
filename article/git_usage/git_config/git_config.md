# 运行git的配置
**声明：博主推出的git系列教程，大部分内容都是来源于官方的文档，在此基础上加上自己的使用见解以及一些更加详细的说明与测试，以便各位更好的理解和使用git&github，[点击访问官方文档(中文版)](https://git-scm.com/book/zh/v2)**  
### 初次运行git前的配置
既然已经在系统上安装了 Git，你会想要做几件事来定制你的 Git 环境。 每台计算机上只需要配置一次，程序升级时会保留配置信息。 你可以在任何时候再次通过运行命令来修改它们。

Git 自带一个 git config 的工具来帮助设置控制 Git 外观和行为的配置变量。 这些变量存储在三个不同的位置：
1. /etc/gitconfig 文件: 包含系统上每一个用户及他们仓库的通用配置。 如果使用带有 --system 选项的 git config 时，它会从此文件读写配置变量。
2. ~/.gitconfig 或 ~/.config/git/config 文件：只针对当前用户。 可以传递 --global 选项让 Git 读写此文件。
3. 当前使用仓库的 Git 目录中的 config 文件（就是 .git/config）：针对该仓库。
根据博主的测试，在linux系统上安装Git时并不会默认生成/etc/gitconfig和~/.gitconfig文件，而是在用户配置的时候才会生成相应的文件，例如：  
系统级别的配置：

    sudo git config --system user.name XXXX
    这个配置就是配置整个系统的git参数，初次配置将会生成配置文件/etc/gitconfig
用户级别的配置：

    git config --global user.name XXXX
    这个是针对某个单一用户的配置，初次配置会生成~/.gitconfig文件
仓库级别的配置

    git config --local user.name XXXX
    这个是针对单个仓库的配置文件，初次配置会生成.git/config文件
每一个级别覆盖上一级别的配置，所以 .git/config 的配置变量会覆盖 /etc/gitconfig 中的配置变量，类似于C/C++语言中局部变量覆盖全局变量。

在 Windows 系统中，Git 会查找 $HOME 目录下（一般情况下是 C:\Users\$USER）的 .gitconfig 文件。 Git 同样也会寻找 /etc/gitconfig 文件，但只限于 MSys 的根目录下，即安装 Git 时所选的目标位置。   
### 用户信息
当安装完 Git 应该做的第一件事就是设置你的用户名称与邮件地址。 这样做很重要，因为每一个 Git 的提交都会使用这些信息，并且它会写入到你的每一次提交中，不可更改： 

    git config --global user.name "downey"
    git config --global user.email downey@example.com
再次强调，如果使用了 --global 选项，那么该命令只需要运行一次，因为之后无论你在该系统上做任何事情， Git 都会使用那些信息。 当你想针对特定项目使用不同的用户名称与邮件地址时，可以在那个项目目录下运行没有 --global 选项的命令来配置。

很多 GUI 工具都会在第一次运行时帮助你配置这些信息。

### 文本编辑器
既然用户信息已经设置完毕，你可以配置默认文本编辑器了，当 Git 需要你输入信息时会调用它。 如果未配置，Git 会使用操作系统默认的文本编辑器，通常是 Vim。 如果你想使用不同的文本编辑器，例如 Emacs，可以这样做：

    git config --global core.editor emacs

### 检查配置信息
如果想要检查你的配置，可以使用 git config --list 命令来列出所有 Git 当时能找到的配置。

    git config --list
    user.name=downey
    user.email=downey@example.com
    color.status=auto
    color.branch=auto
    color.interactive=auto
    color.diff=auto
    ...
你可能会看到重复的变量名，因为 Git 会从不同的文件中读取同一个配置（例如：/etc/gitconfig 与 ~/.gitconfig）。 这种情况下，Git 会使用它找到的每一个变量的最后一个配置。  
你可以通过输入 git config <key>： 来检查 Git 的某一项配置

    git config user.name
    downey


***非完全原创博客，可随意转载***

祝各位早日实现项目丛中过，bug不沾身.
（完）