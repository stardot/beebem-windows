/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Mike Wyatt

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

// UEF tape file functions
//
// See UEF file format at
// http://electrem.emuunlim.com/UEFSpecs.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "uef.h"
#include "zlib/zlib.h"

/* Beats representing normal tape speed (not sure why its 5600) */
#define NORMAL_TAPE_SPEED 5600

struct uef_chunk_info
{
	int type;
	int len;
	unsigned char *data;
	int l1;
	int l2;
	int unlock_offset;
	int crc;
	int start_time;
	int end_time;
};

static char uef_file_name[256];
static uef_chunk_info *uef_chunk = NULL;
static int uef_chunks = 0;
static int uef_clock_speed = 5600;
static uef_chunk_info *uef_last_chunk = NULL;
static bool uef_unlock = false;
static int uef_last_put_data=UEF_EOF;
static uef_chunk_info uef_put_chunk;

static UEFResult uef_write_chunk();
static float uef_decode_float(unsigned char *Float);
static void uef_unlock_offset_and_crc(uef_chunk_info *ch);
static int gzget16(gzFile f);
static int gzget32(gzFile f);
static void gzput16(gzFile f, int b);
static void gzput32(gzFile f, int b);

void uef_setclock(int beats)
{
	/* beats = bit samples per second? */
	uef_clock_speed = beats;
}

void uef_setunlock(bool unlock)
{
	uef_unlock = unlock;
}

UEFResult uef_create(const char *filename)
{
	uef_close();

	gzFile uef_file = gzopen(filename, "wb");

	if (uef_file == nullptr)
	{
		return UEFResult::NoFile;
	}

	gzwrite(uef_file, "UEF File!", 10);
	gzput16(uef_file, 0x000a); /* V0.10 */
	gzput16(uef_file, 0);
	gzput32(uef_file, 18);
	gzwrite(uef_file, "Created by BeebEm", 18);

	gzclose(uef_file);
	strcpy(uef_file_name, filename);

	return UEFResult::Success;
}

