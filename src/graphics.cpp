#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "main.h"
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
			printf("%s is not a PIC, BLK or TILE item\n", itemname);
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