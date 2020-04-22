	
qume + ubuntu-base

全程 root 操作	

第一步：
	安装 qume 虚拟环境。
	sudo apt-get install qemu-user-static

第二步：
	下载一个 ubuntu 镜像，镜像下载地址：http://cdimage.ubuntu.com/ubuntu/releases/，可以选择各种 ubuntu 的发行版。  
	选择的时候同时注意需要选择 armhf 的，如果是 arm64 的，就选 aarch 类型的。  

第三步：
	解压 ubuntu 镜像到一个指定目录
		mkdir ubuntu-rootfs
		tar -xvf ubuntu-base-18.04.2-base-arm64.tar.gz -C ubuntu-rootfs

第四步：
	初始化 ubuntu 镜像：
		1、复制 qume 执行环境到 ubuntu 镜像中,
			cp /usr/bin/qemu-aarch64-static   ubuntu-rootfs/usr/bin   //(如果是 arm,就是 qemu-arm-static)：
		2、初始化网络部分
			cp -b /etc/resolv.conf ubuntu-rootfs/etc     //将本机的网络参数复制到目标 ubuntu 环境中
			echo name server 114.114.114.114 > ubuntu-rootfs/etc/resolv.conf     //设置域名解析，也可以设置谷歌的：8.8.8.8,114 是电信的
		3、初始化镜像源，国内最好使用清华、中科大的镜像源以加快访问速度，但是国内镜像源有一个延迟更新的问题，对于新出软件需要格外注意。
			先找到清华或者中科大的源网站：
				https://mirrors.tuna.tsinghua.edu.cn/help/ubuntu/
				http://mirrors.ustc.edu.cn/help/ubuntu-ports.html
			以清华为例，默认的源都是基于 x86_64 的，arm 架构的不能直接使用，但是我也没有在清华官网找到对应的 arm 源，可以直接在 x86 的源上进行一些修改，将：
				deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ trusty main restricted universe multiverse   //这是 ubunt14 的，trusty
			修改成：
				deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ trusty main restricted universe multiverse
			也就是将 ubuntu 修改为 ubuntu-ports
			
第五步：
	切换到目标 ubuntu 下：需要编辑脚本，vim ch-mount.sh：
	
	
		#!/bin/bash
		#
		function mnt() {
		 echo "MOUNTING"
		 sudo mount -t proc /proc ${2}proc
		 sudo mount -t sysfs /sys ${2}sys
		 sudo mount -o bind /dev ${2}dev
		 sudo mount -o bind /dev/pts ${2}dev/pts
		 sudo chroot ${2}
		}
		function umnt() {
		 echo "UNMOUNTING"
		 sudo umount ${2}proc
		 sudo umount ${2}sys
		 sudo umount ${2}dev/pts
		 sudo umount ${2}dev
		}
		if [ "$1" == "-m" ] && [ -n "$2" ] ;
		then
		 mnt $1 $2
		elif [ "$1" == "-u" ] && [ -n "$2" ];
		then
		 umnt $1 $2
		else
		 echo ""
		 echo "Either 1'st, 2'nd or both parameters were missing"
		 echo ""
		 echo "1'st parameter can be one of these: -m(mount) OR -u(umount)"
		 echo "2'nd parameter is the full path of rootfs directory(with trailing '/')"
		 echo ""
		 echo "For example: ch-mount -m /media/sdcard/"
		 echo ""
		 echo 1st parameter : ${1}
		 echo 2nd parameter : ${2}
		fi
		
	然后使用 ./ch-mount -m ubuntu-rootfs/
	就会进入到新的 ubuntu 环境下。  

第六步：
	在新的 ubuntu 环境中安装各种软件，安装gcc，编译出的固件是 arm elf32 类型的，也可以直接执行，表明已经构建起了一个 arm 的执行环境。
	
	
	


