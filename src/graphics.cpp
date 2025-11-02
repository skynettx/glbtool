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

#define MAX_FLIGHT      30
#define MAX_GUNS        24

int tileidflag = 0;
int tileid[15];
int flatidflag = 0;
int flatid[15];
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

typedef struct
{
	int link;
	int slib;
	int x;
	int y;
	int game;
	int level;
}CSPRITE;

typedef struct
{
	int linkflat;
	short bonus;
	short bounty;
}FLATS;

typedef struct
{
	char iname[16];                         // ITEM NAME
	int item;                               // * GLB ITEM #
	int bonus;                              // BONUS # ( -1 == NONE )
	int exptype;                            // EXPLOSION TYPE
	int shootspace;                         // SLOWDOWN SPEED
	int ground;                     //NOT USED IS ON GROUND
	int suck;                               // CAN SUCK WEAPON AFFECT
	int frame_rate;                         // FRAME RATE
	int num_frames;                         // NUM FRAMES
	int countdown;                          // COUNT DOWN TO START ANIM
	int rewind;                             // FRAMES TO REWIND
	int animtype;                           // FREE SPACE FOR LATER USE
	int shadow;                             // USE SHADOW ( TRUE/FALSE )
	int bossflag;                           // SHOULD THIS BE CONSIDERED A BOSS
	int hits;                               // HIT POINTS
	int money;                              // $$ AMOUNT WHEN KILLED
	int shootstart;                         // SHOOT START OFFSET
	int shootcnt;                           // HOW MANY TO SHOOT
	int shootframe;                         // FRAME RATE TO SHOOT
	int movespeed;                          // MOVEMENT SPEED
	int numflight;                          // NUMBER OF FLIGHT POSITIONS
	int repos;                              // REPEAT TO POSITION
	int flighttype;                         // FLIGHT TYPE
	int numguns;                            // NUMBER OF GUNS
	int numengs;                            // NUMBER OF ENGINES
	int sfx;                        //NOT USED SFX # TO PLAY
	int song;                               // SONG # TO PLAY
	short shoot_type[MAX_GUNS];             // ENEMY SHOOT TYPE
	short engx[MAX_GUNS];                   // X POS ENGINE FLAME
	short engy[MAX_GUNS];                   // Y POS ENGINE FLAME
	short englx[MAX_GUNS];                  // WIDTH OF ENGINE FLAME
	short shootx[MAX_GUNS];                 // X POS SHOOT FROM
	short shooty[MAX_GUNS];                 // Y POS SHOOT FROM
	short flightx[MAX_FLIGHT];              // FLIGHT X POS
	short flighty[MAX_FLIGHT];              // FLIGHT Y POS
}SPRITE;

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

	if (item)
	{
		picture = (GFX_PIC*)item;

		int verify;
		verify = LE_LONG(picture->width) * LE_LONG(picture->height) + HEADERSIZE;

		if (LE_LONG((int)picture->type) == GPIC && itemlength != verify ||
			LE_LONG(picture->width) > 320 || LE_LONG(picture->height) > 200 ||
			!LE_LONG(picture->width) || !LE_LONG(picture->height) ||
			LE_LONG((int)picture->type) < 0 || LE_LONG((int)picture->type) > 1 ||
			LE_LONG(picture->opt1) < 0 || LE_LONG(picture->opt2) < 0)
		{
			return 0;
		}

		int pos = 0;
		int n = 0;

		outbuf = (char*)malloc(4 * (LE_LONG(picture->width) * LE_LONG(picture->height)));

		if (LE_LONG((int)picture->type) == GPIC)
		{
			int transcnt = 0;

			rawvga = (char*)malloc(itemlength - HEADERSIZE);
			memcpy(rawvga, item + HEADERSIZE, itemlength - HEADERSIZE);

			for (int i = 0; i < LE_LONG(picture->width) * LE_LONG(picture->height); i++, transcnt++)
			{
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
				transcnt++;
				outbuf[transcnt] = 0;
			}

			for (int i = 0; i < LE_LONG(picture->width) * LE_LONG(picture->height); i++, pos++)
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

			free(rawvga);
		}

		if (LE_LONG((int)picture->type) == GSPRITE)
		{
			char* spritepic;
			char* readspritepic;
			int startpos = HEADERSIZE;

			readspritepic = (char*)malloc(itemlength - 36);
			memcpy(readspritepic, item + 36, itemlength - 36);

			spritepic = (char*)malloc(itemlength - HEADERSIZE);
			memcpy(spritepic, item + HEADERSIZE, itemlength - HEADERSIZE);

			sprite = (GFX_SPRITE*)spritepic;
			int block_end = LE_LONG(sprite->length);

			int transcnt = 0;

			for (int i = 0; i < LE_LONG(picture->width) * LE_LONG(picture->height); i++, transcnt++)
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
				if (LE_LONG(sprite->offset) == 0xFFFFFFFF && LE_LONG(sprite->length) == 0xFFFFFFFF)
				{
					break;
				}

				n = (LE_LONG(sprite->y) * LE_LONG(picture->width) * 4) + LE_LONG(sprite->x) * 4;

				for (int i = 0; i < block_end; i++, pos++)
				{
					unsigned char pixel = readspritepic[pos];

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
				block_end = LE_LONG(sprite->length);
			}

			free(spritepic);
			free(readspritepic);
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
		if (LE_USHORT(verify->opt) == 0 && LE_USHORT(verify->fill) == 0 && LE_USHORT(verify->length) == 0 && LE_USHORT(verify->offset) == 0)
			return 1;
		else
			return 0;
	}
	else if (mode == 0 && itemlength > 8)
	{
		getitem = (char*)malloc(itemlength + 8);
		memcpy(getitem, item + 1, itemlength - 1);
		verify = (ANIMLINE*)getitem;

		if (LE_USHORT(verify->opt) != 1 || LE_USHORT(verify->length) == 0 || LE_USHORT(verify->offset) == 0)
		{
			free(getitem);
			return 0;
		}

		if (LE_USHORT(verify->length) > itemlength)
		{
			free(getitem);
			return 0;
		}

		memcpy(getitem, item + 9 + LE_USHORT(verify->length), itemlength - LE_USHORT(verify->length) + 9);
		verify = (ANIMLINE*)getitem;

		if (verify->opt == 1)
		{
			if (LE_USHORT(verify->length) == 0 || LE_USHORT(verify->offset) == 0 || LE_USHORT(verify->length) > itemlength)
			{
				free(getitem);
				return 0;
			}

			free(getitem);
			return 1;
		}
		else if (LE_USHORT(verify->opt) == 0)
		{
			if (LE_USHORT(verify->length) > 0 || LE_USHORT(verify->offset) > 0 || LE_USHORT(verify->length) > itemlength)
			{
				free(getitem);
				return 0;
			}

			free(getitem);
			return 1;
		}
		else if (LE_USHORT(verify->opt) > 1 || LE_USHORT(verify->opt) < 0)
		{
			free(getitem);
			return 0;
		}
		else
		{
			free(getitem);
			return 1;
		}
	}
	else if (mode == 1 && itemlength > 8)
	{
		getitem = (char*)malloc(itemlength + 8);
		memcpy(getitem, item, itemlength);
		verify = (ANIMLINE*)getitem;

		if (LE_USHORT(verify->opt) != 1 || LE_USHORT(verify->length) == 0 || LE_USHORT(verify->offset) == 0)
		{
			free(getitem);
			return 0;
		}

		if (LE_USHORT(verify->length) > itemlength)
		{
			free(getitem);
			return 0;
		}

		memcpy(getitem, item + 8 + LE_USHORT(verify->length), itemlength - LE_USHORT(verify->length) + 8);
		verify = (ANIMLINE*)getitem;

		if (LE_USHORT(verify->opt) == 1)
		{
			if (LE_USHORT(verify->length) == 0 || LE_USHORT(verify->offset) == 0 || LE_USHORT(verify->length) > itemlength)
			{
				free(getitem);
				return 0;
			}

			free(getitem);
			return 1;
		}
		else if (LE_USHORT(verify->opt) == 0)
		{
			if (LE_USHORT(verify->length) > 0 || LE_USHORT(verify->offset) > 0 || LE_USHORT(verify->length) > itemlength)
			{
				free(getitem);
				return 0;
			}

			free(getitem);
			return 1;
		}
		else if (LE_USHORT(verify->opt) > 1 || LE_USHORT(verify->opt) < 0)
		{
			free(getitem);
			return 0;
		}
		else
		{
			free(getitem);
			return 1;
		}
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
		free(startagx);
		return 0;
	}

	if (LE_USHORT(verifyagx->opt) == 1 || itemlength == 8)
	{
		startoffset = 8;
		readstart = 0;
	}
	else
	{
		startoffset = 9;
		readstart = 1;
	}

	if (LE_USHORT(verifyagx->fill) == 0 && LE_USHORT(verifyagx->length) == 0 &&
		LE_USHORT(verifyagx->offset) == 0 && LE_USHORT(verifyagx->opt) == 0 &&
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

		int block_end = LE_USHORT(agx->length);
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
			if (!LE_USHORT(agx->opt))
			{
				break;
			}

			n = LE_USHORT(agx->offset) * 4;

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
			block_end = LE_USHORT(agx->length);

			startpos += 8;
		}

	drawemptyframe:;

		free(startagx);
		free(bufferpic);
		free(rawvga);
		free(readagxpos);

		return outbuf;
	}

	return 0;
}

