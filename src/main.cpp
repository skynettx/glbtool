#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>
#endif // _WIN32
#ifdef __linux__
#include <sys/io.h>
#endif // __linux__
#ifdef __GNUC__
#include <unistd.h>
char* strupr(char* s)
{
    char* tmp = s;

    for (; *tmp; ++tmp) {
        *tmp = toupper((unsigned char)*tmp);
    }

    return s;
}
#endif
#ifdef _MSC_VER
#include <windows.h>
#define PATH_MAX MAX_PATH
#define strupr _strupr
#define access _access
#endif

char filename[20];
char searchname[20];
int itemcount = 0;
int itemtotal = 0;
int listflag = 0;
int listallflag = 0;
int extractflag = 0;
int searchflag = 0;
int searchnumber = -1;

FILE* infile;

const char* serial = "32768GLB";

struct meminfo_t
{
    char* ptr;
    uint32_t age;
};

struct fitem_t {
    char name[16];
    meminfo_t mem;
    int length;
    int offset;
    int flags;
    int lock;
};

struct filedesc_t {
    char path[PATH_MAX];
    fitem_t* items;
    int itemcount;
    FILE* handle;
    const char* mode;
};

int num_glbs = 1;

#define GLBMAXFILES 15

filedesc_t filedesc[GLBMAXFILES];

#pragma pack(push, 1)
struct glbitem_t {
    int crypt;
    int offset;
    int length;
    char name[16];
};
#pragma pack(pop)

static inline void EXIT_Error(const char* a1, ...)
{
    exit(0);
}

int CheckStrDigit(char* string)
{
    int i;
    size_t len = strlen(string);

    for (i = 0; i < len; i++)
    {
        if (!isdigit(string[i]))
            return 0;
    }

    return 1;
}

void RemoveCharFromString(char* p, char c)
{
    if (NULL == p)
        return;
    
    char* pDest = p;

    while (*p)
    {
        if (*p != c)
            *pDest++ = *p;

        p++;
    }
    
    *pDest = '\0';
}

void GLB_DeCrypt(const char* key, void* buf, int size)
{
    char* data = (char*)buf;
    int keylen, vbx;
    char vc, vd;
    
    keylen = strlen(key);
    vbx = 25 % keylen;
    vc = key[vbx];
    
    while (size--)
    {
        vd = *data;
        *data++ = (vd - key[vbx] - vc) % 256;
        vc = vd;
        
        if (++vbx >= keylen)
            vbx = 0;
    }
}

FILE* GLB_OpenFile(int refail, int filenum, const char* mode)
{
    FILE* h;
    char buffer[PATH_MAX];
    
    sprintf(buffer, filename);
    h = fopen(buffer, mode);
    
    if (h == NULL)
    {
        printf("Input file not found\n");
        EXIT_Error("Input file not found");
    }
    
    strcpy(filedesc[filenum].path, buffer);
    filedesc[filenum].mode = mode;
    filedesc[filenum].handle = h;
    
    return h;
}

int GLB_NumItems(int filenum)
{
    glbitem_t head;
    
    infile = GLB_OpenFile(1, filenum, "rb");

    if (infile == NULL)
        return 0;
    
    fseek(infile, 0, SEEK_SET);
    
    if (!fread(&head, sizeof(head), 1, infile))
        EXIT_Error("GLB_NumItems: Read failed!");
    
    GLB_DeCrypt(serial, &head, sizeof(head));
    
    return head.offset;
}

void GLB_LoadIDT(filedesc_t* fd)
{
    glbitem_t buf[10];
    fitem_t* ve;
    int i, j, k;
    
    ve = fd->items;
    fseek(infile, sizeof(glbitem_t), SEEK_SET);
    
    for (i = 0; i < fd->itemcount; i += j)
    {
        j = fd->itemcount - i;
        
        if ((unsigned int)j > 10)
            j = 10;
        
        fread(buf, sizeof(glbitem_t), j, infile);
        
        for (k = 0; k < j; k++)
        {
            GLB_DeCrypt(serial, &buf[k], sizeof(glbitem_t));
            
            if (buf[k].crypt == 1)
                ve->flags |= 0x40000000;
            
            ve->length = buf[k].length;
            ve->offset = buf[k].offset;
            memcpy(ve->name, buf[k].name, 16);
            ve++;
        }
    }
}

