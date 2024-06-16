/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch

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

// Created by Jon Welch on 27/08/2006.

#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Csw.h"
#include "6502core.h"
#include "BeebWin.h"
#include "Debug.h"
#include "Log.h"
#include "Serial.h"
#include "Sound.h"
#include "TapeMap.h"
#include "Uef.h"
#include "UefState.h"

#include "zlib/zlib.h"

static unsigned char *csw_buff;

static int csw_tonecount;

CSWState csw_state;

enum class CSWDataState {
	ReceivingData,
	StopBits,
	ToneOrStartBit
};

static CSWDataState csw_datastate;
int csw_bit;
int csw_pulselen;
int csw_ptr;
static unsigned long csw_bufflen;
static int csw_byte;
int csw_pulsecount;
static int bit_count;
int CSWPollCycles;

CSWResult CSWOpen(const char *FileName)
{
	CSWClose();

	FILE* csw_file = fopen(FileName, "rb");

	if (csw_file == nullptr)
	{
		return CSWResult::OpenFailed;
	}

	constexpr int BUFFER_LEN = 256;
	unsigned char file_buf[BUFFER_LEN];

	/* Read header */
	if (fread(file_buf, 1, 0x34, csw_file) != 0x34 ||
	    strncmp((const char*)file_buf, "Compressed Square Wave", 0x16) != 0 ||
	    file_buf[0x16] != 0x1a)
	{
		fclose(csw_file);
		return CSWResult::InvalidCSWFile;
	}

	// WriteLog("CSW version: %d.%d\n", (int)file_buf[0x17], (int)file_buf[0x18]);

	int sample_rate = file_buf[0x19] |
	                  (file_buf[0x1a] << 8) |
	                  (file_buf[0x1b] << 16) |
	                  (file_buf[0x1c] << 24);

	// int total_samples = file_buf[0x1d] + (file_buf[0x1e] << 8) + (file_buf[0x1f] << 16) + (file_buf[0x20] << 24);
	// int compression_type = file_buf[0x21];
	// int flags = file_buf[0x22];
	unsigned int header_ext = file_buf[0x23];

	// WriteLog("Sample rate: %d\n", sample_rate);
	// WriteLog("Total Samples: %d\n", total_samples);
	// WriteLog("Compressing: %d\n", compression_type);
	// WriteLog("Flags: %x\n", flags);
	// WriteLog("Header ext: %d\n", header_ext);

	file_buf[0x33] = 0;
	// WriteLog("Enc appl: %s\n", &file_buf[0x24]);

	// Read header extension bytes
	if (fread(file_buf, 1, header_ext, csw_file) != header_ext)
	{
		// WriteLog("Failed to read header extension\n");
		fclose(csw_file);
		return CSWResult::InvalidHeaderExtension;
	}

	int end = ftell(csw_file);
	fseek(csw_file, 0, SEEK_END);
	int sourcesize = ftell(csw_file) - end + 1;
	fseek(csw_file, end, SEEK_SET);

	csw_bufflen = 8 * 1024 * 1024;
	csw_buff = (unsigned char *) malloc(csw_bufflen);
	unsigned char *sourcebuff = (unsigned char *) malloc(sourcesize);

	fread(sourcebuff, 1, sourcesize, csw_file);
	fclose(csw_file);
	csw_file = nullptr;

	uncompress(csw_buff, &csw_bufflen, sourcebuff, sourcesize);

	free(sourcebuff);
	sourcebuff = nullptr;

	// WriteLog("Source Size = %d\n", sourcesize);
	// WriteLog("Uncompressed Size = %d\n", csw_bufflen);

	CSWPollCycles = 2000000 / sample_rate - 1;
	csw_state = CSWState::WaitingForTone;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
	csw_tonecount = 0;
	bit_count = -1;

	return CSWResult::Success;
}

void CSWClose()
{
	if (csw_buff != nullptr)
	{
		free(csw_buff);
		csw_buff = nullptr;
	}
}

/*
void HexDump(const char *buff, int count)
{
	char info[80];

	for (int a = 0; a < count; a += 16) {
		sprintf(info, "%04X  ", a);

		for (int b = 0; b < 16; ++b) {
			sprintf(info+strlen(info), "%02X ", buff[a+b]);
		}

		for (int b = 0; b < 16; ++b) {
			int v = buff[a+b];
			if (v < 32 || v > 127)
				v = '.';
			sprintf(info+strlen(info), "%c", v);
		}

		WriteLog("%s\n", info);
	}
}
*/

