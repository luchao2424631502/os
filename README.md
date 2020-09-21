# OS

1. bochs配置文件

   ```
   megs:32 
   #romimage: file=$BXSHARE/BIOS-bochs-latest
   #vgaromimage: file=$BXSHARE/VGABIOS-lgpl-latest
   romimage: 	file=/usr/share/bochs/BIOS-bochs-latest
   vgaromimage:	file=/usr/share/vgabios/vgabios.bin
   
   floppya: 1_44=a.img, status=inserted
   boot: a
   
   #新增硬盘驱动
   ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0,irq=14
   ata0-master: type=disk, path="80m.img", mode=flat, cylinders=162, heads=16,spt=63
   
   log: bochsout.txt
   
   mouse: enabled=0
   
   keyboard_mapping: enable=1,map=/usr/share/bochs/keymaps/x11-pc-us.map
   ```

   软盘引导+磁盘上的文件系统,bximage创建1.44MB软盘和80Mb硬盘,

2. make image制作软盘, (根据自己路径修改一下makefile中的dd写入)

3. bochs 在 当前目录(.bochsrc配置文件目录)运行

-----



我的编译(链)环境:

```makefile
X86_64 Linux 4.15.0
GNU Make 4.2.1
Nasm 2.13.03
gcc Debian 7.3.0
GNU Ld 2.30
```

#### 编译问题:

1. hello.asm在x64上链接参数修改(因为目标32bit):

   > nasm默认编译出来的是32位的, `-m elf_i386`指定链接32bit的目标文件

   `ld -m elf_i386 -s hello.o -o hello`

2. x64环境下gcc添加-m32指定编译出32位

   `gcc -m32` 

3. 编译start.c `-fno-builtin`

###### chapter5:

1. `chapter5/h/`  中扩充内核完成8259A设置和IDT以及disp_等函数时:警告错误:

   ```Makefile
   kernel/protect.c:122:2: warning: implicit declaration of function ‘disp_int’; did you mean ‘disp_str’? [-Wimplicit-function-declaration]
     disp_int(eflags);
     ^~~~~~~~
   ```

   在/include/proto.h添加一下`void disp_int(int)`声明(应该其中一个头文件中添加都可)

   

   **注意除了disp_str外的disp_color_str()也要保护ebx,否则虽然不是出现乱码,但是eflags和cs,eip这里显示别的中断错误**

2. 





-----

#### 我对代码提出的一点质疑？

1. 在loader.asm中统计内存代码感觉有点问题(**求内存最大上限**)

   应该统计类型1的内存长度

   在我机器上内存最大上限是`1FF0000h`


#### 作者代码bug:

1. [disp_str()字符串函数第二次调用乱码原因](https://blog.csdn.net/qq_43580151/article/details/107349956)

#### 总结:

1. 在内核完善中断时,因为门描述符填充处理例程的物理地址(平坦模式),在kernel/kernel.asm中写基本中断能够确定中断函数所在位置(kernel开头处),填IDT中例程偏移地址和disp交给c来做



----------------------

#### OS的第一个进程:

​	由kernel进程提前填好进程表信息以及TSS和进程的LDT,通过iret实现r0->r1的转移,并且在打开栈中eflags的IF标志,r0->r1也开启了中断(kernel中设置8259A打开了时钟中断)

​	中断重入: 如果在中断过程中发送EOI后,中断过程执行时间长,导致中断重入问题:(目前采取 全局变量暴力维护只允许一个中断存在,另一个进入后判断存在个数后退出)

​	**位于中断例程时,根据TSS的esp0此时位于进程表栈,如果通过中断来调度进程那么首先要切换到内核栈中**

​	**多进程**下中断退出前设置好TSS的esp0,给下次r1->r0使用

#### 添加进程,通过中断进行进程切换()

	1. 通过`task_table`填写进程表信息(自动化添加进程,并且选择子权限),根据NR_TASKS提前在int_prot中初始化LDT描述符
 	2. 目前的进程切换很简单:中断一次切换到下一个进程:

3. 添加进程暂时通过修改

   > 1. 在global.c中task_table添加一项
   > 2. proc.h修改nr_tasks,定义堆栈大小,修改stack_size_total
   > 3. proto.h添加函数声明



修改中断,定义成宏,让所有中断解决重入和栈切换的问题,把设置中路处理例程封装起来,引入`irq_table`

---------------------------

#### 添加第一个系统调用 (类似宏内核调用内核函数的样子)

1. **通过软中断0x90进入内核调用入口sys_call,**

   **根据user调用的API(通过eax选择不同的内核函数)将内核函数通过sys_call_table提供),**

   **作为开发者,我们既要写给user用的函数,也要写在此函数在内核的实现**

