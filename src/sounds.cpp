#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "sounds.h"

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

struct dsp_t
{
	int16_t format;
	int16_t freq;
	int32_t length;
};

struct waveheader_t
{
	uint8_t ChunkID[4];
	uint32_t ChunkSize;
	uint8_t Subchunk1ID[8];
	uint32_t Subchunk1Size;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;
	uint8_t Subchunk2ID[4];
	uint32_t Subchunk2Size;
};

static void SetWAVEHeader(waveheader_t* waveheader, int itemlength)
{
	strcpy((char*)waveheader->ChunkID, "RIFF");
	waveheader->ChunkSize = itemlength + 36;
	strcpy((char*)waveheader->Subchunk1ID, "WAVEfmt ");
	waveheader->Subchunk1Size = 16;
	waveheader->AudioFormat = 1;
	waveheader->NumChannels = 1;
	waveheader->SampleRate = 11025;
	waveheader->BitsPerSample = 8;
	waveheader->ByteRate = waveheader->SampleRate * waveheader->NumChannels * waveheader->BitsPerSample / 8;
	waveheader->BlockAlign = waveheader->NumChannels * waveheader->BitsPerSample / 8;
	strcpy((char*)waveheader->Subchunk2ID, "data");
	waveheader->Subchunk2Size = itemlength - 8;
}

char* ConvertSounds(char* item, char* itemname, int itemlength)
{
	waveheader_t* waveheader;
	dsp_t* dspheader;
	char* outbuffer;
	int n = 8;

	if (!itemlength)
		return 0;

	dspheader = (dsp_t*)item;

	if (dspheader->format != 3 || dspheader->freq != 11025 ||
		dspheader->length != itemlength - 8)
		return 0;

	waveheader = (waveheader_t*)malloc(sizeof(waveheader_t));

	SetWAVEHeader(waveheader, itemlength);

	outbuffer = (char*)malloc(itemlength + 36);

	if (outbuffer && waveheader)
		memcpy(outbuffer, waveheader, sizeof(waveheader_t));
	else
		return 0;

	for (int i = 44; n < itemlength; i++, n++)
	{
		outbuffer[i] = item[n];
	}

	free(waveheader);

	return outbuffer;
}