void CSWCreateTapeMap(std::vector<TapeMapEntry>& TapeMap)
{
	CSWState last_state = CSWState::Undefined;
	char block[65535];
	int block_ptr = -1;

	int n;
	char name[11];
	bool std_last_block = true;
	int last_tone = 0;

	SerialACIA.ClkDivide = 16;

	TapeMap.clear();

	int start_time = 0;
	int blk_num = 0;

	char desc[100];

	while ((unsigned long)csw_ptr + 4 < csw_bufflen)
	{
again:
		last_state = csw_state;
		int data = CSWPoll();

		if (last_state == CSWState::Tone && csw_state == CSWState::Data)
		{
			block_ptr = 0;
			memset(block, 0, 32);
			start_time = csw_ptr;
		}

		if (last_state != csw_state && csw_state == CSWState::Tone)
		{
			// Remember start position of last tone state
			last_tone = csw_ptr;
		}

		if (last_state == CSWState::Data && csw_state == CSWState::WaitingForTone && block_ptr > 0)
		{
			// WriteLog("Decoded Block of length %d, starting at %d\n", block_ptr, start_time);
			// HexDump(block, block_ptr);

			if (block_ptr == 1 && block[0] == 0x80 && SerialACIA.ClkDivide != 64) // 300 baud block?
			{
				SerialACIA.ClkDivide = 64;
				csw_ptr = last_tone;
				csw_state = CSWState::Tone;
				// WriteLog("Detected 300 baud block, resetting ptr to %d\n", csw_ptr);
				goto again;
			}

			if (block_ptr == 3 && SerialACIA.ClkDivide != 16) // 1200 baud block ?
			{
				SerialACIA.ClkDivide = 16;
				csw_ptr = last_tone;
				csw_state = CSWState::Tone;
				// WriteLog("Detected 1200 baud block, resetting ptr to %d\n", csw_ptr);
				goto again;
			}

			// End of block, standard header?
			if (block_ptr > 20 && block[0] == 0x2A)
			{
				if (!std_last_block)
				{
					// Change of block type, must be first block
					blk_num = 0;

					if (!TapeMap.empty() && !TapeMap.back().desc.empty() != 0)
					{
						TapeMap.emplace_back("", start_time);
					}
				}

				// Pull file name from block
				n = 1;
				while (block[n] != 0 && block[n] >= 32 && n <= 10)
				{
					name[n - 1] = block[n];
					n++;
				}
				name[n - 1] = 0;

				if (name[0] != 0)
				{
					sprintf(desc, "%-12s %02X  Length %04X", name, blk_num, block_ptr);
				}
				else
				{
					sprintf(desc, "<No name>    %02X  Length %04X", blk_num, block_ptr);
				}

				TapeMap.emplace_back(desc, start_time);

				// Is this the last block for this file?
				if (block[strlen(name) + 14] & 0x80)
				{
					blk_num = -1;
					TapeMap.emplace_back("", csw_ptr);
				}

				std_last_block = true;
			}
			else
			{
				if (std_last_block)
				{
					// Change of block type, must be first block
					blk_num = 0;

					if (!TapeMap.empty() && !TapeMap.back().desc.empty())
					{
						TapeMap.emplace_back("", csw_ptr);
					}
				}

				sprintf(desc, "Non-standard %02X  Length %04X", blk_num, block_ptr);

				TapeMap.emplace_back(desc, start_time);
				std_last_block = false;
			}

			// Data block recorded
			blk_num = (blk_num + 1) & 255;

			block_ptr = -1;
		}

		if (data != -1 && block_ptr >= 0)
		{
			block[block_ptr++] = (unsigned char)data;
		}
	}

	// for (const MapLine& Line : map_lines)
	//	WriteLog("%s, %d\n", Line.desc.c_str(), Line.time);

	csw_state = CSWState::WaitingForTone;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
	csw_tonecount = 0;
	bit_count = -1;
}

int csw_data()
{
	int t = 0;
	int j = 1;

	if (SerialACIA.ClkDivide == 16) j = 1; // 1200 baud
	if (SerialACIA.ClkDivide == 64) j = 4; // 300 baud

	// JW 18/11/06
	// For 300 baud, just average 4 samples
	// Really need to adjust clock speed as well, as we are loading 300 baud 4 times too quick !
	// But it works

	if (csw_state == CSWState::WaitingForTone || csw_state == CSWState::Tone)
	{
		// Only read 1 bit whilst looking for start bit
		j = 1;
	}

	for (int i = 0; i < j; ++i)
	{
		csw_pulsecount++;

		if (csw_buff[csw_ptr] == 0)
		{
			if ((unsigned long)csw_ptr + 4 < csw_bufflen)
			{
				csw_ptr++;
				csw_pulselen = csw_buff[csw_ptr] |
				               (csw_buff[csw_ptr + 1] << 8) |
				               (csw_buff[csw_ptr + 2] << 16) |
				               (csw_buff[csw_ptr + 3] << 24);
				csw_ptr += 4;
			}
			else
			{
				csw_pulselen = -1;
				csw_state = CSWState::WaitingForTone;
				return csw_pulselen;
			}
		}
		else
		{
			csw_pulselen = csw_buff[csw_ptr++];
		}

		t += csw_pulselen;
	}

	// WriteLog("Pulse %d, duration %d\n", csw_pulsecount, csw_pulselen);

	return t / j;
}

