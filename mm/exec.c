
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
#include "keyboard.h"
#include "proto.h"
#include "elf.h"

int do_exec()
{
    /* 接收消息参数 */
    int name_len    = mm_msg.NAME_LEN;
    int src         = mm_msg.source;
    assert(name_len < MAX_PATH);
    
    char pathname[MAX_PATH];
    /* 得到src的pathname */
    phys_copy((void*)va2la(TASK_MM,pathname),
                (void*)va2la(src,mm_msg.PATHNAME),
                name_len);
    pathname[name_len] = 0;

    /* 得到文件大小 */
    struct stat s;
    int ret = stat(pathname,&s);
    if (ret != 0)
    {
        printl("{MM} MM::do_exec()::stat() returns error. %s",pathname);
        return -1;
    }

    /* read the file */
    int fd = open(pathname,O_RDWR);
    if (fd == -1)
        return -1;
    assert(s.st_size < MMBUF_SIZE);
    read(fd,mmbuf,s.st_size);
    close(fd);

    /* 新文件覆盖当前proc内存image */
    Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*)(mmbuf);
    int i;
    /* 遍历elf文件程序段header */
    for (i=0; i<elf_hdr->e_phnum; i++)
    {
        Elf32_Phdr* prog_hdr = (Elf32_Phdr*)(mmbuf + elf_hdr->e_phoff +
                                (i * elf_hdr->e_phentsize));
        if (prog_hdr->p_type == PT_LOAD)
        {
            assert(prog_hdr->p_vaddr + prog_hdr->p_memsz < 
                        PROC_IMAGE_SIZE_DEFAULT);
            /* 把新elf文件段复制到调用exec的proc的内存image对应段中 */
            phys_copy((void*)va2la(src,(void*)prog_hdr->p_vaddr),
                (void*)va2la(TASK_MM,
                        mmbuf + prog_hdr->p_offset),
                prog_hdr->p_filesz);
        }
    }

    /* 安装参数栈 */
    int orig_stack_len = mm_msg.BUF_LEN;
    char stackcopy[PROC_ORIGIN_STACK];
    /* 将应用层execv的栈数据赋值过来 */
    phys_copy((void*)va2la(TASK_MM,stackcopy),
            (void*)va2la(src,mm_msg.BUF),
            orig_stack_len);
    
    u8 *orig_stack = (u8*)(PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK);
    int delta = (int)orig_stack - (int)mm_msg.BUF;/* 地址差距 */
    
    int argc = 0;
    if (orig_stack_len)
    {
        char **q = (char **)stackcopy;
        for (; *q!=0; q++,argc++)
        {
            *q += delta;
        }
    }

    /* 栈从src_MMBUF->TASK_MM.buf[]->src_orig_stack */
    phys_copy((void*)va2la(src,orig_stack),
                (void*)va2la(TASK_MM,stackcopy),
                orig_stack_len);
    
    proc_table[src].regs.ecx = argc;
    proc_table[src].regs.eax = (u32)orig_stack;

    /* 控制权转移到exec的进程,和安排栈指针 */
    proc_table[src].regs.eip = elf_hdr->e_entry;
    proc_table[src].regs.esp = PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK;

    strcpy(proc_table[src].name,pathname);

    return 0;
}