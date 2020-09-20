#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"

int kernel_main()
{
    disp_str("-----\"kernel_main\" begins-----\n");
    
    int i,j,eflags,prio;
    u8 rpl,priv;

    struct task *t;
    struct proc *p = proc_table;

    //开始分配栈的地址
    char *stk = task_stack + STACK_SIZE_TOTAL;

    /* 由task_table表信息根据类型填写到proc_table中 */
    for (i=0; i<NR_TASKS + NR_PROCS; i++,p++,t++)
    {
        /* 过了OS服务常驻进程+4个开始的进程 就是 普通进程 */
        if (i >= NR_TASKS + NR_NATIVE_PROCS)
        {
            p->p_flags = FREE_SLOT; /* 创建proc_table中的空选项 */
            continue;
        }

        /* OS的5个服务进程 */
        if (i < NR_TASKS)
        {
            t = task_table + i;
            priv    = PRIVILEGE_TASK;
            rpl     = RPL_TASK;
            eflags  = 0x1202; /* IF=1(可响应外部可屏蔽中断),IOPL=1 */
            prio    = 15; //进程调度优先级
        }
        else /* 4个用户进程 */
        {
            t       = user_proc_table + (i - NR_TASKS);
            priv    = PRIVILEGE_USER;//用户级
            rpl     = RPL_USER;
            eflags  = 0x202;/* IF=1 */
            prio    = 5;
        }
        strcpy(p->name,t->name);
        p->p_parent = NO_TASK;
        
        /* 除了Init进程外其他进程的text段和data段都使用平坦模式的段*/
        if (strcmp(t->name,"INIT") != 0)
        {
            p->ldts[INDEX_LDT_C]    = gdt[SELECTOR_KERNEL_CS >> 3];
            p->ldts[INDEX_LDT_RW]   = gdt[SELECTOR_KERNEL_DS >> 3];

            /* DPL */
            p->ldts[INDEX_LDT_C].attr1  = DA_C | priv << 5;
            p->ldts[INDEX_LDT_RW].attr1 = DA_DRW | priv << 5;
        }
        else/* Init进程 */
        {
            unsigned int k_base;
            unsigned int k_limit;
            int ret = get_kernel_map(&k_base,&k_limit);
            assert(ret == 0);
            /* 更改init进程LDT表中的描述符 */
            init_desc(&p->ldts[INDEX_LDT_C],
                        0,
                        (k_base + k_limit) >> LIMIT_4K_SHIFT,
                        DA_32 | DA_LIMIT_4K | DA_C | priv << 5);

            init_desc(&p->ldts[INDEX_LDT_RW],
                        0,
                        (k_base + k_limit) >> LIMIT_4K_SHIFT,
                        DA_32 | DA_LIMIT_4K | DA_DRW | priv << 5);
        }

        /* proc_table[]中的其他信息 */
        p->regs.cs  = INDEX_LDT_C << 3 | SA_TIL | rpl;
        p->regs.ds  = 
            p->regs.es  =
            p->regs.fs  = 
            p->regs.ss  = INDEX_LDT_RW << 3 | SA_TIL | rpl;
        p->regs.gs  = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;/* 普通进程gs寄存器指向内核的gs段 */
        p->regs.eip = (u32)t->initial_eip;/* 将task[]中的eip复制到proctable[]中 */
        p->regs.esp = (u32)stk;
        p->regs.eflags = eflags;

        p->ticks = p->priority = prio; /* 调度优先级 */

        /* 在内核中的状态  */
        p->p_flags  = 0;
        p->p_msg    = 0;
        p->p_recvfrom = NO_TASK;
        p->p_sendto = NO_TASK;
        p->has_int_msg  = 0;
        p->q_sending = 0;
        p->next_sending = 0;

        /* 初始化进程指向的fd数组 */
        for (j=0; j<NR_FILES; j++)
            p->filp[j] = 0;
        
        stk -= t->stacksize;
    }

    k_reenter = 0;     /*解决中断重入的全局变量*/
    ticks = 0;      /*kernel API 0 全局变量初始化*/

    p_proc_ready    = proc_table;

    /* 设置时钟中断 */
    init_clock();
    /* 设置键盘中断 */
    init_keyboard();

    restart(); /*进入第一个进程A(r0->r1)*/

    while(1) {}
}

int get_ticks()
{
    MESSAGE msg;
	reset_msg(&msg);
	msg.type = GET_TICKS;
	send_recv(BOTH, TASK_SYS, &msg);
	return msg.RETVAL;
}

/* struct posix_tar_header
 */
struct posix_tar_header
{                   /* offset */
    char name[100]; /* 0 */
    char mode[8];   /* 100 */
    char uid[8];    /* 108 */
    char gid[8];    /* 116 */
    char size[12];  /* 124 */
    char mtime[12]; /* 136 */
    char chksum[8]; /* 148 */
    char typeflag;  /* 156 */
    char linkname[100]; /* 157 */
    char magic[6];  /* 257 */
    char version[2];    /* 263 */
    char uname[32]; /* 265 */
    char gname[32]; /* 297 */
    char devmajor[8];   /* 329 */
    char devminor[8];   /* 337 */
    char prefix[155];   /* 345 */
                        /* 500 */
};