UEFResult uef_open(const char *name)
{
	gzFile uef_file;
	char UEFId[10];
	int ver;
	bool error = false;
	int i;
	int clock;
	int baud;
	int len;
	uef_chunk_info *ch;

	uef_close();

	uef_file = gzopen(name, "rb");
	if (uef_file == nullptr)
	{
		return UEFResult::NoFile;
	}

	gzread(uef_file, UEFId, 10);
	if (strcmp(UEFId,"UEF File!")!=0)
	{
		uef_close();

		return UEFResult::NotUEF;
	}

	ver = gzget16(uef_file);

	uef_chunks = 0;
	uef_chunk = (uef_chunk_info *)malloc(sizeof(uef_chunk_info));
	if (uef_chunk == nullptr)
	{
		uef_close();

		return UEFResult::OutOfMemory;
	}

	UEFResult Result = UEFResult::Success;

	while (Result == UEFResult::Success && !gzeof(uef_file))
	{
		ch = &uef_chunk[uef_chunks];
		ch->type = gzget16(uef_file);
		ch->len = gzget32(uef_file);

		if (ch->type >= 0x100 && ch->type <= 0x1ff)
		{
			if (ch->len > 0)
			{
				ch->data = (unsigned char *)malloc(ch->len);
				if (ch->data == NULL)
				{
					Result = UEFResult::OutOfMemory;
				}
				else if (gzread(uef_file, ch->data, ch->len) != ch->len)
				{
					Result = UEFResult::NotTape;
				}
				else
				{
					uef_chunks++;
					ch = (uef_chunk_info *)realloc(uef_chunk, (uef_chunks+1) * sizeof(uef_chunk_info));
					if (ch == NULL)
					{
						Result = UEFResult::OutOfMemory;
					}
					else
					{
						uef_chunk = ch;
					}
				}
			}
			else
			{
				ch->data = NULL;
			}
		}
		else if (ch->type >= 0x200)
		{
			Result = UEFResult::NotTape;
		}
		else if (ch->len > 0)
		{
			gzseek(uef_file, ch->len, SEEK_CUR);
		}
	}

	if (Result != UEFResult::Success)
	{
		uef_close();

		return Result;
	}

	clock = 0;
	baud = 1200;

	for (i = 0; i < uef_chunks; ++i)
	{
		ch = &uef_chunk[i];
		ch->start_time = clock;

		switch (ch->type)
		{
		case 0x100: /* Data block */
			clock += (int)((double)(ch->len) * uef_clock_speed*10.0 / baud);
			uef_unlock_offset_and_crc(ch);
			break;
		case 0x101:
			/* Not supported */
			break;
		case 0x102:
			/* Not supported */
			break;
		case 0x104: /* Data block */
			clock += (int)((double)(ch->len-3) * uef_clock_speed*10.0 / baud);
			uef_unlock_offset_and_crc(ch);
			break;
		case 0x110: /* HTone */
			ch->l1 = ch->data[0]+(ch->data[1]<<8);
			clock += (int)((double)ch->l1 * uef_clock_speed / (baud*2.0));
			break;
		case 0x111: /* HTone with dummy byte */
			ch->l1 = ch->data[0]+(ch->data[1]<<8);
			ch->l2 = ch->data[2]+(ch->data[3]<<8);
			len = ch->l1 + ch->l2 + 160; /* 160 for dummy byte */
			clock += (int)((double)len * uef_clock_speed / (baud*2.0));
			break;
		case 0x112: /* Gap */
			ch->l1 = ch->data[0]+(ch->data[1]<<8);
			clock += (int)((double)ch->l1 * uef_clock_speed / (baud*2.0));
			break;
		case 0x113: /* Baud rate */
			baud = (int)uef_decode_float(ch->data);
			if (baud <= 0)
				baud = 1200;
			break;
		case 0x114: /* Security waves */
			ch->l1 = ch->data[0]+(ch->data[1]<<8)+(ch->data[2]<<16);
			ch->l1 = (ch->l1 + 7) / 8;
			clock += (int)((double)(ch->l1) * uef_clock_speed*10.0 / baud);
			break;
		case 0x115:
			/* Not supported */
			break;
		case 0x116: /* Gap */
			clock += (int)(uef_decode_float(ch->data) * uef_clock_speed);
			break;
		case 0x120:
			/* Not supported */
			break;
		}

		ch->end_time = clock;
	}

	gzclose(uef_file);
	strcpy(uef_file_name, name);

	return UEFResult::Success;
}

int uef_getdata(int time)
{
	int i, j;
	int data;
	bool found = false;
	uef_chunk_info *ch = nullptr;

	if (uef_last_chunk != NULL &&
		time >= uef_last_chunk->start_time && time < uef_last_chunk->end_time)
	{
		ch = uef_last_chunk;
		found = true;
	}
	else
	{
		for (i = 0; i < uef_chunks && !found; ++i)
		{
			ch = &uef_chunk[i];
			if (time >= ch->start_time && time < ch->end_time)
				found = true;
		}
	}

	if (!found)
		return(UEF_EOF);

	uef_last_chunk = ch;
	data = UEF_GAP | 0x1000;

	switch (ch->type)
	{
	case 0x100: /* Data block */
	case 0x104: /* Data block */
		if (ch->type == 0x104)
			j=3;
		else
			j=0;

		i = (int)((double)(time - ch->start_time) / (ch->end_time - ch->start_time) * (ch->len-j));
		data = UEF_DATA | ch->data[i+j] | ((i & 0x7f) << 24);
		if (uef_unlock)
		{
			if (i == ch->unlock_offset)
			{
				data &= 0xfffffffe;
			}
			else if (ch->crc != -1)
			{
				if (i == ch->unlock_offset+5)
				{
					data &= 0xffffff00;
					data |= ch->crc >> 8;
				}
				else if (i == ch->unlock_offset+6)
				{
					data &= 0xffffff00;
					data |= ch->crc & 0xff;
				}
			}
		}
		break;
	case 0x101:
		/* Not supported */
		break;
	case 0x102:
		/* Not supported */
		break;
	case 0x110: /* HTone */
		data = UEF_HTONE | 0x1000;
		break;
	case 0x111: /* HTone with dummy byte */
		i = (int)((double)(time - ch->start_time) / (ch->end_time - ch->start_time) * (ch->l1+160+ch->l2));
		if (i < ch->l1 || i >= (ch->l1+160))
			data = UEF_HTONE | 0x1000;
		else
			data = UEF_DATA | 0xAA;
		break;
	case 0x112: /* Gap */
		data = UEF_GAP | 0x1000;
		break;
	case 0x113: /* Baud rate */
		break;
	case 0x114: /* Security waves */
		i = (int)((double)(time - ch->start_time) / (ch->end_time - ch->start_time) * ch->l1);
		data = UEF_DATA | ch->data[i+5] | ((i & 0x7f) << 24);
		break;
	case 0x115:
		/* Not supported */
		break;
	case 0x116: /* Gap */
		data = UEF_GAP | 0x1000;
		break;
	case 0x120:
		/* Not supported */
		break;
	}

	return(data);
}

