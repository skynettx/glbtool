#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <filesystem>
#include <set>
#include "main.h"
#include "decrypt.h"
#include "encrypt.h"
#include "graphics.h"

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

char filename[260];
char palfilename[260];
char searchname[260];
char infilename[260];
char outfilename[260];
char getdirectory[260];
char** allinfilenames;
char** alloutfilenames;
int allinfilenamescnt;
int itemcount = 0;
int itemcountsave;
int itemtotal = 0;
int itemtotalsize = 0;
int listflag = 0;
int listallflag = 0;
int writeheaderflag = 0;
int extractflag = 0;
int encryptflag = 0;
int encryptallflag = 0;
int encryptlinkflag = 0;
int searchflag = 0;
int searchnumber = -1;
int convgraphicflag = 0;
int convgraphicmapflag = 0;
int convsoundflag = 0;

FILE* infile;
FILE* outfile;

const char* serial = "32768GLB";

using namespace std;
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
	const char* encryptlink = "-el";
	const char* list = "-l";
	const char* listall = "-la";
	const char* writeheader = "-w";
	const char* convgraphics = "-g";
	const char* convgraphicsmap = "-gm";
	const char* convsounds = "-s";
	char line;

	printf("********************************************************************************\n"
		" GLB Tool for Raptor Call Of The Shadows GLB Files                     ver 1.0.2\n"
		"********************************************************************************\n");

	if (!argv[1])
	{
		printf("Usage: -h for help\n");

		return 0;
	}

	if (strcmp(argv[1], help) == 0)
	{
		printf("-x  Extract items from <INPUTFILE.GLB>\n"
			"    optional <SearchItemNameNumber> only extract found items\n"
			"-e  Encrypt items from <INPUTFILE>... to <OUTPUTFILE.GLB>\n"
			"-ea Encrypt all items from <INPUTFOLDER> to <OUTPUTFILE.GLB>\n"
			"-el Encrypt all items from <LINKFILE.txt> to <OUTPUTFILE.GLB>\n"
			"-l  List items from <INPUTFILE.GLB>\n"
			"    optional <FILENUMBER> for correct item numbers in files > FILE0000.GLB\n"
			"    optional <SearchItemNameNumber> only list found items\n"
			"-la List complete content (with labels) from <INPUTFILE.GLB>\n"
			"    optional <FILENUMBER> for correct item numbers in files > FILE0000.GLB\n"
			"    optional <SearchItemNameNumber> only list found items\n"
			"-w  Write header file from <INPUTFILE.GLB> and add <FILENUMBER>\n"
			"    for correct item numbers\n"
			"-g  Convert PIC, BLK, TILE and AGX items from <INPUTFILE.GLB> and <PALETTEFILE>\n"
			"    to PNG format\n"
			"    optional <SearchItemNameNumber> only convert found items\n"
			"-gm Convert MAP items from <INPUTFILE.GLB>... and <PALETTEFILE>\n"
			"    to PNG format\n"
			"-s  Convert digital FX items from <INPUTFILE.GLB> to WAVE format\n"
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

		if (!argv[2])
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

		if (!argv[2])
		{
			printf("No input folder specified\n");
			return 0;
		}

		strncpy(infilename, argv[2], 260);
		std::string path = infilename;
		set<fs::path> sort_filename;

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
			for (auto& entry : fs::directory_iterator(infilename))
				sort_filename.insert(entry.path());

			for (auto& filename : sort_filename)
			{
				size_t length = filename.string().length() + 1;
				allinfilenames[allinfilenamescnt] = (char*)malloc(length);
				strcpy(allinfilenames[allinfilenamescnt], filename.string().c_str());

				allinfilenamescnt++;
			}
		}
	}

	if (strcmp(argv[1], encryptlink) == 0)
	{
		encryptlinkflag = 1;

		if (!argv[2])
		{
			printf("No link file specified\n");
			return 0;
		}

		strncpy(infilename, argv[2], 260);

		if (access(infilename, 0))
		{
			printf("Link file not found\n");
			return 0;
		}

		if (!argv[3])
		{
			printf("No output file specified\n");
			return 0;
		}

		if (argv[3])
			strncpy(outfilename, argv[3], 260);

		FILE* linkfile = fopen(infilename, "r");
		allinfilenamescnt = 0;

		if (linkfile == NULL)
		{
			printf("Cannot open link file\n");
			return 1;
		}
		else
		{
			do
			{
				line = fgetc(linkfile);

				if (line == '\n')
					allinfilenamescnt++;

			} while (line != EOF);

			rewind(linkfile);

			allinfilenames = (char**)malloc((allinfilenamescnt) * sizeof * allinfilenames);
			alloutfilenames = (char**)malloc((allinfilenamescnt) * sizeof * alloutfilenames);

			for (int i = 0; i < allinfilenamescnt; i++)
			{
				allinfilenames[i] = (char*)malloc(allinfilenamescnt);
				alloutfilenames[i] = (char*)malloc(allinfilenamescnt);

				fscanf(linkfile, "%s %s\n", allinfilenames[i], alloutfilenames[i]);

				if (strcmp(alloutfilenames[i], "LABEL") == 0)
					strcpy(alloutfilenames[i], "");
			}
		}

		fclose(linkfile);
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

	if (strcmp(argv[1], writeheader) == 0)
	{
		writeheaderflag = 1;

		if (!argv[2])
		{
			printf("No input file specified\n");
			return 0;
		}

		if (argv[2])
		{
			strncpy(filename, argv[2], 260);
		}

		if (access(filename, 0))
		{
			printf("Input file not found\n");
			return 0;
		}

		if (argc == 4 || argc > 4)
		{
			itemcount = atoi(argv[3]);

			itemcountsave = itemcount;

			if (itemcount)
				itemcount *= 0x10000;
		}
		else
		{
			printf("No file number specified\n");
			return 0;
		}
	}

	if (strcmp(argv[1], convgraphics) == 0)
	{
		if (strcmp(argv[1], convgraphics) == 0)
			convgraphicflag = 1;

		if (argc == 5)
		{
			searchflag = 1;
			searchnumber = atoi(argv[4]);
			strncpy(searchname, argv[4], 260);

			if (CheckStrDigit(searchname) == 0)
				searchnumber = -1;
		}
	}

	if (strcmp(argv[1], convgraphicsmap) == 0)
	{
		convgraphicmapflag = 1;
	}

	if (strcmp(argv[1], convsounds) == 0)
	{
		convsoundflag = 1;
	}

	if (!extractflag && !listflag && !listallflag && !encryptflag && !encryptallflag && !encryptlinkflag && !writeheaderflag
		&& !convgraphicflag && !convgraphicmapflag && !convsoundflag)
	{
		printf("Command not found\n"
			"Usage: -h for help\n");

		return 0;
	}

	if (extractflag || listflag || listallflag || writeheaderflag || convgraphicflag || convsoundflag)
	{
		if (argv[2])
			strncpy(filename, argv[2], 260);

		if (argv[2] && extractflag)
		{
			strncpy(getdirectory, filename, 260);
			RemoveCharFromString(getdirectory, '.');
		}

		if (argv[2] && convgraphicflag)
		{
			if (!argv[3])
			{
				printf("Palette file not specified\n");
				return 0;
			}

			if (argv[3])
				strncpy(palfilename, argv[3], 260);

			if (access(palfilename, 0))
			{
				printf("Palette file not found\n");
				return 0;
			}
			else
				if (!SetPalette(palfilename))
					return 0;

			strncpy(getdirectory, filename, 260);
			RemoveCharFromString(getdirectory, '.');
			sprintf(getdirectory, "%s%s", getdirectory, "graph");
		}

		if (argv[2] && convsoundflag)
		{
			strncpy(getdirectory, filename, 260);
			RemoveCharFromString(getdirectory, '.');
			sprintf(getdirectory, "%s%s", getdirectory, "sound");
		}

		GLB_InitSystem();
	}

	if (convgraphicmapflag)
	{
		allinfilenames = (char**)malloc((argc + 1) * sizeof * allinfilenames);
		allinfilenamescnt = argc;

		if (argv[2] && argc > 3)
		{

			for (int i = 0; i < argc; ++i)
			{
				size_t length = strlen(argv[i]) + 1;
				allinfilenames[i] = (char*)malloc(length);
				memcpy(allinfilenames[i], argv[i], length);

				if (i > 1 && i < argc)
				{
					strncpy(infilename, argv[i], 260);

					if (access(infilename, 0))
					{
						printf("Input file not found\n");
						return 0;
					}
				}

				if (i == argc - 1)
					strncpy(palfilename, argv[i], 260);
			}

			if (!SetPalette(palfilename))
				return 0;
		}

		if (!argv[2])
		{
			printf("No input file specified\n");
			return 0;
		}

		if (!argv[3])
		{
			printf("Palette file not specified\n");
			return 0;
		}

		sprintf(getdirectory, "%s", "mapgraph");

		GLB_InitSystem();
	}

	if (encryptflag || encryptallflag || encryptlinkflag)
	{
		GLB_Create(outfilename);
		printf("Total items encrypted: %02d to %s %d Bytes written\n", itemtotal, outfilename, itemtotalsize);
		free(allinfilenames);

		if (encryptlinkflag)
			free(alloutfilenames);
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

	if (writeheaderflag)
	{
		GLB_WriteHeaderFile();
		printf("Header fileids.h written\n");
		GLB_FreeAll();
	}

	if (convgraphicflag || convgraphicmapflag || convsoundflag)
	{
		GLB_ConvertItems();
		printf("Total items encoded: %02d\n", itemtotal);

		if (convgraphicmapflag)
		{
			free(allinfilenames);
		}
	}

	if (extractflag || listflag || listallflag || writeheaderflag || convgraphicflag || convgraphicmapflag || convsoundflag)
	{
		fclose(infile);
	}

	return 0;
}