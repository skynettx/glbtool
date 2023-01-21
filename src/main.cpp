#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <filesystem>
#include "main.h"
#include "decrypt.h"
#include "encrypt.h"
#ifdef _WIN32
#include <io.h>
#endif // _WIN32
#ifdef __linux__
#include <sys/io.h>
#include <search.h>
#endif // __linux__
#ifdef __GNUC__
#include <unistd.h>
#endif
#ifdef _MSC_VER
#include <windows.h>
#define PATH_MAX MAX_PATH
#define access _access
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

namespace fs = std::filesystem;

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
                //size_t length = file.path().stem().string().length() + 1;
                size_t length = file.path().string().length() + 1;
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
        GLB_Create(outfilename);
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