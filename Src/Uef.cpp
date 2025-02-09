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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "Uef.h"
#include "DebugTrace.h"
#include "Version.h"
#include "zlib/zlib.h"

//------------------------------------------------------------------------------

// Beats representing normal tape speed (not sure why its 5600)
constexpr int NORMAL_TAPE_SPEED = 5600;

static float UEFDecodeFloat(unsigned char* data);
static void UEFUnlockOffsetAndCRC(UEFChunkInfo& ch);

static int gzget16(gzFile f);
static int gzget32(gzFile f);
static void gzput16(gzFile f, int Value);
static void gzput32(gzFile f, int Value);

//------------------------------------------------------------------------------

UEFTapeImage::UEFTapeImage() :
	m_ClockSpeed(5600),
	m_LastChunk(nullptr),
	m_Unlock(false),
	m_LastPutData(UEF_EOF),
	m_Modified(false)
{
}

//------------------------------------------------------------------------------

UEFTapeImage::~UEFTapeImage()
{
	Close();
}

//------------------------------------------------------------------------------

UEFResult UEFTapeImage::PutData(int Data, int Time)
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
		if (m_Chunk.start_time == -1)
		{
			m_Chunk.start_time = Time;
			m_Chunk.end_time = Time;
		}
		else
		{
			m_Chunk.end_time = Time;
		}

		// Put data in buffer
		m_Chunk.data.push_back(UEFRES_BYTE(Data));
		m_Chunk.len++;
	}

	m_LastPutData = Data;

	m_Modified = true;

	return Result;
}

//------------------------------------------------------------------------------

UEFResult UEFTapeImage::WriteChunk()
{
	UEFResult Result = UEFResult::Success;

	switch (m_Chunk.type)
	{
		case 0x100:
			// Data block
			m_Chunks.push_back(m_Chunk);
			break;

		case 0x110: {
			// Carrier tone
			// Assume 1200 baud
			int Length = (int)((double)(m_Chunk.end_time - m_Chunk.start_time) * (1200 * 2.0) / NORMAL_TAPE_SPEED);

			m_Chunk.len = 2;
			m_Chunk.data.resize(2);
			m_Chunk.data[0] = Length & 0xFF;
			m_Chunk.data[1] = (Length >> 8) & 0xFF;
			m_Chunks.push_back(m_Chunk);
			break;
		}

		case 0x112: {
			// Gap
			// Assume 1200 baud
			int Length = (int)((double)(m_Chunk.end_time - m_Chunk.start_time) * (1200 * 2.0) / NORMAL_TAPE_SPEED);

			m_Chunk.len = 2;
			m_Chunk.data.resize(2);
			m_Chunk.data[0] = Length & 0xFF;
			m_Chunk.data[1] = (Length >> 8) & 0xFF;
			m_Chunks.push_back(m_Chunk);
			break;
		}
	}

	return Result;
}

//------------------------------------------------------------------------------

bool UEFTapeImage::IsModified() const
{
	return m_Modified;
}

//------------------------------------------------------------------------------

UEFResult UEFTapeImage::Save(const char* FileName)
{
	gzFile UEFFile = nullptr;

	UEFFile = gzopen(FileName, "wb");

	if (UEFFile == nullptr)
	{
		return UEFResult::WriteFailed;
	}

	try
	{
		// Write the UEF file header
		gzwrite(UEFFile, "UEF File!", 10);
		gzput16(UEFFile, 0x000a); // V0.10

		// Write each chunk
		for (const UEFChunkInfo& Chunk : m_Chunks)
		{
			gzput16(UEFFile, Chunk.type);
			gzput32(UEFFile, Chunk.data.size());
			gzwrite(UEFFile, &Chunk.data[0], Chunk.data.size());
		}

		gzclose(UEFFile);

		m_Modified = false;

		return UEFResult::Success;
	}
	catch (UEFResult Result)
	{
		gzclose(UEFFile);

		return Result;
	}
}

//------------------------------------------------------------------------------

void UEFTapeImage::SetClock(int Speed)
{
	// Speed = bit samples per second?
	m_ClockSpeed = Speed;
}

//------------------------------------------------------------------------------

void UEFTapeImage::SetUnlock(bool Unlock)
{
	m_Unlock = Unlock;
}

//------------------------------------------------------------------------------

void UEFTapeImage::New()
{
	Reset();

	std::string CreatedBy = "Created by BeebEm ";
	CreatedBy += VERSION_STRING;

	UEFChunkInfo OriginInfoChunk{};
	OriginInfoChunk.type = 0;
	OriginInfoChunk.len = CreatedBy.size() + 1;
	OriginInfoChunk.data.resize(OriginInfoChunk.len);
	memcpy(&OriginInfoChunk.data[0], CreatedBy.c_str(), CreatedBy.size() + 1);

	m_Chunks.push_back(OriginInfoChunk);
}

//------------------------------------------------------------------------------

UEFResult UEFTapeImage::Open(const char *FileName)
{
	Close();

	UEFResult Result = LoadData(FileName);

	if (Result != UEFResult::Success)
	{
		return Result;
	}

	SetChunkClock();

	return UEFResult::Success;
}

