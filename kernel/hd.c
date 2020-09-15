
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
#include "hd.h"

static void init_hd();
static void hd_open(int);
static void hd_close(int);
static void hd_rdwt(MESSAGE *);
static void hd_ioctl(MESSAGE *);
static void hd_cmd_out(struct hd_cmd* cmd);
static void get_part_table(int,int,struct part_ent *);
static void partition(int ,int );
static void print_hdinfo(struct hd_info*);
static int waitfor(int mask,int val,int timeout);
static void interrupt_wait();
static void hd_identify(int drive);
static void print_identify_info(u16* hdinfo);

static u8 hd_status;
static u8 hdbuf[SECTOR_SIZE * 2];
static struct hd_info hd_info[1];

#define DRV_OF_DEV(dev) (dev <= MAX_PRIM ? \
            dev / NR_PRIM_PER_DRIVE : \
            (dev - MINOR_hd1a) / NR_SUB_PER_DRIVE)


void task_hd()
{
    MESSAGE msg;

    init_hd();

    while(1)/* 让硬盘驱动程序处理更多消息(对磁盘的读写删除控制等) */
    {
        send_recv(RECEIVE,ANY,&msg);/* 等待从任何进程接受消息 */

        int src = msg.source;

        switch (msg.type)
        {
        case DEV_OPEN:/* 消息类型DEV_OPEN */
            hd_open(msg.DEVICE);
            break;
        case DEV_CLOSE:
            hd_close(msg.DEVICE);
            break;
        case DEV_READ:
        case DEV_WRITE:
            hd_rdwt(&msg);
            break;
        case DEV_IOCTL:
            hd_ioctl(&msg);
            break;
        default:
            dump_msg("HD driver::unknown msg", &msg);
			spin("FS::main_loop (invalid msg.type)");
            break;
        }
        send_recv(SEND,src,&msg);
    }
}

/* 初始化磁盘中断处理例程 */
static void init_hd()
{
    int i;
    u8 *pNrDrives = (u8 *)(0x475);
    printl("{HD} NrDrives:%d.\n",*pNrDrives);
    assert(*pNrDrives);

    put_irq_handler(AT_WINI_IRQ,hd_handler);
    enable_irq(CASCADE_IRQ);/* 级联从片的信号线也要打开 */
    enable_irq(AT_WINI_IRQ);/* AT温盘 */

    /* 初始化填充hd_info */
    for (i=0; i<(sizeof(hd_info) / sizeof(hd_info[0])); i++)
        memset(&hd_info[i],0,sizeof(hd_info[0]));
    hd_info[0].open_cnt = 0;
}

/* 传入的参数是次设备号码 */
static void hd_open(int device)
{
    int drive = DRV_OF_DEV(device);/* 根据设备号得到对应驱动 */
    assert(drive == 0);

    hd_identify(drive);

    if (hd_info[drive].open_cnt++ == 0)
    {
        partition(drive * (NR_PART_PER_DRIVE + 1),P_PRIMARY);
        print_hdinfo(&hd_info[drive]);
    }
}

/*  */
static void hd_close(int device)
{
    int drive = DRV_OF_DEV(device);
    assert(drive == 0);

    hd_info[drive].open_cnt--;
}

/*  */
static void hd_rdwt(MESSAGE *p)
{
    int drive = DRV_OF_DEV(p->DEVICE);

    u64 pos = p->POSITION;
    assert((pos >> SECTOR_SIZE_SHIFT) < (1 << 31));

    assert((pos & 0x1FF) == 0);

    /* 根据扇区号得到读写的物理地址*/
    u32 sect_nr     = (u32)(pos >> SECTOR_SIZE_SHIFT);
    int logidx      = (p->DEVICE - MINOR_hd1a) % NR_SUB_PER_DRIVE;
    sect_nr         += p->DEVICE < MAX_PRIM ?
                        hd_info[drive].primary[p->DEVICE].base :
                        hd_info[drive].logical[logidx].base;
    struct hd_cmd cmd;
    cmd.features    = 0;
    cmd.count       = (p->CNT + SECTOR_SIZE - 1) / SECTOR_SIZE;
    cmd.lba_low     = sect_nr & 0xFF;
    cmd.lba_mid     = (sect_nr >> 8) & 0xFF;
    cmd.lba_high    = (sect_nr >> 16) & 0xFF;
    cmd.device      = MAKE_DEVICE_REG(1,drive,(sect_nr >> 24) & 0xF);
    cmd.command     = (p->type == DEV_READ) ? ATA_READ : ATA_WRITE;
    hd_cmd_out(&cmd);

    int bytes_left  = p->CNT;
    void *la        = (void *)va2la(p->PROC_NR,p->BUF);

    while (bytes_left)
    {
        int bytes   = min(SECTOR_SIZE,bytes_left);
        if (p->type == DEV_READ)
        {
            interrupt_wait();
            port_read(REG_DATA,hdbuf,SECTOR_SIZE);
            phys_copy(la,(void *)va2la(TASK_HD,hdbuf),bytes);
        }
        else 
        {
            /* 并没有等到 数据可以写的通知 */
            if (!waitfor(STATUS_DRQ,STATUS_DRQ,HD_TIMEOUT))
                panic("hd writing error .");
            port_write(REG_DATA,la,bytes);
            interrupt_wait();
        }
        bytes_left  -= SECTOR_SIZE;
        la          += SECTOR_SIZE;
    }
}

