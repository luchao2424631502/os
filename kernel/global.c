#define GLOBAL_VARIABLES_HERE//定义一次全局变量

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "fs.h"
#include "tty.h"
#include "console.h"
#include "proc.h"
#include "global.h"
#include "proto.h"

/*进程表*/
struct proc proc_table[NR_TASKS + NR_PROCS];

/* 5个常驻服务进程 */
struct task    task_table[NR_TASKS] = {
                                {task_tty,STACK_SIZE_TTY,"TTY"  },
                                {task_sys,STACK_SIZE_SYS,"SYS"  },
                                {task_hd,STACK_SIZE_HD,"HD"     },
                                {task_fs,STACK_SIZE_FS,"FS"     },
                                {task_mm,STACK_SIZE_MM,"MM"     }};
                                
/* 4个OS引导结束后刚开始运行的进程 */
struct task    user_proc_table[NR_NATIVE_PROCS] = {
                                {Init,STACK_SIZE_INIT,"INIT"    },
                                {TestA,STACK_SIZE_TESTA,"TestA" },
                                {TestB,STACK_SIZE_TESTB,"TestB" },
                                {TestC,STACK_SIZE_TESTC,"TestC" }};
                        
char        task_stack[STACK_SIZE_TOTAL];
TTY         tty_table[NR_CONSOLES];
CONSOLE     console_table[NR_CONSOLES];

irq_handler irq_table[NR_IRQ];  //中断处理例程地址

/* 系统调用 */
system_call sys_call_table[NR_SYS_CALL] = {sys_printx,
                    sys_sendrec};


/* 设备号对应的驱动程序的PID */
struct dev_drv_map dd_map[] = {
    {INVALID_DRIVER},/* 设备0对应的驱动 (保留) */
    {INVALID_DRIVER},
    {INVALID_DRIVER},
    {TASK_HD},      /* 设备3对应的驱动 */
    {TASK_TTY},     /* 设备4对应的驱动 */
    {INVALID_DRIVER}/*  */
};

/* 6MB~7Mb fs的缓冲区 */
u8 *fsbuf = (u8*)0x600000;
const int FSBUF_SIZE = 0x100000;

/* 7MB~8MB buffer for MM*/
u8 *mmbuf = (u8*)0x700000;
const int MMBUF_SIZE = 0x100000;

/* 8MB~10MB: buffer for log(debug) */
char *logbuf = (char*)0x800000;
const int LOGBUF_SIZE = 0x100000;
char *logdiskbuf = (char *)0x900000;
const int LOGDISKBUF_SIZE = 0x100000;