UEFResult uef_putdata(int data, int time)
{
	UEFResult Result = UEFResult::Success;
	unsigned char *datap;

	if (UEFRES_TYPE(data) != UEFRES_TYPE(uef_last_put_data))
	{
		/* Finished a chunk */
		if (UEFRES_TYPE(uef_last_put_data) == UEF_HTONE)
		{
			if (uef_put_chunk.start_time != uef_put_chunk.end_time)
			{
				uef_put_chunk.type = 0x110;
				Result = uef_write_chunk();
			}
		}
		else if (UEFRES_TYPE(uef_last_put_data) == UEF_GAP)
		{
			if (uef_put_chunk.start_time != uef_put_chunk.end_time)
			{
				uef_put_chunk.type = 0x112;
				Result = uef_write_chunk();
			}
		}
		else if (UEFRES_TYPE(uef_last_put_data) == UEF_DATA)
		{
			uef_put_chunk.type = 0x100;
			Result = uef_write_chunk();
		}

		/* Reset for next chunk */
		uef_put_chunk.len = 0;
		uef_put_chunk.data = NULL;
		uef_put_chunk.start_time = -1;
	}

	/* Add to chunk */
	if (UEFRES_TYPE(data) == UEF_HTONE || UEFRES_TYPE(data) == UEF_GAP)
	{
		if (uef_put_chunk.start_time == -1)
		{
			uef_put_chunk.start_time = time;
			uef_put_chunk.end_time = time;
		}
		else
		{
			uef_put_chunk.end_time = time;
		}
	}
	else if (UEFRES_TYPE(data) == UEF_DATA)
	{
		/* Put data in buffer */
		if (uef_put_chunk.data == NULL)
			datap = (unsigned char *)malloc(1);
		else
			datap = (unsigned char *)realloc(uef_put_chunk.data, uef_put_chunk.len+1);

		if (datap == nullptr)
		{
			Result = UEFResult::OutOfMemory;
		}
		else
		{
			uef_put_chunk.data = datap;
			uef_put_chunk.data[uef_put_chunk.len] = UEFRES_BYTE(data);
			uef_put_chunk.len++;
		}
	}

	uef_last_put_data = data;

	return Result;
}

void uef_close(void)
{
	int i;

	if (uef_chunk != NULL)
	{
		for (i = 0; i < uef_chunks; ++i)
		{
			if (uef_chunk[i].data != NULL)
				free(uef_chunk[i].data);
		}
		free(uef_chunk);
		uef_chunk = NULL;
		uef_chunks = 0;
	}

	uef_file_name[0] = 0;
	uef_last_chunk = NULL;
	uef_last_put_data = UEF_EOF;
	uef_put_chunk.len = 0;
	if (uef_put_chunk.data != NULL)
		free(uef_put_chunk.data);
	uef_put_chunk.data = NULL;
	uef_put_chunk.start_time = -1;
}

