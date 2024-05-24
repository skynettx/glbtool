typedef enum
{
	GSPRITE,
	GPIC
}GFX_TYPE;

typedef struct
{
	GFX_TYPE type;          // type of picture
	int opt1;               // option 1
	int opt2;               // option 2
	int width;              // width of pic
	int height;             // heigth of pic
}GFX_PIC;

int SetPalette(char* palfilename);
char* ConvertGraphics(char* item, char* itemname, int itemlength);
void WriteGraphics(char* outbuffer, char* outdirectory, int picwidth, int picheight);
