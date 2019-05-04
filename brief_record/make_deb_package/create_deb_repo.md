## 创建deb仓库
通常我们在linux下安装软件就是直接使用指令：

    sudo apt-get install xxx
我们都知道，这是通过网络从deb仓库拉取deb包安装到本地，如果我们自己需要创建一个私有的deb仓库，应该怎么做呢？  

首先肯定是需要一个服务器，以前通常的做法是自己租用一个服务器，然后在服务器上部署web服务，nginx或者Apache，但
是现在不需要了，git提供了这种服务，我们可以直接在github上创建一个仓库。  

我们需要使用github个人站点来作为deb包的服务器，即github pages

github规定，每个用户只能有一个个人站点，个人站点的名称与github用户名同名。  

在仓库中点击设置，在github pages部分选择master。

使用reprepro工具将包添加到仓库中。

添加source list。

添加apt-keys。

使用sudo apt-get install 安装。  

















