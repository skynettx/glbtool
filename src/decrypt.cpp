#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <filesystem>
#include "main.h"
#include "decrypt.h"

#ifdef _WIN32
#include <io.h>
#endif // _WIN32
#ifdef __GNUC__
#include <unistd.h>
#endif // __GNUC__
#ifdef _MSC_VER
#include <windows.h>
#define PATH_MAX MAX_PATH
#define access _access
#endif // _MSC_VER

struct filedesc_t {
    char path[PATH_MAX];
    fitem_t* items;
    int itemcount;
    FILE* handle;
    const char* mode;
};

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

int num_glbs = 1;

using namespace std;
namespace fs = std::filesystem;

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

void GLB_Extract(void)
{
    fitem_t* fi;
    int fc;
    int i, j;
    int foundflag = 0;
    char* buffer;
    char dup[32];
    char noname[32];
    char label[32];
    char labelsave[32];
    char getpath[260];
    char outdirectory[260];
   
    for (i = 0; i < num_glbs; i++)
    {
        fi = filedesc[i].items;
        fc = filedesc[i].itemcount;

        for (j = 0; j < fc; j++, fi++)
        {
            if (fi->length == 0)
            {
                strncpy(label, fi->name, 32);
                strncpy(labelsave, fi->name, 32);
            }

            if (strcmp(fi->name, "") == 0)
            {
                sprintf(noname, "_%03x", j);
                strcat(label, noname);
                strcpy(fi->name, label);
                strcpy(label, labelsave);
            }

            if (strcmp(fi->name, searchname) == 0 || j == searchnumber)
            {
                foundflag = 1;
                buffer = GLB_GetItem(j);
                
                if (!fs::is_directory(getdirectory) || !fs::exists(getdirectory))
                    fs::create_directory(getdirectory);

                RemoveCharFromString(fi->name, '/');

                strncpy(outdirectory, getdirectory, 260);
                sprintf(getpath, "/%s", fi->name);
                strcat(outdirectory, getpath);
                
                if (!access(outdirectory, 0))
                {
                    sprintf(dup, "_%03x", j);
                    strcat(outdirectory, dup);
                }

                outfile = fopen(outdirectory, "wb");

                if (outfile && buffer)
                {
                    fwrite(buffer, fi->length, 1, outfile);
                    free(buffer);
                    fclose(outfile);
                }

                if (searchflag && fi->name[0] != '\0')
                {
                    printf("Decrypting item: %s\n", fi->name);
                    itemtotal++;
                }
            }

            if (!searchflag)
            {
                buffer = GLB_GetItem(j);

                if (!fs::is_directory(getdirectory) || !fs::exists(getdirectory))
                    fs::create_directory(getdirectory);

                RemoveCharFromString(fi->name, '/');

                strncpy(outdirectory, getdirectory, 260);
                sprintf(getpath, "/%s", fi->name);
                strcat(outdirectory, getpath);
                
                if (!access(outdirectory, 0))
                {
                    sprintf(dup, "_%03x", j);
                    strcat(outdirectory, dup);
                }

                outfile = fopen(outdirectory, "wb");

                if (outfile && buffer)
                {
                    fwrite(buffer, fi->length, 1, outfile);
                    free(buffer);
                    fclose(outfile);
                }

                if (fi->name[0] != '\0')
                    printf("Decrypting item: %s\n", fi->name);
            }

            if (fi->name[0] != '\0' && !searchflag)
                itemtotal++;
        }
    }

    if (searchflag && !foundflag)
        printf("Item not found\n");
}

void GLB_List(void)
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
        }
    }

    if (searchflag && !foundflag)
        printf("Item not found\n");
}