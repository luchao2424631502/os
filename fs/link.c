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

/* 删除文件 */
int do_unlink()
{
    char pathname[MAX_PATH];

    /* 从message中拿到参数 */
    int name_len    = fs_msg.NAME_LEN;
    int src         = fs_msg.source;
    assert(name_len < MAX_PATH);
    phys_copy((void *)va2la(TASK_FS,pathname),
                (void *)va2la(src,fs_msg.PATHNAME),
                name_len);
    pathname[name_len]=0;

    if (strcmp(pathname,"/") == 0)
    {
        printl("{FS} FS:do_unlink():: cannot unlink the root\n");
        return -1;
    }

    int inode_nr    = search_file(pathname);
    if (inode_nr == INVALID_INODE)/* file没有找到 */
    {
        printl("{FS} FS::do_unlink():: search_file() returns "
			"invalid inode: %s\n", pathname);
		return -1;
    }

    char filename[MAX_PATH];
    struct inode *dir_inode;
    /* 从路径中提取文件名 */
    if (strip_path(filename,pathname,&dir_inode) != 0)
        return -1;
    
    struct inode *pin = get_inode(dir_inode->i_dev,inode_nr);

    if (pin->i_mode != I_REGULAR)/* 只能删除常规文件 */
    {
        printl("{FS} cannot remove file %s, because "
		       "it is not a regular file.\n",
		       pathname);
		return -1;
    }

    if (pin->i_cnt > 1)/* 文件仍然被打开 */
    {
        printl("{FS} cannot remove file %s, because pin->i_cnt is %d.\n",
		       pathname, pin->i_cnt);
		return -1;
    }

    struct super_block *sb = get_super_block(pin->i_dev);

    /* 释放imap对应bit */
    int byte_idx    = inode_nr / 8;
    int bit_idx     = inode_nr % 8;
    assert(byte_idx < SECTOR_SIZE);/* inode map占用一个扇区 */
    RD_SECT(pin->i_dev,2);
    /* 一定是1(打开) */
    assert(fsbuf[byte_idx % SECTOR_SIZE] & (1 << bit_idx));
    /* 关闭 */
    fsbuf[byte_idx % SECTOR_SIZE] &= ~(1 << bit_idx);
    /* 写回 */
    WR_SECT(pin->i_dev,2);

    /* 释放smap对应bit */
    bit_idx         = pin->i_start_sect - sb->n_1st_sect + 1;
    byte_idx        = bit_idx / 8;
    int bits_left   = pin->i_nr_sects;/* 大小 */
    int byte_cnt    = (bits_left - (8 - (bit_idx % 8))) / 8;
            /* boot + super + inodemap + 第几个扇区(sect map) */
    int s = 2 + sb->nr_imap_sects + byte_idx / SECTOR_SIZE;

    RD_SECT(pin->i_dev,s);

    int i;

    /* 清除首个(部分)字节 */
    for (i = bit_idx % 8; (i < 8) && bits_left; i++,bits_left--)
    {
        /* 这里是倒着来,从后往前扫 */
        assert((fsbuf[byte_idx % SECTOR_SIZE] >> i & 1) == 1);
        fsbuf[byte_idx % SECTOR_SIZE] &= ~(1 << i);
    }

    int k;
    i = (byte_idx % SECTOR_SIZE) + 1;
    for (k=0; k<byte_cnt; k++,i++,bits_left -= 8)
    {
        if (i == SECTOR_SIZE)
        {
            i = 0;
            WR_SECT(pin->i_dev,s);
            RD_SECT(pin->i_dev,++s);
        }
        assert(fsbuf[i] == 0xFF);
        fsbuf[i]        = 0;
    }

    if (i == SECTOR_SIZE)
    {
        i = 0;
        WR_SECT(pin->i_dev,s);
        RD_SECT(pin->i_dev,++s);
    }
    unsigned char mask      = ~((unsigned char)(~0) << bits_left);
    assert((fsbuf[i] & mask) == mask);
    fsbuf[i] &= (~0) << bits_left;
    WR_SECT(pin->i_dev,s);

    /* 清除inode */
    pin->i_mode     = 0;
    pin->i_size     = 0;
    pin->i_start_sect=0;
    pin->i_nr_sects = 0;
    sync_inode(pin);

    put_inode(pin);

    /* 在根目录中把inode_nr设置为0 */
    int dir_blk0_nr     = dir_inode->i_start_sect;
    int nr_dir_blks     = (dir_inode->i_size + SECTOR_SIZE) / SECTOR_SIZE;
    int nr_dir_entries  = dir_inode->i_size / DIR_ENTRY_SIZE;

    int m           = 0;
    struct dir_entry *pde = 0;
    int flg         = 0;
    int dir_size    = 0;

    /* 遍历根目录扇区 */
    for (i=0; i<nr_dir_blks; i++)
    {
        RD_SECT(dir_inode->i_dev,dir_blk0_nr + i);

        pde = (struct dir_entry *)fsbuf;
        int j;
        /* 遍历一个扇区中所有的目录项目 */
        for (j=0; j<SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++)
        {
            if (++m > nr_dir_entries)
                break;
            if (pde->inode_nr == inode_nr)
            {
                memset(pde,0,DIR_ENTRY_SIZE);
                WR_SECT(dir_inode->i_dev,dir_blk0_nr + i);
                flg = 1;
                break;
            }

            if (pde->inode_nr != INVALID_INODE)/* 不是无效inode(空洞),就修改大小 */
                dir_size += DIR_ENTRY_SIZE;
        }

        /* 遍历完毕或者找到此目录项了 */
        if (m > nr_dir_entries || flg)
            break;
    }
    assert(flg);/* 一定会找到,否则删除失败 */
    if (m == nr_dir_entries)/* 刚好此项是root中最后一个 */
    {
        dir_inode->i_size   = dir_size;
        sync_inode(dir_inode);
    }

    return 0;
}