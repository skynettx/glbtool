#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <filesystem>
#ifdef _WIN32
#include <io.h>
#endif // _WIN32
#ifdef __linux__
#include <sys/io.h>
#include <search.h>
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
#define fileno _fileno
#define write _write
#define read _read
#endif

char filename[260];
char searchname[260];
char infilename[260];
char outfilename[260];
char** allinfilenames;
int allinfilenamescnt;
int itemcount = 0;
int itemtotal = 0;
int itemtotalsize = 0;
int listflag = 0;
int listallflag = 0;
int extractflag = 0;
int encryptflag = 0;
int encryptallflag = 0;
int searchflag = 0;
int searchnumber = -1;

FILE* infile;
FILE* outfile;

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

struct State {
    char current_byte;
    char prev_byte;
    uint8_t key_pos;
};

struct fitem_t hfat;
struct fitem_t temp;
struct fitem_t* ffat;
struct State state;

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

namespace fs = std::filesystem;

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

char* RemovePathFromString(char* p)
{
    char* fn;
    char* input;
    
    input = p;

    if (input[(strlen(input) - 1)] == '/' || input[(strlen(input) - 1)] == '\\')
        input[(strlen(input) - 1)] = '\0';

    if (strchr(input, '\\'))
        (fn = strrchr(input, '\\')) ? ++fn : (fn = input);
    else
        (fn = strrchr(input, '/')) ? ++fn : (fn = input);
    
    return fn;
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

int calculate_key_pos(int len)
{
    return 25 % len;
}

void reset_state(struct State* state)
{
    state->key_pos = calculate_key_pos(strlen(serial));
    state->prev_byte = serial[state->key_pos];

    return;
}

static int encrypt_varlen(struct State* state, void* data, int size)
{
    char* current_byte;
    char* prev_byte;
    char* byte_data;
    uint8_t* key_pos;

    int i;

    current_byte = &state->current_byte;
    prev_byte = &state->prev_byte;
    key_pos = &state->key_pos;
    byte_data = (char*)data;

    for (i = 0; i < size; i++) 
    {
        *current_byte = byte_data[i];
        byte_data[i] = *current_byte + serial[*key_pos] + *prev_byte;
        byte_data[i] &= 0xFF;
        (*key_pos)++;
        *key_pos %= strlen(serial);
        *prev_byte = byte_data[i];
    }

    return i;
}

int encrypt_int(struct State* state, int* data)
{
    return encrypt_varlen(state, data, 4);
}

int encrypt_filename(struct State* state, char* str)
{
    *(str + 16 - 1) = '\0';
    return encrypt_varlen(state, str, 16);
}

int encrypt_fat_single(struct State* state, struct fitem_t* fat)
{
    int retval;

    reset_state(state);

    retval = encrypt_int(state, &fat->flags);
    retval += encrypt_int(state, &fat->offset);
    retval += encrypt_int(state, &fat->length);
    retval += encrypt_filename(state, fat->name);

    return retval;
}

int encrypt_file(struct State* state, char* str, int length)
{
    if (!length) return 0;
    reset_state(state);
    return encrypt_varlen(state, str, length);
}

int fat_io_write(struct fitem_t* fat, int fd)
{
    char buffer[28];

    int pos;

    pos = 0;

    memcpy(buffer, &fat->flags, 4);
    pos += 4;
    memcpy(buffer + pos, &fat->offset, 4);
    pos += 4;
    memcpy(buffer + pos, &fat->length, 4);
    pos += 4;
    memcpy(buffer + pos, fat->name, 16);
    pos += 16;

    return write(fd, buffer, 28);
}

int fat_entry_init(struct fitem_t* fat, char* path, int offset)
{
    struct stat st;

    int len;

    if (stat(path, &st)) 
    {
        return -1;
    }

    fat->flags = 0;
    fat->offset = offset;
    fat->length = st.st_size;

    len = strlen(path);
    
    if (len > 16) 
        path += len - 16 + 1;

    strcpy(fat->name, path);

    strncpy(fat->name, RemovePathFromString(fat->name),16);

    return 0;
}

void fat_flag_encryption(struct fitem_t* ffat, int nfiles)
{
    struct fitem_t* end;

    end = ffat + nfiles;
   
    for (; ffat < end; ffat++)
    {
       ffat->flags = 1;
    }

    return;
}

void GLB_Create(char* infilename, char* outfilename)
{
    char* buffer;
    char* filename;

    int largest;
    int offset;
    int bytes;

    int nfiles;
    int i;

    int filecnt = 0;

    int rd, wd;
    
    filename = NULL;
    largest = 0;

    memset(&hfat, 0, sizeof(hfat));
    
    if (encryptflag)
    {
        filecnt = 2;
        nfiles = allinfilenamescnt - 3;
    }
    else
    {
        filecnt = 0;
        nfiles = allinfilenamescnt;
    }

    outfile = fopen(outfilename, "wb");

    wd = fileno(outfile);
    hfat.offset = nfiles;
    encrypt_fat_single(&state, &hfat);
    fat_io_write(&hfat, wd);

    ffat = (struct fitem_t*)malloc(sizeof(*ffat) * nfiles);
    offset = nfiles * 28 + 28;
    
    for (i = 0; i < nfiles; i++)
    {
        
        if (fat_entry_init(&ffat[i], allinfilenames[filecnt], offset))
        {
            printf("Could not init fat entry %s\n", allinfilenames[filecnt]);
            EXIT_Error("Could not init fat entry");
        }
        
        if (ffat[i].length > largest) largest = ffat[i].length;
        offset += ffat[i].length;
        
        filecnt++;
    }

    fat_flag_encryption(ffat, nfiles);
    
    for (i = 0; i < nfiles; i++)
    {
        memcpy(&temp, &ffat[i], sizeof(ffat[i]));
        encrypt_fat_single(&state, &temp);

        bytes = fat_io_write(&temp, wd);
        
        if (bytes == -1)
        {
            EXIT_Error("DIE");
        }
        else if (bytes != 28) {
            printf("Bytes written not equal to file length %s\n", ffat[i].name);
        }
    }

    buffer = (char*)malloc(largest);
    
    if (encryptflag)
        filecnt = 2;

    else
        filecnt = 0;

    for (i = 0; i < nfiles; i++)
    {
        infile = fopen(allinfilenames[filecnt], "rb");
        
        if (!infile)
        {
            printf("Could not open file %s\n", allinfilenames[filecnt]);
            EXIT_Error("Could not open file");
        }

        printf("Encrypting item: %s\n", allinfilenames[filecnt]);
        itemtotal++;
        itemtotalsize += ffat[i].length;

        rd = fileno(infile);

        bytes = read(rd, buffer, ffat[i].length);
        
        if (bytes == -1) 
        {
            EXIT_Error("DIE");
        }
        else if (bytes != ffat[i].length) {
            printf("Bytes read not equal to file length %s\n", allinfilenames[filecnt]);
        }

        encrypt_file(&state, buffer, ffat[i].length);
       
        bytes = write(wd, buffer, ffat[i].length);
        
        if (bytes == -1)
        {
            EXIT_Error("DIE");
        }
        else if (bytes != ffat[i].length)
        {
            printf("Bytes written not equal to file length %s\n", allinfilenames[filecnt]);
        }

        fclose(infile);
        filecnt++;
    }

    free(ffat);
    free(buffer);
    fclose(outfile);
}

void GLB_Extract()
{
    fitem_t* fi;
    int fc;
    int i, j;
    int foundflag = 0;
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
                outfile = fopen(fi->name, "wb");

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

                if (!access(fi->name, 0))
                {
                    sprintf(dup, "_%03x", j);
                    strcat(fi->name, dup);
                }
                
                RemoveCharFromString(fi->name, '/');
                outfile = fopen(fi->name, "wb");
                    
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
    const char* encrypt = "-e";
    const char* encryptall = "-ea";
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
               "-e  Encrypt items from <INPUTFILE>... to <OUTPUTFILE.GLB>\n"
               "-ea Encrypt all items from <INPUTFOLDER> to <OUTPUTFILE.GLB>\n"
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
            strncpy(searchname, argv[3], 260);

            if (CheckStrDigit(searchname) == 0)
                searchnumber = -1;
        }
    }
    
    if (strcmp(argv[1], encrypt) == 0)
    {
        encryptflag = 1;
        
        allinfilenames = (char**)malloc((argc + 1) * sizeof * allinfilenames);
        allinfilenamescnt = argc;
        
        if (argv[2] && argc > 3)
        {
            
            for (int i = 0; i < argc; ++i)
            {
                size_t length = strlen(argv[i]) + 1;
                allinfilenames[i] = (char*)malloc(length);
                memcpy(allinfilenames[i], argv[i], length);
             
                if (i > 1 && i < argc - 1)
                {
                    strncpy(infilename, argv[i], 260);
                    
                    if (access(infilename, 0))
                    {
                        printf("Input file not found\n");
                        return 0;
                    }
                }

                if (i == argc - 1)
                    strncpy(outfilename, argv[i], 260);
            }
        }
        
        if (!access(outfilename, 0))
        {
            printf("Output filename already exists\n");
            return 0;
        }

        if(!argv[2])
        {
            printf("No input file specified\n");
            return 0;
        }

        if (!argv[3])
        {
            printf("No output file specified\n");
            return 0;
        }
    }
    
    if (strcmp(argv[1], encryptall) == 0)
    {
        encryptallflag = 1;
        int i = 0;
        
        if (!argv[2])
        {
            printf("No input folder specified\n");
            return 0;
        }
        
        strncpy(infilename, argv[2], 260);
        std::string path = infilename;
        
        if (access(infilename, 0))
        {
            printf("Input folder not found\n");
            return 0;
        }
        
        if (!argv[3])
        {
            printf("No output file specified\n");
            return 0;
        }

        if (argv[3])
            strncpy(outfilename, argv[3], 260);
        
        allinfilenames = (char**)malloc((4096) * sizeof * allinfilenames);
        allinfilenamescnt = 0;
        
        if (argv[2] && argv[3])
        {
            for (const auto& file : std::filesystem::directory_iterator(infilename))
            {
                size_t length = file.path().stem().string().length() + 1;
                allinfilenames[i] = (char*)malloc(length);
                
                allinfilenames[i] = new char[file.path().string().length() + 1];
                strcpy(allinfilenames[i], file.path().string().c_str());
                
                //allinfilenames[i] = new char[file.path().stem().string().length() + 1];
                //strcpy(allinfilenames[i], file.path().stem().string().c_str());
                
                ++i;
                allinfilenamescnt++;
            }
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
            strncpy(searchname, argv[4], 260);

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
            strncpy(filename, argv[2], 260);
        
        GLB_InitSystem();
    }

    if (encryptflag || encryptallflag)
    {
        GLB_Create(infilename, outfilename);
        printf("Total items encrypted: %02d to %s %d Bytes written\n", itemtotal, outfilename, itemtotalsize);
        free(allinfilenames);
    }

    if (extractflag)
    {
        GLB_Extract();
        printf("Total items decrypted: %02d\n", itemtotal);
    }

    if (listflag || listallflag)
    {
        GLB_List();
        printf("Total items listed: %02d\n", itemtotal);
        GLB_FreeAll();
    }
    
    if (extractflag || listflag || listallflag)
        fclose(infile);
    
    return 0;
}