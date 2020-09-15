#include "type.h"
#include "config.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "tty.h"
#include "console.h"
#include "string.h"     //好像并没有使用到?
#include "fs.h"
#include "proc.h"
#include "global.h"
#include "proto.h"

#include "elf.h"

/* 在内存中后去在loader.asm中保存的信息 */
void get_boot_params(struct boot_params *pbp)
{
    int *p = (int *)BOOT_PARAM_ADDR;
    assert(p[BI_MAG] == BOOT_PARAM_MAGIC);

    pbp->mem_size = p[BI_MEM_SIZE];
    pbp->kernel_file = (unsigned char *)(p[BI_KERNEL_FILE]);
    
    unsigned int temp = ELFMAG;
    assert(memcmp(pbp->kernel_file,(void*)&temp,SELFMAG) == 0);
}

/* 解析内核文件,获取内核镜像的内存范围 */
int get_kernel_map(unsigned int *b,unsigned int *l)
{
    struct boot_params bp;
    get_boot_params(&bp);

    /* ELF header */
    Elf32_Ehdr *elf_header = (Elf32_Ehdr*)(bp.kernel_file);
    
    unsigned int temp = ELFMAG;
    /* 确保kernel file应该是ELF格式 */
    if (memcmp(elf_header->e_ident,(void*)&temp,SELFMAG) != 0)
        return -1;

    *b = ~0;
    unsigned int t = 0;
    int i;
    for (i=0; i<elf_header->e_shnum; i++)
    {
        /* 拿到每一个section header项 */
        Elf32_Shdr* section_header = 
            (Elf32_Shdr*)(bp.kernel_file + 
                            elf_header->e_shoff + 
                            i * elf_header->e_shentsize);
        
        /* 判断此段是否需要载入内存 */
        if (section_header->sh_flags & SHF_ALLOC)
        {
            int bottom  = section_header->sh_addr;
            int top     = section_header->sh_addr+section_header->sh_size;

            if (*b > bottom)
                *b = bottom;
            if (t < top)
                t = top; 
        }
    }
    assert(*b < t);
    *l = t - *b - 1;

    return 0;
}

/*----0开头去掉,数字转化成字符串----*/
char *itoa(char *str,int num)
{
    char *p = str;
    char ch;
    int i;
    int flag = 0;
    *p++ = '0';//后++优先级高
    *p++ = 'x';
    
    if (num == 0)
    {
        *p++ = '0';
    }
    else 
    {
        for (i=28; i>=0; i-=4) 
        {
            ch = (num >> i) & 0xF;//得到4bit代表的数值
            if (flag || (ch > 0))
            {
                flag = 1;
                ch += '0';//转化
                if (ch > '9')
                {
                    ch += 7;//ASCII数字和A间隔字符数
                }
                *p++ = ch;
            }
        }
    }
    *p = 0;

    return str;//个人觉得毫无意义
}


/*----显示整数(按照16进制----*/
void disp_int(int input)
{
    char output[16];
    itoa(output,input);
    disp_str(output);//转化成16进制的字符串打印
}

void delay(int time)
{
    int i,j,k;
    for (k=0; k<time; k++)
    {
        for (i=0; i<10; i++)
        {
            for (j=0; j<30000; j++)
            {
                
            }
        }
    }
}