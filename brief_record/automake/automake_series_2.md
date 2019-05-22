# linux autotools工具使用详解(2) - 完整示例

动态库、静态库、多目录、不同安装路径

结果需要将/usr/local/lib放到系统目录中：

    sudo sh -c "echo /usr/local/lib >> /etc/ld.so.conf"

    sudo ldconfig

config.h中有自定义的宏。









