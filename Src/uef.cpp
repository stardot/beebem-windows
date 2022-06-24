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

#include <string>
#include <vector>

#include "uef.h"
#include "zlib/zlib.h"

// Beats representing normal tape speed (not sure why its 5600)
constexpr int NORMAL_TAPE_SPEED = 5600;

static std::string UEFFileName;
static std::vector<UEFChunkInfo> uef_chunks;
static int uef_clock_speed = 5600;
static UEFChunkInfo *uef_last_chunk = nullptr;
static bool uef_unlock = false;

static float UEFDecodeFloat(unsigned char* data);
static void UEFUnlockOffsetAndCRC(UEFChunkInfo& ch);

static int gzget16(gzFile f);
static int gzget32(gzFile f);
static void gzput16(gzFile f, int b);
static void gzput32(gzFile f, int b);

void UEFSetClock(int Speed)
{
	// Speed = bit samples per second?
	uef_clock_speed = Speed;
}

void UEFSetUnlock(bool Unlock)
{
	uef_unlock = Unlock;
}

UEFFileWriter::UEFFileWriter() :
	m_OutputFile(nullptr),
	m_LastPutData(UEF_EOF)
{
}

UEFFileWriter::~UEFFileWriter()
{
	Close();
}

UEFResult UEFFileWriter::Open(const char *FileName)
{
	Close();

	m_OutputFile = gzopen(FileName, "wb");

	if (m_OutputFile == nullptr)
	{
		return UEFResult::NoFile;
	}

	gzwrite(m_OutputFile, "UEF File!", 10);
	gzput16(m_OutputFile, 0x000a); // V0.10
	gzput16(m_OutputFile, 0);
	gzput32(m_OutputFile, 18);
	gzwrite(m_OutputFile, "Created by BeebEm", 18);

	m_FileName = FileName;

	return UEFResult::Success;
}

UEFResult UEFFileWriter::PutData(int Data, int Time)
{
	UEFResult Result = UEFResult::Success;

	if (UEFRES_TYPE(Data) != UEFRES_TYPE(m_LastPutData))
	{
		// Finished a chunk
		if (UEFRES_TYPE(m_LastPutData) == UEF_CARRIER_TONE)
		{
			if (m_Chunk.start_time != m_Chunk.end_time)
			{
				m_Chunk.type = 0x110;
				Result = WriteChunk();
			}
		}
		else if (UEFRES_TYPE(m_LastPutData) == UEF_GAP)
		{
			if (m_Chunk.start_time != m_Chunk.end_time)
			{
				m_Chunk.type = 0x112;
				Result = WriteChunk();
			}
		}
		else if (UEFRES_TYPE(m_LastPutData) == UEF_DATA)
		{
			m_Chunk.type = 0x100;
			Result = WriteChunk();
		}

		// Reset for next chunk
		m_Chunk.len = 0;
		m_Chunk.data.clear();
		m_Chunk.start_time = -1;
	}

	// Add to chunk
	if (UEFRES_TYPE(Data) == UEF_CARRIER_TONE || UEFRES_TYPE(Data) == UEF_GAP)
	{
		if (m_Chunk.start_time == -1)
		{
			m_Chunk.start_time = Time;
			m_Chunk.end_time = Time;
		}
		else
		{
			m_Chunk.end_time = Time;
		}
	}
	else if (UEFRES_TYPE(Data) == UEF_DATA)
	{
		// Put data in buffer
		m_Chunk.data.push_back(UEFRES_BYTE(Data));
		m_Chunk.len++;
	}

	m_LastPutData = Data;

	return Result;
}

UEFResult UEFFileWriter::WriteChunk()
{
	UEFResult Result = UEFResult::Success;

	gzput16(m_OutputFile, m_Chunk.type);

	switch (m_Chunk.type)
	{
		case 0x100:
			// Data block
			gzput32(m_OutputFile, (int)m_Chunk.data.size());
			gzwrite(m_OutputFile, &m_Chunk.data[0], (int)m_Chunk.data.size());
			break;
		case 0x110: {
			// Carrier tone
			gzput32(m_OutputFile, 2);
			// Assume 1200 baud
			int Length = (int)((double)(m_Chunk.end_time - m_Chunk.start_time) * (1200 * 2.0) / NORMAL_TAPE_SPEED);
			gzput16(m_OutputFile, Length);
			break;
		}
		case 0x112: {
			// Gap
			gzput32(m_OutputFile, 2);
			// Assume 1200 baud
			int Length = (int)((double)(m_Chunk.end_time - m_Chunk.start_time) * (1200 * 2.0) / NORMAL_TAPE_SPEED);
			gzput16(m_OutputFile, Length);
			break;
		}
	}

	return Result;
}