//------------------------------------------------------------------------------

UEFResult UEFTapeImage::LoadData(const char *FileName)
{
	gzFile InputFile = gzopen(FileName, "rb");

	if (InputFile == nullptr)
	{
		return UEFResult::NoFile;
	}

	char UEFId[10];
	int BytesRead = gzread(InputFile, UEFId, 10);

	if (BytesRead < 10 || strcmp(UEFId, "UEF File!") != 0)
	{
		Close();

		gzclose(InputFile);

		return UEFResult::NotUEF;
	}

	UEFResult Result = UEFResult::Success;

	try
	{
		/* const int Version = */gzget16(InputFile);

		m_Chunks.clear();

		while (Result == UEFResult::Success)
		{
			UEFChunkInfo chunk{};
			chunk.type = gzget16(InputFile);
			chunk.len = gzget32(InputFile);

			// Only store UEF chunks 0xx and 1xx which contain
			// content information and tape data
			if (chunk.type < 0x200)
			{
				if (chunk.len > 0)
				{
					chunk.data.resize(chunk.len);

					if (gzread(InputFile, &chunk.data[0], chunk.len) == chunk.len)
					{
						m_Chunks.push_back(chunk);
					}
					else
					{
						Result = UEFResult::ReadFailed;
					}
				}
			}
			else
			{
				Result = UEFResult::NotTape;
			}
		}
	}
	catch (UEFResult r)
	{
		if (r != UEFResult::EndOfFile)
		{
			Result = r;
		}
	}

	gzclose(InputFile);

	if (Result != UEFResult::Success)
	{
		Close();
		return Result;
	}

	m_FileName = FileName;

	return Result;
}

//------------------------------------------------------------------------------

void UEFTapeImage::SetChunkClock()
{
	int clock = 0;
	int baud = 1200;

	for (UEFChunkInfo& chunk : m_Chunks)
	{
		chunk.start_time = clock;

		switch (chunk.type)
		{
			case 0x100:
				// Implicit start/stop bit tape data block
				clock += (int)(chunk.len * m_ClockSpeed * 10.0 / baud);
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
				clock += (int)((double)(chunk.len - 3) * m_ClockSpeed * 10.0 / baud);
				UEFUnlockOffsetAndCRC(chunk);
				break;

			case 0x110:
				// Carrier tone
				chunk.l1 = chunk.data[0] + (chunk.data[1] << 8);
				clock += (int)((double)chunk.l1 * m_ClockSpeed / (baud * 2.0));
				break;

			case 0x111: {
				// Carrier tone with dummy byte
				chunk.l1 = chunk.data[0] + (chunk.data[1] << 8);
				chunk.l2 = chunk.data[2] + (chunk.data[3] << 8);
				int len = chunk.l1 + chunk.l2 + 160; // 160 for dummy byte
				clock += (int)((double)len * m_ClockSpeed / (baud * 2.0));
				break;
			}

			case 0x112:
				// Integer gap
				chunk.l1 = chunk.data[0] + (chunk.data[1] << 8);
				clock += (int)((double)chunk.l1 * m_ClockSpeed / (baud * 2.0));
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
				clock += (int)(chunk.l1 * m_ClockSpeed * 10.0 / baud);
				break;

			case 0x115:
				// Phase change (not supported)
				break;

			case 0x116:
				// Gap (not in http://electrem.emuunlim.com/UEFSpecs.htm)
				clock += (int)(UEFDecodeFloat(&chunk.data[0]) * m_ClockSpeed);
				break;

			case 0x120:
				// Position marker (not supported)
				break;
		}

		chunk.end_time = clock;
	}
}

//------------------------------------------------------------------------------

