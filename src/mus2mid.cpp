//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2006 Ben Ryves 2006
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// mus2mid.c - Ben Ryves 2006 - http://benryves.com - benryves@benryves.com
// Use to convert a MUS file into a single track, type 0 MIDI file.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mus2mid.h"

#define NUM_CHANNELS 16

#define MIDI_PERCUSSION_CHAN 9
#define MUS_PERCUSSION_CHAN 15

// MUS event codes
typedef enum
{
	mus_releasekey = 0x00,
	mus_presskey = 0x10,
	mus_pitchwheel = 0x20,
	mus_systemevent = 0x30,
	mus_changecontroller = 0x40,
	mus_scoreend = 0x60
} musevent;

// MIDI event codes
typedef enum
{
	midi_releasekey = 0x80,
	midi_presskey = 0x90,
	midi_aftertouchkey = 0xA0,
	midi_changecontroller = 0xB0,
	midi_changepatch = 0xC0,
	midi_aftertouchchannel = 0xD0,
	midi_pitchwheel = 0xE0
} midievent;

// Structure to hold MUS file header
typedef struct
{
	uint8_t id[4];
	unsigned short scorelength;
	unsigned short scorestart;
	unsigned short primarychannels;
	unsigned short secondarychannels;
	unsigned short instrumentcount;
} musheader;

// Standard MIDI type 0 header + track header
static const uint8_t midiheader[] =
{
	'M', 'T', 'h', 'd',     // Main header
	0x00, 0x00, 0x00, 0x06, // Header size
	0x00, 0x00,             // MIDI type (0)
	0x00, 0x01,             // Number of tracks
	0x00, 0x23,             // Resolution
	'M', 'T', 'r', 'k',        // Start of track
	0x00, 0x00, 0x00, 0x00  // Placeholder for track length
};

// Cached channel velocities
static uint8_t channelvelocities[] =
{
	127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127
};

// Timestamps between sequences of MUS events

static unsigned int queuedtime = 0;

// Counter for the length of the track

static unsigned int tracksize;

static const uint8_t controller_map[] =
{
	0x00, 0x20, 0x01, 0x07, 0x0A, 0x0B, 0x5B, 0x5D,
	0x40, 0x43, 0x78, 0x7B, 0x7E, 0x7F, 0x79
};

static int channel_map[NUM_CHANNELS];

int len_outfile;
int pos;

int mus_getoutfilelen(void)
{
	return len_outfile;
}

static char* write_buffer(char* inbuffer, int size, char* outbuffer)
{
	for (int i = 0; i < size; i++, pos++)
	{
		outbuffer[pos] = inbuffer[i];
	}

	return outbuffer;
}

// Write timestamp to a MIDI file.
static bool WriteTime(unsigned int time, char* midioutput)
{
	unsigned int buffer = time & 0x7F;
	uint8_t writeval;

	while ((time >>= 7) != 0)
	{
		buffer <<= 8;
		buffer |= ((time & 0x7F) | 0x80);
	}

	for (;;)
	{
		writeval = (uint8_t)(buffer & 0xFF);

		write_buffer((char*)&writeval, 1, midioutput);

		++tracksize;

		if ((buffer & 0x80) != 0)
		{
			buffer >>= 8;
		}
		else
		{
			queuedtime = 0;
			return false;
		}
	}
}


// Write the end of track marker
static bool WriteEndTrack(char* midioutput)
{
	uint8_t endtrack[] = { 0xFF, 0x2F, 0x00 };

	if (WriteTime(queuedtime, midioutput))
	{
		return true;
	}

	write_buffer((char*)endtrack, 3, midioutput);

	tracksize += 3;
	return false;
}

// Write a key press event
static bool WritePressKey(uint8_t channel, uint8_t key,
	uint8_t velocity, char* midioutput)
{
	uint8_t working = midi_presskey | channel;

	if (WriteTime(queuedtime, midioutput))
	{
		return true;
	}

	write_buffer((char*)&working, 1, midioutput);

	working = key & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	working = velocity & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	tracksize += 3;

	return false;
}

// Write a key release event
static bool WriteReleaseKey(uint8_t channel, uint8_t key,
	char* midioutput)
{
	uint8_t working = midi_releasekey | channel;

	if (WriteTime(queuedtime, midioutput))
	{
		return true;
	}

	write_buffer((char*)&working, 1, midioutput);

	working = key & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	working = 0;

	write_buffer((char*)&working, 1, midioutput);

	tracksize += 3;

	return false;
}

