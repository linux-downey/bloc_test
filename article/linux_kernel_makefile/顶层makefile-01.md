在多种情况下主makefile并非是所有的开始:
1.编译模块时make -C 进入主makefile
2、M=DIR 时，主makefile 被复制到目标目录下，
3、循环递归调用主makefile


if(sub_make_done)
在执行完一次之后，sub_make_done会被置1，也就是递归调用时并不会再次进入到这里。

第一部分就是为了确认是否为 O=DIR 的情况，进入到其他目录进行编译：
    先确定need-sub-nake 的值，
    如果 need-sub-nake 为1，则
        sub-make:
        $(Q)$(MAKE) -C $(abs_objtree) -f $(abs_srctree)/Makefile $(MAKECMDGOALS)
    进入到目标目录中的 makefile 进行编译。  

第二部分，真正处理：

//两个语句意思：第一个：目标里有 $(no-dot-config-targets) 目标，第二个，目标里只有  $(no-dot-config-targets)，这时 dot-config := 0 。
    ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
        ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
            dot-config := 0
        endif
    endif

// 目标里有且仅有 $(no-sync-config-targets) 目标。
ifneq ($(filter $(no-sync-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-sync-config-targets), $(MAKECMDGOALS)),)
		may-sync-config := 0
	endif
endif

// KBUILD_EXTMOD 由M=传递进来
//如果 KBUILD_EXTMOD 不为空，may-sync-config := 0


// 没有传入 M= 参数，且目标中有 %config , config-targets := 1 ，如果目标不止一个，说明是混合多个目标，mixed-targets := 1
// 随着config执行的，比如 make config all
ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter config %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(words $(MAKECMDGOALS)),1)
                        mixed-targets := 1
                endif
        endif
endif


// 与 clean 一起执行的，比如 make clean all，  mixed-targets := 1
ifneq ($(filter $(clean-targets),$(MAKECMDGOALS)),)
        ifneq ($(filter-out $(clean-targets),$(MAKECMDGOALS)),)
                mixed-targets := 1
        endif
endif

//install和modules_install 同样需要一个一个被执行 mixed-targets := 1
ifneq ($(filter install,$(MAKECMDGOALS)),)
        ifneq ($(filter modules_install,$(MAKECMDGOALS)),)
	        mixed-targets := 1
        endif
endif


开始处理的部分：
//如果是复合目标，则需要一个个编译，其实就是一次次递归调用主makefile
ifeq ($(mixed-targets),1)
_build_one_by_one:
	$(Q)set -e; \
	for i in $(MAKECMDGOALS); do \
		$(MAKE) -f $(srctree)/Makefile $$i; \
	done


//否则就是单个的目标的编译
//这部分才是重点

include scripts/Kbuild.include

ifeq ($(config-targets),1)  判断是不是config-targe 并对其进行编译

如果 KBUILD_EXTMOD 为空，目标就是all，否则目标就是modules

如果 没有指定，仅仅是make，同样也会编译 modules。


KBUILD_MODULES 表示是否编译外部模块
KBUILD_BUILTIN 表示编译内部模块

KBUILD_EXTMOD 则是由 M=DIR 传入的参数





部分：
1、编译完全独立的目标，比如 install、*headers，clean等，不需要进行同步
2、编译多个目标，那就一个一个来，递归进入
3、O= DIR,进入到另一个目标进行编译
4、真正地编译一个目标
5、仅编译一个外部模块