const UEFChunkInfo* UEFTapeImage::FindChunk(int Time)
{
	if (m_LastChunk != nullptr &&
	    Time >= m_LastChunk->start_time && Time < m_LastChunk->end_time)
	{
		return m_LastChunk;
	}
	else
	{
		for (const UEFChunkInfo& chunk : m_Chunks)
		{
			if (Time >= chunk.start_time && Time < chunk.end_time)
			{
				m_LastChunk = &chunk;
				return m_LastChunk;
			}
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------

int UEFTapeImage::GetData(int Time)
{
	const UEFChunkInfo *ch = FindChunk(Time);

	if (ch == nullptr)
		return UEF_EOF;

	int data = UEF_GAP | 0x1000;

	int i, j;

	switch (ch->type)
	{
		case 0x100:
			// Implicit start/stop bit tape data block

		case 0x104:
			// Defined tape format data block
			j = ch->type == 0x104 ? 3 : 0;
			i = (int)((double)(Time - ch->start_time) / (ch->end_time - ch->start_time) * (ch->len - j));
			data = UEF_DATA | ch->data[i + j] | ((i & 0x7f) << 24);

			if (m_Unlock)
			{
				if (i == ch->unlock_offset)
				{
					data &= 0xfffffffe;
				}
				else if (ch->crc != -1)
				{
					if (i == ch->unlock_offset + 5)
					{
						data &= 0xffffff00;
						data |= ch->crc >> 8;
					}
					else if (i == ch->unlock_offset + 6)
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

//------------------------------------------------------------------------------

void UEFTapeImage::Close()
{
	m_Chunks.clear();
	m_FileName.clear();
	m_LastChunk = nullptr;
	m_Modified = false;
}

//------------------------------------------------------------------------------

void UEFTapeImage::Reset()
{
	Close();

	m_ClockSpeed = 5600;
	m_Unlock = false;
	m_LastPutData = UEF_EOF;
}

//------------------------------------------------------------------------------

void UEFTapeImage::CreateTapeMap(std::vector<TapeMapEntry>& TapeMap)
{
	bool done = false;
	int blk = 0;
	unsigned char block[500];
	bool std_last_block = true;

	int i = 0;
	int start_time = 0;
	int last_data = 0;
	int blk_num = 0;

	TapeMap.clear();

	char desc[100];

	while (!done)
	{
		int data = GetData(i);

		if (data != last_data)
		{
			if (UEFRES_TYPE(data) != UEFRES_TYPE(last_data))
			{
				if (UEFRES_TYPE(last_data) == UEF_DATA)
				{
					// End of block, standard header?
					if (blk > 20 && block[0] == 0x2A)
					{
						if (!std_last_block)
						{
							// Change of block type, must be first block
							blk_num = 0;

							if (!TapeMap.empty() && !TapeMap.back().desc.empty())
							{
								TapeMap.emplace_back("", start_time);
							}
						}

						// Pull file name from block
						std::string name;

						int n = 1;

						while (block[n] != 0 && block[n] >= 32 && block[n] <= 127 && n <= 10)
						{
							name += block[n];
							n++;
						}

						if (!name.empty())
						{
							sprintf(desc, "%-12s %02X  Length %04X", name.c_str(), blk_num, blk);
						}
						else
						{
							sprintf(desc, "<No name>    %02X  Length %04X", blk_num, blk);
						}

						TapeMap.emplace_back(desc, start_time);

						// Is this the last block for this file?
						if (block[name.size() + 14] & 0x80)
						{
							blk_num = -1;

							TapeMap.emplace_back("", start_time);
						}

						std_last_block = true;
					}
					else
					{
						// Replace time counter in previous blank line
						if (!TapeMap.empty() && TapeMap.back().desc.empty())
						{
							TapeMap.back().time = start_time;
						}

						sprintf(desc, "Non-standard %02X  Length %04X", blk_num, blk);
						TapeMap.emplace_back(desc, start_time);
						std_last_block = false;
					}

					// Data block recorded
					blk_num++;
				}

				if (UEFRES_TYPE(data) == UEF_CARRIER_TONE)
				{
					// map_lines.emplace_back("Tone", i);
					start_time = i;
				}
				else if (UEFRES_TYPE(data) == UEF_GAP)
				{
					if (!TapeMap.empty() && !TapeMap.back().desc.empty())
					{
						TapeMap.emplace_back("", i);
					}

					start_time = i;
					blk_num = 0;
				}
				else if (UEFRES_TYPE(data) == UEF_DATA)
				{
					blk = 0;
					block[blk++] = UEFRES_BYTE(data);
				}
				else if (UEFRES_TYPE(data) == UEF_EOF)
				{
					done = true;
				}
			}
			else
			{
				if (UEFRES_TYPE(data) == UEF_DATA)
				{
					if (blk < 500)
					{
						block[blk++] = UEFRES_BYTE(data);
					}
					else
					{
						blk++;
					}
				}
			}
		}

		last_data = data;
		i++;
	}
}

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

static int gzget16(gzFile f)
{
	int Result = 0;

	for (int Shift = 0; Shift < 16; Shift += 8)
	{
		int Value = gzgetc(f);

		if (Value == -1)
		{
			throw gzeof(f) ? UEFResult::EndOfFile : UEFResult::ReadFailed;
		}

		Result |= Value << Shift;
	}

	return Result;
}

//------------------------------------------------------------------------------

static int gzget32(gzFile f)
{
	int Result = 0;

	for (int Shift = 0; Shift < 32; Shift += 8)
	{
		int Value = gzgetc(f);

		if (Value == -1)
		{
			throw gzeof(f) ? UEFResult::EndOfFile : UEFResult::ReadFailed;
		}

		Result |= Value << Shift;
	}

	return Result;
}

//------------------------------------------------------------------------------

static void gzput16(gzFile f, int Value)
{
	for (int Shift = 0; Shift < 16; Shift += 8)
	{
		int Result = gzputc(f, (Value >> Shift) & 0xFF);

		if (Result == -1)
		{
			throw UEFResult::WriteFailed;
		}
	}
}

//------------------------------------------------------------------------------

static void gzput32(gzFile f, int Value)
{
	for (int Shift = 0; Shift < 32; Shift += 8)
	{
		int Result = gzputc(f, (Value >> Shift) & 0xFF);

		if (Result == -1)
		{
			throw UEFResult::WriteFailed;
		}
	}
}

//------------------------------------------------------------------------------
