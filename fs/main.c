#include "type.h"
#include "config.h"
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

#include "hd.h"

static void init_fs();
static void mkfs();
static void read_super_block(int);

static int  fs_fork();
static int  fs_exit();

/* ring 1 文件系统服务进程 */
void task_fs()
{
    printl("{FS} Task FS begins.\n");

    init_fs();
    
    while(1)
    {
        send_recv(RECEIVE,ANY,&fs_msg);

        int src = fs_msg.source;
        pcaller = &proc_table[src];

        switch (fs_msg.type)
        {
        /* 文件打开和关闭 */
        case OPEN:
            fs_msg.FD = do_open();
            break;
        case CLOSE:
            fs_msg.RETVAL = do_close();
            break;
        /* 文件读写 */
        case READ:
        case WRITE:
            fs_msg.CNT = do_rdwt();
            break;
        /* 删除文件 */
        case UNLINK:
            fs_msg.RETVAL = do_unlink();
            break;
        case RESUME_PROC:
            src = fs_msg.PROC_NR;
            break;
        /* 不理睬SUSPEND_PROC消息() */

        /* 处理MM发送过来的消息 */
        case FORK:
            fs_msg.RETVAL = fs_fork();
            break;
        case EXIT:
            fs_msg.RETVAL = fs_exit();
            break;
        case STAT:
            fs_msg.RETVAL = do_stat();
            break;
        default:
            dump_msg("FS::unknown message:",&fs_msg);
            assert(0);
            break;
        }

        /* 回复 */
        if (fs_msg.type != SUSPEND_PROC)
        {
            fs_msg.type = SYSCALL_RET;
            send_recv(SEND,src,&fs_msg);
        }
    }
}

