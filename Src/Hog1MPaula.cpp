/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2016  Mike Wyatt

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

//
// Dossytronics 1M Paula / blitter sound demo board for Hoglet
// 1MHz fpga board
//
// Dominic Beesley - 2019
//
// This is a rough functional approximation of the the real board


#include <windows.h>
#include <stdio.h>
#include "soundstream.h"
#include "hog1mpaula.h"
#include "uefstate.h"

#include <math.h>


UINT8		jimDev;			//fcff
UINT16		jimPage;		//fcfe,fcfd  - big endian
bool		Hog1MPaulaEnabled = false;
extern int	SoundVolume;

#define RAM_SIZE				(512*1024)
#define NUM_CHANNELS			4
#define JIM_DEV					0xD0
#define JIM_PAGE				0xFEFC
#define REG_BASE				0x80


typedef struct
{

	//user facing registers
	INT8	data;
	UINT8	addr_bank;
	UINT16	addr;

	UINT8   period_h_latch;
	UINT16	period;
	UINT16	len;
	bool	act;
	bool	repeat;
	UINT8	vol;
	UINT16	repoff;
	UINT8	peak;

	//internal registers
	bool	clken_sam_next;
	bool	act_prev;
	UINT16	samper_ctr;
	INT8	data_next;
	UINT16	sam_ctr;


} CHANNELREGS;

static UINT8	ChipRam[RAM_SIZE];
CHANNELREGS		ChannelRegs[NUM_CHANNELS];
UINT8			ChannelSel;
UINT8			Volume;

// 8MHz clock, 128 cycles to update all channels
static UINT CycleCount = 0;

static INT16 *SampleBuf = NULL;
static UINT32 SampleBufSize = 0;
static UINT32 SampleWritePtr = 0;

static SoundStreamer *pSoundStreamerPaula = NULL;

#define H1M_STREAM_RATE 46875
#define H1M_CYCLES_PER_STREAM_SAMPLE 8000000/H1M_STREAM_RATE

#define H1M_PAULA_CLK 3547672
#define H1M_RAM_CLK   8000000
#define H1M_PCLK_LIM  65536
#define H1M_PCLK_A    (int)((((long long)H1M_PCLK_LIM) * ((long long)H1M_PAULA_CLK)) / (long long)H1M_RAM_CLK)

UINT32 sample_clock_acc = 0; //sample clock accumulator - when this overflows do a sound sample
UINT32 paula_clock_acc = 0; //paula clock acumulator - when this overflows do a Paula period

void Hog1MPaulaInit()
{

	memset(ChipRam, 0, sizeof(ChipRam));
	CycleCount = 0;

	// Init the streamer
	delete pSoundStreamerPaula;
	pSoundStreamerPaula = CreateSoundStreamer(H1M_STREAM_RATE, 16, 2);
	if (!pSoundStreamerPaula)
	{
		Hog1MPaulaEnabled = false;
	}
	else
	{
		pSoundStreamerPaula->Play();
		SampleBufSize = (UINT32)pSoundStreamerPaula->BufferSize();
		if (SampleBuf)
			free(SampleBuf);
		SampleBuf = (INT16*)malloc(SampleBufSize * 4);
		if (SampleBuf == NULL)
		{
			Hog1MPaulaEnabled = false;
		}
	}
}

void Hog1MPaulaReset()
{
	delete pSoundStreamerPaula;
	pSoundStreamerPaula = NULL;

	ChannelSel = 0;
	Volume = 0x3F;
	for (int i = 0; i < NUM_CHANNELS; i++) {
		ChannelRegs[i].data = 0;
		ChannelRegs[i].addr = 0;
		ChannelRegs[i].addr_bank = 0;

		ChannelRegs[i].act = false;
		ChannelRegs[i].act_prev = false;
		ChannelRegs[i].repeat = false;

		ChannelRegs[i].len = 0;
		ChannelRegs[i].period_h_latch = 0;
		ChannelRegs[i].period = 0;
		ChannelRegs[i].vol = 0;
		ChannelRegs[i].peak = 0;

		ChannelRegs[i].repoff = 0;

	}
	
}

