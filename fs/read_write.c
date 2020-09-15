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


int do_rdwt()
{
    int fd = fs_msg.FD;
    void *buf = fs_msg.BUF;
    int len = fs_msg.CNT; /* 读写的字节数 */

    int src = fs_msg.source;/* 调用此函数的进程 */

    assert((pcaller->filp[fd] >= &f_desc_table[0]) && 
            (pcaller->filp[fd] < &f_desc_table[NR_FILE_DESC]));
    
    /* 判断文件是否可读可写 */
    if (!(pcaller->filp[fd]->fd_mode & O_RDWR))
        return 0;
    
    /* 读写地点 */
    int pos = pcaller->filp[fd]->fd_pos;

    struct inode *pin = pcaller->filp[fd]->fd_inode;

    assert(pin >= &inode_table[0] && pin < &inode_table[NR_INODE]);

    int imode = pin->i_mode & I_TYPE_MASK;

    if (imode == I_CHAR_SPECIAL)/* 字符设备把消息转发给驱动 */
    {
        int t = fs_msg.type == READ ? DEV_READ : DEV_WRITE;
        fs_msg.type = t;

        int dev = pin->i_start_sect;
        assert(MAJOR(dev) == 4);

        fs_msg.DEVICE = MINOR(dev);
        fs_msg.BUF    = buf;
        fs_msg.CNT    = len;
        fs_msg.PROC_NR= src;
        assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
        send_recv(BOTH,dd_map[MAJOR(dev)].driver_nr, &fs_msg);
        assert(fs_msg.CNT == len);

        return fs_msg.CNT;
    }
    else 
    {
        assert(pin->i_mode == I_REGULAR || pin->i_mode == I_DIRECTORY);
        assert((fs_msg.type == READ) || (fs_msg.type == WRITE));

        int pos_end;
        if (fs_msg.type == READ)
            pos_end = min(pos+len,pin->i_size);
        else 
            pos_end = min(pos+len,pin->i_nr_sects * SECTOR_SIZE);  

        int off = pos % SECTOR_SIZE;/* 字节偏移 */
        int rw_sect_min = pin->i_start_sect + (pos >> SECTOR_SIZE_SHIFT);
        int rw_sect_max = pin->i_start_sect + (pos_end >> SECTOR_SIZE_SHIFT);

        int chunk = min(rw_sect_max - rw_sect_min + 1,FSBUF_SIZE >> SECTOR_SIZE_SHIFT);
        
        int bytes_rw = 0;  
        int bytes_left = len;
        int i;
        for (i=rw_sect_min; i<=rw_sect_max; i+=chunk)
        {
            int bytes = min(bytes_left,chunk * SECTOR_SIZE - off);
            /* 把文件内容读入内存 */
            rw_sector(DEV_READ,
                        pin->i_dev,
                        i * SECTOR_SIZE,/* 开始读取的位置 */
                        chunk * SECTOR_SIZE,/* 读取的数量 */
                        TASK_FS,fsbuf);
            
            if (fs_msg.type == READ)
            {
                /* 复制到MSG.BUF中 */
                phys_copy((void *)va2la(src,buf + bytes_rw),
                                (void *)va2la(TASK_FS,fsbuf + off),
                                bytes);
            }
            else 
            {
                /* 把要写入的内容写到缓冲中(fsbuf) */
                phys_copy((void *)va2la(TASK_FS,fsbuf + off),
                            (void *)va2la(src,buf + bytes_rw),
                            bytes);
                /* 写回扇区 */
                rw_sector(DEV_WRITE,
                            pin->i_dev,
                            i * SECTOR_SIZE,
                            chunk * SECTOR_SIZE,
                            TASK_FS,
                            fsbuf);
            }
            off = 0;
            bytes_rw += bytes;
            pcaller->filp[fd]->fd_pos += bytes;
            bytes_left -= bytes;
        }    
        /* write可能导致文件变大 */
        if (pcaller->filp[fd]->fd_pos > pin->i_size)
        {
            /* 根据fd的pos更新文件size */
            pin->i_size = pcaller->filp[fd]->fd_pos;

            sync_inode(pin);/* 更新inode节点在磁盘中的数据 */
        }

        return bytes_rw;/* 总共读写了多少数据 */
    }
}