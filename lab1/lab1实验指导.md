# Lab 1: RV64 内核引导与时钟中断处理

## 实验目的
* 学习 RISC-V 汇编， 编写 head.S 实现跳转到内核运行的第一个 C 函数。
* 学习 OpenSBI，理解 OpenSBI 在实验中所起到的作用，并调用 OpenSBI 提供的接口完成字符的输出。
* 学习 Makefile 相关知识， 补充项目中的 Makefile 文件， 来完成对整个工程的管理。
* 学习 RISC-V 的 trap 处理相关寄存器与指令，完成对 trap 处理的初始化。
* 理解 CPU 上下文切换机制，并正确实现上下文切换功能。
* 编写 trap 处理函数，完成对特定 trap 的处理。
* 调用 OpenSBI 提供的接口，完成对时钟中断事件的设置。

## 实验环境

- Environment in Lab0

## 实验基础知识介绍


### RV64 内核引导
#### 前置知识

为了顺利完成 OS 实验，我们需要一些前置知识和较多调试技巧。在 OS 实验中我们需要 **RISC-V汇编** 的前置知识，课堂上不会讲授，请同学们通过阅读以下四份文档自学：

- [RISC-V Assembly Programmer's Manual](https://github.com/riscv-non-isa/riscv-asm-manual/blob/master/riscv-asm.md)
- [RISC-V Unprivileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf)
- [RISC-V Privileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf)
- [RISC-V 手册（中文）](http://riscvbook.com/chinese/RISC-V-Reader-Chinese-v2p1.pdf)

> 注：RISC-V 手册（中文）中有一些 Typo，请谨慎参考。

#### RISC-V 的三种特权模式

RISC-V 有三个特权模式：U (user) 模式、S (supervisor) 模式和 M (machine) 模式。

| Level  | Encoding |       Name       | Abbreviation |
| ------ | -------- |----------------- | ------------ |
|   0    |    00    | User/Application |      U       |
|   1    |    01    |     Supervisor   |      S       |
|   2    |    10    |      Reserved    |              |
|   3    |    11    |      Machine     |      M       |

其中：

- M 模式是对硬件操作的抽象，有**最高**级别的权限
- S 模式介于 M 模式和 U 模式之间，在操作系统中对应于内核态 (Kernel)。当用户需要内核资源时，向内核申请，并切换到内核态进行处理
- U 模式用于执行用户程序，在操作系统中对应于用户态，有**最低**级别的权限

#### 3.1.3 从计算机上电到 OS 运行

我们以最基础的嵌入式系统为例，计算机上电后，首先硬件进行一些基础的初始化后，将 CPU 的 Program Counter 移动到内存中 Bootloader 的起始地址。
Bootloader 是操作系统内核运行之前，用于初始化硬件，加载操作系统内核。
在 RISC-V 架构里，Bootloader 运行在 M 模式下。Bootloader 运行完毕后就会把当前模式切换到 S 模式下，机器随后开始运行 Kernel。

这个过程简单而言就是这样：

```
   Hardware             RISC-V M Mode           RISC-V S Mode 
+------------+         +--------------+         +----------+
|  Power On  |  ---->  |  Bootloader  |  ---->  |  Kernel  |
+------------+         +--------------+         +----------+
```

#### SBI 与 OpenSBI

SBI (Supervisor Binary Interface) 是 S-mode 的 Kernel 和 M-mode 执行环境之间的接口规范，而 OpenSBI 是一个 RISC-V SBI 规范的开源实现。RISC-V 平台和 SoC 供应商可以自主扩展 OpenSBI 实现，以适应特定的硬件配置。

简单的说，为了使操作系统内核适配不同硬件，OpenSBI 提出了一系列规范对 M-mode 下的硬件进行了统一定义，运行在 S-mode 下的内核可以按照这些规范对不同硬件进行操作。

![RISC-V SBI 介绍](img/riscv-sbi.png)

为降低实验难度，我们选择 OpenSBI 作为 Bootloader 来完成机器启动时 M-mode 下的硬件初始化与寄存器设置，并使用 OpenSBI 所提供的接口完成诸如字符打印的操作。

在实验中，QEMU 已经内置了 OpenSBI 作为 Bootloader，我们可以使用 `-bios default` 启用。如果启用，QEMU 会将 OpenSBI 代码加载到 0x80000000 起始处。OpenSBI 初始化完成后，会跳转到 0x80200000 处（也就是 Kernel 的起始地址）。因此，我们所编译的代码需要放到 0x80200000 处。

如果你对 RISC-V 架构的 Boot 流程有更多的好奇，可以参考这份 [bootflow](https://riscv.org/wp-content/uploads/2019/12/Summit_bootflow.pdf)。

#### Makefile

Makefile 可以简单的认为是一个工程文件的编译规则，描述了整个工程的编译和链接流程。在 Lab0 中我们已经使用了 make 工具利用 Makefile 文件来管理整个工程。在阅读了 [Makefile介绍](https://seisman.github.io/how-to-write-makefile/introduction.html) 这一章节后，同学们可以根据工程文件夹里 Makefile 的代码来掌握一些基本的使用技巧。

#### 内联汇编
内联汇编（通常由 asm 或者 \_\_asm\_\_ 关键字引入）提供了将汇编语言源代码嵌入 C 程序的能力。
内联汇编的详细介绍请参考 [Assembler Instructions with C Expression Operands](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html) 。
下面简要介绍一下这次实验会用到的一些内联汇编知识：

内联汇编基本格式为：

```c
    __asm__ volatile (
        "instruction1\n"
        "instruction2\n"
        ......
        ......
        "instruction3\n"
        : [out1] "=r" (v1),[out2] "=r" (v2)
        : [in1] "r" (v1), [in2] "r" (v2)
        : "memory"
    );
```

其中，三个 `:` 将汇编部分分成了四部分：

- 第一部分是汇编指令，指令末尾需要添加 '\n'。
- 第二部分是输出操作数部分。
- 第三部分是输入操作数部分。
- 第四部分是可能影响的寄存器或存储器，用于告知编译器当前内联汇编语句可能会对某些寄存器或内存进行修改，使得编译器在优化时将其因素考虑进去。

这四部分中后三部分不是必须的。

##### 示例一

```c
unsigned long long s_example(unsigned long long type,unsigned long long arg0) {
    unsigned long long ret_val;
    __asm__ volatile (
        "mv x10, %[type]\n"
        "mv x11, %[arg0]\n"
        "mv %[ret_val], x12"
        : [ret_val] "=r" (ret_val)
        : [type] "r" (type), [arg0] "r" (arg0)
        : "memory"
    );
    return ret_val;
}
```
示例一中指令部分，`%[type]`、`%[arg0]` 以及 `%[ret_val]` 代表着特定的寄存器或是内存。

输入输出部分中，`[type] "r" (type)`代表着将 `()` 中的变量 `type` 放入寄存器中（`"r"` 指放入寄存器，如果是 `"m"` 则为放入内存），并且绑定到 `[]` 中命名的符号中去。`[ret_val] "=r" (ret_val)` 代表着将汇编指令中 `%[ret_val]` 的值更新到变量 `ret_val`中。

##### 示例二

```c
#define write_csr(reg, val) ({
    __asm__ volatile ("csrw " #reg ", %0" :: "r"(val)); })
```

示例二定义了一个宏，其中 `%0` 代表着输出输入部分的第一个符号，即 `val`。

`#reg` 是c语言的一个特殊宏定义语法，相当于将reg进行宏替换并用双引号包裹起来。

例如 `write_csr(sstatus,val)` 经宏展开会得到：

```c
({
    __asm__ volatile ("csrw " "sstatus" ", %0" :: "r"(val)); })
```

#### 编译相关知识介绍
##### vmlinux.lds

GNU ld 即链接器，用于将 `*.o` 文件（和库文件）链接成可执行文件。在操作系统开发中，为了指定程序的内存布局，ld 使用链接脚本（Linker Script）来控制，在 Linux Kernel 中链接脚本被命名为 vmlinux.lds。更多关于 ld 的介绍可以使用 `man ld` 命令。

下面给出一个 vmlinux.lds 的例子：

```lds
/* 目标架构 */
OUTPUT_ARCH( "riscv" )

/* 程序入口 */
ENTRY( _start )

/* kernel代码起始位置 */
BASE_ADDR = 0x80200000;

SECTIONS
{
    /* . 代表当前地址 */
    . = BASE_ADDR;

    /* 记录kernel代码的起始地址 */
    _skernel = .;

    /* ALIGN(0x1000) 表示4KB对齐 */
    /* _stext, _etext 分别记录了text段的起始与结束地址 */
    .text : ALIGN(0x1000){
        _stext = .;

        *(.text.entry)
        *(.text .text.*)
        
        _etext = .;
    }

    .rodata : ALIGN(0x1000){
        _srodata = .;

        *(.rodata .rodata.*)
        
        _erodata = .;
    }

    .data : ALIGN(0x1000){
        _sdata = .;

        *(.data .data.*)
        
        _edata = .;
    }

    .bss : ALIGN(0x1000){
        _sbss = .;

        *(.bss.stack)
        sbss = .;
        *(.bss .bss.*)
        
        _ebss = .;
    }

    /* 记录kernel代码的结束地址 */
    _ekernel = .;
}
```

首先我们使用 OUTPUT_ARCH 指定了架构为 RISC-V ，之后使用 ENTRY 指定程序入口点为 `_start` 函数，程序入口点即程序启动时运行的函数，经过这样的指定后在head.S中需要编写 `_start` 函数，程序才能正常运行。

链接脚本中有`.` `*`两个重要的符号。单独的`.`在链接脚本代表当前地址，它有赋值、被赋值、自增等操作。而`*`有两种用法，其一是`*()`在大括号中表示将所有文件中符合括号内要求的段放置在当前位置，其二是作为通配符。

链接脚本的主体是SECTIONS部分，在这里链接脚本的工作是将程序的各个段按顺序放在各个地址上，在例子中就是从0x80200000地址开始放置了 `.text` ， `.rodata` ， `.data` 和 `.bss` 段。各个段的作用可以简要概括成：

| 段名     | 主要作用                       |
| ------- | ----------------------------- |
| .text   | 通常存放程序执行代码              |
| .rodata | 通常存放常量等只读数据            |
| .data   | 通常存放已初始化的全局变量、静态变量 |
| .bss    | 通常存放未初始化的全局变量、静态变量 |

在链接脚本中可以自定义符号，例如以上所有 `_s` 与  `_e`开头的符号都是我们自己定义的。

更多有关链接脚本语法可以参考[这里](https://sourceware.org/binutils/docs/ld/Scripts.html)。

##### vmlinux

vmlinux 通常指 Linux Kernel 编译出的可执行文件 (Executable and Linkable Format / ELF)，特点是未压缩的，带调试信息和符号表的。在整套 OS 实验中，vmlinux 通常指将你的代码进行编译，链接后生成的可供 QEMU 运行的 RV64 架构程序。如果对 vmlinux 使用 `file` 命令，你将看到如下信息：

```bash
$ file vmlinux 
vmlinux: ELF 64-bit LSB executable, UCB RISC-V, version 1 (SYSV), statically linked, not stripped
```

##### System.map

System.map 是内核符号表（Kernel Symbol Table）文件，是存储了所有内核符号及其地址的一个列表。“符号”通常指的是函数名，全局变量名等等。使用 `nm vmlinux` 命令即可打印 vmlinux 的符号表，符号表的样例如下：

```
0000000000000800 A __vdso_rt_sigreturn
ffffffe000000000 T __init_begin
ffffffe000000000 T _sinittext
ffffffe000000000 T _start
ffffffe000000040 T _start_kernel
ffffffe000000076 t clear_bss
ffffffe000000080 t clear_bss_done
ffffffe0000000c0 t relocate
ffffffe00000017c t set_reset_devices
ffffffe000000190 t debug_kernel
```

使用 System.map 可以方便地读出函数或变量的地址，为 Debug 提供了方便。

### RV64 时钟中断处理
如果完成了 **3.1** 中的 **RV64 内核引导**，我们能成功地将一个最简单的 OS 启动起来， 但还没有办法与之交互。我们在课程中讲过操作系统启动之后由**事件**（ **event** ）驱动，在本次实验的后半部分中，我们将引入一种重要的事件 **trap**，trap 给了 OS 与硬件、软件交互的能力。在 **3.1** 中我们介绍了在 RISC-V 中有三种特权级 ( M 态、 S 态、 U 态 )， 在 Boot 阶段， OpenSBI 已经帮我们将 M 态的 trap 处理进行了初始化，这一部分不需要我们再去实现，因此后续我们重点关注 S 态的 trap 处理。
#### RISC-V 中的 Interrupt 和 Exception
##### 什么是 Interrupt 和 Exception

> We use the term **exception** to refer to an unusual condition occurring at run time **associated with an instruction** in the current RISC-V hart. We use the term **interrupt** to refer to an **external asynchronous event** that may cause a RISC-V hart to experience an unexpected transfer of control. We use the term **trap** to refer to **the transfer of control to a trap handler** caused by either an exception or an interrupt.

上述是 [RISC-V Unprivileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf) 1.6 节中对于 `Trap`、 `Interrupt` 与 `Exception` 的描述。总结起来 `Interrupt` 与 `Exception` 的主要区别如下表：

|Interrupt|Exception|
|:---|:---|
|Hardware generate|Software generate|
|These are **asynchronous external requests** for service (like keyboard or printer needs service).|These are **synchronous internal requests** for service based upon abnormal events (think of illegal instructions, illegal address, overflow etc).|
|These are **normal events** and shouldn’t interfere with the normal running of a computer.|These are **abnormal events** and often result in the termination of a program|

上文中的 `Trap` 描述的是一种控制转移的过程, 这个过程是由 `Interrupt` 或者 `Exception` 引起的。这里为了方便起见，我们在这里约定 `Trap` 为 `Interrput` 与 `Exception` 的总称。

##### 相关寄存器

除了32个通用寄存器之外，RISC-V 架构还有大量的 **控制状态寄存器** `Control and Status Registers(CSRs)`，下面将介绍几个和 trap 机制相关的重要寄存器。

Supervisor Mode 下 trap 相关寄寄存器:

- `sstatus` ( Supervisor Status Register )中存在一个 SIE ( Supervisor Interrupt Enable ) 比特位，当该比特位设置为 1 时，会**响应**所有的 S 态 trap， 否则将会禁用所有 S 态 trap。
- `sie` ( Supervisor Interrupt Eable Register )。在 RISC-V 中，`Interrupt` 被划分为三类 `Software Interrupt`， `Timer Interrupt`， `External Interrupt`。在开启了 `sstatus[SIE]`之后，系统会根据 `sie` 中的相关比特位来决定是否对该 `Interrupt` 进行**处理**。
- `stvec` ( Supervisor Trap Vector Base Address Register ) 即所谓的“中断向量表基址”。 `stvec` 有两种模式：`Direct 模式`，适用于系统中只有一个中断处理程序, 其指向中断处理入口函数 （ 本次实验中我们所用的模式 ）。`Vectored 模式`，指向中断向量表， 适用于系统中有多个中断处理程序 （ 该模式可以参考[ RISC-V 内核源码](https://elixir.bootlin.com/linux/latest/source/arch/riscv/kernel/entry.S#L564) ）。
- `scause` ( Supervisor Cause Register ), 会记录 trap 发生的原因，还会记录该 trap 是 `Interrupt` 还是 `Exception`。
- `sepc` ( Supervisor Exception Program Counter ), 会记录触发 exception 的那条指令的地址。

Machine Mode 异常相关寄寄存器:

- 类似于 Supervisor Mode， Machine Mode 也有相对应的寄存器，但由于本实验同学不需要操作这些寄存器，故不在此作介绍。

以上寄存器的详细介绍请同学们参考 [RISC-V Privileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf)

##### 相关特权指令
- `ecall` ( Environment Call )，当我们在 S 态执行这条指令时，会触发一个 `ecall-from-s-mode-exception`，从而进入 M Mode 下的处理流程( 如设置定时器等 )；当我们在 U 态执行这条指令时，会触发一个 `ecall-from-u-mode-exception`，从而进入 S Mode 下的处理流程 ( 常用来进行系统调用 )。
- `sret` 用于 S 态 trap 返回, 通过 `sepc` 来设置 `pc` 的值， 返回到之前程序继续运行。

以上指令的详细介绍请同学们参考 [RISC-V Privileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf)

#### 上下文处理
由于在处理 trap 时，有可能会改变系统的状态。所以在真正处理 trap 之前，我们有必要对系统的当前状态进行保存，在处理完成之后，我们再将系统恢复至原先的状态，就可以确保之前的程序继续正常运行。
这里的系统状态通常是指寄存器，这些寄存器也叫做CPU的上下文 ( `Context` ).

#### trap 处理程序
trap 处理程序根据 `scause` 的值， 进入不同的处理逻辑，在本次试验中我们需要关心的只有 `Superviosr Timer Interrupt` 。

#### 时钟中断
时钟中断需要 CPU 硬件的支持。CPU 以“时钟周期”为工作的基本时间单位，对逻辑门的时序电路进行同步。而时钟中断实际上就是“每隔若干个时钟周期执行一次的程序”。下面介绍与时钟中断相关的寄存器以及如何产生时钟中断。

- `mtime` 与 `mtimecmp` ( Machine Timer Register )。 `mtime` 是一个实时计时器， 由硬件以恒定的频率自增。`mtimecmp` 中保存着下一次时钟中断发生的时间点，当 `mtime` 的值大于或等于 `mtimecmp` 的值，系统就会触发一次时钟中断。因此我们只需要更新 `mtimecmp` 中的值，就可以设置下一次时钟中断的触发点。 `OpenSBI` 已经为我们提供了更新 `mtimecmp` 的接口 `sbi_set_timer` ( 见 `lab1` 4.4节 )。
- `mcounteren` ( Counter-Enable Registers )。由于 `mtime` 是属于 M 态的寄存器，我们在 S 态无法直接对其读写， 幸运的是 OpenSBI 在 M 态已经通过设置 `mcounteren` 寄存器的 `TM` 比特位， 让我们可以在 S 态中可以通过 `time` 这个**只读**寄存器读取到 `mtime`的当前值，相关汇编指令是 `rdtime`。

以上寄存器的详细介绍请同学们参考 [RISC-V Privileged Spec](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf)
## 实验步骤

### 准备工程

从 [repo](https://github.com/ZJU-SEC/os23fall-stu) 同步实验代码框架。为了减少大家的工作量，在这里我们提供了简化版的 `printk` 来输出格式化字符串。


```
├── arch
│   └── riscv
│       ├── include
│       │   ├── defs.h
│       │   └── sbi.h
│       ├── kernel
│       │   ├── head.S
│       │   ├── Makefile
│       │   ├── sbi.c
│       │   └── vmlinux.lds
│       └── Makefile
├── include
│   ├── printk.h
|   ├── stddef.h
│   └── types.h
├── init
│   ├── main.c
│   ├── Makefile
│   └── test.c
├── lib
│   ├── Makefile
│   └── printk.c
└── Makefile
```

完成 **RV64 内核引导**，需要完善以下文件：

- arch/riscv/kernel/head.S
- lib/Makefile
- arch/riscv/kernel/sbi.c
- arch/riscv/include/defs.h

完成 **RV64 时钟中断处理**，需要完善 / 添加以下文件：

- arch/riscv/kernel/head.S
- arch/riscv/kernel/entry.S
- arch/riscv/kernel/trap.c
- arch/riscv/kernel/clock.c

### RV64 内核引导
#### 编写head.S

学习riscv的汇编。

完成 arch/riscv/kernel/head.S 。我们首先为即将运行的第一个 C 函数设置程序栈（栈的大小可以设置为4KB），并将该栈放置在`.bss.stack` 段。接下来我们只需要通过跳转指令，跳转至 main.c 中的 `start_kernel` 函数即可。


#### 完善 Makefile 脚本

阅读文档中关于 [Makefile](#35-makefile) 的章节，以及工程文件中的 Makefile 文件，根据注释学会 Makefile 的使用规则后，补充 `lib/Makefile`，使工程得以编译。  

完成此步后在工程根文件夹执行 make，可以看到工程成功编译出 vmlinux。

#### 补充 `sbi.c`

OpenSBI 在 M 态，为 S 态提供了多种接口，比如字符串输入输出。因此我们需要实现调用 OpenSBI 接口的功能。给出函数定义如下：
```c
struct sbiret {
	long error;
	long value;
};

struct sbiret sbi_ecall(int ext, int fid, 
                    uint64 arg0, uint64 arg1, uint64 arg2,
                    uint64 arg3, uint64 arg4, uint64 arg5);
```

sbi_ecall 函数中，需要完成以下内容：

1. 将 ext (Extension ID) 放入寄存器 a7 中，fid (Function ID) 放入寄存器 a6 中，将 arg0 ~ arg5 放入寄存器 a0 ~ a5 中。
2. 使用 `ecall` 指令。`ecall` 之后系统会进入 M 模式，之后 OpenSBI 会完成相关操作。
3. OpenSBI 的返回结果会存放在寄存器 a0 ， a1 中，其中 a0 为 error code， a1 为返回值， 我们用 sbiret 来接受这两个返回值。

同学们可以参照内联汇编的示例一完成该函数的编写。
编写成功后，调用 `sbi_ecall(0x1, 0x0, 0x30, 0, 0, 0, 0, 0)` 将会输出字符'0'。其中`0x1`代表 `sbi_console_putchar` 的 ExtensionID，`0x0`代表FunctionID, 0x30代表'0'的ascii值，其余参数填0。

请在 `arch/riscv/kernel/sbi.c` 中补充 `sbi_ecall()`。

下面列出了一些在后续的实验中可能需要使用的功能。

|Function Name | Function ID | Extension ID |
|---|---|---|
|sbi_set_timer （设置时钟相关寄存器） |0|0x00| 
|sbi_console_putchar （打印字符）|0|0x01| 
|sbi_console_getchar （接收字符）|0|0x02| 
|sbi_shutdown （关机）|0|0x08| 下面是实验指导中一些有用的信息：


#### 修改 defs

内联汇编的相关知识见[内联汇编](#36)。 

学习了解了以上知识后，补充 `arch/riscv/include/defs.h` 中的代码完成：

补充完 `read_csr` 这个宏定义。这里有相关[示例](#_2)。

如果完成到此处，你就已经可以在 qemu 运行 `make` 得到的内核，从而至少完成思考题 1～4 了。

### RV64 时钟中断处理

* 准备工作，先修改 `vmlinux.lds` 以及 `head.S`
    ```
    <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 原先的 vmlinux.lds
    ...

    .text : ALIGN(0x1000){
        _stext = .;

        *(.text.entry)
        *(.text .text.*)
        
        _etext = .;
    }

    ...

    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 修改之后的 vmlinux.lds
    ...

    .text : ALIGN(0x1000){
        _stext = .;

        *(.text.init)      <- 加入了 .text.init
        *(.text.entry)     <- 之后我们实现 中断处理逻辑 会放置在 .text.entry
        *(.text .text.*)
        
        _etext = .;
    }

    ...
    ```

    ```
    <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 原先的 head.S
    extern start_kernel

        .section .text.entry        <- 之前的 _start 放置在 .text.entry section       
        .globl _start
    _start:
        ...

        .section .bss.stack
        .globl boot_stack
    boot_stack:
        .space 4096

        .globl boot_stack_top
    boot_stack_top:

    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 修改之后的 head.S
    extern start_kernel

        .section .text.init         <- 将 _start 放入.text.init section 
        .globl _start
    _start:
        ...

        .section .bss.stack
        .globl boot_stack
    boot_stack:
        .space 4096

        .globl boot_stack_top
    boot_stack_top:
    ```

#### 开启 trap 处理
在运行 `start_kernel` 之前，我们要对上面提到的 CSR 进行初始化，初始化包括以下几个步骤：

1. 设置 `stvec`， 将 `_traps` ( `_trap` 在 4.3 中实现 ) 所表示的地址写入 `stvec`，这里我们采用 `Direct 模式`, 而 `_traps` 则是 trap 处理入口函数的基地址。
2. 开启时钟中断，将 `sie[STIE]` 置 1。
3. 设置第一次时钟中断，参考 `clock_set_next_event()` ( `clock_set_next_event()` 在 4.3.4 中介绍 ) 中的逻辑用汇编实现。
4. 开启 S 态下的中断响应， 将 `sstatus[SIE]` 置 1。

按照下方模版修改 `arch/riscv/kernel/head.S`， 并补全 `_start` 中的逻辑。

```asm
.extern start_kernel

    .section .text.init
    .globl _start
_start:
    # YOUR CODE HERE

    # ------------------
        
        # set stvec = _traps
    
    # ------------------
    
        # set sie[STIE] = 1
    
    # ------------------
    
        # set first time interrupt
    
    # ------------------
    
        # set sstatus[SIE] = 1

    # ------------------
    
    # ------------------------
    # - your lab1 other code -
    # ------------------------

    ...
```
> Debug 提示：可以先不实现 stvec 和 first time interrupt，先关注 sie 和 sstatus 是否设置正确。

#### 实现上下文切换
我们要使用汇编实现上下文切换机制， 包含以下几个步骤：

1. 在 `arch/riscv/kernel/` 目录下添加 `entry.S` 文件。
2. 保存 CPU 的寄存器（上下文）到内存中（栈上）。
3. 将 `scause` 和 `sepc` 中的值传入 trap 处理函数 `trap_handler` ( `trap_handler` 在 4.4 中介绍 ) ，我们将会在 `trap_handler` 中实现对 trap 的处理。
4. 在完成对 trap 的处理之后， 我们从内存中（栈上）恢复CPU的寄存器（上下文）。
5. 从 trap 中返回。

按照下方模版修改 `arch/riscv/kernel/entry.S`， 并补全 `_traps` 中的逻辑。
```asm
    .section .text.entry
    .align 2
    .globl _traps 
_traps:
    # YOUR CODE HERE
    # -----------

        # 1. save 32 registers and sepc to stack

    # -----------

        # 2. call trap_handler

    # -----------

        # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack

    # -----------

        # 4. return from trap

    # -----------
```
> Debug 提示： 可以先不实现 call trap_handler， 先实现上下文切换逻辑。通过 gdb 跟踪各个寄存器，确保上下文的 save 与 restore 正确实现并正确返回。

#### 实现 trap 处理函数
1. 在 `arch/riscv/kernel/` 目录下添加 `trap.c` 文件。
2. 在 `trap.c` 中实现 trap 处理函数 `trap_handler()`, 其接收的两个参数分别是 `scause` 和 `sepc` 两个寄存器中的值。
```c
// trap.c 

void trap_handler(unsigned long scause, unsigned long sepc) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略
    
    // YOUR CODE HERE
}
```

#### 实现时钟中断相关函数
1. 在 `arch/riscv/kernel/` 目录下添加 `clock.c` 文件。
2. 在 `clock.c` 中实现 get_cycles ( ) : 使用 `rdtime` 汇编指令获得当前 `time` 寄存器中的值。
3. 在 `clock.c` 中实现 clock_set_next_event ( ) : 调用 `sbi_ecall`，设置下一个时钟中断事件。
```c
// clock.c

// QEMU中时钟的频率是10MHz, 也就是1秒钟相当于10000000个时钟周期。
unsigned long TIMECLOCK = 10000000;

unsigned long get_cycles() {
    // 编写内联汇编，使用 rdtime 获取 time 寄存器中 (也就是mtime 寄存器 )的值并返回
    // YOUR CODE HERE

}

void clock_set_next_event() {
    // 下一次 时钟中断 的时间点
    unsigned long next = get_cycles() + TIMECLOCK;

    // 使用 sbi_ecall 来完成对下一次时钟中断的设置
    // YOUR CODE HERE
} 

```

#### 编译及测试
由于加入了一些新的 .c 文件，可能需要修改一些Makefile文件，请同学自己尝试修改，使项目可以编译并运行。

下面是一个正确实现的输出样例。（ 仅供参考 ）
```
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
kernel is running!
[S] Supervisor Mode Timer Interrupt
```

## 其他架构的交叉编译——以 Aarch64 为例

### 交叉编译工具链的安装
那么如何安装不同架构的交叉编译工具链呢？最简单的方法是用 Ubuntu 自带的软件包管理器 `apt`，先找到有什么交叉编译工具可以装
```
# 搜索包含 aarch64 的软件包，一般是交叉编译工具
apt-cache search aarch64
...
# 搜索结果中如果有 gcc-xxx-linux-gnu，一般需求下装它就行了（具体情况具体分析哈）
sudo apt install gcc-aarch64-linux-gnu
```
现在我们有 aarch64 的交叉编译工具链了，开始编译吧！

### 怎么获得编译过程的中间产物
**注意：这里说的“编译过程”包括预处理、编译、汇编、链接**

对于 Linux kernel，编译命令和选项在不同架构之间都大同小异，一般遵循以下形式（类比 lab0 做过的 riscv64 即可）
```
make ARCH=xxx CROSS_COMPILE=some-certain-arch- <options> <files>
```

比如，想获得 kernel 中 `xxx.c` 的预处理产物（回忆一下预处理做了什么）`xxx.i`，我们可以
```
# 先 config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig

# 然后指定要生成的文件
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- path/to/file/xxx.i
```

课件里也给出了 `make` 工具。

## 思考题

1. 请总结一下 RISC-V 的 calling convention，并解释 Caller / Callee Saved Register 有什么区别？
2. 编译之后，通过 System.map 查看 vmlinux.lds 中自定义符号的值（截图）。
3. 用 `csr_read` 宏读取 `sstatus` 寄存器的值，对照 RISC-V 手册解释其含义（截图）。
4. 用 `csr_write` 宏向 `sscratch` 寄存器写入数据，并验证是否写入成功（截图）。

5. Detail your steps about how to get `arch/arm64/kernel/sys.i`
6. Find system call table of Linux v6.0 for `ARM32`, `RISC-V(32 bit)`, `RISC-V(64 bit)`, `x86(32 bit)`, `x86_64`
List source code file, the whole system call table with macro expanded, screenshot every step.
7. Explain what is ELF file? Try readelf and objdump command on an ELF file, give screenshot of the output.
Run an ELF file and cat `/proc/PID/maps` to give its memory layout.
8. 在我们使用make run时， OpenSBI 会产生如下输出:

```plaintext
    OpenSBI v0.9
     ____                    _____ ____ _____
    / __ \                  / ____|  _ \_   _|
   | |  | |_ __   ___ _ __ | (___ | |_) || |
   | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
   | |__| | |_) |  __/ | | |____) | |_) || |_
    \____/| .__/ \___|_| |_|_____/|____/_____|
          | |
          |_|

    ......

    Boot HART MIDELEG         : 0x0000000000000222
    Boot HART MEDELEG         : 0x000000000000b109

    ......
```

通过查看 `RISC-V Privileged Spec` 中的 `medeleg` 和 `mideleg` ，解释上面 `MIDELEG` 值的含义。

5, 6, 7, 8 need to have screenshots.

## 作业提交

实验报告需要包含对所有思考题的回答，有截图要求的要截图。

同学需要提交实验报告以及整个工程代码。在提交前请使用 `make clean` 清除所有构建产物。