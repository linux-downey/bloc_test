# vscode 的配置

主要应用于看内核代码：

文件过滤：
	File->prefenrence->setting->workspace,settings.json:

{
    "C_Cpp.updateChannel": "Insiders",
    "explorer.confirmDelete": false,
    "files.exclude": {
        "**/.tmp*": true,
        "**/.vscode": true,
        "**/*.buildin": true,
        "**/*.builtin": true,
        "**/*.cmd": true,
        "**/*.ko": true,
        "**/*.o": true,
        "**/arch/alpha": true,
        "**/arch/arc": true,
        "**/arch/c6x": true,
        "**/arch/h8300": true,
        "**/arch/hexagon": true,
        "**/arch/ia64": true,
        "**/arch/m32r": true,
        "**/arch/m68k": true,
        "**/arch/microblaze": true,
        "**/arch/mn10300": true,
        "**/arch/nds32": true,
        "**/arch/nios2": true,
        "**/arch/parisc": true,
        "**/arch/powerpc": true,
        "**/arch/s390": true,
        "**/arch/score": true,
        "**/arch/sh": true,
        "**/arch/sparc": true,
        "**/arch/um": true,
        "**/arch/unicore32": true,
        "**/arch/blackfin": true,
        "**/arch/cris": true,
        "**/arch/frv": true,
        "**/arch/metag": true,
        "**/arch/mips": true,
        "**/arch/openrisc": true,
        "**/arch/tile": true,
        "**/arch/x86": true,
        "**/arch/xtensa": true,
        "**/arch/arm": true,
        "**/Documentation": true,
        "**/tools": true,
        "**/vmlinux": true,
        "**/boot/dts/[a-e]*":true,
        "**/boot/dts/[g-z]*":true,
    }
    
}

根据情况来，因为是看 arm64 的代码，平台是 imx8，所以将除了 arch/arm64 之外的 arch 全部删除，同时将 dts 中除了 Fresscale 的 dts 文件全部删除。  


内核会将 .config 生成的头文件包含到编译过程中，但是看代码的时候并不会添加到文件中，需要手动地添加到代码中，以更方便地看代码。  

.config 文件生成的对应头文件为：include/generated/uapi/autoconf.h 中，也可能在 include/generated/autoconf.h 中，根据平台不同而不同。  

在 include/linux/kernel.h 中添加这些头文件：
	#include <generated/autoconf.h>
	#include <generated/autoksyms.h>
	#include <generated/bounds.h>
	#include <generated/compile.h>
	#include <generated/timeconst.h>

这会导致 make clean 之后重新编译出错，要注意。 
