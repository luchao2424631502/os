
/* 保留一些扇区供我们在其中复制tar文件，
该文件将被os提取并使用 */
#define INSTALL_START_SECT  0x8000/* cmd.tar使用的首扇区号 */
#define INSTALL_NR_SECTS    0x800/* cmd.tar最大1MB */

#define	MINOR_BOOT			MINOR_hd2a

/* loader.asm中保存的信息 */
#define BOOT_PARAM_ADDR     0x900
#define BOOT_PARAM_MAGIC    0xB007
#define BI_MAG              0
#define BI_MEM_SIZE         1
#define BI_KERNEL_FILE      2

/*
 * disk log
 */
#define ENABLE_DISK_LOG
#define SET_LOG_SECT_SMAP_AT_STARTUP
#define MEMSET_LOG_SECTS
#define	NR_SECTS_FOR_LOG		NR_DEFAULT_FILE_SECTS