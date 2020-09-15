/*内存操作函数集合*/
void*   memcpy(void *p_dst,void *p_scr,int size);
void    memset(void *p_dst,char ch,int size);
int     strlen(const char *p_str);

int     memcmp(const void *,const void *,int );
int     strcmp(const char *,const char *);
char    *strcat(char *,const char *);

// char*   strcpy(char *p_dst,char *p_src);

#define phys_copy   memcpy
#define phys_set    memset