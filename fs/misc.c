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
#include "hd.h"
#include "fs.h"

/* 搜索文件并且返回inode */
int search_file(char *path)
{
    int i,j;

    char filename[MAX_PATH];
    memset(filename,0,MAX_FILENAME_LEN);
    struct inode *dir_inode;
    /* 给路径,提取文件名 */
    if (strip_path(filename,path,&dir_inode) != 0)
        return 0;
    if (filename[0] == 0)/*给出的路径是/*/
        return dir_inode->i_num;

    /* 从/目录中找到文件 */
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries = dir_inode->i_size / DIR_ENTRY_SIZE;

    int m = 0;
    struct dir_entry *pde;
    for (i=0; i<nr_dir_blks; i++)
    {
        RD_SECT(dir_inode->i_dev,dir_blk0_nr + i);
        pde = (struct dir_entry *)fsbuf;
        /* 遍历所有目录项目 */
        for (j=0; j<SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++)
        {
            if (memcmp(filename,pde->name,MAX_FILENAME_LEN) == 0)
            {
                return pde->inode_nr;/* 找到了 */
            }
            if (++m > nr_dir_entries)/* 遍历所有没有找到 */
            {
                break;
            }
        }
        if (m > nr_dir_entries)
        {
            break;
        }
    }

    /* not found */
    return 0;
}

/* 因为是扁平目录,提取文件名只需要去掉/ */
int strip_path(char *filename,const char* pathname,struct inode **ppinode)
{
    const char *s = pathname;
    char *t = filename;

    if (s == 0)/* 路径名为空 提取不到 */
        return -1;
    if (*s == '/')
        s++;
    while(*s)
    {
        if (*s == '/')
            return -1;
        *t++ = *s++;/* 提取文件名字 */
        if (t - filename >= MAX_FILENAME_LEN)
            break;
    }
    *t = 0;/* 文件结尾 */

    *ppinode = root_inode;

    return 0;
}