void Hog1MPaulaWrite(UINT16 address, UINT8 value)
{
	if (!Hog1MPaulaEnabled)
		return;

	if (address == 0xfcff)
	{
		jimDev = value;
		return;
	}

	if (jimDev == JIM_DEV)
	{
		if (address == 0xfcfe)
			jimPage = (jimPage & 0xFF00) | value;			
		else if (address == 0xfcfd)
			jimPage = (jimPage & 0x00FF) | (value << 8);
		else if ((address & 0xff00) == 0xfd00) {
			//jim
			if ((jimPage == JIM_PAGE) && ((address & 0x00f0) == REG_BASE))
			{
				// sound registers
				//note registers are all exposed big-endian style
				switch (address & 0x0F)
				{
				case 0:
					ChannelRegs[ChannelSel].data = value;
					break;
				case 1:
					ChannelRegs[ChannelSel].addr_bank = value;
					break;
				case 2:
					ChannelRegs[ChannelSel].addr = (ChannelRegs[ChannelSel].addr & 0x00FF) | (value << 8);
					break;
				case 3:
					ChannelRegs[ChannelSel].addr = (ChannelRegs[ChannelSel].addr & 0xFF00) | value;
					break;
				case 4:
					ChannelRegs[ChannelSel].period_h_latch  = value;
					break;
				case 5:
					ChannelRegs[ChannelSel].period = (ChannelRegs[ChannelSel].period_h_latch << 8) | value;
					break;
				case 6:
					ChannelRegs[ChannelSel].len = (ChannelRegs[ChannelSel].len & 0x00FF) | (value << 8);
					break;
				case 7:
					ChannelRegs[ChannelSel].len = (ChannelRegs[ChannelSel].len & 0xFF00) | value;
					break;
				case 8:
					ChannelRegs[ChannelSel].act = (value & 0x80) != 0;
					ChannelRegs[ChannelSel].repeat = (value & 0x01) != 0;
					ChannelRegs[ChannelSel].sam_ctr = 0;
					break;
				case 9:
					ChannelRegs[ChannelSel].vol = value & 0xFC;
					break;
				case 10:
					ChannelRegs[ChannelSel].repoff = (ChannelRegs[ChannelSel].repoff  & 0x00FF) | (value << 8);
					break;
				case 11:
					ChannelRegs[ChannelSel].repoff = (ChannelRegs[ChannelSel].repoff & 0xFF00) | value;
					break;
				case 12:
					ChannelRegs[ChannelSel].peak = 0;
					break;
				case 14:
					Volume = value & 0xFC; 
					break;
				case 15:
					ChannelSel = value % NUM_CHANNELS;
					break;
				}
			}
			else 
			{
				// everything else write chip ram!
				int chipaddr = (((int)jimPage) << 8) + (((int)address) & 0xFF);
				ChipRam[chipaddr % RAM_SIZE] = value;
			}
		}		
	}
}

bool Hog1MPaulaRead(UINT16 address, UINT8 *value)
{
	if (!Hog1MPaulaEnabled)
		return false;

	if (jimDev == JIM_DEV)
	{
		if (address == 0xfcff) {
			*value = ~jimDev;
			return true;
		} 
		else if (address == 0xfcfe) {
			*value = (UINT8)jimPage;
			return true;
		} 
		else if (address == 0xfcfd) {
			*value = jimPage >> 8;
			return true;
		}
		else if ((address & 0xff00) == 0xfd00) {
			//jim
			if ((jimPage == JIM_PAGE) && ((address & 0x00f0) == REG_BASE))
			{
				//note registers are all exposed big-endian style
				switch (address & 0x0F)
				{
				case 0:
					*value = ChannelRegs[ChannelSel].data;
					break;
				case 1:
					*value = ChannelRegs[ChannelSel].addr_bank;
					break;
				case 2:
					*value = ChannelRegs[ChannelSel].addr >> 8;
					break;
				case 3:
					*value = (UINT8)ChannelRegs[ChannelSel].addr;
					break;
				case 4:
					*value = ChannelRegs[ChannelSel].period >> 8;
					break;
				case 5:
					*value = (UINT8)ChannelRegs[ChannelSel].period;
					break;
				case 6:
					*value = ChannelRegs[ChannelSel].len >> 8;
					break;
				case 7:
					*value = (UINT8)ChannelRegs[ChannelSel].len;
					break;
				case 8:
					*value = (ChannelRegs[ChannelSel].act?0x80:0x00)| (ChannelRegs[ChannelSel].repeat ? 0x01 : 0x00);
					break;
				case 9:
					*value = ChannelRegs[ChannelSel].vol;
					break;
				case 10:
					*value = ChannelRegs[ChannelSel].repoff >> 8;
					break;
				case 11:
					*value = (UINT8)ChannelRegs[ChannelSel].repoff;
					break;
				case 12:
					*value = ChannelRegs[ChannelSel].peak;
					break;
				case 14:
					*value = Volume; //as of 2019/11/23 real paula returns sel here - is a bug to be fixed
					break;
				case 15:
					*value = ChannelSel;
					break;
				default:
					*value = 255;
					break;
				}
			}
			else
			{
				// everything else write chip ram!
				int chipaddr = (((int)jimPage) << 8) + (((int)address) & 0xFF);
				*value = ChipRam[chipaddr % RAM_SIZE];
			}
			return true;
		}
		else
			return false;
	}

	return false;
}

