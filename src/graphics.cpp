#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "main.h"
#include "decrypt.h"
#include "lodepng.h"
#include "graphics.h"

#ifdef _WIN32
#include <io.h>
#endif // _WIN32
#ifdef __GNUC__
#include <unistd.h>
#endif
#ifdef _MSC_VER
#include <windows.h>
#define PATH_MAX MAX_PATH
#define access _access
#endif

#define HEADERSIZE 20
#define BLOCKHEADERSIZE 16
#define MAP_ROWS        150
#define MAP_COLS        9
#define MAP_SIZE        ( MAP_ROWS * MAP_COLS )

int tileidflag = 0;
int tileid[15];
FILE* palfile;

struct palette_t {

	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

static palette_t palette[256];

typedef struct
{
	int x;                  // X POS OF SEG
	int y;                  // Y POS OF SEG
	int offset;             // OFFSET FROM X, Y
	int length;             // LENGTH OF LINE
}GFX_SPRITE;

typedef struct
{
	unsigned short opt;
	unsigned short fill;
	unsigned short offset;
	unsigned short length;
}ANIMLINE;

typedef struct
{
	short flats;
	short fgame;
}MAZEDATA;

typedef struct
{
	int sizerec;
	int spriteoff;
	int numsprites;
	MAZEDATA map[MAP_SIZE];
}MAZELEVEL;

lodepng::State statepng;

int SetPalette(char* palfilename)
{
	int lengthpalfile;
	uint8_t* inpalette;

	palfile = fopen(palfilename, "rb");

	fseek(palfile, 0, SEEK_END);
	lengthpalfile = ftell(palfile);
	fseek(palfile, 0, SEEK_SET);

	if (lengthpalfile != 768)
	{
		printf("Not a valid palette file\n");
		fclose(palfile);

		return 0;
	}

	inpalette = (uint8_t*)malloc(lengthpalfile);

	if (inpalette)
		fread(inpalette, lengthpalfile, 1, palfile);

	for (int i = 0; i < 256; i++)
	{
		palette[i].r = *inpalette++ << 2;
		palette[i].g = *inpalette++ << 2;
		palette[i].b = *inpalette++ << 2;
		palette[i].a = 255;
	}

	fclose(palfile);

	return 1;
}

void WriteGraphics(char* outbuffer, char* outdirectory, int picwidth, int picheight)
{
	unsigned error = lodepng_encode_file(outdirectory, (const unsigned char*)outbuffer, picwidth, picheight, LCT_RGBA, 8);
}

char* ConvertGraphics(char* item, char* itemname, int itemlength)
{
	GFX_PIC* picture;
	GFX_SPRITE* sprite;
	char* rawvga;
	char* outbuf;

	rawvga = (char*)malloc(itemlength - HEADERSIZE);

	if (item)
	{
		picture = (GFX_PIC*)item;

		int verify;
		verify = picture->width * picture->height + HEADERSIZE;

		if (picture->type == GPIC && itemlength != verify ||
			picture->width > 320 || picture->height > 200 ||
			!picture->width || !picture->height ||
			picture->type < 0 || picture->type > 1 ||
			picture->opt1 < 0 || picture->opt2 < 0)
		{
			return 0;
		}

		memcpy(rawvga, item + HEADERSIZE, itemlength - HEADERSIZE);

		int pos = 0;
		int n = 0;

		outbuf = (char*)malloc(4 * (picture->width * picture->height));

		if (picture->type == GPIC)
		{
			int transcnt = 0;

			for (int i = 0; i < picture->width * picture->height; i++, transcnt++)
			{
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
			}

			for (int i = 0; i < picture->width * picture->height; i++, pos++)
			{
				unsigned char pixel = rawvga[pos];

				outbuf[n] = palette[pixel].r;
				n++;
				outbuf[n] = palette[pixel].g;
				n++;
				outbuf[n] = palette[pixel].b;
				n++;
				outbuf[n] = palette[pixel].a;
				n++;
			}
		}

		if (picture->type == GSPRITE)
		{
			char* spritepic;
			char* readspritepic;
			int startpos = HEADERSIZE;

			readspritepic = (char*)malloc(itemlength - 36);
			memcpy(readspritepic, item + 36, itemlength - 36);

			spritepic = (char*)malloc(itemlength - HEADERSIZE);
			memcpy(spritepic, item + HEADERSIZE, itemlength - HEADERSIZE);

			sprite = (GFX_SPRITE*)spritepic;
			int block_end = sprite->length;

			int transcnt = 0;

			for (int i = 0; i < picture->width * picture->height; i++, transcnt++)
			{
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
			}

			while (1)
			{
				if (sprite->offset == 0xFFFFFFFF && sprite->length == 0xFFFFFFFF)
				{
					break;
				}

				n = (sprite->y * picture->width * 4) + sprite->x * 4;

				for (int i = 0; i < block_end; i++, pos++)
				{
					unsigned char pixel = readspritepic[pos];
					int pixel_ix = sprite->y * picture->width + sprite->x + i;

					outbuf[n] = palette[pixel].r;
					n++;
					outbuf[n] = palette[pixel].g;
					n++;
					outbuf[n] = palette[pixel].b;
					n++;
					outbuf[n] = palette[pixel].a;
					n++;
				}

				startpos += block_end + BLOCKHEADERSIZE;
				pos += BLOCKHEADERSIZE;

				memcpy(spritepic, item + startpos, itemlength - startpos);

				sprite = (GFX_SPRITE*)spritepic;
				block_end = sprite->length;
			}
		}

		return outbuf;
	}

	return 0;
}

static int VerifyAGX(char* item, int mode, int itemlength)
{
	ANIMLINE* verify;
	char* getitem;

	verify = (ANIMLINE*)item;

	if (itemlength == 8)
	{
		if (verify->opt == 0 && verify->fill == 0 && verify->length == 0 && verify->offset == 0)
			return 1;
		else
			return 0;
	}
	else if (mode == 0 && itemlength > 8)
	{
		getitem = (char*)malloc(itemlength + 8);
		memcpy(getitem, item + 1, itemlength - 1);
		verify = (ANIMLINE*)getitem;

		if (verify->opt != 1 || verify->length == 0 || verify->offset == 0)
			return 0;

		if (verify->length > itemlength)
			return 0;

		memcpy(getitem, item + 9 + verify->length, itemlength - verify->length + 9);
		verify = (ANIMLINE*)getitem;

		if (verify->opt == 1)
		{
			if (verify->length == 0 || verify->offset == 0 || verify->length > itemlength)
				return 0;

			return 1;
		}
		else if (verify->opt == 0)
		{
			if (verify->length > 0 || verify->offset > 0 || verify->length > itemlength)
				return 0;

			return 1;
		}
		else if (verify->opt > 1 || verify->opt < 0)
			return 0;
		else
			return 1;
	}
	else if (mode == 1 && itemlength > 8)
	{
		getitem = (char*)malloc(itemlength + 8);
		memcpy(getitem, item, itemlength);
		verify = (ANIMLINE*)getitem;

		if (verify->opt != 1 || verify->length == 0 || verify->offset == 0)
			return 0;

		if (verify->length > itemlength)
			return 0;

		memcpy(getitem, item + 8 + verify->length, itemlength - verify->length + 8);
		verify = (ANIMLINE*)getitem;

		if (verify->opt == 1)
		{
			if (verify->length == 0 || verify->offset == 0 || verify->length > itemlength)
				return 0;

			return 1;
		}
		else if (verify->opt == 0)
		{
			if (verify->length > 0 || verify->offset > 0 || verify->length > itemlength)
				return 0;

			return 1;
		}
		else if (verify->opt > 1 || verify->opt < 0)
			return 0;
		else
			return 1;
	}
	else
		return 0;
}

char* ConvertGraphicsAGX(char* item, char* itemname, int itemlength)
{
	ANIMLINE* agx;
	ANIMLINE* verifyagx;
	char* bufferpic;
	char* rawvga;
	char* startagx;
	int readstart;
	int startoffset;
	int emptyframe = 0;

	if (itemlength < 8)
		return 0;

	startagx = (char*)malloc(itemlength);
	memcpy(startagx, item, itemlength);

	readstart = startagx[0];
	verifyagx = (ANIMLINE*)startagx;

	if (!VerifyAGX(item, readstart, itemlength))
	{
		return 0;
	}

	if (verifyagx->opt == 1 || itemlength == 8)
	{
		startoffset = 8;
		readstart = 0;
	}
	else
	{
		startoffset = 9;
		readstart = 1;
	}

	if (verifyagx->fill == 0 && verifyagx->length == 0 &&
		verifyagx->offset == 0 && verifyagx->opt == 0 &&
		itemlength == 8)
		emptyframe = 1;

	bufferpic = (char*)malloc(itemlength);
	rawvga = (char*)malloc(itemlength - startoffset);

	if (bufferpic)
	{
		memcpy(bufferpic, item + readstart, itemlength - readstart);
		agx = (ANIMLINE*)bufferpic;

		if (!emptyframe)
			memcpy(rawvga, item + startoffset, itemlength - startoffset);

		int pos = 0;
		int n = 0;
		char* outbuf;

		outbuf = (char*)malloc(4 * (320 * 200));

		char* readagxpos;
		int startpos = startoffset;

		readagxpos = (char*)malloc(itemlength - startoffset);

		if (!emptyframe)
			memcpy(readagxpos, item + startoffset, itemlength - startoffset);

		int block_end = agx->length;
		int transcnt = 0;

		for (int i = 0; i < 320 * 200; i++, transcnt++)
		{
			outbuf[transcnt] = 0;
			transcnt++;
			outbuf[transcnt] = 0;
			transcnt++;
			outbuf[transcnt] = 0;
			transcnt++;

			if (readstart)
				outbuf[transcnt] = 255;
			else
				outbuf[transcnt] = 0;
		}

		if (emptyframe)
			goto drawemptyframe;

		while (1)
		{
			if (!agx->opt)
			{
				break;
			}

			n = agx->offset * 4;

			for (int i = 0; i < block_end; i++, pos++)
			{
				unsigned char pixel = rawvga[pos];

				outbuf[n] = palette[pixel].r;
				n++;
				outbuf[n] = palette[pixel].g;
				n++;
				outbuf[n] = palette[pixel].b;
				n++;
				outbuf[n] = palette[pixel].a;
				n++;
			}

			startpos += block_end;
			pos += 8;

			memcpy(readagxpos, item + startpos, itemlength - startpos);
			agx = (ANIMLINE*)readagxpos;
			block_end = agx->length;

			startpos += 8;
		}

	drawemptyframe:;

		return outbuf;
	}

	return 0;
}

char* ConvertGraphicsMAP(char* item, char* itemname, int itemlength)
{
	MAZELEVEL* map;
	char* buffer;
	char* tilebuffer;
	char* outbuffer;
	char starttilelabel[14];
	int pos = 0;
	int updatepos = 4 * 32;
	int rowcount = 0;
	int tilecount = 0;
	int startpos = 0;

	if (!itemlength)
		return 0;

	map = (MAZELEVEL*)item;

	if (map->sizerec != itemlength)
		return 0;

	outbuffer = (char*)malloc(4 * 288 * 4800);

	if (!tileidflag)
	{
		for (int i = 0; i < 15; i++)
		{
			sprintf(starttilelabel, "STARTG%dTILES", i);
			tileid[i] = GLB_GetItemID(starttilelabel);

			if (tileid[i] != -1)
			{
				tileidflag = 1;
				tileid[i] += 1;
			}
		}
	}

	if (!tileidflag)
		return 0;

	for (int i = 0; i < 1350; i++)
	{
		if (tileid[map->map[i].fgame + 1] == -1)
		{
			printf("%s needs tileset %d conversion aborted\n", itemname, map->map[i].fgame);
			return 0;
		}

		buffer = GLB_GetItem(map->map[i].flats + tileid[map->map[i].fgame + 1]);

		if (!buffer)
			return 0;

		tilebuffer = ConvertGraphics(buffer, NULL, 1044);

		for (int n = 0; n < 1024 * 4; pos++, n++)
		{
			outbuffer[pos] = tilebuffer[n];

			if (n == updatepos - 1)
			{
				pos += 4 * 256;
				updatepos += 4 * 32;
			}
		}

		rowcount++;
		tilecount++;

		updatepos = 4 * 32;

		if (rowcount == 9)
		{
			startpos += 31 * 1152;
			rowcount = 0;
		}

		pos = tilecount * 4 * 32 + startpos;
	}

	return outbuffer;
}