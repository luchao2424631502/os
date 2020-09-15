#ifdef GLOBAL_VARIABLES_HERE
#undef  EXTERN      //取消本来的define定义(具体定义在global.c文件中)
#define EXTERN
#endif 

EXTERN int          ticks;

/*如果定义GLOBAL_VARIBALES_HERE,这里是定义全局变量,如果没,这里是声明引入全局变量*/
EXTERN int          disp_pos;//在const.h中define了disp_pos
EXTERN u8           gdt_ptr[6];
EXTERN struct descriptor   gdt[GDT_SIZE];
/*之前这些都定义在原start.c中,现在全部移动到此处,新增idt定义*/
EXTERN u8           idt_ptr[6];
EXTERN struct gate         idt[IDT_SIZE];

EXTERN u32          k_reenter;/*中断例程计数 (暴力)解决重入问题*/
/* 此变量用来记录当前的控制台是哪一个 */
EXTERN int          current_console;

EXTERN int          key_pressed;

EXTERN struct tss          tss;
EXTERN struct proc*     p_proc_ready;

extern char         task_stack[];
extern struct proc      proc_table[];
extern struct task        task_table[];
extern struct task       user_proc_table[];
extern irq_handler  irq_table[];
extern TTY          tty_table[];
extern CONSOLE      console_table[];

/* MM */
EXTERN MESSAGE      mm_msg;
extern u8*          mmbuf;/* 声明一下已经定义好的常量 */
extern const int    MMBUF_SIZE;
EXTERN int          memory_size;

/* FS */
EXTERN struct file_desc     f_desc_table[NR_FILE_DESC];
EXTERN struct inode         inode_table[NR_INODE];
EXTERN struct super_block   super_block[NR_SUPER_BLOCK];
extern u8                   *fsbuf;
extern const int            FSBUF_SIZE;
EXTERN MESSAGE              fs_msg;
EXTERN struct proc          *pcaller;
EXTERN struct inode         *root_inode;
extern struct dev_drv_map   dd_map[];

/* for test only */
extern char *               logbuf;
extern const int            LOGBUF_SIZE;
extern char *               logdiskbuf;
extern const int            LOGDISKBUF_SIZE;

