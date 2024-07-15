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
#include "graphics.h"
#include "sounds.h"
#include "mus2mid.h"

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

	sprintf(buffer, "%s", filename);
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

	if (convgraphicmapflag || convgraphicmapdebrisflag)
		num_glbs = allinfilenamescnt - 3;

	if (convgraphicmapspriteflag)
		num_glbs = allinfilenamescnt - 5;

	for (i = 0; i < num_glbs; i++)
	{
		if (convgraphicmapflag || convgraphicmapdebrisflag || convgraphicmapspriteflag)
			strncpy(filename, allinfilenames[i + 2], 260);

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

int GLB_GetItemID(const char* in_name)
{
	fitem_t* fi;
	int fc;
	int i, j;

	if (*in_name != ' ' && *in_name != '\0')
	{
		for (i = 0; i < num_glbs; i++)
		{
			fi = filedesc[i].items;
			fc = filedesc[i].itemcount;

			for (j = 0; j < fc; j++, fi++)
			{
				if (!strcmp(in_name, fi->name))
				{
					return (i << 16) | j;
				}
			}
		}
	}

	return -1;
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

int GLB_ItemSize(int handle)
{
	uint16_t f = (handle >> 16) & 0xffff;
	uint16_t n = (handle >> 0) & 0xffff;

	return filedesc[f].items[n].length;
}

void GLB_Extract(void)
{
	fitem_t* fi;
	FILE* lf;
	int fc;
	int i, j;
	int foundflag = 0;
	int labelflag = 0;
	char* buffer;
	char dup[32];
	char noname[32];
	char label[32];
	char labelsave[32];
	char getpath[260];
	char outdirectory[260];
	char linkfile[260];

	strncpy(linkfile, getdirectory, 260);
	strcat(linkfile, "link.txt");

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
				labelflag = 1;
			}
			else
				labelflag = 0;

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

				lf = fopen(linkfile, "a");
				fseek(lf, 0, SEEK_END);

				if (!labelflag)
				{
					fprintf(lf, "%s ", outdirectory);
					fprintf(lf, "%s\n", fi->name);
				}
				else
				{
					fprintf(lf, "%s ", outdirectory);
					fprintf(lf, "%s\n", "LABEL");
				}

				fclose(lf);

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

				lf = fopen(linkfile, "a");
				fseek(lf, 0, SEEK_END);

				if (!labelflag)
				{
					fprintf(lf, "%s ", outdirectory);
					fprintf(lf, "%s\n", fi->name);
				}
				else
				{
					fprintf(lf, "%s ", outdirectory);
					fprintf(lf, "%s\n", "LABEL");
				}

				fclose(lf);

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

void GLB_WriteHeaderFile(void)
{
	fitem_t* fi;
	int fc;
	int filecount = 0;
	FILE* hf;

	if (access("fileids.h", 0))
	{
		hf = fopen("fileids.h", "a");
		fseek(hf, 0, SEEK_END);

		fprintf(hf, "#pragma once\n");

		fclose(hf);
	}

	hf = fopen("fileids.h", "a");
	fseek(hf, 0, SEEK_END);

	fprintf(hf, "\n");
	fprintf(hf, "//%s Items\n", filename);
	fprintf(hf, "\n");

	fclose(hf);

	for (int i = 0; i < num_glbs; i++)
	{
		fi = filedesc[i].items;
		fc = filedesc[i].itemcount + itemcount;

		for (int j = itemcount; j < fc; j++, fi++)
		{
			RemoveCharFromString(fi->name, '/');

			if (fi->name[0] != '\0')
			{
				hf = fopen("fileids.h", "a");
				fseek(hf, 0, SEEK_END);

				fprintf(hf, "#define FILE%01d%02x_%s 0x%05x\n", itemcountsave, filecount, fi->name, j);

				fclose(hf);
			}

			filecount++;

			if (filecount > 0xff)
				filecount = 0x0;
		}
	}
}

void GLB_ConvertItems(void)
{
	fitem_t* fi;
	int fc;
	int i, j;
	int foundflag = 0;
	int labelflag = 0;
	int glbidnum = 0;
	int agxmode;
	int mapmode;
	int soundmode;
	int musicmode;
	int midoutlen;
	char* buffer;
	char* outbuffer;
	char dup[32];
	char noname[32];
	char label[32];
	char labelsave[32];
	char getpath[260];
	char outdirectory[260];
	GFX_PIC* picture;

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
				labelflag = 1;
			}
			else
				labelflag = 0;

			agxmode = 0;
			mapmode = 0;
			soundmode = 0;
			musicmode = 0;

			if (strcmp(fi->name, searchname) == 0 || j == searchnumber)
			{
				foundflag = 1;
				buffer = GLB_GetItem(j + glbidnum);

				outbuffer = NULL;

				if (!convgraphicmapflag && !convgraphicmapdebrisflag && !convgraphicmapspriteflag && !convsoundflag && !convmusicflag)
					outbuffer = ConvertGraphics(buffer, fi->name, fi->length);

				if (!outbuffer && convgraphicmapflag || !outbuffer && convgraphicmapdebrisflag || !outbuffer && convgraphicmapspriteflag)
				{
					outbuffer = ConvertGraphicsMAP(buffer, fi->name, fi->length);

					if (outbuffer)
						mapmode = 1;
				}

				if (!outbuffer && convsoundflag)
				{
					outbuffer = ConvertSounds(buffer, fi->name, fi->length);

					if (outbuffer)
						soundmode = 1;
				}

				if (!outbuffer && convmusicflag)
				{
					outbuffer = ConvertMusic(buffer, fi->name, fi->length);

					if (outbuffer)
						musicmode = 1;
				}

				if (!outbuffer && !convgraphicmapflag && !convgraphicmapdebrisflag && !convgraphicmapspriteflag && !convsoundflag && !convmusicflag)
				{
					outbuffer = ConvertGraphicsAGX(buffer, fi->name, fi->length);

					if (outbuffer)
						agxmode = 1;
				}

				if (!outbuffer)
				{
					if (convmusicflag)
						printf("%s is not a MUS item\n", fi->name);
					else
						printf("%s is not a PIC, BLK, TILE or AGX item\n", fi->name);
					goto nextitem;
				}

				if (!fs::is_directory(getdirectory) || !fs::exists(getdirectory))
					fs::create_directory(getdirectory);

				RemoveCharFromString(fi->name, '/');

				strncpy(outdirectory, getdirectory, 260);
				sprintf(getpath, "/%s", fi->name);
				strcat(outdirectory, getpath);

				if (convsoundflag)
					sprintf(outdirectory, "%s.wav", outdirectory);
				else if (convmusicflag)
					sprintf(outdirectory, "%s.mid", outdirectory);
				else
					sprintf(outdirectory, "%s.png", outdirectory);

				if (!access(outdirectory, 0))
				{
					int lstring;
					lstring = strlen(outdirectory);
					outdirectory[lstring - 4] = '\0';

					sprintf(dup, "_%03x", j);
					strcat(outdirectory, dup);

					if (convsoundflag)
						sprintf(outdirectory, "%s.wav", outdirectory);
					else if (convmusicflag)
						sprintf(outdirectory, "%s.mid", outdirectory);
					else
						sprintf(outdirectory, "%s.png", outdirectory);
				}

				picture = (GFX_PIC*)buffer;

				if (outbuffer)
				{
					if (!agxmode && !mapmode && !soundmode && !musicmode)
						WriteGraphics(outbuffer, outdirectory, picture->width, picture->height);
					else if (mapmode)
						WriteGraphics(outbuffer, outdirectory, 288, 4800);
					else if (soundmode)
					{
						outfile = fopen(outdirectory, "wb");

						if (outfile)
						{
							fwrite(outbuffer, fi->length + 36, 1, outfile);
							fclose(outfile);
						}
					}
					else if (musicmode)
					{
						outfile = fopen(outdirectory, "wb");

						if (outfile)
						{
							midoutlen = mus_getoutfilelen();
							fwrite(outbuffer, midoutlen, 1, outfile);
							fclose(outfile);
						}
					}
					else if (agxmode)
						WriteGraphics(outbuffer, outdirectory, 320, 200);

					free(outbuffer);
				}

				if (searchflag && fi->name[0] != '\0')
				{
					printf("Decoding item: %s\n", fi->name);

					if (!agxmode && !mapmode && !soundmode && !musicmode)
					{
						if (picture->type == GPIC)
							printf("Itemtype: GPIC\n");

						if (picture->type == GSPRITE)
							printf("Itemtype: GSPRITE\n");

						printf("Width of item: %0d\n", picture->width);
						printf("Height of item: %0d\n", picture->height);
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (mapmode)
					{
						printf("Itemtype: MAP\n");
						printf("Width of item: %0d\n", 288);
						printf("Height of item: %0d\n", 4800);
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (soundmode)
					{
						printf("Itemtype: FX\n");
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (musicmode)
					{
						printf("Itemtype: MUS\n");
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (agxmode)
					{
						printf("Itemtype: AGX\n");
						printf("Width of item: %0d\n", 320);
						printf("Height of item: %0d\n", 200);
						printf("Encoding item to: %s\n", outdirectory);
					}

					if (buffer)
						free(buffer);

					itemtotal++;
				}
			}

			if (!searchflag)
			{
				buffer = GLB_GetItem(j + glbidnum);

				outbuffer = NULL;

				if (!convgraphicmapflag && !convgraphicmapdebrisflag && !convgraphicmapspriteflag && !convsoundflag && !convmusicflag)
					outbuffer = ConvertGraphics(buffer, fi->name, fi->length);

				if (!outbuffer && convgraphicmapflag || !outbuffer && convgraphicmapdebrisflag || !outbuffer && convgraphicmapspriteflag)
				{
					outbuffer = ConvertGraphicsMAP(buffer, fi->name, fi->length);

					if (outbuffer)
						mapmode = 1;
				}

				if (!outbuffer && convsoundflag)
				{
					outbuffer = ConvertSounds(buffer, fi->name, fi->length);

					if (outbuffer)
						soundmode = 1;
				}

				if (!outbuffer && convmusicflag)
				{
					outbuffer = ConvertMusic(buffer, fi->name, fi->length);

					if (outbuffer)
						musicmode = 1;
				}

				if (!outbuffer && !convgraphicmapflag && !convgraphicmapdebrisflag && !convgraphicmapspriteflag && !convsoundflag && !convmusicflag)
				{
					outbuffer = ConvertGraphicsAGX(buffer, fi->name, fi->length);

					if (outbuffer)
						agxmode = 1;
				}

				if (!outbuffer)
					goto nextitem;

				if (!fs::is_directory(getdirectory) || !fs::exists(getdirectory))
					fs::create_directory(getdirectory);

				RemoveCharFromString(fi->name, '/');

				strncpy(outdirectory, getdirectory, 260);
				sprintf(getpath, "/%s", fi->name);
				strcat(outdirectory, getpath);

				if (convsoundflag)
					sprintf(outdirectory, "%s.wav", outdirectory);
				else if (convmusicflag)
					sprintf(outdirectory, "%s.mid", outdirectory);
				else
					sprintf(outdirectory, "%s.png", outdirectory);

				if (!access(outdirectory, 0))
				{
					int lstring;
					lstring = strlen(outdirectory);
					outdirectory[lstring - 4] = '\0';

					sprintf(dup, "_%03x", j);
					strcat(outdirectory, dup);

					if (convsoundflag)
						sprintf(outdirectory, "%s.wav", outdirectory);
					else if (convmusicflag)
						sprintf(outdirectory, "%s.mid", outdirectory);
					else
						sprintf(outdirectory, "%s.png", outdirectory);
				}

				picture = (GFX_PIC*)buffer;

				if (outbuffer)
				{
					if (!agxmode && !mapmode && !soundmode && !musicmode)
						WriteGraphics(outbuffer, outdirectory, picture->width, picture->height);
					else if (mapmode)
						WriteGraphics(outbuffer, outdirectory, 288, 4800);
					else if (soundmode)
					{
						outfile = fopen(outdirectory, "wb");

						if (outfile)
						{
							fwrite(outbuffer, fi->length + 36, 1, outfile);
							fclose(outfile);
						}
					}
					else if (musicmode)
					{
						outfile = fopen(outdirectory, "wb");

						if (outfile)
						{
							midoutlen = mus_getoutfilelen();
							fwrite(outbuffer, midoutlen, 1, outfile);
							fclose(outfile);
						}
					}
					else if (agxmode)
						WriteGraphics(outbuffer, outdirectory, 320, 200);

					free(outbuffer);
				}

				if (fi->name[0] != '\0')
				{
					printf("Decoding item: %s\n", fi->name);

					if (!agxmode && !mapmode && !soundmode && !musicmode)
					{

						if (picture->type == GPIC)
							printf("Itemtype: GPIC\n");

						if (picture->type == GSPRITE)
							printf("Itemtype: GSPRITE\n");

						printf("Width of item: %0d\n", picture->width);
						printf("Height of item: %0d\n", picture->height);
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (mapmode)
					{
						printf("Itemtype: MAP\n");
						printf("Width of item: %0d\n", 288);
						printf("Height of item: %0d\n", 4800);
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (soundmode)
					{
						printf("Itemtype: FX\n");
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (musicmode)
					{
						printf("Itemtype: MUS\n");
						printf("Encoding item to: %s\n", outdirectory);
					}
					else if (agxmode)
					{
						printf("Itemtype: AGX\n");
						printf("Width of item: %0d\n", 320);
						printf("Height of item: %0d\n", 200);
						printf("Encoding item to: %s\n", outdirectory);
					}

					if (buffer)
						free(buffer);
				}
			}

			if (fi->name[0] != '\0' && !searchflag)
				itemtotal++;

		nextitem:;
		}

		glbidnum += 0x10000;
	}

	if (searchflag && !foundflag)
		printf("Item not found\n");
}