void GLB_InitSystem(void)
{
    int i, j;
    filedesc_t* fd;
    
    memset(filedesc, 0, sizeof(filedesc));

    for (i = 0; i < num_glbs; i++)
    {
        fd = &filedesc[i];
        j = GLB_NumItems(i);

        if (j)
        {
            fd->itemcount = j;
            fd->items = (fitem_t*)calloc(j, sizeof(fitem_t));
            
            if (!fd->items)
            {
                fclose(infile);

                printf("Input file format not valid\n");
                EXIT_Error("Input file format not valid");
            }
            
            GLB_LoadIDT(fd);
        }
    }
}

int GLB_Load(char* inmem, int filenum, int itemnum)
{
    fitem_t* fi;
    FILE* handle = filedesc[filenum].handle;
    
    if (!handle)
        return 0;

    fi = &filedesc[filenum].items[itemnum];
    
    if (inmem)
    {
        if (fi->mem.ptr && inmem != fi->mem.ptr)
        {
            memcpy(inmem, fi->mem.ptr, fi->length);
        }
        else
        {
            fseek(handle, fi->offset, SEEK_SET);
            fread(inmem, fi->length, 1, handle);
            if (fi->flags & 0x40000000)
                GLB_DeCrypt(serial, inmem, fi->length);
        }
    }
    
    return fi->length;
}

char* GLB_FetchItem(int handle, int mode)
{
    char* m;
    fitem_t* fi;
    
    if (handle == -1)
        return NULL;
    
    uint16_t f = (handle >> 16) & 0xffff;
    uint16_t n = (handle >> 0) & 0xffff;
    fi = &filedesc[f].items[n];
    
    if (mode == 2)
        fi->flags |= 0x80000000;
    
    if (!fi->mem.ptr)
    {
        fi->lock = 0;
        
        if (fi->length)
        {
            m = (char*)calloc(fi->length, 1);
            
            if (mode == 2)
                fi->lock = 1;
            fi->mem.ptr = m;
            
            if (fi->mem.ptr)
            {
                GLB_Load(fi->mem.ptr, f, n);
            }
        }
    }
  
    return fi->mem.ptr;
}

char* GLB_GetItem(int handle)
{
    return GLB_FetchItem(handle, 1);
}

void GLB_FreeAll(void)
{
    int i, j;
    fitem_t* fi;
    for (i = 0; i < num_glbs; i++)
    {
        fi = filedesc[i].items;
        for (j = 0; j < filedesc[i].itemcount; j++)
        {
            if (fi->mem.ptr && !(fi->flags & 0x80000000))
            {
                free(fi->mem.ptr);
                fi->mem.ptr = NULL;
            }
            fi++;
        }
    }
}

void GLB_Extract()
{
    fitem_t* fi;
    int fc;
    int i, j;
    int foundflag = 0;
    FILE* outputfile;
    char* buffer;
    char dup[16];

    for (i = 0; i < num_glbs; i++)
    {
        fi = filedesc[i].items;
        fc = filedesc[i].itemcount;

        for (j = 0; j < fc; j++, fi++)
        {
            if (strcmp(fi->name, searchname) == 0 || j == searchnumber)
            {
                foundflag = 1;
                buffer = GLB_GetItem(j);

                if (!access(fi->name, 0))
                {
                    sprintf(dup, "_%03x", j);
                    strcat(fi->name, dup);
                }
                
                RemoveCharFromString(fi->name, '/');
                outputfile = fopen(fi->name, "wb");

                if (outputfile && buffer)
                {
                    fwrite(buffer, fi->length, 1, outputfile);
                    free(buffer);
                    fclose(outputfile);
                }
                
                if (searchflag && fi->name[0] != '\0')
                {
                    printf("Extracting item: %s\n", fi->name);
                    itemtotal++;
                }
            }

            if (!searchflag)
            {
                buffer = GLB_GetItem(j);

                if (!access(fi->name, 0))
                {
                    sprintf(dup, "_%03x", j);
                    strcat(fi->name, dup);
                }
                
                RemoveCharFromString(fi->name, '/');
                outputfile = fopen(fi->name, "wb");
                    
                if (outputfile && buffer)
                {
                    fwrite(buffer, fi->length, 1, outputfile);
                    free(buffer);
                    fclose(outputfile);
                }
                
                if (fi->name[0] != '\0')
                    printf("Extracting item: %s\n", fi->name);
            }
            
            if (fi->name[0] != '\0' && !searchflag)
                itemtotal++;
        }
    }
    
    if (searchflag && !foundflag)
        printf("Item not found\n");
}

