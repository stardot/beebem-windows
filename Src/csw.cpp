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

/*
 *  csw.cc
 *  BeebEm3
 *
 *  Created by Jon Welch on 27/08/2006.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zlib.h"
#include "main.h"
#include "csw.h"

#include "6502core.h"
#include "uef.h"
#include "serial.h"
#include "beebsound.h"
#include "beebwin.h"
#include "debug.h"
#include "uefstate.h"

FILE *csw_file;
unsigned char file_buf[BUFFER_LEN];
unsigned char *csw_buff;
unsigned char *sourcebuff;

int csw_tonecount;
int csw_state;
int csw_datastate;
int csw_bit;
int csw_pulselen;
int csw_ptr;
unsigned long csw_bufflen;
int csw_byte;
int csw_pulsecount;
int bit_count;
unsigned char CSWOpen = 0;
int CSW_BUF;
int CSW_CYCLES;

void LoadCSW(char *file)
{
	int end;
	int sourcesize;
	
//	csw_file = fopen("/Users/jonwelch/MyProjects/csw/AticAtac.csw", "rb");

	CloseCSW();

	csw_file = fopen(file, "rb");

	if (csw_file == NULL)
	{
		WriteLog("Failed to open file\n");
		return;
	}
	
	/* Read header */
	if (fread(file_buf, 1, 0x34, csw_file) != 0x34 ||
		strncmp((const char*)file_buf, "Compressed Square Wave", 0x16) != 0 ||
		file_buf[0x16] != 0x1a)
	{
		WriteLog("Not a valid CSW file\n");
		fclose(csw_file);
		return;
	}
	
	WriteLog("CSW version: %d.%d\n", (int)file_buf[0x17], (int)file_buf[0x18]);
	
	int sample_rate = file_buf[0x19] + (file_buf[0x1a] << 8) + (file_buf[0x1b] << 16) + (file_buf[0x1c] << 24);
	int total_samples = file_buf[0x1d] + (file_buf[0x1e] << 8) + (file_buf[0x1f] << 16) + (file_buf[0x20] << 24);
	int compression_type = file_buf[0x21];
	int flags = file_buf[0x22];
	unsigned int header_ext = file_buf[0x23];
	
	WriteLog("Sample rate: %d\n", sample_rate);
	WriteLog("Total Samples: %d\n", total_samples);
	WriteLog("Compressing: %d\n", compression_type);
	WriteLog("Flags: %x\n", flags);
	WriteLog("Header ext: %d\n", header_ext);
	
	file_buf[0x33] = 0;
	WriteLog("Enc appl: %s\n", &file_buf[0x24]);
	
	/* Read header extension bytes */
	if (fread(file_buf, 1, header_ext, csw_file) != header_ext)
	{
		WriteLog("Failed to read header extension\n");
		fclose(csw_file);
		return;
	}
	
    end = ftell(csw_file);
	fseek(csw_file, 0, SEEK_END);
	sourcesize = ftell(csw_file) - end + 1;
	fseek(csw_file, end, SEEK_SET);
	
	csw_bufflen = 8 * 1024 * 1024;
	csw_buff = (unsigned char *) malloc(csw_bufflen);
	sourcebuff = (unsigned char *) malloc(sourcesize);
	
	fread(sourcebuff, 1, sourcesize, csw_file);
	fclose(csw_file);
	
	uncompress(csw_buff, &csw_bufflen, sourcebuff, sourcesize);
	
	free(sourcebuff);
	
	WriteLog("Source Size = %d\n", sourcesize);
	WriteLog("Uncompressed Size = %d\n", csw_bufflen);
	
	CSW_CYCLES = 2000000 / sample_rate - 1;
	csw_state = 0;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
	csw_tonecount = 0;
	bit_count = -1;
	
	strcpy(UEFTapeName, file);
	
	CSWOpen = 1;
	CSW_BUF = 0;
	TxD = 0;
	RxD = 0;
	TapeClock = 0;
	OldClock = 0;
	TapeTrigger = TotalCycles + CSW_CYCLES;

	if (TapeControlEnabled)
	{
		map_csw_file();
		TapeControlOpenFile(UEFTapeName);
	}
}

void CloseCSW(void)
{
	if (CSWOpen) 
	{
		free(csw_buff);
		CSWOpen = 0;
		TxD = 0;
		RxD = 0;
	}
}

void HexDump(char *buff, int count)
{
int a, b;
int v;
char info[80];
	
	for (a = 0; a < count; a += 16)
	{
		sprintf(info, "%04X  ", a);
		
		for (b = 0; b < 16; ++b)
		{
			sprintf(info+strlen(info), "%02X ", buff[a+b]);
		}
		
		for (b = 0; b < 16; ++b)
		{
			v = buff[a+b];
			if (v < 32 || v > 127)
				v = '.';
			sprintf(info+strlen(info), "%c", v);
		}
		
		WriteLog("%s\n", info);
	}
	
}

