; BaseOfLoader            equ 09000h  ;Loader加载到的段地址
; OffsetOfLoader          equ 0100h

; BaseOfLoaderPhyAddr     equ BaseOfLoader*10h

; BaseOfKernelFile        equ 08000h
; OffsetOfKernelFile      equ 0h

; BaseOfKernelFilePhyAddr equ BaseOfKernelFile * 10h
; KernelEntryPointPhyAddr equ 030400h
;             ;kernel -Ttext 030400h,指定了内核的入口地址,
; PageDirBase		equ	200000h	; 页目录开始地址:		2M
; PageTblBase		equ	201000h	; 页表开始地址:			2M + 4K

;---------------------------------------------------
;重新放置可执行内核
KRNL_ENT_PT_PHY_ADDR    equ     0x1000

;页目录地址,+4kb后是页表地址
PAGE_DIR_BASE           equ     0x100000
PAGE_TBL_BASE           equ     0x101000

;loader.bin被加载到的物理地址
LOADER_SEG              equ     0x9000
LOADER_OFF              equ     0x100
LOADER_PHY_ADDR         equ     LOADER_SEG * 0x10

;内核文件kernel.bin放置的地方
KERNEL_FILE_SEG         equ     0x7000
KERNEL_FILE_OFF         equ     0
KERNEL_FILE_PHY_ADDR    equ     KERNEL_FILE_SEG * 0x10

;为kernel.bin保留的字节
KERNEL_VALID_SPACE      equ     LOADER_PHY_ADDR - KERNEL_FILE_PHY_ADDR

SUPER_BLK_SEG           equ     0x70

;供klib.c函数使用
BOOT_PARAM_ADDR         equ     0x900
BOOT_PARAM_MAGIC        equ     0xB007

ROOT_BASE               equ     0x4EFF

;fs.h
SB_MAGIC_V1		equ	0x111
SB_MAGIC		equ	4 *  0
SB_NR_INODES		equ	4 *  1
SB_NR_SECTS		equ	4 *  2
SB_NR_IMAP_SECTS	equ	4 *  3
SB_NR_SMAP_SECTS	equ	4 *  4
SB_N_1ST_SECT		equ	4 *  5
SB_NR_INODE_SECTS	equ	4 *  6
SB_ROOT_INODE		equ	4 *  7
SB_INODE_SIZE		equ	4 *  8
SB_INODE_ISIZE_OFF	equ	4 *  9
SB_INODE_START_OFF	equ	4 * 10
SB_DIR_ENT_SIZE		equ	4 * 11
SB_DIR_ENT_INODE_OFF	equ	4 * 12
SB_DIR_ENT_FNAME_OFF	equ	4 * 13