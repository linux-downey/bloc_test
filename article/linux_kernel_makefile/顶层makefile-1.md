# 顶层 makefile -1

首先，确定版本号
```
VERSION = 5
PATCHLEVEL = 2
SUBLEVEL = 0
EXTRAVERSION = -rc4
NAME = Golden Lions
```

```
MAKEFLAGS += -rR
unexport LC_ALL
LC_COLLATE=C
LC_NUMERIC=C
export LC_COLLATE LC_NUMERIC
```
设置语言环境


```
ifeq ("$(origin V)", "command line")
  KBUILD_VERBOSE = $(V)
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif
```
```
ifneq ($(findstring s,$(filter-out --%,$(MAKEFLAGS))),)
  quiet=silent_
endif
```
通过命令行参数 V= 和 -s 参数确定Q的值.


/****************************************************************************/
```
ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(O)
endif
```
通过命令行传入的 O= 来确定生成文件的目录，否则就在当前目录。

```
ifneq ($(KBUILD_OUTPUT),)
# Make's built-in functions such as $(abspath ...), $(realpath ...) cannot
# expand a shell special character '~'. We use a somewhat tedious way here.
abs_objtree := $(shell mkdir -p $(KBUILD_OUTPUT) && cd $(KBUILD_OUTPUT) && pwd)
$(if $(abs_objtree),, \
     $(error failed to create output directory "$(KBUILD_OUTPUT)"))

# $(realpath ...) resolves symlinks
abs_objtree := $(realpath $(abs_objtree))
else
abs_objtree := $(CURDIR)
endif # ifneq ($(KBUILD_OUTPUT),)
```

如果 O= 非空，则确定 abs_objtree 即生成文件目标目录的值
```
abs_srctree := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
```
确定 abs_srctree 的值，源码位置
/****************************************************************************/





