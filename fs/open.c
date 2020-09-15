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

static struct inode* create_file(char *,int);
static int alloc_imap_bit(int);
static int alloc_smap_bit(int,int);
static struct inode* new_inode(int,int,int);
static void new_dir_entry(struct inode*,int,char *);

/* open函数内核版本 (由task_fs调用)*/
int do_open()
{
    int fd = -1;

    char pathname[MAX_PATH];

    /* 从消息里面拿到参数 */
    int flags       = fs_msg.FLAGS;
    int name_len    = fs_msg.NAME_LEN;
    int src         = fs_msg.source;
    assert(name_len < MAX_PATH);
    phys_copy((void *)va2la(TASK_FS,pathname),
                (void *)va2la(src,fs_msg.PATHNAME),
                name_len);
    pathname[name_len]      = 0;

    /* fd=进程表中空的fd */
    int i;
    for (i = 0; i<NR_FILES; i++)
    {
        if (pcaller->filp[i] == 0)
        {
            fd = i;
            break;
        }
    }
    if ((fd < 0) || (fd >= NR_FILES))
        panic("filp[] is full (PID:%d)", proc2pid(pcaller));

    /* 全局fd表中,找空的fd */
    for (i=0; i < NR_FILE_DESC; i++)
        if (f_desc_table[i].fd_inode == 0)
            break;
    if (i >= NR_FILE_DESC)
        panic("f_desc_table[] is full (PID:%d)", proc2pid(pcaller));
    
    int inode_nr = search_file(pathname);

    struct inode *pin       = 0;
    if (flags & O_CREAT)/* 判断是否是创建文件 */
    {
        if (inode_nr)/* 文件已存在(还创建文件?) */
        {
            printl("{FS} file exists.\n");
            return -1;
        }
        else 
        {
            pin = create_file(pathname,flags);
        }
    }
    else 
    {
        assert(flags & O_RDWR);/* 目前只提供读写选项 */

        char filename[MAX_PATH];
        struct inode *dir_inode;
        if (strip_path(filename,pathname,&dir_inode) != 0)
            return -1;
        pin = get_inode(dir_inode->i_dev,inode_nr);
    }

    if (pin)
    {
        /* 进程表中的fd指向os共有的fd */
        pcaller->filp[fd]   = &f_desc_table[i];

        /* os共有的fd指向文件的inode */
        f_desc_table[i].fd_inode = pin;
        f_desc_table[i].fd_mode = flags;
        f_desc_table[i].fd_cnt  = 1;

        /* 当前读写位置 */
        f_desc_table[i].fd_pos = 0;

        int imode = pin->i_mode & I_TYPE_MASK;

        if (imode == I_CHAR_SPECIAL)/* tty设备 */
        {
            MESSAGE driver_msg;

            driver_msg.type = DEV_OPEN;
            int dev = pin->i_start_sect;
            driver_msg.DEVICE = MINOR(dev);
            assert(MAJOR(dev) == 4);/* 字符设备对应的server进程 */
            assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);

            /* 给对应的服务进程发送消息 */
            send_recv(BOTH,dd_map[MAJOR(dev)].driver_nr,&driver_msg);
        }
        else if (imode == I_DIRECTORY)/* 目录 */
        {
            /* 如果inode指向目录只可能是根目录 */
            assert(pin->i_num == ROOT_INODE);
        }
        else/* 文件 */ 
        {
            assert(pin->i_mode == I_REGULAR);
        }
    }
    else /* 没有得到指向inode的指针 */
    {
        return -1;
    }

    return fd;
}

/* 创建文件和返回inode指针 */
static struct inode* create_file(char *path,int flags)
{
    char filename[MAX_PATH];
    struct inode* dir_inode;

    if (strip_path(filename,path,&dir_inode) != 0) /* return 0 表示提取成功 */
        return 0;
    int inode_nr    = alloc_imap_bit(dir_inode->i_dev);
    int free_sect_nr= alloc_smap_bit(dir_inode->i_dev,NR_DEFAULT_FILE_SECTS);

    struct inode *newino = new_inode(dir_inode->i_dev,inode_nr,free_sect_nr);

    /* 新增的目录条目 */
    new_dir_entry(dir_inode,newino->i_num,filename);

    return newino;
}

/* close 内核版本 */
int do_close()
{
    int fd = fs_msg.FD;
    put_inode(pcaller->filp[fd]->fd_inode);
    if (--pcaller->filp[fd]->fd_cnt == 0)
        pcaller->filp[fd]->fd_inode = 0;/* 关闭描述符 */
    pcaller->filp[fd] = 0;

    return 0;                                
}

int do_lseek()
{
    int fd = fs_msg.FD;
    int off = fs_msg.OFFSET;
    int whence = fs_msg.WHENCE;
    
    int pos = pcaller->filp[fd]->fd_pos;
    int f_size = pcaller->filp[fd]->fd_inode->i_size;

    switch (whence)
    {
    case SEEK_SET:
        pos = off;
        break;
    case SEEK_CUR:
        pos += off;
        break;
    case SEEK_END:
        pos = f_size + off;
        break;
    default:
        return -1;
        break;
    }
    if ((pos > f_size) || (pos < 0))
        return -1;
    pcaller->filp[fd]->fd_pos = pos;
    return pos;
}

