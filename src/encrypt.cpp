/*
 * Copyright (C) 2016-2017 Yggdrasill (kaymeerah@lambda.is)
 *
 * This file is part of glbtools.
 *
 * glbtools is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glbtools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glbtools.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#define fileno _fileno
#define write _write
#define read _read
#endif

struct State {
     char current_byte;
     char prev_byte;
     uint8_t key_pos;
 };

struct fitem_t hfat;
struct fitem_t temp;
struct fitem_t* ffat;
struct State state;

int calculate_key_pos(int len)
{
    return 25 % len;
}

void reset_state(struct State* state)
{
    state->key_pos = calculate_key_pos(strlen(serial));
    state->prev_byte = serial[state->key_pos];

    return;
}

static int encrypt_varlen(struct State* state, void* data, int size)
{
    char* current_byte;
    char* prev_byte;
    char* byte_data;
    uint8_t* key_pos;

    int i;

    current_byte = &state->current_byte;
    prev_byte = &state->prev_byte;
    key_pos = &state->key_pos;
    byte_data = (char*)data;

    for (i = 0; i < size; i++)
    {
        *current_byte = byte_data[i];
        byte_data[i] = *current_byte + serial[*key_pos] + *prev_byte;
        byte_data[i] &= 0xFF;
        (*key_pos)++;
        *key_pos %= strlen(serial);
        *prev_byte = byte_data[i];
    }

    return i;
}

int encrypt_int(struct State* state, int* data)
{
    return encrypt_varlen(state, data, 4);
}

int encrypt_filename(struct State* state, char* str)
{
    *(str + 16 - 1) = '\0';
    return encrypt_varlen(state, str, 16);
}

int encrypt_fat_single(struct State* state, struct fitem_t* fat)
{
    int retval;

    reset_state(state);

    retval = encrypt_int(state, &fat->flags);
    retval += encrypt_int(state, &fat->offset);
    retval += encrypt_int(state, &fat->length);
    retval += encrypt_filename(state, fat->name);

    return retval;
}

int encrypt_file(struct State* state, char* str, int length)
{
    if (!length) return 0;
    reset_state(state);
    return encrypt_varlen(state, str, length);
}

int fat_io_write(struct fitem_t* fat, int fd)
{
    char buffer[28];

    int pos;

    pos = 0;

    memcpy(buffer, &fat->flags, 4);
    pos += 4;
    memcpy(buffer + pos, &fat->offset, 4);
    pos += 4;
    memcpy(buffer + pos, &fat->length, 4);
    pos += 4;
    memcpy(buffer + pos, fat->name, 16);
    pos += 16;

    return write(fd, buffer, 28);
}

int fat_entry_init(struct fitem_t* fat, char* path, int offset)
{
    struct stat st;

    int len;

    if (stat(path, &st))
    {
        return -1;
    }

    fat->flags = 0;
    fat->offset = offset;
    fat->length = st.st_size;

    len = strlen(path);

    if (len > 16)
        path += len - 16 + 1;

    strcpy(fat->name, path);

    strncpy(fat->name, RemovePathFromString(fat->name), 16);

    return 0;
}

void fat_flag_encryption(struct fitem_t* ffat, int nfiles)
{
    struct fitem_t* end;

    end = ffat + nfiles;

    for (; ffat < end; ffat++)
    {
        ffat->flags = 1;
    }

    return;
}

void GLB_Create(char* outfilename)
{
    char* buffer;
    int largest;
    int offset;
    int bytes;
    int nfiles;
    int i;
    int filecnt = 0;
    int rd, wd;

    largest = 0;

    memset(&hfat, 0, sizeof(hfat));

    if (encryptflag)
    {
        filecnt = 2;
        nfiles = allinfilenamescnt - 3;
    }
    else
    {
        filecnt = 0;
        nfiles = allinfilenamescnt;
    }

    outfile = fopen(outfilename, "wb");

    wd = fileno(outfile);
    hfat.offset = nfiles;
    encrypt_fat_single(&state, &hfat);
    fat_io_write(&hfat, wd);

    ffat = (struct fitem_t*)malloc(sizeof(*ffat) * nfiles);
    offset = nfiles * 28 + 28;

    for (i = 0; i < nfiles; i++)
    {

        if (fat_entry_init(&ffat[i], allinfilenames[filecnt], offset))
        {
            printf("Could not init fat entry %s\n", allinfilenames[filecnt]);
            EXIT_Error("Could not init fat entry");
        }

        if (ffat[i].length > largest) largest = ffat[i].length;
        offset += ffat[i].length;

        filecnt++;
    }

    fat_flag_encryption(ffat, nfiles);

    for (i = 0; i < nfiles; i++)
    {
        memcpy(&temp, &ffat[i], sizeof(ffat[i]));
        encrypt_fat_single(&state, &temp);

        bytes = fat_io_write(&temp, wd);

        if (bytes == -1)
        {
            EXIT_Error("DIE");
        }
        else if (bytes != 28) {
            printf("Bytes written not equal to file length %s\n", ffat[i].name);
        }
    }

    buffer = (char*)malloc(largest);

    if (encryptflag)
        filecnt = 2;

    else
        filecnt = 0;

    for (i = 0; i < nfiles; i++)
    {
        infile = fopen(allinfilenames[filecnt], "rb");

        if (!infile)
        {
            printf("Could not open file %s\n", allinfilenames[filecnt]);
            EXIT_Error("Could not open file");
        }

        printf("Encrypting item: %s\n", allinfilenames[filecnt]);
        itemtotal++;
        itemtotalsize += ffat[i].length;

        rd = fileno(infile);

        bytes = read(rd, buffer, ffat[i].length);

        if (bytes == -1)
        {
            EXIT_Error("DIE");
        }
        else if (bytes != ffat[i].length) {
            printf("Bytes read not equal to file length %s\n", allinfilenames[filecnt]);
        }

        encrypt_file(&state, buffer, ffat[i].length);

        bytes = write(wd, buffer, ffat[i].length);

        if (bytes == -1)
        {
            EXIT_Error("DIE");
        }
        else if (bytes != ffat[i].length)
        {
            printf("Bytes written not equal to file length %s\n", allinfilenames[filecnt]);
        }

        fclose(infile);
        filecnt++;
    }

    free(ffat);
    free(buffer);
    fclose(outfile);
}