/* extract the tar file and store them
    从tar提取文件并且存储他们
 */
void untar(const char *filename)
{
    printf("[extract '%s'\n",filename);
    int fd = open(filename,O_RDWR);
    assert(fd != -1);

    char buf[SECTOR_SIZE * 16];/* 处理16个tar header?*/
    int chunk = sizeof(buf);
    
    while(1)/* 从整个tar中读取所有文件 */
    {
        read(fd,buf,SECTOR_SIZE);/*首先读取文件的header*/
        if (buf[0] == 0)
            break;/* 最后一个文件读取完 */
        struct posix_tar_header *phdr = (struct posix_tar_header*)buf;

        /* 计算这一个文件大小(char size[12]),然后继续从infd读取 */
        char *p = phdr->size;
        int f_len = 0;
        while(*p)/* 8进制字符转->10进制数字 */
            f_len = (f_len * 8) + (*p++ - '0');
        
        int bytes_left = f_len;/* 此文件的大小(接下来要读取的字节) */
        /* 写入到FS中 */
        int fdout = open(phdr->name,O_CREAT | O_RDWR);
        if (fdout == -1)
        {
            printf("    failed to extract file: %s\n",phdr->name);
            printf(" aborted]\n");
            return ;
        }
        printf("    %s (%d bytes)\n",phdr->name,f_len);
        while (bytes_left)/*  */
        {
            int iobytes = min(bytes_left,chunk);/* 判断这一次读取8K,还是剩余的大小 */
            read(fd,buf,
                ((iobytes - 1)/SECTOR_SIZE+1)*SECTOR_SIZE);/* 从80m.img把此文件读取出来,第3个参数是扇区整数 */
            write(fdout,buf,iobytes);/* 写入80m.img,创建了对应的文件 */
            bytes_left -= iobytes;/* 继续读取此文件 */
        }
        close(fdout);/* 此文件读取完毕 */
    }

    close(fd);
    printf("done]\n");
}

/* 简单shell */
void shabby_shell(const char *tty_name)
{
    int fd_stdin = open(tty_name,O_RDWR);
    assert(fd_stdin == 0);
    int fd_stdout= open(tty_name,O_RDWR);
    assert(fd_stdout == 1);

    char rdbuf[128];
    
    while(1)
    {
        write(1,"$",2);
        int r = read(0,rdbuf,70);
        rdbuf[r] = 0;

        int argc = 0;
        char *argv[PROC_ORIGIN_STACK];
        char *p = rdbuf;
        char *s;
        int word = 0;
        char ch;
        do {
            ch = *p;
            if (*p != ' ' && *p != 0 && !word)
            {
                s = p;
                word = 1;
            }
            if ((*p == ' ' || *p == 0) && word)
            {
                word = 0;
                argv[argc++] = s;
                *p = 0;
            }
            p++;
        }while(ch);
        argv[argc] = 0;
        
        int fd = open(argv[0],O_RDWR);
        if (fd == -1)
        {
            if (rdbuf[0])
            {
                write(1,"{",1);
                write(1,rdbuf,r);
                write(1,"}\n",2);
            }
        }
        else 
        {
            close(fd);
            int pid = fork();
            if (pid != 0)
            {
                int s;
                wait(&s);
            }
            else
            {
                execv(argv[0],argv);
            }   
        }
    }
    close(1);
    close(0);
}

/* 为了实现fork,先添加init进程 */
void Init()
{
    int fd_stdin = open("/dev_tty0",O_RDWR);
    assert(fd_stdin == 0);
    int fd_stdout = open("/dev_tty0",O_RDWR);
    assert(fd_stdout == 1);

    printf("Init() is running ... \n");

    /* 解压 cmd.tar文件(在mkfs中规定了名字是cmd.tar) */
    untar("/cmd.tar");
// #if 0

    char *tty_list[] = {"/dev_tty1","/dev_tty2"};

    int i;
    for (i=0; i<sizeof(tty_list)/sizeof(tty_list[0]); i++)
    {
        int pid = fork();
        if (pid != 0)
            printf("[parent is running, child pid:%d]\n",pid);
        else 
        {
            printf("[child is running, pid:%d]\n",getpid());
            close(fd_stdin);
            close(fd_stdout);

            shabby_shell(tty_list[i]);
            assert(0);
        }
    }

    /* MM将进程p的child过继给INIT,由INIT处理zombie进程 */
    while(1)
    {
        int s;
        int child = wait(&s);
        printf("child (%d) exited with status: %d.\n",child,s);
    }
// #endif 
    assert(0);
}

/*进程A*/
void TestA()
{  
    for (;;)
        ;
}

/*进程B*/
void TestB()
{   
    for (;;);
}

void TestC()
{
    for (;;);
}

void panic(const char *fmt,...)
{
    char buf[256];

    va_list arg = (va_list)((char *)&fmt + 4);

    vsprintf(buf,fmt,arg);

    printl("%c !!panic!! %s", MAG_CH_PANIC, buf);

    __asm__ __volatile__("ud2");
}