void map_csw_file(void)

{
int data;
int last_state = -1;
char block[65535];
int block_ptr = -1;
int start_time;

int n;
int blk_num;
char name[11];
bool std_last_block = true;
int last_tone = 0;

	Clk_Divide = 16;
	
	memset(map_desc, 0, sizeof(map_desc));
	memset(map_time, 0, sizeof(map_time));
	start_time = 0;
	map_lines = 0;
	blk_num = 0;
	
	while  ((unsigned long)csw_ptr + 4 < csw_bufflen)
	{

again : ;
		
		last_state = csw_state;
		data = csw_poll(0);
		if ( (last_state == 1) && (csw_state == 2) )
		{
			block_ptr = 0;
			memset(block, 0, 32);
			start_time = csw_ptr;
		}
		if ( (last_state != csw_state) && (csw_state == 1) )		// Remember start position of last tone state
			last_tone = csw_ptr;
		
		if ( (last_state == 2) && (csw_state == 0) && (block_ptr > 0) )
		{

//			WriteLog("Decoded Block of length %d, starting at %d\n", block_ptr, start_time);
//			HexDump(block, block_ptr);

			if ( (block_ptr == 1) && (block[0] == 0x80) && (Clk_Divide != 64) )		// 300 baud block ?
			{
				Clk_Divide = 64;
				csw_ptr = last_tone;
				csw_state = 1;
//				WriteLog("Detected 300 baud block, resetting ptr to %d\n", csw_ptr);
				goto again;
			}

			if ( (block_ptr == 3) && (Clk_Divide != 16) )							// 1200 baud block ?
			{
				Clk_Divide = 16;
				csw_ptr = last_tone;
				csw_state = 1;
//				WriteLog("Detected 1200 baud block, resetting ptr to %d\n", csw_ptr);
				goto again;
			}
						
			// End of block, standard header?
			if (block_ptr > 20 && block[0] == 0x2A)
			{
				if (!std_last_block)
				{
					// Change of block type, must be first block
					blk_num=0;
					if (map_lines > 0 && map_desc[map_lines-1][0] != 0)
					{
						strcpy(map_desc[map_lines], "");
						map_time[map_lines]=start_time;
						map_lines++;

					}
				}
				
				// Pull file name from block
				n = 1;
				while (block[n] != 0 && block[n] >= 32 && n <= 10) 
				{
					name[n-1] = block[n];
					n++;
				}
				name[n-1] = 0;
				if (name[0] != 0)
					sprintf(map_desc[map_lines], "%-12s %02X  Length %04X", name, blk_num, block_ptr);
				else
					sprintf(map_desc[map_lines], "<No name>    %02X  Length %04X", blk_num, block_ptr);
				
				map_time[map_lines]=start_time;
				
				// Is this the last block for this file?
				if (block[strlen(name) + 14] & 0x80)
				{
					blk_num=-1;
					++map_lines;
					strcpy(map_desc[map_lines], "");
					map_time[map_lines]=csw_ptr;
				}
				std_last_block=true;
			}
			else
			{

				if (std_last_block)
				{
					// Change of block type, must be first block
					blk_num=0;
					if (map_lines > 0 && map_desc[map_lines-1][0] != 0)
					{
						strcpy(map_desc[map_lines], "");
						map_time[map_lines]=csw_ptr;
						map_lines++;
					}
				}
				
				sprintf(map_desc[map_lines], "Non-standard %02X  Length %04X", blk_num, block_ptr);
				map_time[map_lines]=start_time;
				std_last_block=false;
			}
			
			// Data block recorded
			map_lines++;
			blk_num = (blk_num + 1) & 255;
			
			block_ptr = -1;
		}

		if ( (data != -1) && (block_ptr >= 0) )
		{
			block[block_ptr++] = data;
		}
	}

//	for (int i = 0; i < map_lines; i++)
//		WriteLog("%s, %d\n", map_desc[i], map_time[i]);

	csw_state = 0;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
	csw_tonecount = 0;
	bit_count = -1;

}

