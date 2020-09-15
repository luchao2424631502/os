    BS_OEMName      db 'ForrestY'
    BPB_BytsPerSec  dw 512          ;每扇区字节数
    BPB_SecPerClus  db 1            ;每簇多少扇区
    BPB_RsvdSecCnt  dw 1            ;Boot占据扇区数
    BPB_NumFATs     db 2            ;FAT表个数
    BPB_RootEntCnt  dw 224          ;根目录文件最大数 0E0h
    BPB_TotSec16    dw 2880         ;逻辑扇区总数量 1.44MB
    BPB_Media       db 0XF0         ;媒体描述符
    BPB_FATSz16     dw 9            ;FAT扇区数量
    BPB_SecPerTrk   dw 18           ;每个磁道扇区数
    BPB_NumHeads    dw 2            ;磁头数
    BPB_HiddSec     dd 0            ;隐藏扇区数
    BPB_TotSec32    dd 0
    BS_DrvNum       db 0            ;中断13的驱动器号
    BS_Reserved1    db 0            ;未使用
    BS_BootSig      db 29h          ;扩展引导标记 29h
    BS_VolID        dd 0            ;卷 序列号
    BS_VolLab       db 'OrangeS0.02';卷标 必须11字节
    BS_FileSysType  db 'FAT12   '   ;文件系统类型,必须8byte


;比原boot.asm新增FATsz 
FATSz                   equ 9
RootDirSectors          equ 14      ;根目录大小
SectorNoOfRootDirectory equ 19      ;根目录起始扇区号
SectorNoOfFAT1          equ 1       ;FAT1表 占据开始扇区号
DeltaSectorNo           equ 17      ;用来计算FAT簇对应的数据簇的扇区号