static void hd_ioctl(MESSAGE *p)
{
    int device  = p->DEVICE;
    int drive   = DRV_OF_DEV(device);

    struct hd_info *hdi = &hd_info[drive];

    if (p->REQUEST == DIOCTL_GET_GEO)
    {
        void *dst   = va2la(p->PROC_NR,p->BUF);
        void *src   = va2la(TASK_HD,
                        device < MAX_PRIM ?
                        &hdi->primary[device] :
                        &hdi->logical[(device - MINOR_hd1a) % NR_SUB_PER_DRIVE]);
        phys_copy(dst,src,sizeof(struct part_info));    
    }
    else 
    {
        assert(0);
    }
}

/* 得到分区表 */
static void get_part_table(int drive,int sect_nr,struct part_ent *entry)
{
    struct hd_cmd cmd;
    cmd.features    = 0;
    cmd.count     = 1;
    cmd.lba_low     = sect_nr & 0xFF;
    cmd.lba_mid     = (sect_nr >> 8) & 0xFF;
    cmd.lba_high    = (sect_nr >> 16) & 0xFF;
    cmd.device      = MAKE_DEVICE_REG(1,drive,(sect_nr >> 24) & 0xF);
    cmd.command     = ATA_READ;

    hd_cmd_out(&cmd);
    interrupt_wait();

    port_read(REG_DATA,hdbuf,SECTOR_SIZE);
    memcpy(entry,
            hdbuf + PARTITION_TABLE_OFFSET,
            sizeof(struct part_ent) * NR_PART_PER_DRIVE); 
}

/* 当设备被打开,此函数读取分区表并且填充hd_info结构 */
static void partition(int device,int style)
{
    int i;
    int drive = DRV_OF_DEV(device);
    struct hd_info *hdi = &hd_info[drive];

    struct part_ent part_tbl[NR_SUB_PER_DRIVE];/* 最多可能有64个扩展分区 */

    if (style == P_PRIMARY)
    {
        get_part_table(drive,drive,part_tbl);
        
        int nr_prim_parts = 0;
        for (i=0; i<NR_PART_PER_DRIVE; i++)/* 主设备1~4所有分区 */
        {
            if (part_tbl[i].sys_id == NO_PART)
                continue;
            
            nr_prim_parts++;
            int dev_nr = i + 1;
            hdi->primary[dev_nr].base = part_tbl[i].start_sect;
            hdi->primary[dev_nr].size = part_tbl[i].nr_sects;

            if (part_tbl[i].sys_id == EXT_PART)/* 是扩展分区 */
                partition(device + dev_nr,P_EXTENDED);
        }
        assert(nr_prim_parts != 0);
    }
    else if (style == P_EXTENDED)
    {
        int j = device % NR_PRIM_PER_DRIVE;
        int ext_start_sect = hdi->primary[j].base;
        int s = ext_start_sect;
        int nr_1st_sub = (j - 1) * NR_SUB_PER_PART;/*0/16/32/48*/

        /* 0~15/16~31/32~47/48~63*/
        for (i=0; i < NR_SUB_PER_PART; i++)
        {
            int dev_nr = nr_1st_sub + i;
            
            get_part_table(drive,s,part_tbl);

            hdi->logical[dev_nr].base = s + part_tbl[0].start_sect;
            hdi->logical[dev_nr].size = part_tbl[0].nr_sects;

            s = ext_start_sect + part_tbl[1].start_sect;

            /* 此扩展分区中逻辑分区走完了 */
            if (part_tbl[1].sys_id == NO_PART)
                break;
        }
    }
    else 
    {
        assert(0);
    }
}

