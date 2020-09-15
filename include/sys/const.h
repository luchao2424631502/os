#ifndef _ORANGES_CONST_H_
#define _ORANGES_CONST_H_

#define max(a,b)        ((a) > (b) ? (a) : (b))
#define min(a,b)        ((a) < (b) ? (a) : (b))

/* color */
#define BLACK       0x0 
#define WHITE       0x7
#define RED         0x4 
#define GREEN       0x2
#define BLUE        0x1 
#define FLASH       0x80
#define BRIGHT      0x08
#define MAKE_COLOR(x,y) ((x<<4)|y)

//去掉没有必要的public 和 private
#define GDT_SIZE    128
#define IDT_SIZE    256

//权限
#define PRIVILEGE_KRNL  0
#define PRIVILEGE_TASK  1
#define PRIVILEGE_USER  3

/*RPL*/
#define RPL_KRNL        SA_RPL0
#define RPL_TASK        SA_RPL1
#define RPL_USER        SA_RPL3

/* Process 状态*/
#define SENDING         0x02
#define RECEIVING       0x04
#define WAITING         0x08 /* 进程等待子进程结束 */
#define HANGING         0x10 /* 进程退出不需要父进程等待 */
#define FREE_SLOT       0x20 /*在进程表中是空的等待被fork利用 */

/* TTY */
#define NR_CONSOLES     3

/*  8259A端口  */
#define INT_M_CTL       0x20    //主片IO端口
#define INT_M_CTLMASK   0x21    
#define INT_S_CTL       0xA0    //从片IO端口
#define INT_S_CTLMASK   0xA1

/* 8253 PIT端口 */
#define TIMER0          0x40
#define TIMER_MODE      0x43
#define RATE_GENERATOR  0x34    /* Oranges's所用的8253模式 */
#define TIMER_FREQ      1193182L/* 默认定时器的频率 */
#define HZ              100     /* 100Hz=每秒100个时钟中断 = 10ms一个时钟中断 */

/* AT 键盘,8042端口 */
#define KB_DATA         0x60
#define KB_CMD          0x64

/* 大小写等3个键位 */
#define LED_CODE        0xED
#define KB_ACK          0xFA

/* VGA */
#define CRTC_ADDR_REG   0x3D4
#define CRTC_DATA_REG   0x3D5
#define START_ADDR_H    0xC
#define START_ADDR_L    0xD
#define CURSOR_H        0xE
#define CURSOR_L        0xF
#define V_MEM_BASE      0xB8000     //显存线性地址
#define V_MEM_SIZE      0x8000      //32k

#define CLK_ELE         0x70
#define CLK_IO          0x71

#define YEAR            9
#define MONTH           8
#define DAY             7
#define HOUR            4
#define MINUTE          2
#define SECOND          0
#define CLK_STATUS      0x0B
#define CLK_HEALTH      0x0E


//硬件中断
#define NR_IRQ          16
#define CLOCK_IRQ       0
#define KEYBOARD_IRQ    1
#define CASCADE_IRQ     2
#define ETHER_IRQ       3       //以太网(串口2)
#define SECONDARY_IRQ   3
#define RS232_IRQ       4       //串口1
#define XT_WINI_IRQ     5       //LPT并行端口2
#define FLOPPY_IRQ      6
#define PRINTER_IRQ     7       //LPT并行端口1
#define AT_WINI_IRQ     14

/* task */
#define INVALID_DRIVER  -20
#define INTERRUPT       -10 
#define TASK_TTY        0
#define TASK_SYS        1
#define TASK_HD         2
#define TASK_FS         3
#define TASK_MM         4
#define INIT            5


#define ANY             (NR_TASKS + NR_PROCS + 10)
#define NO_TASK         (NR_TASKS + NR_PROCS + 20)

#define MAX_TICKS       0x7FFFABCD

//系统调用的数量
#define NR_SYS_CALL     3       /* 添加系统调用时一定要记住修改 */

/* IPC */
#define SEND            1
#define RECEIVE         2
#define BOTH            3

#define MAG_CH_PANIC    '\002'
#define MAG_CH_ASSERT   '\003'

/* 内核发送消息类型 */
enum msgtype{
        HARD_INT = 1,

