systemd unit：
单位文件是纯文本ini样式的文件，它编码有关服务，套接字，设备，安装点，自动安装点，交换文件或分区，启动目标，监视的文件系统路径，由systemd（1），资源管理片或一组外部创建的进程控制和监督的计时器  

该手册页列出了所有单元类型的常用配置选项。这些选项需要在单元文件的[Unit]或[Install]部分中进行配置。

除了此处描述的通用[Unit]和[Install]部分之外，每个单元还可以包含特定于类型的部分，例如服务单位的[Service]

单位文件是从编译期间确定的一组路径中加载的，这将在下一部分中介绍

有效的单元名称由“名称前缀”，点和后缀指定单元类型组成。 “单位前缀”必须包含一个或多个有效字符（ASCII字母，数字，“：”，“-”，“ _”，“。”和“ \”）。单元名称的总长度（包括后缀）不得超过256个字符。类型后缀必须是“ .service”，“。socket”，“。device”，“。mount”，“。automount”，“。swap”，“。target”，“。path”，“。timer”之一”，“。slice”或“ .scope”。  

单位名称可以通过一个称为“实例名称”的参数进行参数化。然后基于“模板文件”构造该单元，该“模板文件”用作多个服务或其他单元的定义。模板单元的名称末尾（在类型后缀之前）必须有一个“ @”。完整单元的名称是通过在“ @”和单元类型后缀之间插入实例名称来形成的。

除此处列出的选项外，单位文件可能还包含其他选项。如果systemd遇到未知选项，它将写入警告日志消息，但继续加载设备。如果选项或节名称以X-为前缀，则systemd会完全忽略它。被忽略部分中的选项不需要前缀。应用程序可以使用它在单位文件中包含其他信息。

通过在单位搜索路径之一中创建从新名称到现有名称的符号链接，可以对单位进行别名（使用其他名称）。例如，systemd-networkd.service的别名为dbus-org.freedesktop.network1.service，在安装过程中以符号链接的形式创建，因此当通过D-Bus请求systemd加载dbus-org.freedesktop.network1.service时，它会将加载systemd-networkd.service。再举一个例子，default.target（在启动时启动的默认系统目标）通常被符号链接（别名）到multi-user.target或graphic.target以选择默认启动的对象。别名可以在禁用，启动，停止，状态等命令中使用，也可以在所有单元依赖指令中使用，包括Wants =，Requires =，Before =，After =。别名不能与预置命令一起使用。

别名遵循以下限制：某种类型的单元（“ .service”，“。socket”，…）只能以具有相同类型后缀的名称作为别名。普通单元（不是模板或实例）只能以普通名称作为别名。一个模板实例只能由另一个模板实例作为别名，并且实例部分必须相同。模板可以被另一个模板作为别名（在这种情况下，别名适用于模板的所有实例）。作为特殊情况，模板实例（例如“ alias@inst.service”）可以是指向不同模板（例如“ template@inst.service”）的符号链接。在这种情况下，仅此特定实例具有别名，而模板的其他实例（例如“ alias@foo.service”，“ alias@bar.service”）则不具有别名。这些规则保留了始终为给定单元及其所有别名唯一定义实例（如果有）的要求。

单元文件可以通过[Install]部分中的Alias =指令指定别名。启用该单元后，将为这些名称创建符号链接，并在禁用该单元时将其删除。例如，reboot.target指定Alias = ctrl-alt-del.target，因此，启用后，将创建指向reboot.target文件的符号链接/etc/systemd/systemd/ctrl-alt-del.service以及何时创建。调用Ctrl + Alt + Del，systemd将查找ctrl-alt-del.service并执行reboot.service。在正常运行期间，systemd根本不查看[Install]部分，因此该部分中的任何指令仅通过启用期间创建的符号链接才有效。

与单元文件foo.service一起，目录foo.service.wants /可能存在。从此类目录符号链接的所有单位文件都作为类型Wants =的依赖项隐式添加到该单位。对于Requires =类型依赖项也存在类似的功能，在这种情况下，目录后缀为.requires /。此功能对于将单元挂接到其他单元的启动很有用，而无需修改其单元文件。有关Wants =语义的详细信息，请参见下文。在单位文件的.wants /或.requires /目录中创建符号链接的首选方法是，将依赖项嵌入目标单元的[Install]部分，然后使用以下命令的enable或预设命令在文件系统中创建符号链接： systemctl（1）。