void Hog1MPaulaUpdate(UINT cycles)
{
	CycleCount = 4 * cycles; //8M cycles

	while (CycleCount > 0)
	{
		for (int i = 0; i < NUM_CHANNELS; i++)
		{
			CHANNELREGS *curchan = &ChannelRegs[i];
			curchan->clken_sam_next = false;
		}


		//do this once every 3.5MhZ
		paula_clock_acc += H1M_PCLK_A;
		while (paula_clock_acc > H1M_PCLK_LIM)
		{
			paula_clock_acc -= H1M_PCLK_LIM;

			for (int i = 0; i < NUM_CHANNELS; i++)
			{
				CHANNELREGS *curchan = &ChannelRegs[i];

				if (!curchan->act) {
					curchan->samper_ctr = 0;
					curchan->clken_sam_next = 0;
				}
				else {
					if (!curchan->act_prev)
					{
						curchan->clken_sam_next = true;
						curchan->samper_ctr = curchan->period;
					}
					else if (curchan->samper_ctr == 0)
					{
						curchan->clken_sam_next = true;
						curchan->samper_ctr = curchan->period;
						curchan->data = curchan->data_next;
					}
					else
						curchan->samper_ctr--;
				}

				curchan->act_prev = curchan->act;
			}

		}



		//this is a very rough approximation of what really happens in terms of prioritisation
		//but should be close enough - normally data accesses that clash will be queued up
		//by intcon and could take many 8M cycles but here we just always deliver the data!
		for (int i = 0; i < NUM_CHANNELS; i++)
		{
			CHANNELREGS *curchan = &ChannelRegs[i];
		
			if (curchan->clken_sam_next)
			{
				int addr = (((int)curchan->addr_bank) << 16) + (int)curchan->addr + (int)curchan->sam_ctr;
				curchan->data_next = ChipRam[addr % RAM_SIZE];

				if (curchan->sam_ctr == curchan->len)
				{
					if (curchan->repeat)
					{
						curchan->sam_ctr = curchan->repoff;
					}
					else 
					{
						curchan->act = false;
						curchan->sam_ctr = 0;
					}
				}
				else 
				{
					curchan->sam_ctr++;
				}
			}
		}

		static float a = 0;

		sample_clock_acc++;
		while (sample_clock_acc >= H1M_CYCLES_PER_STREAM_SAMPLE)
		{
			
			int sample = 0;

			sample = 0;

			for (int i = 0; i < NUM_CHANNELS; i++)
			{
				int snd_dat = (int)ChannelRegs[i].data * (int)(ChannelRegs[i].vol >> 2);
				sample += snd_dat;
				int snd_mag = ((snd_dat > 0) ? snd_dat : -snd_dat) >> 6;
				if (snd_mag > ChannelRegs[i].peak)
					ChannelRegs[i].peak = snd_mag;
			}
			sample = sample * (Volume >> 2) / NUM_CHANNELS;
			sample = sample / 4;

			SampleBuf[SampleWritePtr] = sample;
			SampleWritePtr++;
			SampleBuf[SampleWritePtr] = sample;
			SampleWritePtr++;
			if (SampleWritePtr / 2 >= SampleBufSize)
			{
				pSoundStreamerPaula->Stream(SampleBuf);
				SampleWritePtr = 0;
			}


			sample_clock_acc -= H1M_CYCLES_PER_STREAM_SAMPLE;
		}

		CycleCount--;
	}


}

/*
void SaveMusic5000UEF(FILE *SUEF)
{
	if (!Music5000Enabled)
		return;

	unsigned int sz = RAM_SIZE + sizeof(PhaseRam) + 4 + 8;
	fput16(0x0477, SUEF);
	fput32(sz, SUEF);
	fwrite(WaveRam, 1, RAM_SIZE, SUEF);
	fwrite(PhaseRam, 1, sizeof(PhaseRam), SUEF);
	fputc(CurCh, SUEF);
	fputc(CycleCount, SUEF);
	fputc(ActiveRegSet, SUEF);
	fputc(0, SUEF);//Unused pad
	fput32(SampleLeft, SUEF);
	fput32(SampleRight, SUEF);
}

void LoadMusic5000UEF(FILE *SUEF)
{
	fread(WaveRam, 1, RAM_SIZE, SUEF);
	fread(PhaseRam, 1, sizeof(PhaseRam), SUEF);
	CurCh = fgetc(SUEF);
	CycleCount = fgetc(SUEF);
	ActiveRegSet = fgetc(SUEF);
	fgetc(SUEF);//Unused pad
	SampleLeft = fget32(SUEF);
	SampleRight = fget32(SUEF);
}
*/