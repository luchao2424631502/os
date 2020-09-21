/*----kliba.asm中的函数----*/
void    out_byte(u16 port,u8 value);
u8      in_byte(u16 port);
void    disp_str(char *info);
void    disp_color_str(char *info,int color);

void    disable_irq(int irq);
void    enable_irq(int irq);

void    disable_int();
void    enable_int();

void    port_read(u16 port,void *buf,int n);
void    port_write(u16 port,void *buf,int n);
void    glitter(int row,int col);

/* string.asm */
char *strcpy(char*,const char*);


/*----protect.c----*/
void    init_prot();       
u32     seg2phys(u16 seg); 
void    init_desc(struct descriptor*,u32,u32,u16);
u32     seg2linear(u16);

/*----klib.c----*/
void    get_boot_params(struct boot_params *pbp);
int     get_kernel_map(unsigned int *,unsigned int *);
void    delay(int time);
void    disp_int(int input);
char *itoa(char *str,int num);

/*----kernel.asm----*/
void restart();

/*----main.c----*/
void    Init();
int     get_ticks();
void    TestA();
void    TestB();
void    TestC();
void    panic(const char *fmt,...);

/*----i8259.c----*/
void    init_8259A();      
void put_irq_handler(int irq,irq_handler handler);
void spurious_irq(int irq);

/*----clock.c----*/
void clock_handler(int irq);
void init_clock();
void milli_delay(int milli_sec);

/* hd.c */
void task_hd();
void hd_handler(int irq);

/* keyboard.c */
void init_keyboard();
void keyboard_handler(int irq);
void keyboard_read(TTY *);

/* tty.c */
void task_tty();
void in_process(TTY *p_tty,u32 key);

/* systask.c */
void task_sys();

/* fs/main.c */
void task_fs();
int  rw_sector(int io_type,int dev,u64 pos,int bytes,int proc_nr,void* buf);
struct inode *get_inode(int dev,int num);
void put_inode(struct inode*pinode);
void sync_inode(struct inode *p);
struct super_block *get_super_block(int dev);

/* fs/open.c */
int do_open();
int do_close();
int do_lseek();

/* fs/read_write.c */
int do_rdwt();

/* fs/link.c */
int do_unlink();

/* fs/misc.c */
int do_stat();
int strip_path(char *,const char*,struct inode**);
int search_file(char *path);

/* mm/main.c */
void    task_mm();
int     alloc_mem(int pid,int memsize);
int     free_mem(int pid);

/* mm/forkexit.c */
int    do_fork();
void    do_exit(int status);
void    do_wait();

/* mm/exec.c */
int     do_exec();

/* console.c */
void out_char(CONSOLE *p_con,char ch);
void scroll_screen(CONSOLE *,int);
void select_console(int nr_console);
void init_screen(TTY *p_tty);
int is_current_console(CONSOLE *p_con);

/*proc.c(系统调用内核版本)*/
void    schedule();
void*	va2la(int pid, void* va);
int	ldt_seg_linear(struct proc* p, int idx);
void	reset_msg(MESSAGE* p);
void	dump_msg(const char * title, MESSAGE* m);
void	dump_proc(struct proc * p);
int	send_recv(int function, int src_dest, MESSAGE* msg);
void inform_int(int);

/* lib/misc.c */
void spin(char *func_name);

/*---------------------------系统调用相关-----------------------------------------------*/



/* 内核级别 */
int	sys_sendrec(int function, int src_dest, MESSAGE* m, struct proc* p);
int	sys_printx(int _unused1, int _unused2, char* s, struct proc * p_proc);/*syscall.asm(给用户提供的版本)*/

/* syscall.asm */
void    sys_call();        //kernel.asm提供内核API入口

/* 用户级 */
int     sendrec(int function,int src_dest,MESSAGE* p_msg);
int	    printx(char* str);       //API 0 

