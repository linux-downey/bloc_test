bash中判断脚本是否由bash运行：

如果是root， id -u 为0

unset命令用于删除已经定义的shell变量


从github上check分支：
git clone -b $branch  url
例：git clone -b v1.9.0-grove.py https://github.com/respeaker/mraa.git

查看CPU的信息可以直接查看文件 :
/proc/cpuinfo


sudo echo 0 > file时，sudo并不能达到预期的效果，sudo仅对echo有效。  

如果要针对file的修改，需要使用：

	sudo sh -c "echo 0 > file"