/* 打印磁盘分区信息 */
static void print_hdinfo(struct hd_info *hdi)
{
    int i;
    for (i=0; i < NR_PART_PER_DRIVE + 1; i++)
    {
        printl("{HD} %sPART_%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
		       i == 0 ? " " : "     ",
		       i,
		       hdi->primary[i].base,
		       hdi->primary[i].base,
		       hdi->primary[i].size,
		       hdi->primary[i].size);
    }
    for (i=0; i < NR_SUB_PER_DRIVE; i++)
    {
        if (hdi->logical[i].size == 0)
            continue;
        printl("{HD}         "
		       "%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
		       i,
		       hdi->logical[i].base,
		       hdi->logical[i].base,
		       hdi->logical[i].size,
		       hdi->logical[i].size);    
    }
}

/* 得到磁盘的信息 */
static void hd_identify(int drive)
{
    struct hd_cmd cmd;
    cmd.device = MAKE_DEVICE_REG(0,drive,0);
    cmd.command= ATA_IDENTIFY;
    hd_cmd_out(&cmd);
    interrupt_wait();/* 等待中断产生 */
    port_read(REG_DATA,hdbuf,SECTOR_SIZE);

    /* 打印磁盘信息 */
    print_identify_info((u16 *)hdbuf);

    u16* hdinfo = (u16*)hdbuf;

    hd_info[drive].primary[0].base = 0;
    hd_info[drive].primary[0].size = ((int)hdinfo[61] << 16) + hdinfo[60];
}

/*  */
static void print_identify_info(u16* hdinfo)
{
	int i, k;
	char s[64];

	struct iden_info_ascii {
		int idx;
		int len;
		char * desc;
	} iinfo[] = {{10, 20, "HD SN"}, /* Serial number in ASCII */
		     {27, 40, "HD Model"} /* Model number in ASCII */ };

	for (k = 0; k < sizeof(iinfo)/sizeof(iinfo[0]); k++) {
		char * p = (char*)&hdinfo[iinfo[k].idx];
		for (i = 0; i < iinfo[k].len/2; i++) {
			s[i*2+1] = *p++;
			s[i*2] = *p++;
		}
		s[i*2] = 0;
		printl("{HD} %s: %s\n", iinfo[k].desc, s);
	}

	int capabilities = hdinfo[49];
	printl("{HD} LBA supported: %s\n",
	       (capabilities & 0x0200) ? "Yes" : "No");

	int cmd_set_supported = hdinfo[83];
	printl("{HD} LBA48 supported: %s\n",
	       (cmd_set_supported & 0x0400) ? "Yes" : "No");

	int sectors = ((int)hdinfo[61] << 16) + hdinfo[60];
	printl("{HD} HD size: %dMB\n", sectors * 512 / 1000000);
}

static void hd_cmd_out(struct hd_cmd* cmd)
{
    /* 对于所有命令，主机必须首先检查bsy = 1，
    并且不应该继续进行，除非且直到bsy = 0 */
    if (!waitfor(STATUS_BSY,0,HD_TIMEOUT))
        panic("hd error");

    /* Activate the Interrupt Enable (nIEN) bit */
	out_byte(REG_DEV_CTRL, 0);
	/* Load required parameters in the Command Block Registers */
	out_byte(REG_FEATURES, cmd->features);
	out_byte(REG_NSECTOR,  cmd->count);
	out_byte(REG_LBA_LOW,  cmd->lba_low);
	out_byte(REG_LBA_MID,  cmd->lba_mid);
	out_byte(REG_LBA_HIGH, cmd->lba_high);
	out_byte(REG_DEVICE,   cmd->device);
	/* Write the command code to the Command Register */
	out_byte(REG_CMD,     cmd->command);
}

/* Ring 1: 等待直到磁盘中断发生 */
static void interrupt_wait()
{
    MESSAGE msg;
    send_recv(RECEIVE,INTERRUPT,&msg);
}

static int waitfor(int mask,int val,int timeout)
{
    int t = get_ticks();

    while(((get_ticks() - t) * 1000/HZ) < timeout)
    {
        if ((in_byte(REG_STATUS) & mask) == val)
            return 1;
    }
    return 0;
}

/* 磁盘中断处理过程 */
void hd_handler(int irq)
{
    hd_status = in_byte(REG_STATUS);/* 读取状态寄存器来恢复i中断响应 */

    inform_int(TASK_HD);/* 通知驱动程序 */
}