void UEFFileWriter::Close()
{
	if (m_OutputFile != nullptr)
	{
		gzclose(m_OutputFile);
		m_OutputFile = nullptr;
	}

	m_LastPutData = UEF_EOF;

	m_Chunk.len = 0;
	m_Chunk.data.clear();
	m_Chunk.start_time = -1;
}

static UEFResult LoadUEFData(const char *FileName)
{
	gzFile InputFile = gzopen(FileName, "rb");

	if (InputFile == nullptr)
	{
		return UEFResult::NoFile;
	}

	char UEFId[10];
	gzread(InputFile, UEFId, 10);

	if (strcmp(UEFId, "UEF File!") != 0)
	{
		UEFClose();

		return UEFResult::NotUEF;
	}

	const int ver = gzget16(InputFile);

	uef_chunks.clear();

	UEFResult Result = UEFResult::Success;

	while (Result == UEFResult::Success && !gzeof(InputFile))
	{
		UEFChunkInfo chunk{};
		chunk.type = gzget16(InputFile);
		chunk.len = gzget32(InputFile);

		// Only store UEF chunks 1xx which contain tape data
		if (chunk.type >= 0x100 && chunk.type <= 0x1ff)
		{
			if (chunk.len > 0)
			{
				chunk.data.resize(chunk.len);

				if (gzread(InputFile, &chunk.data[0], chunk.len) == chunk.len)
				{
					uef_chunks.push_back(chunk);
				}
				else
				{
					Result = UEFResult::NotTape;
				}
			}
		}
		else if (chunk.type >= 0x200)
		{
			Result = UEFResult::NotTape;
		}
		else if (chunk.len > 0)
		{
			gzseek(InputFile, chunk.len, SEEK_CUR);
		}
	}

	gzclose(InputFile);

	if (Result != UEFResult::Success)
	{
		UEFClose();
		return Result;
	}

	UEFFileName = FileName;

	return Result;
}

UEFResult UEFOpen(const char *FileName)
{
	UEFClose();

	UEFResult Result = LoadUEFData(FileName);

	if (Result != UEFResult::Success)
	{
		return Result;
	}

	int clock = 0;
	int baud = 1200;

	for (UEFChunkInfo& chunk : uef_chunks)
	{
		chunk.start_time = clock;

		switch (chunk.type)
		{
			case 0x100:
				// Implicit start/stop bit tape data block
				clock += (int)(chunk.len * uef_clock_speed * 10.0 / baud);
				UEFUnlockOffsetAndCRC(chunk);
				break;
			case 0x101:
				// Multiplexed data block (not supported)
				break;
			case 0x102:
				// Explicit tape data block (not supported)
				break;
			case 0x104:
				// Defined tape format data block
				clock += (int)((double)(chunk.len-3) * uef_clock_speed * 10.0 / baud);
				UEFUnlockOffsetAndCRC(chunk);
				break;
			case 0x110:
				// Carrier tone
				chunk.l1 = chunk.data[0] + (chunk.data[1] << 8);
				clock += (int)((double)chunk.l1 * uef_clock_speed / (baud * 2.0));
				break;
			case 0x111: {
				// Carrier tone with dummy byte
				chunk.l1 = chunk.data[0] + (chunk.data[1] << 8);
				chunk.l2 = chunk.data[2] + (chunk.data[3] << 8);
				int len = chunk.l1 + chunk.l2 + 160; // 160 for dummy byte
				clock += (int)((double)len * uef_clock_speed / (baud * 2.0));
				break;
			}
			case 0x112:
				// Integer gap
				chunk.l1 = chunk.data[0] + (chunk.data[1] << 8);
				clock += (int)((double)chunk.l1 * uef_clock_speed / (baud * 2.0));
				break;
			case 0x113:
				// Change of base frequency
				baud = (int)UEFDecodeFloat(&chunk.data[0]);
				if (baud <= 0)
					baud = 1200;
				break;
			case 0x114:
				// Security cycles
				chunk.l1 = chunk.data[0] | (chunk.data[1] << 8) | (chunk.data[2] << 16);
				chunk.l1 = (chunk.l1 + 7) / 8;
				clock += (int)(chunk.l1 * uef_clock_speed * 10.0 / baud);
				break;
			case 0x115:
				// Phase change (not supported)
				break;
			case 0x116:
				// Gap (not in http://electrem.emuunlim.com/UEFSpecs.htm)
				clock += (int)(UEFDecodeFloat(&chunk.data[0]) * uef_clock_speed);
				break;
			case 0x120:
				// Position marker (not supported)
				break;
		}

		chunk.end_time = clock;
	}

	return UEFResult::Success;
}

static UEFChunkInfo* UEFFindChunk(int Time)
{
	if (uef_last_chunk != nullptr &&
	    Time >= uef_last_chunk->start_time && Time < uef_last_chunk->end_time)
	{
		return uef_last_chunk;
	}
	else
	{
		for (UEFChunkInfo& chunk : uef_chunks)
		{
			if (Time >= chunk.start_time && Time < chunk.end_time)
			{
				uef_last_chunk = &chunk;
				return uef_last_chunk;
			}
		}
	}

	return nullptr;
}

