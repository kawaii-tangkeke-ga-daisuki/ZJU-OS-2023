# Lab 0: GDB & QEMU 调试 64 位 RISC-V LINUX

## 1 实验目的
- 使用交叉编译工具, 完成Linux内核代码编译
- 使用QEMU运行内核
- 熟悉GDB和QEMU联合调试

## 2 实验环境

- [Ubuntu 22.04.3 LTS](https://ubuntu.com/download/desktop)
- [Ubuntu 22.04.2 LTS Windows Subsystem for Linux 2](https://www.microsoft.com/store/productId/9PN20MSR04DW)
- Mac with Apple Silicon
- 其他可行的平台，但我们不提供技术支持

## 3 实验基础知识介绍

### 3.1 Linux 使用基础

在 Linux 环境下，人们通常使用命令行接口来完成与计算机的交互。终端（Terminal）是用于处理该过程的一个应用程序，通过终端你可以运行各种程序以及在自己的计算机上处理文件。在类 Unix 的操作系统上，终端可以为你完成一切你所需要的操作。下面我们仅对实验中涉及的一些概念进行介绍，你可以通过下面的链接来对命令行的使用进行学习：

1. [The Missing Semester of Your CS Education](https://missing-semester-cn.github.io/2020/shell-tools)[&gt;&gt;Video&lt;&lt;](https://www.bilibili.com/video/BV1x7411H7wa?p=2)
2. [GNU/Linux Command-Line Tools Summary](https://tldp.org/LDP/GNU-Linux-Tools-Summary/html/index.html)
3. [Basics of UNIX](https://github.com/berkeley-scf/tutorial-unix-basics)

### 3.2 QEMU 使用基础

#### 什么是QEMU

QEMU 是一个功能强大的模拟器，可以在 x86 平台上执行不同架构下的程序。我们实验中采用 QEMU 来完成 RISC-V 架构的程序的模拟。

#### 如何使用 QEMU（常见参数介绍）

以以下命令为例，我们简单介绍 QEMU 的参数所代表的含义

```bash
$ qemu-system-riscv64 \
    -nographic \
    -machine virt \
    -kernel path/to/linux/arch/riscv/boot/Image \
    -device virtio-blk-device,drive=hd0 \
    -append "root=/dev/vda ro console=ttyS0" \
    -bios default \
    -drive file=rootfs.img,format=raw,id=hd0 \
    -S -s
```

- `-nographic`: 不使用图形窗口，使用命令行
- `-machine`: 指定要 emulate 的机器，可以通过命令 `qemu-system-riscv64 -machine help` 查看可选择的机器选项
- `-kernel`: 指定内核 image
- `-append cmdline`: 使用cmdline作为内核的命令行
- `-device`: 指定要模拟的设备，可以通过命令 `qemu-system-riscv64 -device help` 查看可选择的设备，通过命令 `qemu-system-riscv64 -device <具体的设备>,help` 查看某个设备的命令选项
- `-drive, file=<file_name>`: 使用 `file_name` 作为文件系统
- `-S`: 启动时暂停CPU执行
- `-s`: `-gdb tcp::1234` 的简写
- `-bios default`: 使用默认的 OpenSBI firmware 作为 bootloader

更多参数信息可以参考[这里](https://www.qemu.org/docs/master/system/index.html)

### 3.3 GDB 使用基础

#### 什么是 GDB

GNU 调试器（英语：GNU Debugger，缩写：gdb）是一个由 GNU 开源组织发布的、UNIX/LINUX 操作系统下的、基于命令行的、功能强大的程序调试工具。借助调试器，我们能够查看另一个程序在执行时实际在做什么（比如访问哪些内存、寄存器），在其他程序崩溃的时候可以比较快速地了解导致程序崩溃的原因。
被调试的程序可以和 GDB 运行在同一台机器上，并由 GDB 控制（本地调试 native debug）。也可以只和 `gdb-server` 运行在同一台机器上，由连接着 `gdb-server` 的 GDB 进行控制（远程调试 remote debug）。

GDB 的功能十分强大，我们经常在调试中用到的有:

- 启动程序，并指定可能影响其行为的所有内容
- 使程序在指定条件下停止
- 检查程序停止时发生了什么
- 更改程序中的内容，以便纠正一个bug的影响

#### GDB 基本命令介绍

- `(gdb) layout asm`: 显示汇编代码
- `(gdb) start`: 单步执行，运行程序，停在第一执行语句
- `(gdb) continue`: 从断点后继续执行，简写 `c`
- `(gdb) next`: 单步调试（逐过程，函数直接执行），简写 `n`
- `(gdb) step instruction`: 执行单条指令，简写 `si`
- `(gdb) run`: 重新开始运行文件（run-text：加载文本文件，run-bin：加载二进制文件），简写 `r`
- `(gdb) backtrace`：查看函数的调用的栈帧和层级关系，简写 `bt`
- `(gdb) break` 设置断点，简写 `b`
    - 断在 `foo` 函数：`b foo`
    - 断在某地址: `b * 0x80200000`
- `(gdb) finish`: 结束当前函数，返回到函数调用点
- `(gdb) frame`: 切换函数的栈帧，简写 `f`
- `(gdb) print`: 打印值及地址，简写 `p`
- `(gdb) info`: 查看函数内部局部变量的数值，简写 `i`
    - 查看寄存器 ra 的值: `i r ra`
- `(gdb) display`: 追踪查看具体变量值
- `(gdb) x/4x <addr>`: 以 16 进制打印 `<addr>` 处开始的 16 Bytes 内容

更多命令可以参考[100个gdb小技巧](https://wizardforcel.gitbooks.io/100-gdb-tips/content/)


### 3.4 Linux 内核编译基础

#### 交叉编译

交叉编译指的是在一个平台上编译可以在另一个架构运行的程序。例如在 x86 机器上编译可以在 RISC-V 架构运行的程序，交叉编译需要交叉编译工具链的支持，在我们的实验中所用的交叉编译工具链就是 `riscv-gnu-toolchain`。

#### 内核配置

内核配置是用于配置是否启用内核的各项特性，内核会提供一个名为 `defconfig` (即default configuration) 的默认配置，该配置文件位于各个架构目录的 `configs` 文件夹下，例如对于RISC-V而言，其默认配置文件为 `arch/riscv/configs/defconfig`。使用 `make ARCH=riscv defconfig` 命令可以在内核根目录下生成一个名为 `.config` 的文件，包含了内核完整的配置，内核在编译时会根据 `.config` 进行编译。

配置之间存在相互的依赖关系，直接修改defconfig文件或者 `.config` 有时候并不能达到想要的效果，或是给进一步内核配置带来同步问题。因此如果需要修改配置一般采用 `make ARCH=riscv menuconfig` 的方式对内核进行配置。

#### 编译工具

`make` 是用于程序构建的重要工具，它的行为由当前目录或 `make -C` 指定目录下的 `Makefile` 来决定。更多有关 `Makefile` 的内容可以参考 [Learn Makefiles With the tastiest examples](https://makefiletutorial.com/)。下面用本次实验中可能用到的用于编译 Linux 内核的编译命令作为示例：

```bash
$ make help             # 查看make命令的各种参数解释

$ make <target-name>    # 编译名为 <target-name> 的目标文件或目标任务 
$ make defconfig        # 使用当前平台的默认配置，在x86机器上会使用x86的默认配置
$ make clean            # 清除所有编译好的 object 文件
$ make mrproper         # 删除所有编译产物和配置文件

$ make -j<thread-count> # 使用 <thread-count> 个物理线程来进行多线程编译 
$ make -j4              # 编译当前平台的内核，-j4 为使用 4 线程进行多线程编译
$ make -j$(nproc)       # 编译当前平台的内核，-j$(nproc) 为以全部机器硬件线程数进行多线程编译

$ make <var-name>=<var-value>                       # 在编译过程中将 <var-name> 变量的值手动设置为 <val-value>
$ make ARCH=riscv defconfig                         # 使用 RISC-V 平台的默认配置
$ make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu-  # 编译 RISC-V 平台内核
```

我们可以手动为 `make` 指定变量的值，本次实验中用到的如下：

- `ARCH` 指定架构，可选的值包括 arch 目录下的文件夹名，如 x86、arm、arm64 等，不同于 arm 和 arm64，32 位和 64 位的RISC-V共用 `arch/riscv` 目录，通过使用不同的 config 可以编译 32 位或 64 位的内核。
- `CROSS_COMPILE` 指定使用的交叉编译工具链，例如指定 `CROSS_COMPILE=riscv64-linux-gnu-`，则编译时会采用 `riscv64-linux-gnu-gcc` 作为编译器，编译在 RISC-V 64 位平台上运行的 Linux 内核。


## 4 实验步骤

**在执行每一条命令前，请你对将要进行的操作进行思考，给出的命令不需要全部执行，并且不是所有的命令都可以无条件执行，请不要直接复制粘贴命令去执行。**

### 4.1 搭建实验环境环境

> 如果你在使用 Mac with Apple Silicon, 请直接使用 Docker Desktop 进行课程实验。
> Docker Desktop 的安装可以参考 [Docker Desktop for Apple silicon](https://docs.docker.com/desktop/mac/apple-silicon/)。
> 之后使用 `docker pull ubuntu:22.04 && docker run -it --name <some-name> ubuntu:22.04 bash` 来启动一个运行在虚拟机上的 Ubuntu for ARM 容器，并将这个 Ubuntu 作为实验环境。

首先安装编译内核所需要的交叉编译工具链和用于构建程序的软件包

```bash
$ sudo apt install  gcc-riscv64-linux-gnu
$ sudo apt install  autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev \
                    gawk build-essential bison flex texinfo gperf libtool patchutils bc \
                    zlib1g-dev libexpat-dev git
```

接着是用于启动 riscv64 平台上的内核的模拟器 `qemu`

```bash
$ sudo apt install qemu-system-misc
```

我们还需要用 `gdb` 来对在 `qemu` 上运行的 Linux 内核进行调试
```bash
$ sudo apt install gdb-multiarch
```

### 4.2 获取 Linux 源码和已经编译好的文件系统

从 [https://www.kernel.org](https://www.kernel.org) 下载最新的 Linux 源码。
> 截至写作时，最新的 Linux 内核版本是 6.6-rc1.

并且使用 git 工具 clone [本仓库](https://gitee.com/zju_xiayingjie/os23fall-stu.git)。其中已经准备好了根文件系统的镜像。

> 根文件系统为 Linux Kernel 提供了基础的文件服务，在启动 Linux Kernel 时是必要的。

```bash
$ git clone https://gitee.com/zju_xiayingjie/os23fall-stu.git
$ cd os23fall-stu/src/lab0
$ ls
rootfs.img  # 已经构建完成的根文件系统的镜像
```

### 4.3 编译 linux 内核

```bash
$ cd path/to/linux
$ make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- defconfig    # 使用默认配置
$ make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -j$(nproc)   # 编译
```

> 使用多线程编译一般会耗费大量内存，如果 `-j` 选项导致内存耗尽 (out of memory)，请尝试调低线程数，比如 `-j4`, `-j8` 等。

### 4.4 使用QEMU运行内核

```bash
$ qemu-system-riscv64 -nographic -machine virt -kernel path/to/linux/arch/riscv/boot/Image \
    -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \
    -bios default -drive file=path/to/rootfs.img,format=raw,id=hd0
```
退出 QEMU 的方法为：使用 Ctrl+A，**松开**后再按下 X 键即可退出 QEMU。

### 4.5 使用 GDB 对内核进行调试

> 这一步需要开启两个 Terminal Session，一个 Terminal 使用 QEMU 启动 Linux，另一个 Terminal 使用 GDB 与 QEMU 远程通信（使用 tcp::1234 端口）进行调试。

```bash
# Terminal 1
$ qemu-system-riscv64 -nographic -machine virt -kernel path/to/linux/arch/riscv/boot/Image \
    -device virtio-blk-device,drive=hd0 -append "root=/dev/vda ro console=ttyS0" \
    -bios default -drive file=path/to/rootfs.img,format=raw,id=hd0 -S -s

# Terminal 2
$ gdb-multiarch path/to/linux/vmlinux
(gdb) target remote :1234   # 连接 qemu
(gdb) b start_kernel        # 设置断点
(gdb) continue              # 继续执行
(gdb) quit                  # 退出 gdb
```

## 5 实验任务与要求

- 请各位同学独立完成作业，任何抄袭行为都将使本次作业判为0分。
- 编译内核，使用 QEMU 启动后，远程连接 GDB 进行调试，并尝试使用 GDB 的各项命令（如 `backtrace`, `finish`, `frame`, `info`, `break`, `display`, `next`, `layout` 等）。
- 在学在浙大中提交 pdf 格式的实验报告，记录实验过程并截图（4.1-4.4），对每一步的命令以及结果进行必要的解释，记录遇到的问题和心得体会。

## 思考题

1. 使用 `riscv64-linux-gnu-gcc` 编译单个 `.c` 文件
2. 使用 `riscv64-linux-gnu-objdump` 反汇编 1 中得到的编译产物
3. 调试 Linux 时:
    1. 在 GDB 中查看汇编代码
    2. 在 0x80000000 处下断点
    3. 查看所有已下的断点
    4. 在 0x80200000 处下断点
    5. 清除 0x80000000 处的断点
    6. 继续运行直到触发 0x80200000 处的断点
    7. 单步调试一次
    8. 退出 QEMU
4. 使用 `make` 工具清除 Linux 的构建产物
5. `vmlinux` 和 `Image` 的关系和区别是什么？