void GLB_List()
{
    fitem_t* fi;
    int fc;
    int i, j;
    int foundflag = 0;

    for (i = 0; i < num_glbs; i++)
    {
        fi = filedesc[i].items;
        fc = filedesc[i].itemcount + itemcount;
        
        for (j = itemcount; j < fc; j++, fi++)
        {
            if (fi->name[0] != '\0' && !listallflag)
            {
                if ((strcmp(fi->name, searchname) == 0 && searchflag) || (j == searchnumber && searchflag))
                {
                    foundflag = 1;
                    printf("ItemNum Dec: %02d, ItemNum Hex: 0x%02x, Size: %d Byte, ItemName: %s\n", j, j, fi->length, fi->name);
                    itemtotal++;
                }
               
                if (!searchflag)
                {
                    printf("ItemNum Dec: %02d, ItemNum Hex: 0x%02x, Size: %d Byte, ItemName: %s\n", j, j, fi->length, fi->name);
                    itemtotal++;
                }
            }
            
            if (listallflag)
            {
                if ((strcmp(fi->name, searchname) == 0  && searchflag) || (j == searchnumber && searchflag))
                {
                    foundflag = 1;
                    printf("ItemNum Dec: %02d, ItemNum Hex: 0x%02x, Size: %d Byte, ItemName: %s\n", j, j, fi->length, fi->name);
                    itemtotal++;
                }

                if (!searchflag)
                {
                    printf("ItemNum Dec: %02d, ItemNum Hex: 0x%02x, Size: %d Byte, ItemName: %s\n", j, j, fi->length, fi->name);
                    itemtotal++;
                }
            }
        }
    }

    if (searchflag && !foundflag)
        printf("Item not found\n");
}

int main(int argc, char** argv)
{
    const char* help = "-h";
    const char* extract = "-x";
    const char* list = "-l";
    const char* listall = "-la";
    
    printf("********************************************************************************\n"
           " GLB Tool for Raptor GLB Files                                                  \n"
           "********************************************************************************\n");

    if (!argv[1])
    {
        printf("Usage: -h for help\n");
        
        return 0;
    }
    
    if (strcmp (argv[1], help) == 0)
    {
        printf("-x  Extract items from <INPUTFILE.GLB>\n"
               "    optional <SearchItemNameNumber> only extract found items\n"
               "-l  List items from <INPUTFILE.GLB>\n" 
               "    optional <FILENUMBER> for correct item numbers in files > FILE0000.GLB\n"
               "    optional <SearchItemNameNumber> only list found items\n"
               "-la List complete content (with markers) from <INPUTFILE.GLB>\n"
               "    optional <FILENUMBER> for correct item numbers in files > FILE0000.GLB\n"
               "    optional <SearchItemNameNumber> only list found items\n"
               "-h  Show this help\n");
        
        return 0;
    }
    
    if (strcmp(argv[1], extract) == 0)
    {
        extractflag = 1;
        
        if (argc == 4)
        {
            searchflag = 1;
            searchnumber = atoi(argv[3]);
            strncpy(searchname, argv[3], 20);

            if (CheckStrDigit(searchname) == 0)
                searchnumber = -1;
        }
    }
    
    if (strcmp(argv[1], list) == 0)
        listflag = 1;

    if (strcmp(argv[1], listall) == 0)
        listallflag = 1;

    if (strcmp(argv[1], list) == 0 || strcmp(argv[1], listall) == 0)
    {
        if (argc == 4 || argc > 4)
        {
            itemcount = atoi(argv[3]);

            if (itemcount)
                itemcount *= 0x10000;
        }

        if (argc == 5)
        {
            searchflag = 1;
            searchnumber = atoi(argv[4]);
            strncpy(searchname, argv[4], 20);

            if (CheckStrDigit(searchname) == 0)
                searchnumber = -1;
        }
    }
   
    if (!argv[2])
    {
       printf("Command not found\n"
              "Usage: -h for help\n");
       
       return 0;
    }
    
    if (extractflag || listflag || listallflag)
    {
        if (argv[2])
            strncpy(filename, argv[2], 20);
        
        GLB_InitSystem();
    }

    if (extractflag)
    {
        GLB_Extract();
        printf("Total items extracted: %02d\n", itemtotal);
    }

    if (listflag || listallflag)
    {
        GLB_List();
        printf("Total items listed: %02d\n", itemtotal);
    }
    
    if (listflag || listallflag)
        GLB_FreeAll();
    
    fclose(infile);
    
    return 0;
}