/* 在inode_map中分配一位 */
static int alloc_imap_bit(int dev)
{
    int inode_nr    = 0;
    int i,j,k;

    int imap_blk0_nr = 1 + 1;
    struct super_block * sb = get_super_block(dev);

    for (i=0; i < sb->nr_imap_sects; i++)/* nr_imap_sect=1 */
    {
        RD_SECT(dev,imap_blk0_nr + i);

        for (j=0; j<SECTOR_SIZE; j++)
        {
            if (fsbuf[j] == 0xFF)/* 全满 */
                continue;
            for (k=0; ((fsbuf[j] >> k) & 1) != 0; k++) {}/* 找到这个字节中=0的哪一个bit */

            inode_nr    = (i * SECTOR_SIZE + j) * 8 + k;/* inode_nr = 第几个bit */
            fsbuf[j] |= (1 << k);/* 修改内存(扇区)的具体bit */

            WR_SECT(dev,imap_blk0_nr + i);/* 修改哪一个扇区,写回哪一个扇区 */
            break;
        }
        return inode_nr;
    }
    /* 没有位置给inode用了 */
    panic("inode-map is probably full.\n");

    return 0;
}

/*  */
static int alloc_smap_bit(int dev,int nr_sects_to_alloc)
{
    int i;
    int j;
    int k;

    struct super_block *sb = get_super_block(dev);/* 拿到超级块内容 */

    int smap_blk0_nr = 1 + 1 + sb->nr_imap_sects;
    int free_sect_nr = 0;

    for (i=0; i<sb->nr_smap_sects; i++)
    {
        RD_SECT(dev,smap_blk0_nr + i);/* 读取一个扇区到fsbuf中 */

        for (j=0; j < SECTOR_SIZE && nr_sects_to_alloc > 0; j++)
        {
            k = 0;
            if (!free_sect_nr)/* 一直没有找到空闲的东西 */
            {
                if (fsbuf[j] == 0xFF) 
                    continue;
                for (; ((fsbuf[j] >> k) & 1) != 0; k++) {}
                free_sect_nr = (i * SECTOR_SIZE + j) * 8 + 
                        k - 1 + sb->n_1st_sect;/* 从第几个扇区开始 */
            }

            for (; k < 8; k++)
            {
                assert(((fsbuf[j] >> k) & 1) == 0);
                fsbuf[j] |= (1 << k);
                if (--nr_sects_to_alloc == 0)
                    break;
            }
        }
        if (free_sect_nr)/* 空的bit有,并且已经修改了 */
            WR_SECT(dev,smap_blk0_nr + i);
        if (nr_sects_to_alloc == 0)
            break;/* 分配完了 */
    }

    assert(nr_sects_to_alloc == 0);

    return free_sect_nr;
}

static struct inode* new_inode(int dev,int inode_nr,int start_sect)
{
    struct inode *new_inode = get_inode(dev,inode_nr);

    new_inode->i_mode = I_REGULAR;
    new_inode->i_size = 0;
    new_inode->i_start_sect = start_sect;
    new_inode->i_nr_sects = NR_DEFAULT_FILE_SECTS;

    new_inode->i_dev = dev;
    new_inode->i_cnt = 1;
    new_inode->i_num = inode_nr;

    /* 将此新的inode写入inode array */
    sync_inode(new_inode);

    return new_inode;
}

/* 在相应目录中写一个目录项
    dir_inode : i-node of the directory
    inode_nr: i-node nr of the new file 
    filename filename of the new file 
 */
static void new_dir_entry(struct inode *dir_inode,int inode_nr,char *filename)
{
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE) / SECTOR_SIZE;
    int nr_dir_entries = 
        dir_inode->i_size / DIR_ENTRY_SIZE;/* 目录indoe包含多少个项 */
    int m = 0;
    struct dir_entry *pde;
    struct dir_entry *new_de = 0;

    int i,j;
    for (i=0; i<nr_dir_blks; i++)
    {
        RD_SECT(dir_inode->i_dev,dir_blk0_nr + i);

        pde = (struct dir_entry *)fsbuf;
        /* 遍历此扇区所有条目 */
        for (j=0; j<SECTOR_SIZE/DIR_ENTRY_SIZE; j++,pde++)
        {
            if (++m > nr_dir_entries)
                break;
            /* 找到一个条目 */
            if (pde->inode_nr == 0)
            {
                new_de = pde;
                break;
            }
        }
        /* 所有条目扫完了或者找到了空的 */
        if (m > nr_dir_entries || new_de)
            break;
    }
    if (!new_de)/* 没有空的条目,到达dir尾部*/
    {
        new_de = pde;
        dir_inode->i_size += DIR_ENTRY_SIZE;
    }
    new_de->inode_nr = inode_nr;/* 新条目对应新文件inode */
    strcpy(new_de->name,filename);/* 新文件的name */

    /* 写dir区 */
    WR_SECT(dir_inode->i_dev,dir_blk0_nr + i);

    /* 更新 dir inode */
    sync_inode(dir_inode);
}