        /* SYS task */
        GET_TICKS,GET_PID,GET_RTC_TIME,

        /* FS */
        OPEN,CLOSE,READ,WRITE,LSEEK,STAT,UNLINK,

        /* FS & TTY */
        SUSPEND_PROC,RESUME_PROC,

        /* MM */
        EXEC,WAIT,

        /* FS & MM */
        FORK,EXIT,

        /* TTY,SYS,FS,MM, 服务例程消息 */
        SYSCALL_RET,

        /* 驱动消息 */
        DEV_OPEN = 1001,
        DEV_CLOSE,
        DEV_READ,
        DEV_WRITE,
        DEV_IOCTL,

        /* debug用(暂时没用到) */
        DISK_LOG
};



#define FD              u.m3.m3i1
#define PATHNAME        u.m3.m3p1
#define FLAGS           u.m3.m3i1
#define NAME_LEN        u.m3.m3i2

#define CNT             u.m3.m3i2
#define REQUEST         u.m3.m3i2
#define PROC_NR         u.m3.m3i3
#define DEVICE          u.m3.m3i4
#define POSITION        u.m3.m3l1
#define BUF             u.m3.m3p2
#define OFFSET          u.m3.m3i2
#define WHENCE          u.m3.m3i3

#define PID             u.m3.m3i2
#define STATUS          u.m3.m3i1
#define RETVAL          u.m3.m3i1


/* -------------------- */
#define DIOCTL_GET_GEO  1

/* Hard Drive */
#define SECTOR_SIZE     512
#define SECTOR_BITS     (SECTOR_SIZE * 8)
#define SECTOR_SIZE_SHIFT       9


/* 主要设备号 */
#define	NO_DEV			0
#define	DEV_FLOPPY		1
#define	DEV_CDROM		2
#define	DEV_HD			3
#define	DEV_CHAR_TTY		4
#define	DEV_SCSI		5

/*  */
#define MAJOR_SHIFT             8
#define MAKE_DEV(a,b)           ((a << MAJOR_SHIFT) | b)

#define MAJOR(x)                ((x >> MAJOR_SHIFT) & 0xFF)
#define MINOR(x)                (x & 0xFF)

#define INVALID_INODE           0
#define ROOT_INODE              1

#define	MAX_DRIVES		2
#define	NR_PART_PER_DRIVE	4
#define	NR_SUB_PER_PART		16
#define	NR_SUB_PER_DRIVE	(NR_SUB_PER_PART * NR_PART_PER_DRIVE)
#define	NR_PRIM_PER_DRIVE	(NR_PART_PER_DRIVE + 1)

#define	MAX_PRIM		(MAX_DRIVES * NR_PRIM_PER_DRIVE - 1)

#define	MAX_SUBPARTITIONS	(NR_SUB_PER_DRIVE * MAX_DRIVES)

/* 硬盘的设备号 */
#define MINOR_hd1a      0x10
#define MINOR_hd2a      (MINOR_hd1a + NR_SUB_PER_PART)
#define ROOT_DEV                MAKE_DEV(DEV_HD,MINOR_BOOT)

#define	P_PRIMARY	0
#define	P_EXTENDED	1

#define ORANGES_PART	0x99	/* Orange'S partition */
#define NO_PART		0x00	/* unused entry */
#define EXT_PART	0x05	/* extended partition */

#define	NR_FILES	64
#define	NR_FILE_DESC	64	/* FIXME */
#define	NR_INODE	64	/* FIXME */
#define	NR_SUPER_BLOCK	8


/* INODE::i_mode (octal, lower 32 bits reserved) */
#define I_TYPE_MASK     0170000
#define I_REGULAR       0100000
#define I_BLOCK_SPECIAL 0060000
#define I_DIRECTORY     0040000
#define I_CHAR_SPECIAL  0020000
#define I_NAMED_PIPE	0010000

#define	is_special(m)	((((m) & I_TYPE_MASK) == I_BLOCK_SPECIAL) ||	\
			 (((m) & I_TYPE_MASK) == I_CHAR_SPECIAL))

#define	NR_DEFAULT_FILE_SECTS	2048 /* 2048 * 512 = 1MB */

#endif 