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

int printf(const char *fmt,...)
{
    int i;
    char buf[STR_DEFAULT_LEN];

    va_list arg = (va_list)((char *)(&fmt) + 4);
    
    i = vsprintf(buf,fmt,arg);/* 将所有参数(数字)根据fmt格式写入buf中 */
    int c = write(1,buf,i);
    
    assert(c == i);

    return i;
}

int printl(const char *fmt,...)
{
    int i;
    char buf[STR_DEFAULT_LEN];

    va_list arg = (va_list)((char*)(&fmt) + 4);

    i = vsprintf(buf,fmt,arg);
    printx(buf);

    return i;
}