------

#### 设置8253定时器(PIT)

1. 设置中断每10ms发生一次,
2. 编写10ms精度的延迟函数milli_delay(int milli_sec)

#### 进程调度schedule()

​	1. 基于优先级调度,当优先级相同时,轮流调度,(目前控制进程一个中断执行一次)



---------------------------------------------

#### IO-键盘

1. 添加IRQ1键盘中断处理,

2. 因为每次只能读取1个byte,开一个缓冲区把扫描码存入,新增一个tty(修改proc.c等)任务来处理键盘输出和屏幕输出

   > 扫描解析码:
   >
   > tty.c完成
   >
   > 将字符交给in_process.c来处理,
   >
   > 1. 对于可打印字符实现光标重定位,
   >
   > 2. 非可打印字符检测是否是某种操作,实现:shift+up 向上滚屏,shift+down向下滚屏
   >
   
#### IO-TTY 

   >1. 3个tty被一个tty_task轮询,根据全局nr_current_console,确定是否从键盘缓冲区中读取
   >
   >2. 3个console,并且通过Alt + F1 2 3来切换,Alt+F(4-12)暂时无反应,(如果和当前os的快捷键冲突bochs会死机)

#### 区分用户进程r3和r1服务

>  禁止用户进程IO权限

**为进程指定tty**

实现简单printf()

新增系统调用(传参)

**过程:**

### 给磁盘镜像分区时如果在fdisk中如果是按照扇区计算的话, u 改为按柱面计算

+-------------------------------------------+-------------------+
|                                           |   const.h         |
|   NR_SYS_CALL加1                           |                   |
+---------------------------------------------------------------+
|   sys_call_table[]增加一个成员                  |   global.c        |
|                                           |                   |
+---------------------------------------------------------------+
|   sys_系统调用 函数体                            |   根据函数作用放在对应文件    |
|                                           |                   |
+---------------------------------------------------------------+
|   sys_系统调用 声明                             |   proto.h         |
|                                           |                   |
+---------------------------------------------------------------+
|   系统调用 函数声明                               |   proto.h         |
|                                           |                   |
+---------------------------------------------------------------+
|   _NR_系统调用 定义   导出(系统调用)符号                |   syscall.asm     |
|                                           |                   |
+---------------------------------------------------------------+
|   必要时,(例如修改sys_call参数传递,)                 |   kernel.asm      |

>+-------------------------------------------+-------------------+

#### 微内核IPC实现

1. 新增系统调用sendrec





#### 磁盘驱动读取的信息 和 添加文件系统服务程序

bximage制作80MB硬盘,并且修改bochs配置文件,添加硬盘

```shell
NrDrives:1.
Task FS begins.
HD SN: BXHD00011
HD Model: Generic 1234
LBA supported: Yes
LBA48 supported: Yes
HD size: 83MB
```







------

到目前的思路:

|----------引导扇区中加载loader

|----------控制权给loader,加载kernel

|----------控制权跳到kernel,将loader中的GDT移动到kernel中

|----------init_prot()

​					|----------初始化8259A

​					|----------初始化IDT

​					|----------初始化唯一TSS以及其描述符, 和(开始时第一个进程)多进程的LDT描述符

|----------kernel_main()

​					|----------根据task_table[]初始化进程表,(regs,寄存器选择子以及权限等)

​					|----------开启时钟中断,指定时钟中断处理程序

​					|----------restart():正式从kernel进入到第一个进程(本质上时r0 -> r1的转换)

|----------多进程开始运转(目前3个,并且通过时钟中断进行调度,(简单的顺序调度),并且中断重入已解决)

------

# IPC->(文件系统,驱动,硬盘操作),->tty纳入fs，（mm暂时没有实现

# 实现mm,从fork开始
 1. 整个进程在内存中的stack,text,data等全部都在proc.h的进程struct中的LDT[INDEX_LDT_RW(选择子)]定义了,
 涉及的数据结构

 GDT中LDT desc for proc i -> proc_table[i]中的Ldts[]数组开始 -> 指向进程在内存中全部分段
	|		proc_table[i]中的ldt_sel选择子
	|			            |
	|			            |
	|---指回到gdt中对应进程的LDT描述符--|
	