int csw_data(void)
{
int i;
int j;
int t;
static int last = -1;

	t = 0;
	j = 1;

    if (last != Clk_Divide)
	{
//		WriteLog("Baud Rate changed to %s\n", (Clk_Divide == 16) ? "1200" : "300");
		last = Clk_Divide;
	}
	
	if (Clk_Divide == 16) j = 1;		// 1200 baud
	if (Clk_Divide == 64) j = 4;		// 300 baud

/*
 * JW 18/11/06
 * For 300 baud, just average 4 samples
 * Really need to adjust clock speed as well, as we are loading 300 baud 4 times too quick !
 * But it works
 */
	
	if (csw_state < 2)					// Only read 1 bit whilst looking for start bit
		j = 1;
	
	for (i = 0; i < j; ++i)
	{
		csw_pulsecount++;
	
		if (csw_buff[csw_ptr] == 0)
		{
			if ((unsigned long)csw_ptr + 4 < csw_bufflen)
			{
				csw_ptr++;
				csw_pulselen = csw_buff[csw_ptr] + (csw_buff[csw_ptr + 1] << 8) + (csw_buff[csw_ptr + 2] << 16) + (csw_buff[csw_ptr + 3] << 24);
				csw_ptr+= 4;
			}
			else
			{
				csw_pulselen = -1;
				csw_state = 0;
				return csw_pulselen;
			}
		}
		else
		{
			csw_pulselen = csw_buff[csw_ptr++];
		}
		t = t + csw_pulselen;
	}
	
//	WriteLog("Pulse %d, duration %d\n", csw_pulsecount, csw_pulselen);
	
	return t / j;
	
}

/* Called every sample rate 44,100 Hz */

int csw_poll(int clock)
{
int ret;
	
	ret = -1;
	
	if (bit_count == -1)
	{
		bit_count = csw_data();
		if (bit_count == -1)
		{
			CloseCSW();
			return ret;
		}
	}
	
	if (bit_count > 0)
	{
		bit_count--;
		return ret;
	}
	
//	WriteLog("csw_pulsecount %d, csw_bit %d\n", csw_pulsecount, csw_bit);
	
	switch (csw_state)
	{
		case 0 :			// Waiting for tone
			
			if (csw_pulselen < 0x0d)					/* Count tone pulses */
			{
				csw_tonecount++;
				if (csw_tonecount > 20)					/* Arbitary figure */
				{
//					WriteLog("Detected tone at %d\n", csw_pulsecount);
					csw_state = 1;
				}
			} else {
				csw_tonecount = 0;
			}
			break;
			
		case 1 :			// In tone
			
			if (csw_pulselen > 0x14)					/* Noise so reset back to wait for tone again */
			{
				csw_state = 0;
				csw_tonecount = 0;
			} else
			if ( (csw_pulselen > 0x0d) && (csw_pulselen < 0x14) )			/* Not in tone any more - data start bit */
			{
//				WriteLog("Entered data at %d\n", csw_pulsecount);
				
				if (Clk_Divide == 64) { csw_data();  csw_data(); csw_data(); } 	// Skip 300 baud data

				bit_count = csw_data();					/* Skip next half of wave */

				if (Clk_Divide == 64) { csw_data();  csw_data(); csw_data(); } 	// Skip 300 baud data

				csw_state = 2;
				csw_bit = 0;
				csw_byte = 0;
				csw_datastate = 0;
			}
			break;
			
		case 2 :			// In data
			
			switch (csw_datastate)
			{
				case 0 :		// Receiving data
					
					bit_count = csw_data();				/* Skip next half of wave */
					csw_byte >>= 1;

					if (csw_pulselen > 0x14) 
					{
						csw_state = 0;					/* Noisy pulse so reset to tone */
						csw_tonecount = 0;
						break;
					}
						
					if (csw_pulselen <= 0x0d)
					{
						bit_count += csw_data();
						bit_count += csw_data();
						csw_byte |= 0x80;
					}
					csw_bit++;
					if (csw_bit == 8)
					{
						ret = csw_byte;
//						WriteLog("Returned data byte of %02x at %d\n", ret, csw_pulsecount);
						csw_datastate = 1;				/* Stop bits */
					}
					break;
					
				case 1 :		// Stop bits
					
					bit_count = csw_data();

					if (csw_pulselen > 0x14) 
					{
						csw_state = 0;					/* Noisy pulse so reset to tone */
						csw_tonecount = 0;
						break;
					}
						
					if (csw_pulselen <= 0x0d)
					{
						bit_count += csw_data();
						bit_count += csw_data();
					}
					csw_datastate = 2;					/* tone/start bit */
					break;
					
				case 2 :
					
					if (csw_pulselen > 0x14) 
					{
						csw_state = 0;					/* Noisy pulse so reset to tone */
						csw_tonecount = 0;
						break;
					}

					if (csw_pulselen <= 0x0d)			/* Back in tone again */
					{
//						WriteLog("Back in tone again at %d\n", csw_pulsecount);
						csw_state = 0;
						csw_tonecount = 0;
						csw_bit = 0;
					}
					else /* Start bit */
					{
						bit_count = csw_data();			/* Skip next half of wave */
						csw_bit = 0;
						csw_byte = 0;
						csw_datastate = 0;				/* receiving data */
					}
					break;
			}
			break;
	}
	
	bit_count += csw_data();		/* Get next bit */

	return ret;
}