static UEFResult uef_write_chunk()
{
	gzFile uef_file = nullptr;
	UEFResult Result = UEFResult::Success;
	int l;

	if (uef_file_name[0])
	{
		/* Always append to file */
		uef_file = gzopen(uef_file_name, "ab");
		if (uef_file == NULL)
		{
			Result = UEFResult::NoFile;
		}
	}
	else
	{
		Result = UEFResult::NoFile;
	}

	if (Result == UEFResult::Success)
	{
		gzput16(uef_file, uef_put_chunk.type);

		switch (uef_put_chunk.type)
		{
		case 0x100: /* Data block */
			gzput32(uef_file, uef_put_chunk.len);
			gzwrite(uef_file, uef_put_chunk.data, uef_put_chunk.len);
			break;
		case 0x110: /* HTone */
			gzput32(uef_file, 2);
			/* Assume 1200 baud */
			l = (int)((double)(uef_put_chunk.end_time-uef_put_chunk.start_time) * (1200*2.0) / NORMAL_TAPE_SPEED);
			gzput16(uef_file, l);
			break;
		case 0x112: /* Gap */
			gzput32(uef_file, 2);
			/* Assume 1200 baud */
			l = (int)((double)(uef_put_chunk.end_time-uef_put_chunk.start_time) * (1200*2.0) / NORMAL_TAPE_SPEED);
			gzput16(uef_file, l);
			break;
		}

		gzclose(uef_file);
	}

	return Result;
}

static void uef_unlock_offset_and_crc(uef_chunk_info *ch)
{
	unsigned char *data = nullptr;
	int len = 0;
	int n;
	int i;
	int d;

	ch->unlock_offset = -1;
	ch->crc = -1;

	if (ch->type == 0x100)
	{
		data = ch->data;
		len = ch->len;
	}
	else if (ch->type == 0x104)
	{
		data = &ch->data[3];
		len = ch->len - 3;
	}

	if (len > 20 && data[0] == 0x2A)
	{
		n = 1;
		while (data[n] != 0 && n <= 10)
			++n;

		if (data[n] == 0)
		{
			ch->unlock_offset = n + 13;
		}
	}

	if (ch->unlock_offset != -1 && (data[ch->unlock_offset] & 1))
	{
		ch->crc = 0;
		for (n = 1; n <= ch->unlock_offset+4; ++n)
		{
			d = data[n];
			if (n == ch->unlock_offset)
			{
				d &= 0xfe;
			}
			ch->crc ^= d << 8;

			for (i = 0; i < 8; ++i)
			{
				if (ch->crc & 0x8000)
				{
					ch->crc ^= 0x0810;
					ch->crc = (ch->crc << 1) | 1;
				}
				else
				{
					ch->crc = (ch->crc << 1);
				}
				ch->crc &= 0xffff;
			}
		}
	}
}

static float uef_decode_float(unsigned char *Float)
{
	int Mantissa;
	int Exponent;
	float Result;

	/* decode mantissa */
	Mantissa = Float[0] | (Float[1] << 8) | ((Float[2]&0x7f)|0x80) << 16;

	Result = (float)Mantissa;
	Result = (float)ldexp(Result, -23);

	/* decode exponent */
	Exponent = ((Float[2]&0x80) >> 7) | (Float[3]&0x7f) << 1;
	Exponent -= 127;
	Result = (float)ldexp(Result, Exponent);

	/* flip sign if necessary */
	if(Float[3]&0x80)
		Result = -Result;

	return(Result);
}

static int gzget16(gzFile f)
{
	int b1, b2;
	b1 = gzgetc(f);
	b2 = gzgetc(f);
	return(b1+(b2<<8));
}

static int gzget32(gzFile f)
{
	int b1, b2, b3, b4;
	b1 = gzgetc(f);
	b2 = gzgetc(f);
	b3 = gzgetc(f);
	b4 = gzgetc(f);
	return(b1+(b2<<8)+(b3<<16)+(b4<<24));
}

static void gzput16(gzFile f, int b)
{
	gzputc(f, b & 0xff);
	gzputc(f, (b >> 8) & 0xff);
}

static void gzput32(gzFile f, int b)
{
	gzputc(f, b & 0xff);
	gzputc(f, (b >> 8) & 0xff);
	gzputc(f, (b >> 16) & 0xff);
	gzputc(f, (b >> 24) & 0xff);
}