* 又重新修改了boot.asm和loader.asm()		
修改kernel_main初始化函数,需要添加2个klib函数,(从Loader中保存内存分配信息)
* 添加Init进程(作为所有进程的父进程 (并且init进程的内存(LDTs中描述符指向内存大小)限制在内核范围中
* 完善上面涉及的数据结构 (GDT中描述符对应每个进程的LDTs表内容,以及proc中ldts
> fork作为给用户使用的,在stdio.h中声明
* fork的子进程复制父进程proc_table[]内容,除了内存需要重新申请(GDT中的LDT选择子重新指向ldts
* 发消息通知fs task处理fork后的fd复制(增加inode节点引用)
---
#### 内存分配 alloc_mem()
策略:在指定内存base地址上按照顺序(PID)和固定大小分配内存

1. 实现exit(),和 wait(),进程状态分析看源码注释,cleanup():彻底清除proc,INIT进程回收僵尸进程while()->wait(),记得在mm/main.c添加内核EXIT,WAIT消息处理

#### 实现exec()
> 从硬盘上读取另一个可执行文件,替换掉fork出来的子进程,
1. 写一个程序用来测试exec()
> 目前支持的API:
> 
    	* 系统调用

    		-lib/syscall.asm : sendrec,printx
    
    * 字符串操作
    
      -lib/string.asm : memcpy,memset,strcpy,strlen
    
    * FS接口
    
      -lib/open.c
    
      -lib/read.c
    
      -lib/write.c
    
      -lib/close.c
    
      -lib/unlink.c
    
    * MM接口
    
      -lib/fork.c
    
      -lib/exit.c
    
      -lib/wait.c
    
    * SYS接口
    
      -lib/getpid.c
    
    * 其他
    
      -lib/misc.c
    
      -lib/printf.c

将上述函数打包成静态库orangescrt.a,

make 生成.o -> `ar rcs lib/orangescrt.a lib/syscall.o lib/printf.o lib/vsprintf.o lib/string.o lib/misc.o lib/open.o lib/read.o lib/write.o lib/close.o lib/unlink.o lib/getpid.o lib/fork.o lib/exit.o lib/wait.o `

编译echo.c:(注意32bit,制定了代码入口是0x1000)

`gcc -I ../include/ -c -fno-builtin -Wall -m32 -o echo.o echo.c`

Ld(`ld -m elf_i386 -Ttext 0x1000 -o echo echo.o ../lib/orangescrt.a `)发生问题:

```makefile
ld: 警告: 无法找到项目符号 _start; 缺省为 0000000000001000
../lib/orangescrt.a(write.o)：在函数‘write’中：
write.c:(.text+0x38)：对‘send_recv’未定义的引用
```

解决`send_recv`未定义的引用:将 kernel/proc.c 中的函数移动到 lib/misc.c 中 

> 顺便整理文件将:
>
> lib/klib.c 移动-> kernel/klib.c 
>
> lib/kliba.asm -> kernel/kliba.asm
>
> 注意修改Makefile

### 解决`_start`符号,(解决的是C的CRT库问题,**对于crt来说,main只不过是一个Undefined符号**):添加command/start.asm (1.调用main 2.main参数, 3.crt调用exit()结束一个c程序 )

成功编译echo.c应用:

```makefile
gcc -I ../include/ -m32 -c -fno-builtin -Wall -o echo.o echo.c  
nasm -I ../include/ -f elf -o start.o start.asm
ld -m elf_i386 -Ttext 0x1000 -o echo echo.o start.o ../lib/orangescrt.a 
```

#### 将应用安装到OS

1. 修改fd/main mkfs(),将image中的cmd.tar存入FS中

2. 暂时用test.txt代替pwd.

   1. 将inst.tar写入磁盘镜像
      打包成inst.tar: tar -cvf inst.tar kernel.bin echo pwd
   
   2. load.inc中ROOT_BASE定义(根设备开始扇区号):`egrep -e '^ROOT_BASE' boot/include/load.inc | sed -e 's/.*0x//g'`
   
3. 从config.h找到INSTALL_START_SECT定义:`egrep -e '#define[[:space:]]*INSTALL_START_SECT' ./include/sys/config.h  | sed -e 's/.*0x//g'`
   
   4. cmd.tar字节偏移:
   
      `echo "obase=10;ibase=16;(4EFF+8000)*200" | bc`
   
   5. 查看inst.tar大小
   
      `ls -l inst.tar | awk -F " " '{print $5}'`
   
   > dd写入:
   >
   > `dd if=inst.tar of=80m.img seek=27131392 bs=1 count=81920 conv=notrunc`
   
   我的80m.img是借用源码目录里面的(并没有自己区手动分区,因为fdisk好像会默认从1024个扇区开始,因为efi分区的原因)
   
   > 注意inst.tar不要包含路径进去,并且dd的count是自己添加进去的文件的字节大小和
   
3. 成功看到解压后的结果

#### 实现exec,(以及2个execv,execl)

添加应用接口lib/exec.c

mm/main.c处理EXEC消息,调用mm/exec.c(do_exec()函数) execl参数赋值有点问题?

#### 简单shell解析

#### mkfs只执行一次

------

> 2020/9/21结束