/* Called every sample rate 44,100 Hz */

int CSWPoll()
{
	int ret = -1;

	if (bit_count == -1)
	{
		bit_count = csw_data();

		if (bit_count == -1)
		{
			CSWClose();
			return ret;
		}
	}

	if (bit_count > 0)
	{
		bit_count--;
		return ret;
	}

	// WriteLog("csw_pulsecount %d, csw_bit %d\n", csw_pulsecount, csw_bit);

	switch (csw_state)
	{
		case CSWState::WaitingForTone:
			if (csw_pulselen < 13)
			{
				// Count tone pulses
				csw_tonecount++;
				if (csw_tonecount > 20) // Arbitary figure
				{
					// WriteLog("Detected tone at %d\n", csw_pulsecount);
					csw_state = CSWState::Tone;
				}
			}
			else
			{
				csw_tonecount = 0;
			}
			break;

		case CSWState::Tone:
			if (csw_pulselen > 20)
			{
				// Noise so reset back to wait for tone again
				csw_state = CSWState::WaitingForTone;
				csw_tonecount = 0;
			}
			else if (csw_pulselen > 13 && csw_pulselen < 20)
			{
				// Not in tone any more - data start bit
				// WriteLog("Entered data at %d\n", csw_pulsecount);

				if (SerialACIA.ClkDivide == 64)
				{
					// Skip 300 baud data
					csw_data();
					csw_data();
					csw_data();
				}

				bit_count = csw_data(); // Skip next half of wave

				if (SerialACIA.ClkDivide == 64)
				{
					// Skip 300 baud data
					csw_data();
					csw_data();
					csw_data();
				}

				csw_state = CSWState::Data;
				csw_bit = 0;
				csw_byte = 0;
				csw_datastate = CSWDataState::ReceivingData;
			}
			break;

		case CSWState::Data:
			switch (csw_datastate)
			{
				case CSWDataState::ReceivingData:
					bit_count = csw_data(); // Skip next half of wave
					csw_byte >>= 1;

					if (csw_pulselen > 20)
					{
						// Noisy pulse so reset to tone
						csw_state = CSWState::WaitingForTone;
						csw_tonecount = 0;
						break;
					}
					else if (csw_pulselen <= 13)
					{
						bit_count += csw_data();
						bit_count += csw_data();
						csw_byte |= 0x80;
					}

					if (++csw_bit == 8)
					{
						ret = csw_byte;
						// WriteLog("Returned data byte of %02x at %d\n", ret, csw_pulsecount);
						csw_datastate = CSWDataState::StopBits;
					}
					break;

				case CSWDataState::StopBits:
					bit_count = csw_data();

					if (csw_pulselen > 20)
					{
						// Noisy pulse so reset to tone
						csw_state = CSWState::WaitingForTone;
						csw_tonecount = 0;
						break;
					}
					else if (csw_pulselen <= 13)
					{
						bit_count += csw_data();
						bit_count += csw_data();
					}

					csw_datastate = CSWDataState::ToneOrStartBit; // tone/start bit
					break;

				case CSWDataState::ToneOrStartBit:
					if (csw_pulselen > 20)
					{
						// Noisy pulse so reset to tone
						csw_state = CSWState::WaitingForTone;
						csw_tonecount = 0;
						break;
					}
					else if (csw_pulselen <= 13) // Back in tone again
					{
						// WriteLog("Back in tone again at %d\n", csw_pulsecount);
						csw_state = CSWState::WaitingForTone;
						csw_tonecount = 0;
						csw_bit = 0;
					}
					else
					{
						// Start bit
						bit_count = csw_data(); // Skip next half of wave
						csw_bit = 0;
						csw_byte = 0;
						csw_datastate = CSWDataState::ReceivingData;
					}
					break;
			}
			break;
	}

	bit_count += csw_data(); // Get next bit

	return ret;
}

void SaveCSWState(FILE *SUEF)
{
	UEFWrite8((unsigned int)csw_state, SUEF);
	UEFWrite8((unsigned int)csw_datastate, SUEF);
	UEFWrite8(csw_bit, SUEF);
	UEFWrite32(csw_pulselen, SUEF);
	UEFWrite32(csw_ptr, SUEF);
	UEFWrite32(csw_byte, SUEF);
	UEFWrite32(bit_count, SUEF);
}

void LoadCSWState(FILE *SUEF)
{
	csw_state = static_cast<CSWState>(UEFRead8(SUEF));
	csw_datastate = static_cast<CSWDataState>(UEFRead8(SUEF));
	csw_bit = UEFRead8(SUEF);
	csw_pulselen = UEFRead32(SUEF);
	csw_ptr = UEFRead32(SUEF);
	csw_byte = UEFRead32(SUEF);
	bit_count = UEFRead32(SUEF);
}
