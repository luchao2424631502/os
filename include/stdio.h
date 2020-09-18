#ifndef _ORANGES_STDIO_H_
#define _ORANGES_STDIO_H_

#include "type.h"

/* 断言宏定义 */
#define ASSERT
#ifdef ASSERT
void assertion_failure(char *exp, char *file, char *base_file, int line);
#define assert(exp)  if (exp) ; \
        else assertion_failure(#exp, __FILE__, __BASE_FILE__, __LINE__)
#else
#define assert(exp)
#endif

/**在const.h中引入了extern(声明全局变量),又在global.c中通过宏方法除去(定义全局变量)*/
#define EXTERN extern

#define TRUE        1
#define FALSE       0

/* string */
#define STR_DEFAULT_LEN 1024

#define O_CREAT     1
#define O_RDWR      2

#define SEEK_SET    1
#define SEEK_CUR    2
#define SEEK_END    3

#define MAX_PATH    128

/* ---------*
    库函数 (给用户编程用)
*-----------*/

/* printf.c */
int printf(const char *fmt,...);
int printl(const char *fmt,...);

/* vsprintf.c */
int vsprintf(char *buf,const char *fmt,va_list args);
int sprintf(char *buf,const char *fmt,...);


/* lib/open.c */
int open(const char *,int);

/* lib/close.c */
int close(int);

/* lib/read.c */
int read(int ,void *,int );

/* lib/write.c */
int write(int ,const void *,int );

/* lib/unlink.c */
int unlink(const char *);

/* lib/getpid.c 2020-9-13添加*/
int getpid();

/* lib/fork.c */
int fork();

/* lib/exit.c */
void exit(int status);

/* lib/wait.c */
int wait(int *status);

#endif