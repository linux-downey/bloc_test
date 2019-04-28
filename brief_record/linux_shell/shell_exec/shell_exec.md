# linux shell的执行

在linux中运行一个bash脚本，实际上是当前终端fork出一个子进程，然后在子进程中运行这个脚本。

子进程继承父进程的大部分资源，包括环境变量。

## export 
export表示shell下的环境变量，被执行的shell继承父进程的环境变量。

同时export可以设置，但是在子shell中设置的环境变量并不会体现到父进程中，也就是在子进程退出之后export的变量也会随之消失。  

这也是因为用export设置的环境变量以进程为独立的载体。

直接运行export可以直接打印出当前进程的环境变量。 
nev也可以打印环境变量


## source
以source命令运行一个shell脚本时，不会fork子进程运行，而是直接在当前进程中运行脚本。


## sudo
sudo命令可以执行的条件是fork一个子进程，然后传入root用户的环境变量，如果没有fork的过程，就不能使用sudo指令来获取root权限。 

比如：cd为内建命令，所以不能使用
        sudo cd xxx
同时不能使用
        sudo source xxx

在以sudo执行连续的指令时，
比如：echo "xyz" > /sys/devices/platform/leds/uevent
sudo 只会作用于第一条指令，即echo "xyz",
而不会作用于 > /sys/devices/platform/leds/uevent
这个时候，我们需要这样写：
sudo sh -C "echo "xyz" > /sys/devices/platform/leds/uevent"