int UEFGetData(int Time)
{
	UEFChunkInfo *ch = UEFFindChunk(Time);

	if (ch == nullptr)
		return UEF_EOF;

	int data = UEF_GAP | 0x1000;

	int i, j;

	switch (ch->type)
	{
		case 0x100: // Implicit start/stop bit tape data block
		case 0x104: // Defined tape format data block
			j = ch->type == 0x104 ? 3 : 0;
			i = (int)((double)(Time - ch->start_time) / (ch->end_time - ch->start_time) * (ch->len - j));
			data = UEF_DATA | ch->data[i + j] | ((i & 0x7f) << 24);

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
			// Multiplexed data block (not supported)
			break;
		case 0x102:
			// Explicit tape data block (not supported)
			break;
		case 0x110:
			// Carrier tone
			data = UEF_CARRIER_TONE | 0x1000;
			break;
		case 0x111:
			// Carrier tone with dummy byte
			i = (int)((double)(Time - ch->start_time) / (ch->end_time - ch->start_time) * (ch->l1 + 160 + ch->l2));
			if (i < ch->l1 || i >= (ch->l1 + 160))
				data = UEF_CARRIER_TONE | 0x1000;
			else
				data = UEF_DATA | 0xAA;
			break;
		case 0x112:
			// Integer gap
			data = UEF_GAP | 0x1000;
			break;
		case 0x113:
			// Change of base frequency (baud rate)
			break;
		case 0x114:
			// Security cycles
			i = (int)((double)(Time - ch->start_time) / (ch->end_time - ch->start_time) * ch->l1);
			data = UEF_DATA | ch->data[i + 5] | ((i & 0x7f) << 24);
			break;
		case 0x115:
			// Phase change (not supported)
			break;
		case 0x116:
			// Gap
			data = UEF_GAP | 0x1000;
			break;
		case 0x120:
			// Position marker (not supported)
			break;
	}

	return data;
}

void UEFClose()
{
	uef_chunks.clear();

	UEFFileName.clear();
	uef_last_chunk = nullptr;
}

static void UEFUnlockOffsetAndCRC(UEFChunkInfo& chunk)
{
	unsigned char *data = nullptr;
	int len = 0;
	int n;
	int i;
	int d;

	chunk.unlock_offset = -1;
	chunk.crc = -1;

	if (chunk.type == 0x100)
	{
		data = &chunk.data[0];
		len = chunk.len;
	}
	else if (chunk.type == 0x104)
	{
		data = &chunk.data[3];
		len = chunk.len - 3;
	}

	// https://beebwiki.mdfs.net/Acorn_cassette_format
	// Check for synchronisation byte (&2A)
	if (len > 20 && data[0] == 0x2A)
	{
		// Skip file name (1 to 10 characters)
		n = 1;

		while (data[n] != 0 && n <= 10)
		{
			++n;
		}

		// Check for end of file name marker byte (&00)
		if (data[n] == 0)
		{
			chunk.unlock_offset = n + 13; // Offset of block flag
		}
	}

	// Check if Locked bit is set in tbe block flag
	if (chunk.unlock_offset != -1 && (data[chunk.unlock_offset] & 1))
	{
		chunk.crc = 0;

		for (n = 1; n <= chunk.unlock_offset+4; ++n)
		{
			d = data[n];

			if (n == chunk.unlock_offset)
			{
				d &= 0xfe;
			}

			chunk.crc ^= d << 8;

			for (i = 0; i < 8; ++i)
			{
				if (chunk.crc & 0x8000)
				{
					chunk.crc ^= 0x0810;
					chunk.crc = (chunk.crc << 1) | 1;
				}
				else
				{
					chunk.crc = (chunk.crc << 1);
				}

				chunk.crc &= 0xffff;
			}
		}
	}
}

static float UEFDecodeFloat(unsigned char* data)
{
	// Decode mantissa
	int Mantissa = data[0] | (data[1] << 8) | ((data[2] & 0x7f) | 0x80) << 16;

	float Result = (float)ldexp(Mantissa, -23);

	// Decode exponent
	int Exponent = ((data[2] & 0x80) >> 7) | (data[3] & 0x7f) << 1;
	Exponent -= 127;
	Result = (float)ldexp(Result, Exponent);

	// Flip sign if necessary
	if (data[3] & 0x80)
		Result = -Result;

	return Result;
}

static int gzget16(gzFile f)
{
	int b1 = gzgetc(f);
	int b2 = gzgetc(f);

	return b1 | (b2 << 8);
}

static int gzget32(gzFile f)
{
	int b1 = gzgetc(f);
	int b2 = gzgetc(f);
	int b3 = gzgetc(f);
	int b4 = gzgetc(f);

	return b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
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