// Write a pitch wheel/bend event
static bool WritePitchWheel(uint8_t channel, short wheel,
	char* midioutput)
{
	uint8_t working = midi_pitchwheel | channel;

	if (WriteTime(queuedtime, midioutput))
	{
		return true;
	}

	write_buffer((char*)&working, 1, midioutput);

	working = wheel & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	working = (wheel >> 7) & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	tracksize += 3;
	return false;
}

// Write a patch change event
static bool WriteChangePatch(uint8_t channel, uint8_t patch,
	char* midioutput)
{
	uint8_t working = midi_changepatch | channel;

	if (WriteTime(queuedtime, midioutput))
	{
		return true;
	}

	write_buffer((char*)&working, 1, midioutput);

	working = patch & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	tracksize += 2;

	return false;
}

// Write a valued controller change event

static bool WriteChangeController_Valued(uint8_t channel,
	uint8_t control,
	uint8_t value,
	char* midioutput)
{
	uint8_t working = midi_changecontroller | channel;

	if (WriteTime(queuedtime, midioutput))
	{
		return true;
	}

	write_buffer((char*)&working, 1, midioutput);

	working = control & 0x7F;

	write_buffer((char*)&working, 1, midioutput);

	// Quirk in vanilla DOOM? MUS controller values should be
	// 7-bit, not 8-bit.

	working = value;// & 0x7F;

	// Fix on said quirk to stop MIDI players from complaining that
	// the value is out of range:

	if (working & 0x80)
	{
		working = 0x7F;
	}

	write_buffer((char*)&working, 1, midioutput);

	tracksize += 3;

	return false;
}

// Write a valueless controller change event
static bool WriteChangeController_Valueless(uint8_t channel,
	uint8_t control,
	char* midioutput)
{
	return WriteChangeController_Valued(channel, control, 0,
		midioutput);
}

// Allocate a free MIDI channel.

static int AllocateMIDIChannel(void)
{
	int result;
	int max;
	int i;

	// Find the current highest-allocated channel.

	max = -1;

	for (i = 0; i < NUM_CHANNELS; ++i)
	{
		if (channel_map[i] > max)
		{
			max = channel_map[i];
		}
	}

	// max is now equal to the highest-allocated MIDI channel.  We can
	// now allocate the next available channel.  This also works if
	// no channels are currently allocated (max=-1)

	result = max + 1;

	// Don't allocate the MIDI percussion channel!

	if (result == MIDI_PERCUSSION_CHAN)
	{
		++result;
	}

	return result;
}

// Given a MUS channel number, get the MIDI channel number to use
// in the outputted file.

static int GetMIDIChannel(int mus_channel, char* midioutput)
{
	// Find the MIDI channel to use for this MUS channel.
	// MUS channel 15 is the percusssion channel.

	if (mus_channel == MUS_PERCUSSION_CHAN)
	{
		return MIDI_PERCUSSION_CHAN;
	}
	else
	{
		// If a MIDI channel hasn't been allocated for this MUS channel
		// yet, allocate the next free MIDI channel.

		if (channel_map[mus_channel] == -1)
		{
			channel_map[mus_channel] = AllocateMIDIChannel();

			// First time using the channel, send an "all notes off"
			// event. This fixes "The D_DDTBLU disease" described here:
			// http://www.doomworld.com/vb/source-ports/66802-the
			WriteChangeController_Valueless(channel_map[mus_channel], 0x7b,
				midioutput);
		}

		return channel_map[mus_channel];
	}
}

// Read a MUS file from a stream (musinput) and output a MIDI file to
// a stream (midioutput).
//
// Returns 0 on success or 1 on failure.

