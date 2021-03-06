# 中断处理流程-汇编处理

## arm 处理器模式

鉴于中断涉及到 arm 处理器模式的切换，在分析中断之前需要先对 arm 处理器的模式有一个大概的了解，在中断的汇编代码中也涉及到各类寄存器的操作，具体的 arm 处理器相关知识可以参考本专栏 [armv7-A 系列博客](https://zhuanlan.zhihu.com/p/362640343).



## 中断的触发

arm CPU 对中断的处理开始于 irq line 的触发，如果中断没有被屏蔽，也就是  CPSR.I  bit 不为 1 时，程序的执行流会强制跳转到中断向量表的 IRQ 中断向量处，执行中断处理函数。 

在 non-secure 模式下，arm 中的中断向量表有两个位置选项，0x0 和 0xffff0000 ，通过 SCTLR.V bit 控制，一般都会配置为 0xffff0000 地址，这属于内核地址，处理起来更方便。 

硬件上，伴随中断跳转执行的动作还有：

* 处理器模式切换到 irq 模式(中断之前的模式可能是在 user 或者 svc 模式)，同时还伴随着状态寄存器的切换，从 user 模式切换到 irq 模式时，user 下的 apsr 寄存器会自动复制到 irq 模式下的 spsr 寄存器中，如果是 svc 到 irq 模式的切换，则是 svc 模式的 cpsr 复制给 irq 模式的 spsr 寄存器。总之，spsr 用于保存跳转前模式下的状态寄存器。
  
* 中断的返回地址保存在lr 寄存器中，注意这个 lr 是 irq 模式下的 lr 寄存器而不是原模式下的，毕竟原模式下的寄存器是需要保存并在中断返回时完整恢复的。
* irq 模式下的 cpsr 其它位也将被一一更新：
  * 低五位模式位切换为 irq 模式。
  * cpsr .i bit 被设置为 1，也就是发生中断跳转的那一刻，CPU 的中断也就被屏蔽了。 
  * 根据系统设置设置指令集选择位
  * 根据系统设置设置大小端
  * CPSR.IT bit 被清除
* PC 值被设置为对应向量表地址，对于 irq 中断而言，向量地址为 BASE + 0x18，每个向量占 4 个字节



## 从链接脚本开始

要知道向量表被放置在哪个位置，一个方法是去读取 cp15 协处理器的 SCTLR 寄存器，另一个更方便的方法呢，就是通过查看链接脚本。

```
	__vectors_start = .;
	.vectors 0xffff0000 : AT(__vectors_start) {
		*(.vectors)
	}
	. = __vectors_start + SIZEOF(.vectors);
	__vectors_end = .;
	
	__stubs_start = .;
	.stubs ADDR(.vectors) + 0x1000 : AT(__stubs_start) {
		*(.stubs)
	}
	. = __stubs_start + SIZEOF(.stubs);
	__stubs_end = .;
```

.vectors 正是保存向量表的 section，从链接脚本可以看出，linux 的向量表将会被放置在 0xffff0000 这个虚拟地址上，而实际的加载地址为 \_\_vectors_start，这个地址具体的地址取决于地址定位符 "." 的地址，这个地址会随着 section 的放置而自动增长，同时，在链接脚本中定义了 \_\_vectors_end 符号，用来记录向量表的终止地址。 

除了 .vectors section 之外，还有一个比较重要的 .stubs section，这个 section 的虚拟地址为 .vectors section 地址 + 0x1000(4K)，也就是 0xffff0000 + 4K，用于存放异常向量真正的处理代码，当中断发生时，会跳转到这个 section 中的代码执行，具体见下文. 





## 背景知识1-中断向量的初始化

对于 .stubs 和 .vectors section 而言，既然加载地址和虚拟地址(运行地址)不一致，当镜像加载到内核中时，软件上会将向量表从加载地址赋值到虚拟地址，这部分操作在 arch/arm/mm/mmu.c 中的 devicemaps_init 进行初始化：

```c++
static void __init devicemaps_init(const struct machine_desc *mdesc)
{	
	...
	vectors = early_alloc(PAGE_SIZE * 2);
	early_trap_init(vectors);
	...
	map.pfn = __phys_to_pfn(virt_to_phys(vectors));
	map.virtual = 0xffff0000;
	map.length = PAGE_SIZE;
	create_mapping(&map);
	...
}
```

在 devicemaps_init 中，申请两个页面，一个页面占用 4K.

再执行 early_trap_init 函数负责对这两个中断向量页面进行初始化:

```c++
void __init early_trap_init(void *vectors_base)
{
    unsigned long vectors = (unsigned long)vectors_base;
    
    for (i = 0; i < PAGE_SIZE / sizeof(u32); i++)
		((u32 *)vectors_base)[i] = 0xe7fddef1;
    
	extern char __stubs_start[]， __stubs_end[];
	extern char __vectors_start[]， __vectors_end[];
    memcpy((void *)vectors， __vectors_start， __vectors_end - __vectors_start);
	memcpy((void *)vectors + 0x1000， __stubs_start， __stubs_end - __stubs_start);
    ...
}
```

初始化的内容并不复杂，一方面，将 vectors 所属的页面全部填充为无效指令，这是一种保护措施，在出现异常情况下出现预期之外的访问时，产生 undefined 异常从而报告给用户，也方便做有效性的检查. 

另一方面也是最重要的，将 .vectors 和 .stubs section 从加载地址 copy 到对应的目标地址.

early_trap_init 函数执行完成之后，再对这两个页面进行映射，最后的结果就是:

第一个页面虚拟基地址映射为 0xffff0000，起始部分存放异常向量表和 kuser helper 函数，在开启 MMU 之后，这两个页面至于到底放在哪个物理地址上，是无所谓的，总之中断发生时，CPU 地址线发出的地址为 0xffff0000，MMU 负责找到对应物理页面上的中断向量表. 

第二个页面存放 .stub section 中的内容，也就是中断的实际处理代码，异常向量表的 irq 向量处存放的 4 字节指令就是一条跳转指令，跳转到对应的 stub 指令处. 



 ## 背景知识2-中断栈

中断发生时，硬件上强制跳转到异常向量表，处理器模式也切换到 irq 模式，但是，千万别认为整个中断的处理就是在 irq 模式下，而是仅仅在 irq 模式中做一个短暂的停留，然后切换到 svc 模式，也就是内核运行的处理器模式下，在内核中对中断进行统一的处理. 

irq 模式下停留时间再短，也存在保存临时数据的需求，这就需要使用到栈，irq 模式下的 SP 指针是 bank 类型的，在发生模式切换时系统的 sp 也自动切换为 irq 模式的 sp，只是这个属于 irq 模式下的栈空间小得可怜，只有 12 个字节，这个栈空间由系统启动时进行初始化，对应的源码为:

```c++
struct stack {
	u32 irq[3];
	u32 abt[3];
	u32 und[3];
	u32 fiq[3];
} ____cacheline_aligned;
```

可以看出，不仅仅是 irq 模式的栈只有 12 个字节，连 abt/und/fiq 的栈空间也是一样，这也说明这些异常处理和 irq 一样，只是在对应的异常模式下做短暂停留，然后跳转到 svc 模式下执行相应的代码，不过当前版本中的 linux 已经不再支持 fiq 中断. 

设置栈的代码为:

```assembly
void notrace cpu_init(void)
{
	...
	struct stack *stk = &stacks[cpu];
	__asm__ (
	"msr	cpsr_c， %1\n\t"
	"add	r14， %0， %2\n\t"
	"mov	sp， r14\n\t"
	"msr	cpsr_c， %3\n\t"
	"add	r14， %0， %4\n\t"
	"mov	sp， r14\n\t"
	"msr	cpsr_c， %5\n\t"
	"add	r14， %0， %6\n\t"
	"mov	sp， r14\n\t"
	"msr	cpsr_c， %7\n\t"
	"add	r14， %0， %8\n\t"
	"mov	sp， r14\n\t"
	"msr	cpsr_c， %9"
	    :
	    : "r" (stk)，
	      PLC (PSR_F_BIT | PSR_I_BIT | IRQ_MODE)，
	      "I" (offsetof(struct stack， irq[0]))，
	      PLC (PSR_F_BIT | PSR_I_BIT | ABT_MODE)，
	      "I" (offsetof(struct stack， abt[0]))，
	      PLC (PSR_F_BIT | PSR_I_BIT | UND_MODE)，
	      "I" (offsetof(struct stack， und[0]))，
	      PLC (PSR_F_BIT | PSR_I_BIT | FIQ_MODE)，
	      "I" (offsetof(struct stack， fiq[0]))，
	      PLC (PSR_F_BIT | PSR_I_BIT | SVC_MODE)
	    : "r14");
}
```

这是一段嵌入汇编代码，嵌入汇编的语法可以参考[arm嵌入汇编](https://zhuanlan.zhihu.com/p/362897071)，这段代码并不复杂. 

由于不同模式下的 cpsr 和 sp 指针是 bank 类型的，这部分代码将 CPU 依次切换到 irq/abt/und/fiq 模式下，屏蔽 FIQ/IRQ 中断，同时设置 sp 对应的栈空间(上面静态定义的 12 字节的数组)，最后再切换回 svc 模式，毕竟 svc 模式才是内核主要运行的模式. 



## 中断向量的跳转

在代码中全局搜索链接脚本中的 vectors section，就可以找到该段的定义处，也就是中断向量的定义处，在 arch/arm/kernel/entry-arm.S 中：

```assembly
	.section .vectors， "ax"， %progbits
.L__vectors_start:
	W(b)	vector_rst
	W(b)	vector_und
	W(ldr)	pc， .L__vectors_start + 0x1000
	W(b)	vector_pabt
	W(b)	vector_dabt
	W(b)	vector_addrexcptn
	W(b)	vector_irq
	W(b)	vector_fiq
```

arm 中每个异常向量对应的地址宽度为 4 字节，因此除了最后一个 fiq 异常向量以外，其它所有的异常向量都只能在对应的向量表处放置一条跳转指令，对于 irq 而言，这条指令为 W(b)	vector_irq。 

W 实际是一个宏，表示指令的 .w 后缀，强制将该指令编码为 32 位，相对应的还有 .n 后缀，表示强制编码为 16 位，W(b) 表示 b.w，也就是一条固定为 32 位长度的跳转指令。 

真正处理中断的汇编部分代码放置在 .stub section 中:

```assembly
.section .stubs， "ax"， %progbits
...
vector_stub	irq， IRQ_MODE， 4
	.long	__irq_usr			@  0  (USR_26 / USR_32)
	.long	__irq_invalid			@  1  (FIQ_26 / FIQ_32)
	.long	__irq_invalid			@  2  (IRQ_26 / IRQ_32)
	.long	__irq_svc			@  3  (SVC_26 / SVC_32)
	...
```

```assembly
.macro	vector_stub， name， mode， correction=0
	.align	5

vector_\name:
	...
ENDPROC(vector_\name)
	...
.endm
```

从中断向量的跳转指令( W(b)	vector_irq )，可以发现，程序执行流会跳转到 .stubs section 中的 vector_irq 处执行，对应的 vector_stub 参数为 name = irq，mode = IRQ_MODE，correction = 4.



## irq 模式下的中断处理

```assembly
vector_\name:
	.if \correction
	sub	lr， lr， #\correction               ...........................1
	.endif
	
	stmia	sp， {r0， lr}		@ save r0， lr
	mrs	lr， spsr
	str	lr， [sp， #8]		@ save spsr     ...........................2
	
	mrs	r0， cpsr
	eor	r0， r0， #(\mode ^ SVC_MODE | PSR_ISETSTATE)
	msr	spsr_cxsf， r0                        ............................3
	
	and	lr， lr， #0x0f
	mov	r0， sp
 	ldr	lr， [pc， lr， lsl #2]	
	movs	pc， lr			                 ..........................4
```

注1:中断跳转之后的第一条指令是修正 lr 寄存器，当前模式下的 lr 寄存器保存的是中断返回地址，在发生中断时，会将当前执行指令的下一条指令保存到 irq_lr 中，以保证中断处理完成之后返回到被中断的点继续执行，实际上，在流水线系统中， PC 指针并不指向下一条将要执行的指令，而是存在一个偏移值，这里需要对返回值进行修正. 

注2:无论是发生中断还是系统调用导致的模式切换，是必须保存原模式下的寄存器信息的(大部分寄存器都不是 bank 类型的)，因此，在 irq 模式下执行处理时如果需要使用到寄存器，自然就会破坏原来的寄存器信息，就需要将这些寄存器信息暂时保存在栈上，因此，考虑到后续需要使用到 r0 和 lr，就需要先把这两者压到 irq_sp 指定的栈空间中. 

spsr 是原模式下的状态寄存器(user_apsr 或者 svc_cpsr) 值，同样需要保存，进行压栈，irq 模式下的 12 字节栈也就用完了:

* lr : 中断返回地址
* r0: 原模式下的 r0 寄存器值
* spsr:原模式下的状态寄存器

注3:将 irq 模式下的状态寄存器赋值到 r0 中，将 r0 寄存器中的 mode bits 设置为 svc 的模式，再将 r0 中的值更新到 spsr 中，注意，更新到 spsr 并不会导致模式的切换，更新到 cpsr 中才是. 

注4:当前 lr 寄存器的值依旧是 spsr 寄存器，属于原处理器模式下的状态寄存器，对 lr 进行 and 操作的目的在于判断原处理模式是 user 还是 svc，因为中断可能在用户空间触发，也可能在内核空间触发，通过对原模式下状态寄存器的低 4 位进行 0xf 位与操作，就可以以此作为判断依据.

同时，将 irq_sp 保存在 r0 中，再将 \_\_irq_usr 或者 \_\_irq_svc 的地址(取决于中断触发前的原模式)赋值给 lr，最后执行 movs pc，lr.

mov 指令的后缀 s 在 arm 中有两种意思，如果目标寄存器为非 pc，它的隐含操作为当前执行的操作结果会更新状态寄存器，如果目标寄存器为 pc，它的隐含操作为将 spsr 中的值赋值给 cpsr，通常代表着模式切换，因此这个时候也就跳转到了 svc 模式下. 

需要注意的是，通过操作 cpsr 寄存器切换状态并不会自动跳转到异常向量表处，跳转到异常向量表会带来模式切换，但反过来模式切换并不会导致硬件跳转，这个要区分清楚. 



## 用户模式下的中断处理

用户模式下发生的中断将会跳转到 \_\_irq_usr 代码处，依旧是 arch/arm/arm/kernel/entry-armv.S 中的一段汇编代码:

```assembly
__irq_usr:
	usr_entry
	irq_handler
	get_thread_info tsk
	mov	why， #0
	b	ret_to_user_from_irq
 UNWIND(.fnend		)
ENDPROC(__irq_usr)
```

在分析 irq_usr 代码之前，还是有必要回顾以下当前模式下所有寄存器的内容，毕竟这涉及到中断返回的问题.

首先，user，svc，irq 三种模式下， r0~r12 是共用的，此时处于 svc 模式下， r1~r12 依旧是发生中断时的断点数据，而 r0 保存的是 irq_sp 的值，指向保存着用户模式下 r0，lr，apsr 的 irq 的栈空间，而 bank 类型的寄存器就不需要保存了. 

有一个比较有意思的问题是，在 svc 模式下执行中断处理时使用的栈是哪儿来的呢，实际上对于内核中默认的配置，这个栈是借用了 current 进程的内核栈，中断处理并没有设置特殊的栈空间，在中断不支持嵌套以及中断处理程序合理编写的情况下，中断的处理并不会占用太多的内核栈空间，栈溢出的可能性很小.只是保证用完之后清理干净就行了. 

### user_entry

实际上，对于用户模式下的中断处理，和发生系统调用时的处理类似，第一步就是保存断点信息，也就是用户模式下的 r0~r15 以及 apsr 寄存器值，内核中使用 struct pt_regs 结构体记录这些寄存器参数，这个结构体包含一个数组成员，该数组包含 18 个 long 型成员的数组( sizeof(long) 等于寄存器宽度). 

实际上，r0~r15 再加上 apsr 总共只需要 17 个寄存器，而额外的第 18 个成员用于保存 ARM_ORIG_r0，实际上这个成员和中断没有关系，和内核中的系统调用有关系，在系统调用中 r0 既用来保存第一个参数，又是系统调用的返回值，因此两个 r0 都需要被保存，也就是第 18 个 long 型成员的来源，和中断并没有什么关系，只是复用了这个结构而已. 

user entry 的实现为:

```assembly
.macro	usr_entry， trace=1， uaccess=1
	sub	sp， sp， #PT_REGS_SIZE
	stmib	sp， {r1 - r12}              ...............................1
	
	ldmia	r0， {r3 - r5}               ...............................2
	add	r0， sp， #S_PC		            
	mov	r6， #-1			

	str	r3， [sp]		@ save the "real" r0 copied
	stmia	r0， {r4 - r6}
	stmdb	r0， {sp， lr}^               ...............................3
	zero_fp
	.endm
```

除去 thumb 指令和 trace 相关的代码，上面就是 usr_entry 的核心实现部分.  

注1:PT_REGS_SIZE 的值就是 sizeof(struct pt_regs) 的长度，在当前栈上开辟出空间用于后续保存断点寄存器的值，接着将 r1-r12 寄存器的值保存在当前栈上，r1-r12 中都是原始的断点寄存器的值.

注2:当前模式下的 r0 保存的是 irq 模式下的栈空间地址，将 irq 模式下的栈空间中的内容加载到 r3 - r5 寄存器中，r3 中保存的是断点处的 r0 寄存器，将它保存在开辟的栈中. 

注3:经过 add	r0， sp， #S_PC 这条指令， r0 保存的是 struct regs 中指向 PC 的地址，依次再将 r4(lr，实际上是中断返回地址) r5(apsr) r6(-1) 保存在栈上 pc/apcr/origin_r0 的位置 ，因为 sp 和 lr 是bank 类型的，因此 svc 模式下直接访问到的是当前模式下的，和断点处的 sp/lr 寄存器无关.

因此， stmdb	r0， {sp， lr}^ 指令中的后缀 ^ 表示访问的是 user 模式下的 sp 和 lr 寄存器，也就是将断点处的 sp 和 lr 保存到了栈上. 

到这里，中断现场所有的寄存器已经保存在了栈上，多余的 origin_r0 中保存的是 -1. 

### irq_handler

从名称不难看出，这是中断处理的核心函数:

```assembly
	.macro	irq_handler
	ldr	r1， =handle_arch_irq                        ......................1
	mov	r0， sp
	badr	lr， 9997f
	ldr	pc， [r1]
9997:
	.endm
```

注1: handle_arch_irq 是一个函数指针，对于集成 gicv2 版本的 arm 系统而言，这个函数指针在 gic 初始化的时候被赋值，自然地，这也是核心的中断处理函数，这个函数的详情可以直接参考这篇博客(TODO).

调用 handle_arch_irq 参数为当前的 sp 指针，也就是上面保存的断点信息，以 struct pt_regs* 的形式传递给函数，当然，如果函数对这些栈上的值做了修改，就需要将它们恢复. 



## ret_to_user_from_irq

在执行完 irq_handler 之后，中断也就处理完了，接下来就是收尾的工作，主要是返回到中断现场.

```assembly
	get_thread_info tsk
	mov	why， #0
	b	ret_to_user_from_irq
```

tsk 为 r9 寄存器，保存 current task 的 thread_info 的基地址，why 是 r8 寄存器，赋值为 0.



```assembly
ENTRY(ret_to_user_from_irq)
	ldr	r1， [tsk， #TI_FLAGS]
	tst	r1， #_TIF_WORK_MASK
	bne	slow_work_pending           ...................................1
no_work_pending:
	...
	restore_user_regs fast = 0， offset = 0    .........................2
ENDPROC(ret_to_user_from_irq)
```

注1：在返回到用户空间的时候，需要先检查是否需要重新调度或者当前进程是否有信号处于 pending 状态，如果是，就需要跳转到 slow_work_pending，也就是先处理这些额外的工作，再执行返回的工作。
对应的处理为：

```assembly
slow_work_pending:
	mov	r0， sp				@ 'regs'
	mov	r2， why				@ 'syscall'
	bl	do_work_pending
	cmp	r0， #0
	beq	no_work_pending
	...
```

在 slow_work_pending 分支将会调用 do_work_pending，参数分别为 struct pt_regs*，thread_flags，syscall，主要执行的工作有两部分：1、如果 thread_flags & _TIF_NEED_RESCHED 为真，说明需要重新调度，因此调用 schedule() 执行调度。2、如果 thread_flags & _TIF_SIGPENDING 为真，说明当前进程有接收到信号，需要调用  do_signal 函数对未处理信号进行处理。
处理完成之后，跳转到 no_work_pending，也就是执行最终的返回操作，见注2。

注2：如果系统没有额外的事件需要处理，就直接执行中断的返回流程，调用 restore_user_regs ，在进入到 svc 模式之后，中断断点处的所有寄存器信息已经被保存到栈上，r0-r12 是共享的，返回的工作无非就是将所有的断点寄存器恢复:

```assembly
	mov	r2， sp
	ldr	r1， [r2， #\offset + S_PSR]	@ get calling cpsr
	ldr	lr， [r2， #\offset + S_PC]!	@ get pc
	
	msr	spsr_cxsf， r1			@ save in spsr_svc
	
	ldmdb	r2， {r0 - lr}^			@ get calling r0 - lr
	
	add	sp， sp， #\offset + PT_REGS_SIZE
	movs	pc， lr				@ return & move spsr_svc into cpsr
```



返回用户空间的核心代码就是这些，因为 restore_user_regs 是和系统调用共用的代码，因此精简了一些系统调用相关的指令。 

在处理完中断之后，sp 上保存了 17 个 long 型成员的断点寄存器值(第18个 orig_r0 用不到)，在上面的指令中，先会将 apsr 断点寄存器保存到当前模式下的 spsr 寄存器中，接着使用 ldmdb 将 r0-lr(r14) 将所有的寄存器恢复，需要注意的是 ^ 后缀表示操作的是 user 模式下的寄存器，也就将所有寄存器恢复到了发生中断前的状态。

既然是借用了 svc 模式下的栈，自然要随借随还，接着将栈上保存的寄存器内容清除，其实也没有清除，就是移动 sp 指针。

在所有其它寄存器都恢复到断点寄存器之后，将执行   

```
movs	pc， lr
```

这条指令实行真正的跳转工作，并且后缀 s 会将 svc 的 spsr 更新到 cpsr 中，恢复到 user 模式下，同时中断也就重新打开了。



## 内核状态下的中断处理

如果代码正运行在内核中，也就是 svc 模式下，发生中断将会跳转到 __irq_svc 指令处：

```assembly
__irq_svc:
	svc_entry                                 .............................1
	irq_handler                               .............................2

#ifdef CONFIG_PREEMPT                         .............................3
	ldr	r8， [tsk， #TI_PREEMPT]		@ get preempt count
	ldr	r0， [tsk， #TI_FLAGS]		@ get flags
	teq	r8， #0				@ if preempt count != 0
	movne	r0， #0				@ force flags to 0
	tst	r0， #_TIF_NEED_RESCHED
	blne	svc_preempt
#endif

	svc_exit r5， irq = 1			@ return from exception
 UNWIND(.fnend		)
ENDPROC(__irq_svc)
```

避免混淆，我们再来回顾一下执行 __irq_svc 时当前的系统状态：

当中断发生时，自动跳转到 irq 模式下，在 irq 模式下，会将中断返回地址、原模式下的 r0 寄存器值以及原模式下的状态寄存器保存在 irq 栈上，并将这个栈地址存放到 r0 寄存器中，跳转到 svc 模式下。

因此，在执行 __irq_svc 时，r0 指向 irq 模式下的栈空间，r1-r12 为断点寄存器的值，因为内核下发生中断就是从 svc 切换到 irq 再切换回 svc，sp 和 lr 寄存器并没有被修改，因此所有的断点信息依旧存在。

注1：svc_entry 和 usr_entry 差不多，都是在栈上保存中断现场，只不过一个是针对用户空间，一个是针对内核空间，和 usr_entry 不一样的是，svc_entry 保存的断点信息由 struct svc_pt_regs 描述，该结构体中包含了 struct pt_regs，也就是原本的 18 个寄存器信息，额外多出两个 32 位的 dacr 和 addr_limit。 

dacr 寄存器全名为 Domain Access Control Register，针对内存域(memory domains)的访问权限，这一部分可以参考 armv7-A 参考手册中 B4.1.43  小节对该寄存器的描述，这是一个通过 cp15 协处理器管理的寄存器。 

addr_limit 表示访问地址限制。

保存断点信息的操作入栈位置和 usr_entry  的操作差不多，这里就不过多赘述了，区别正如上文所说，开辟的栈空间需要多出 8 字节用于保存 dacr 寄存器值和 addr_limit 值，dacr 是通过 cp15 协处理器操作指令读取，addr_limit 则是 current 进程对应的 thread_info->addr_limit 值，这两部分主要涉及到安全检查相关，本文暂时不做分析。 

注2：irq_handler 就是中断的核心处理部分，详情可以参考这篇文章(TODO)。

注3：在处理用户空间发生的中断时，在返回之前需要先检查是否需要重新调度，同时还会检查是否存在 pending 的信号，如果是内核空间发生的中断，不需要做信号的检查，只要检查重新调度的标志，同时即使设置了重新调度标志，也需要先检查抢占标志位是否为 0，如果不为 0，说明内核抢占被禁止，不执行调度，否则执行 svc_preempt 执行重新调度的工作，在重新调度的过程中会临时开启中断，_schedule 函数之后又会关闭中断。

注4：内核中断的返回比较简单，没有模式切换，也就是将保存在栈上的断点寄存器信息恢复到各个寄存器中

```assembly
msr	spsr_cxsf， \rpsr
ldmia	sp， {r0 - pc}^
```

后缀 "^" 表示将 spsr 赋值给 cpsr，因此同时恢复的还有中断发生前的 cpsr 寄存器，完全恢复到中断断点处时，被禁用的中断也就恢复了。 





### 参考

[蜗窝科技：中断处理流程](http://www.wowotech.net/irq_subsystem/irq_handler.html)

4.14 内核源码



[专栏首页(博客索引)](https://zhuanlan.zhihu.com/p/362640343)

原创博客，转载请注明出处。

