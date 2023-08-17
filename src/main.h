#pragma once
#include <stdlib.h>

extern FILE* infile;
extern FILE* outfile;

extern const char* serial;

extern char filename[260];
extern char searchname[260];
extern char infilename[260];
extern char outfilename[260];
extern char getdirectory[260];
extern char** allinfilenames;
extern char** alloutfilenames;
extern int allinfilenamescnt;
extern int itemcount;
extern int itemtotal;
extern int itemtotalsize;
extern int listflag;
extern int listallflag;
extern int extractflag;
extern int encryptflag;
extern int encryptallflag;
extern int encryptlinkflag;
extern int searchflag;
extern int searchnumber;

struct meminfo_t
{
    char* ptr;
    uint32_t age;
};

struct fitem_t {
    char name[32];
    meminfo_t mem;
    int length;
    int offset;
    int flags;
    int lock;
};

static inline void EXIT_Error(const char* a1, ...)
{
    exit(0);
}

void RemoveCharFromString(char* p, char c);
char* RemovePathFromString(char* p);