bool mus2mid(char* musinput, char* midioutput, int infile_len)
{
	// Header for the MUS file
	musheader* musfileheader;

	// Descriptor for the current MUS event
	uint8_t eventdescriptor;
	int channel; // Channel number
	musevent event;


	// Bunch of vars read from MUS lump
	uint8_t key;
	uint8_t controllernumber;
	uint8_t controllervalue;

	// Buffer used for MIDI track size record
	uint8_t tracksizebuffer[4];

	// Flag for when the score end marker is hit.
	int hitscoreend = 0;

	// Temp working byte
	uint8_t working;
	// Used in building up time delays
	unsigned int timedelay;

	// Initialise channel map to mark all channels as unused.

	for (channel = 0; channel < NUM_CHANNELS; ++channel)
	{
		channel_map[channel] = -1;
	}

	// Grab the header


	musfileheader = (musheader*)musinput;

#ifdef CHECK_MUS_HEADER
	// Check MUS header
	if (musfileheader->id[0] != 'M'
		|| musfileheader->id[1] != 'U'
		|| musfileheader->id[2] != 'S'
		|| musfileheader->id[3] != 0x1A)
	{
		return true;
	}
#endif

	// Seek to where the data is held

	int lenadd = musfileheader->scorestart;

	// So, we can assume the MUS file is faintly legit. Let's start
	// writing MIDI data...

	write_buffer((char*)midiheader, sizeof(midiheader), midioutput);

	tracksize = 0;

	// Now, process the MUS file:
	while (!hitscoreend)
	{
		// Handle a block of events:

		while (!hitscoreend)
		{
			// Fetch channel number and event code:

			memcpy(&eventdescriptor, musinput + lenadd++, 1);

			channel = GetMIDIChannel(eventdescriptor & 0x0F, midioutput);
			event = static_cast<musevent>(eventdescriptor & 0x70);

			switch (event)
			{
			case mus_releasekey:

				memcpy(&key, musinput + lenadd++, 1);

				if (WriteReleaseKey(channel, key, midioutput))
				{
					return true;
				}

				break;

			case mus_presskey:

				memcpy(&key, musinput + lenadd++, 1);

				if (key & 0x80)
				{
					memcpy(&channelvelocities[channel], musinput + lenadd++, 1);

					channelvelocities[channel] &= 0x7F;
				}

				if (WritePressKey(channel, key,
					channelvelocities[channel], midioutput))
				{
					return true;
				}

				break;

			case mus_pitchwheel:

				memcpy(&key, musinput + lenadd++, 1);

				if (WritePitchWheel(channel, (short)(key * 64), midioutput))
				{
					return true;
				}

				break;

			case mus_systemevent:

				memcpy(&controllernumber, musinput + lenadd++, 1);

				if (controllernumber < 10 || controllernumber > 14)
				{
					return true;
				}

				if (WriteChangeController_Valueless(channel,
					controller_map[controllernumber],
					midioutput))
				{
					return true;
				}

				break;

			case mus_changecontroller:

				memcpy(&controllernumber, musinput + lenadd++, 1);

				memcpy(&controllervalue, musinput + lenadd++, 1);

				if (controllernumber == 0)
				{
					if (WriteChangePatch(channel, controllervalue,
						midioutput))
					{
						return true;
					}
				}
				else
				{
					if (controllernumber < 1 || controllernumber > 9)
					{
						return true;
					}

					if (WriteChangeController_Valued(channel,
						controller_map[controllernumber],
						controllervalue,
						midioutput))
					{
						return true;
					}
				}

				break;

			case mus_scoreend:
				hitscoreend = 1;
				break;

			default:
				return true;
				break;
			}

			if (eventdescriptor & 0x80)
			{
				break;
			}
		}
		// Now we need to read the time code:
		if (!hitscoreend)
		{
			timedelay = 0;
			for (;;)
			{
				memcpy(&working, musinput + lenadd++, 1);

				timedelay = timedelay * 128 + (working & 0x7F);
				if ((working & 0x80) == 0)
				{
					break;
				}
			}

			queuedtime += timedelay;
		}
	}

	// End of track
	if (WriteEndTrack(midioutput))
	{
		return true;
	}

	// Write the track size into the stream
	len_outfile = pos;
	pos = 18;

	tracksizebuffer[0] = (tracksize >> 24) & 0xff;
	tracksizebuffer[1] = (tracksize >> 16) & 0xff;
	tracksizebuffer[2] = (tracksize >> 8) & 0xff;
	tracksizebuffer[3] = tracksize & 0xff;

	write_buffer((char*)tracksizebuffer, 4, midioutput);

	return false;
}

char* ConvertMusic(char* item, char* itemname, int itemlength)
{
	char* outbuffer;
	musheader* musfileheader;

	musfileheader = (musheader*)item;

	if (!itemlength)
		return 0;

	if (musfileheader->id[0] != 'M'
		|| musfileheader->id[1] != 'U'
		|| musfileheader->id[2] != 'S'
		|| musfileheader->id[3] != 0x1A)
	{
		return 0;
	}

	outbuffer = (char*)malloc(2 * itemlength);
	pos = 0;

	if (mus2mid(item, outbuffer, itemlength))
	{
		fprintf(stderr, "mus2mid() failed\n");
		return 0;
	}

	return outbuffer;
}