char* ConvertGraphicsMAP(char* item, char* itemname, int itemlength)
{
	MAZELEVEL* map;
	CSPRITE* sprite;
	SPRITE* spriteitm;
	GFX_PIC* spritepic;
	GFX_SPRITE* convsprite;
	GFX_PIC* convpic;
	FLATS* flat = NULL;
	char* buffer;
	char* tilebuffer;
	char* outbuffer;
	char starttilelabel[14];
	char itmname[14];
	char flatlabel[14];
	char* flatbuffer = NULL;
	char* getflat = NULL;
	char* getsprite;
	char* getspriteitm;
	char* getspriteitmbuf;
	char* spritedata;
	char* spriteairground;
	char* convspritepic;
	char* convreadspritepic;
	char* rawvga;
	int spritepos = 0;
	int itembufsize;
	int spritebufsize;
	int pos = 0;
	int updatepos = 32;
	int rowcount = 0;
	int tilecount = 0;
	int startpos = 0;
	int n = 0;
	int sitmnumber;
	int rowmax;
	int rowx;
	int rowmaxflag = 0;
	int groundflag;
	int block_end;
	int possprite;
	int pospal = 0;
	int getitemid;
	int saveitmid;
	int checkmode[6];
	int flatsize;
	int saveflat = 0;

	if (!itemlength)
		return 0;

	map = (MAZELEVEL*)item;

	if (LE_LONG(map->sizerec) != itemlength)
		return 0;

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

	if (convgraphicmapdebrisflag)
	{
		if (!flatidflag)
		{
			for (int i = 0; i < 15; i++)
			{
				sprintf(flatlabel, "FLATSG%d_ITM", i);
				flatid[i] = GLB_GetItemID(flatlabel);

				if (flatid[i] != -1)
				{
					flatidflag = i;
				}
			}
		}
	}

	outbuffer = (char*)malloc(4 * 288 * 4800);
	tilebuffer = (char*)malloc(1024);

	for (int i = 0; i < 1350; i++)
	{
		if (tileid[LE_SHORT(map->map[i].fgame) + 1] == -1)
		{
			printf("%s needs tileset %d conversion aborted\n", itemname, LE_SHORT(map->map[i].fgame));
			free(outbuffer);
			free(tilebuffer);
			return 0;
		}

		if (convgraphicmapdebrisflag)
		{
			if (flatid[LE_SHORT(map->map[i].fgame) + 1] == -1)
			{
				printf("%s needs FLATSG%d_ITM conversion aborted\n", itemname, LE_SHORT(map->map[i].fgame));
				free(outbuffer);
				free(tilebuffer);
				return 0;
			}

			if (saveflat != LE_SHORT(map->map[i].fgame) + 1)
			{
				flatbuffer = GLB_GetItem(flatid[LE_SHORT(map->map[i].fgame) + 1]);

				if (!flatbuffer)
				{
					printf("Couldnt not load FLATSG%d_ITM conversion aborted\n", flatid[LE_SHORT(map->map[i].fgame) + 1]);
					free(outbuffer);
					free(tilebuffer);
					return 0;
				}

				flatsize = GLB_ItemSize(flatid[LE_SHORT(map->map[i].fgame) + 1]);
				getflat = (char*)malloc(flatsize);

				saveflat = LE_SHORT(map->map[i].fgame) + 1;
			}

			memcpy(getflat, flatbuffer + LE_SHORT(map->map[i].flats) * 8, flatsize - LE_SHORT(map->map[i].flats) * 8);
			flat = (FLATS*)getflat;

			if (LE_SHORT(map->map[i].flats) == LE_LONG(flat->linkflat))
				buffer = GLB_GetItem(LE_SHORT(map->map[i].flats) + tileid[LE_SHORT(map->map[i].fgame) + 1]);
			else
				buffer = GLB_GetItem(LE_LONG(flat->linkflat) + tileid[LE_SHORT(map->map[i].fgame) + 1]);
		}
		else
			buffer = GLB_GetItem(LE_SHORT(map->map[i].flats) + tileid[LE_SHORT(map->map[i].fgame) + 1]);

		if (!buffer)
		{
			free(outbuffer);
			free(tilebuffer);

			if (convgraphicmapdebrisflag)
				free(getflat);

			return 0;
		}

		memcpy(tilebuffer, buffer + 20, 1044 - 20);

		for (int n = 0; n < 1024; pos++, n++)
		{
			outbuffer[pos] = tilebuffer[n];

			if (n == updatepos - 1)
			{
				pos += 256;
				updatepos += 32;
			}
		}

		rowcount++;
		tilecount++;

		updatepos = 32;

		if (rowcount == 9)
		{
			startpos += 31 * 288;
			rowcount = 0;
		}

		pos = tilecount * 32 + startpos;
	}

	if (convgraphicmapflag || convgraphicmapdebrisflag)
		goto skipspritemode;

	getsprite = (char*)malloc(itemlength);
	memcpy(getsprite, item + LE_LONG(map->spriteoff), itemlength - LE_LONG(map->spriteoff));
	sprite = (CSPRITE*)getsprite;
	spriteairground = (char*)malloc(4800 * 288);

	sprintf(itmname, "SPRITE%d_ITM", LE_LONG(sprite->game) + 1);
	saveitmid = LE_LONG(sprite->game);
	getitemid = GLB_GetItemID(itmname);

	if (getitemid == -1)
	{
		printf("%s needs %s conversion aborted\n", itemname, itmname);
		free(outbuffer);
		free(tilebuffer);
		free(getsprite);
		free(spriteairground);
		return 0;
	}

	getspriteitm = GLB_GetItem(getitemid);
	itembufsize = GLB_ItemSize(getitemid);
	getspriteitmbuf = (char*)malloc(itembufsize);
	spriteitm = (SPRITE*)getspriteitm;

	for (int i = 0; i < 4800 * 288; i++)
	{
		spriteairground[i] = 0;
	}

	for (int i = 0; i < 6; i++)
	{
		checkmode[i] = 0;
	}

	if (eemode != -1)
		eemode = -3 + diffmode;

	for (int i = 0; i <= diffmode; i++)
	{
		if (i <= diffmode && i > 2)
			checkmode[i] = 1;

		if (i <= eemode && eemode != -1)
			checkmode[i] = 1;
	}

	for (int i = 0; i <= LE_LONG(map->numsprites); i++)
	{
		if (LE_LONG(sprite->game) == saveitmid)
			memcpy(getspriteitmbuf, getspriteitm + LE_LONG(sprite->slib) * 528, itembufsize - LE_LONG(sprite->slib) * 528);

		if (checkmode[LE_LONG(sprite->level)] != 1)
			goto skipsprite;

		if (LE_LONG(sprite->game) != saveitmid)
		{
			sprintf(itmname, "SPRITE%d_ITM", LE_LONG(sprite->game) + 1);
			saveitmid = LE_LONG(sprite->game);
			getitemid = GLB_GetItemID(itmname);

			if (getitemid == -1)
			{
				printf("%s needs %s conversion aborted\n", itemname, itmname);
				free(outbuffer);
				free(tilebuffer);
				free(getsprite);
				free(spriteairground);
				free(getspriteitmbuf);
				return 0;
			}

			getspriteitm = GLB_GetItem(getitemid);
			itembufsize = GLB_ItemSize(getitemid);
			getspriteitmbuf = (char*)malloc(itembufsize);
			memcpy(getspriteitmbuf, getspriteitm + LE_LONG(sprite->slib) * 528, itembufsize - LE_LONG(sprite->slib) * 528);
		}

		spriteitm = (SPRITE*)getspriteitmbuf;

		sitmnumber = GLB_GetItemID(spriteitm->iname);

		if (sitmnumber == -1)
		{
			printf("%s needs %s conversion aborted\n", itemname, spriteitm->iname);
			free(outbuffer);
			free(tilebuffer);
			free(getsprite);
			free(spriteairground);
			free(getspriteitmbuf);
			return 0;
		}

		buffer = GLB_GetItem(sitmnumber);
		spritebufsize = GLB_ItemSize(sitmnumber);

		spritepic = (GFX_PIC*)buffer;

		startpos = HEADERSIZE;

		convreadspritepic = (char*)malloc(spritebufsize - 36);
		memcpy(convreadspritepic, buffer + 36, spritebufsize - 36);

		convspritepic = (char*)malloc(spritebufsize - HEADERSIZE);
		memcpy(convspritepic, buffer + HEADERSIZE, spritebufsize - HEADERSIZE);

		convsprite = (GFX_SPRITE*)convspritepic;
		convpic = (GFX_PIC*)buffer;

		block_end = LE_LONG(convsprite->length);

		possprite = 0;
		n = 0;

		spritedata = (char*)malloc(LE_LONG(convpic->width) * LE_LONG(convpic->height));

		for (int i = 0; i < LE_LONG(convpic->width) * LE_LONG(convpic->height); i++)
		{
			spritedata[i] = 0;
		}

		while (1)
		{
			if (LE_LONG(convsprite->offset) == 0xFFFFFFFF && LE_LONG(convsprite->length) == 0xFFFFFFFF)
			{
				break;
			}

			n = (LE_LONG(convsprite->y) * LE_LONG(convpic->width)) + LE_LONG(convsprite->x);

			for (int i = 0; i < block_end; i++, possprite++)
			{
				spritedata[n] = convreadspritepic[possprite];
				n++;
			}

			startpos += block_end + BLOCKHEADERSIZE;
			possprite += BLOCKHEADERSIZE;

			memcpy(convspritepic, buffer + startpos, spritebufsize - startpos);

			convsprite = (GFX_SPRITE*)convspritepic;
			block_end = LE_LONG(convsprite->length);
		}

		pos = ((LE_LONG(sprite->y) * 9 * 32 * 32) + (LE_LONG(sprite->x) * 32));

		if (LE_LONG(spritepic->width) == 96 && LE_LONG(spritepic->height) == 96)
		{
			pos -= 9248;
		}
		else if (LE_LONG(spritepic->width) == 96 && LE_LONG(spritepic->height) == 64)
		{
			pos -= 4640;
		}
		else if (LE_LONG(spritepic->width) == 64 && LE_LONG(spritepic->height) == 64)
		{
			pos -= 4624;
		}
		else if (LE_LONG(spritepic->width) == 24 && LE_LONG(spritepic->height) == 24)
		{
			pos += 1156;
		}
		else if (LE_LONG(spritepic->width) == 16 && LE_LONG(spritepic->height) == 16)
		{
			pos += 2312;
		}

		if (pos < 0 || pos > 4800 * 288)
			pos = ((LE_LONG(sprite->y) * 9 * 32 * 32) + (LE_LONG(sprite->x) * 32));

		rowx = 32 * (9 - LE_LONG(sprite->x));
		rowmax = ((LE_LONG(sprite->y) * 9 * 32 * 32) + (9 * 32));
		rowcount = 1;

		if (LE_LONG(spritepic->width) > rowx)
			rowmaxflag = 1;
		else
			rowmaxflag = 0;

		if (LE_LONG(spriteitm->flighttype) == 3 || LE_LONG(spriteitm->flighttype) == 4 || LE_LONG(spriteitm->flighttype) == 5)
			groundflag = 1;
		else
			groundflag = 0;

		for (int i = 0; i < LE_LONG(spritepic->height) * LE_LONG(spritepic->width); i++, pos++)
		{
			if (LE_LONG(spritepic->width) > rowx && pos == rowmax)
			{
				rowmax += 288;
				pos += (288 - rowx);
				i += LE_LONG(spritepic->width) - rowx;

				if (i == LE_LONG(spritepic->height) * LE_LONG(spritepic->width))
					i--;
			}

			if (spritedata[i] != 0)
			{

				outbuffer[pos] = spritedata[i];

				if (groundflag && spriteairground[pos] != 0)
					outbuffer[pos] = spriteairground[pos];

				spriteairground[pos] = outbuffer[pos];
			}

			if (rowcount == LE_LONG(spritepic->width) && !rowmaxflag)
			{
				pos += (288 - LE_LONG(spritepic->width));

				rowcount = 0;
			}

			rowcount++;
		}

		free(convreadspritepic);
		free(convspritepic);

	skipsprite:;

		memcpy(getsprite, item + LE_LONG(map->spriteoff) + spritepos, itemlength - LE_LONG(map->spriteoff) - spritepos);
		sprite = (CSPRITE*)getsprite;

		spritepos += 24;
	}

	free(getsprite);
	free(getspriteitmbuf);
	free(spriteairground);

skipspritemode:;

	pospal = 0;
	n = 0;

	rawvga = (char*)malloc(4800 * 288);
	memcpy(rawvga, outbuffer, 4800 * 288);

	for (int i = 0; i < 288 * 4800; i++, pospal++)
	{
		unsigned char pixel = rawvga[pospal];

		outbuffer[n] = palette[pixel].r;
		n++;
		outbuffer[n] = palette[pixel].g;
		n++;
		outbuffer[n] = palette[pixel].b;
		n++;
		outbuffer[n] = palette[pixel].a;
		n++;
	}

	free(tilebuffer);
	free(rawvga);

	if (convgraphicmapdebrisflag)
	{
		free(getflat);
	}

	return outbuffer;
}