static void init_fs()
{
    int i;

    for (i=0; i<NR_FILE_DESC; i++)
        memset(&f_desc_table[i],0,sizeof(struct file_desc));

    for (i=0; i<NR_INODE; i++)
        memset(&inode_table[i],0,sizeof(struct inode));

    struct super_block *sb = super_block;
    for (; sb < &super_block[NR_SUPER_BLOCK]; sb++)
        sb->sb_dev  = NO_DEV;

    MESSAGE driver_msg;
    driver_msg.type = DEV_OPEN;
    driver_msg.DEVICE = MINOR(ROOT_DEV);
    assert(dd_map[MAJOR(ROOT_DEV)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH,dd_map[MAJOR(ROOT_DEV)].driver_nr,&driver_msg);

    mkfs();

    /* 为跟文件加载超级块  */
    read_super_block(ROOT_DEV);

    sb = get_super_block(ROOT_DEV);
    assert(sb->magic == MAGIC_V1);

    root_inode = get_inode(ROOT_DEV,ROOT_INODE);
}

/*
在磁盘中创建可用的fs
1.它将向扇区1写入一个超级块
2.创建三个特殊文件：dev_tty0，dev_tty1，dev_tty2
3.创建inode映射
4.创建扇区映射
5.创建文件的索引节点
6.创建'/'，根目录 
 */
static void mkfs()
{
    MESSAGE driver_msg;
    int i,j;

    int bits_per_sect = SECTOR_SIZE * 8;/* 最多支持4K个inode */

    /* 向硬盘驱动程序索要ROOT_DEV的起始扇区和大小 */
    struct part_info geo;
    driver_msg.type     = DEV_IOCTL;
    driver_msg.DEVICE   = MINOR(ROOT_DEV);
    driver_msg.REQUEST  = DIOCTL_GET_GEO;
    driver_msg.BUF      = &geo;
    driver_msg.PROC_NR  = TASK_FS;
    assert(dd_map[MAJOR(ROOT_DEV)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH,dd_map[MAJOR(ROOT_DEV)].driver_nr,&driver_msg);

    printl("{FS} dev size: 0x%x sectors\n",geo.size);
    /* 建立超级块 */
    struct super_block sb;
    sb.magic            = MAGIC_V1;/* 随便定义(OS磁盘文件系统标识) */
    sb.nr_inodes        = bits_per_sect;
    sb.nr_inode_sects   = sb.nr_inodes * INODE_SIZE / SECTOR_SIZE;
    sb.nr_sects         = geo.size;/* 文件系统占用了多少扇区(扇区中分区表大小 */
    sb.nr_imap_sects    = 1;/* 一个扇区就可以映射所有inode */
    sb.nr_smap_sects    = sb.nr_sects / bits_per_sect + 1;
    sb.n_1st_sect       = 1 + 1 + /* boot & sb扇区 */
                            sb.nr_imap_sects + sb.nr_smap_sects + sb.nr_inode_sects;
    sb.root_inode       = ROOT_INODE;
    sb.inode_size       = INODE_SIZE;
    struct inode x;
    sb.inode_isize_off  = (int)&x.i_size - (int)&x;
    sb.inode_start_off  = (int)&x.i_start_sect - (int)&x;
    sb.dir_ent_size     = DIR_ENTRY_SIZE;
    struct dir_entry de;
    sb.dir_ent_inode_off= (int)&de.inode_nr - (int)&de;
    sb.dir_ent_fname_off= (int)&de.name - (int)&de;

    memset(fsbuf,0x90,SECTOR_SIZE);
    memcpy(fsbuf,&sb,SUPER_BLOCK_SIZE);

    /* 写超级快 */
    WR_SECT(ROOT_DEV,1);

    printl("{FS} devbase:0x%x00, sb:0x%x00, imap:0x%x00, smap:0x%x00\n"
	       "        inodes:0x%x00, 1st_sector:0x%x00\n", 
	       geo.base * 2,
	       (geo.base + 1) * 2,
	       (geo.base + 1 + 1) * 2,
	       (geo.base + 1 + 1 + sb.nr_imap_sects) * 2,
	       (geo.base + 1 + 1 + sb.nr_imap_sects + sb.nr_smap_sects) * 2,
	       (geo.base + sb.n_1st_sect) * 2);
    
    /*---- inode map ---- */
    memset(fsbuf,0,SECTOR_SIZE);
    for (i=0; i < (NR_CONSOLES + 3); i++)
        fsbuf[0] |= 1 << i;/* inode 0,root,tty0,tty1,tty2 */
    assert(fsbuf[0] == 0x3F);
    WR_SECT(ROOT_DEV,2);
    
    /*---- secter_map (记录所有扇区使用情况)---- */
    memset(fsbuf,0,SECTOR_SIZE);
    int nr_sects    = NR_DEFAULT_FILE_SECTS + 1;/* 数据区已使用:根目录和第一个预留了 */

    for (i=0; i<nr_sects / 8; i++)
        fsbuf[i] = 0xFF;
    for (j=0; j<nr_sects % 8; j++)
        fsbuf[i] |= (1 << j);

    WR_SECT(ROOT_DEV,2 + sb.nr_imap_sects);/* 已使用() */

    memset(fsbuf,0,SECTOR_SIZE);
    for (i=1; i<sb.nr_smap_sects; i++)
        WR_SECT(ROOT_DEV,2 + sb.nr_imap_sects + i);
    /*从image中提取cmd.tar文件(存的是应用程序),提取到OS的FS来 */
    assert(INSTALL_START_SECT + INSTALL_NR_SECTS < sb.nr_sects - NR_SECTS_FOR_LOG);
    int bit_offset = INSTALL_START_SECT - sb.nr_sects + 1;/* 首扇区号 - */
    int bit_off_in_sect = bit_offset % (SECTOR_SIZE * 8);
    int bit_left = INSTALL_NR_SECTS;
    int cur_sect = bit_offset / (SECTOR_SIZE * 8);
    RD_SECT(ROOT_DEV,2 + sb.nr_imap_sects + cur_sect);
    while(bit_left)
    {
        int byte_off = bit_off_in_sect / 8;
        fsbuf[byte_off] |= 1 << (bit_off_in_sect % 8);
        bit_left--;
        bit_off_in_sect++;
        if (bit_off_in_sect == (SECTOR_SIZE * 8))
        {
            WR_SECT(ROOT_DEV,2 + sb.nr_imap_sects + cur_sect);
            cur_sect++;
            RD_SECT(ROOT_DEV,2 + sb.nr_imap_sects + cur_sect);
            bit_off_in_sect = 0;
        }
    }
    WR_SECT(ROOT_DEV,2 + sb.nr_imap_sects + cur_sect);

    /* ---- inodes arrays ---- */ 
    /* inode of '/' */
    memset(fsbuf,0,SECTOR_SIZE);
    struct inode *pi = (struct inode *)fsbuf;
    pi->i_mode = I_DIRECTORY;
    pi->i_size = DIR_ENTRY_SIZE * 5;/* root tty0~2 */

    pi->i_start_sect = sb.n_1st_sect;
    pi->i_nr_sects   = NR_DEFAULT_FILE_SECTS;
    /* inode of '/dev_tty0~2' */
    for (i=0; i<NR_CONSOLES; i++)/* 上面和这里 写入目前所有inode节点信息存到数组中 */
    {
        pi = (struct inode *)(fsbuf + (INODE_SIZE *(i + 1)));
        pi->i_mode  = I_CHAR_SPECIAL;
        pi->i_size  = 0;
        pi->i_start_sect = MAKE_DEV(DEV_CHAR_TTY,i);
        pi->i_nr_sects   = 0;
    }
    /* inode of '/cmd.tar' */
    pi = (struct inode*)(fsbuf + (INODE_SIZE * (NR_CONSOLES + 1)));
    pi->i_mode = I_REGULAR;
    pi->i_size = INSTALL_NR_SECTS * SECTOR_SIZE;/* 预留扇区个数*512 */
    pi->i_start_sect = INSTALL_START_SECT;
    pi->i_nr_sects = INSTALL_NR_SECTS;
    
    WR_SECT(ROOT_DEV,2 + sb.nr_imap_sects + sb.nr_smap_sects);

    /*---- 根目录 (存放文件索引)----*/
    memset(fsbuf,0,SECTOR_SIZE);
    struct dir_entry *pde = (struct dir_entry *)fsbuf;

    pde->inode_nr = 1;
    strcpy(pde->name,".");

    for (i=0; i<NR_CONSOLES; i++)/* tty0~2文件索引 */
    {
        pde++;
        pde->inode_nr = i+2;
        sprintf(pde->name,"dev_tty%d",i);
    }

    (++pde)->inode_nr = NR_CONSOLES + 2;
    strcpy(pde->name,"cmd.tar");
    
    WR_SECT(ROOT_DEV,sb.n_1st_sect);
}

int rw_sector(int io_type,int dev,u64 pos,int bytes,
            int proc_nr,void* buf)
{
    MESSAGE driver_msg;

    driver_msg.type     = io_type;
    driver_msg.DEVICE   = MINOR(dev);
    driver_msg.POSITION = pos;
    driver_msg.BUF      = buf;
    driver_msg.CNT      = bytes;
    driver_msg.PROC_NR  = proc_nr;
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    send_recv(BOTH,dd_map[MAJOR(dev)].driver_nr,&driver_msg);

    return 0;
}

/* 从给定设备读取超级块，
然后将其写入空闲的super_block []插槽 */
static void read_super_block(int dev)
{
    int i;
    MESSAGE driver_msg;

    driver_msg.type = DEV_READ;
    driver_msg.DEVICE = MINOR(dev);
    driver_msg.POSITION = SECTOR_SIZE * 1;
    driver_msg.BUF  = fsbuf;
    driver_msg.CNT  = SECTOR_SIZE;
    driver_msg.PROC_NR = TASK_FS;
    assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
    
    send_recv(BOTH,dd_map[MAJOR(dev)].driver_nr,&driver_msg);

    /* 从super_block[]中找到空的 */
    for (i=0; i<NR_SUPER_BLOCK; i++)
    {
        if (super_block[i].sb_dev == NO_DEV)
            break;
    }

    if (i == NR_SUPER_BLOCK)
        panic("super_block slots used up");
    assert(i == 0);/* 我们只会用到第一个 */

    struct super_block *psb = (struct super_block *)fsbuf;

    super_block[i] = *psb;
    super_block[i].sb_dev = dev;
}

struct super_block * get_super_block(int dev)
{
    struct super_block *sb = super_block;
    for (; sb < &super_block[NR_SUPER_BLOCK]; sb++)
        if (sb->sb_dev == dev)
            return sb;
    
    panic("super block of devie %d not found.\n", dev);

    return 0;
}

struct inode *get_inode(int dev,int num)
{
    if (num == 0)
        return 0;
    struct inode *p;
    struct inode *q = 0;
    for (p = &inode_table[0]; p < &inode_table[NR_INODE]; p++)
    {
        if (p->i_cnt)/* 此inode已经被用了 */
        {
            /* 但是和我们申请的inode所属的设备和nr一样 */
            if ((p->i_dev == dev) && (p->i_num == num))
            {
                p->i_cnt++;
                return p;/* 共享inode */
            }
        }
        else /* 分配第一个没有用的inode */
        {
            if (!q)
                q = p;
        }
    }

    if (!q)
        panic("the inode table is full");/* inode用完了() */

    q->i_dev = dev;
    q->i_num = num;
    q->i_cnt = 1;

    struct super_block *sb = get_super_block(dev);
    int blk_nr = 1 + 1 + sb->nr_imap_sects + sb->nr_smap_sects + ((num - 1) / (SECTOR_SIZE / INODE_SIZE));
    RD_SECT(dev,blk_nr);/* 将此inode所在扇区读取到内存中 */
    struct inode * pinode = (struct inode*)((u8 *)fsbuf + ((num - 1) % (SECTOR_SIZE / INODE_SIZE)) * INODE_SIZE);
    q->i_mode = pinode->i_mode;
    q->i_size = pinode->i_size;
    q->i_start_sect = pinode->i_start_sect;
    q->i_nr_sects = pinode->i_nr_sects;

    return q;
}

/*  inode用完后应该自减 */
void put_inode(struct inode *pinode)
{
    assert(pinode->i_cnt > 0);
    pinode->i_cnt--;                                      
}

/* 将索引节点写回disk */
void sync_inode(struct inode *p)
{
    struct inode *pinode;
    struct super_block *sb = get_super_block(p->i_dev);
    int blk_nr = 1+1+sb->nr_imap_sects+sb->nr_smap_sects+
                ((p->i_num - 1) / (SECTOR_SIZE/INODE_SIZE));
    RD_SECT(p->i_dev,blk_nr);
    pinode = (struct inode*)((u8*)fsbuf +
                            (((p->i_num - 1) % (SECTOR_SIZE / INODE_SIZE)) * INODE_SIZE));
    pinode->i_mode  = p->i_mode;
    pinode->i_size  = p->i_size;
    pinode->i_start_sect = p->i_start_sect;
    pinode->i_nr_sects = p->i_nr_sects;
    WR_SECT(p->i_dev,blk_nr);
}

/* 处理进程fork(),子进程复制父进程fd情况 */
static int fs_fork()
{
    int i;
    struct proc* child = &proc_table[fs_msg.PID];/* mm发送来的消息,pid = childpid */
    for (i=0; i<NR_FILES; i++)/* 进程表中所有的fd */
    {
        if (child->filp[i])
        {
            child->filp[i]->fd_cnt++;
            child->filp[i]->fd_inode->i_cnt++;
        }
    }

    return 0;
}

static int fs_exit()
{
    int i;
    struct proc* p = &proc_table[fs_msg.PID];
    for (i=0; i<NR_FILES; i++)/* 遍历进程所有fd */
    {
        if (p->filp[i])
        {
            /* 释放inode */
            p->filp[i]->fd_inode->i_cnt--;
            /* 释放fd 槽 */
            if (--p->filp[i]->fd_cnt == 0)
                p->filp[i]->fd_inode = 0;
            p->filp[i] = 0;
        }
    }
    return 0;
}