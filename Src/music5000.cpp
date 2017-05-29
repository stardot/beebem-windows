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
// BeebEm Music 5000 emulation
//
// Mike Wyatt - Jan 2016
//

#include <windows.h>
#include <stdio.h>
#include "soundstream.h"
#include "music5000.h"
#include "uefstate.h"

BOOL Music5000Enabled = FALSE;
extern int SoundVolume;

#define RAM_SIZE              2048
#define WAVE_TABLE_SIZE       128
#define WAVE_TABLES           14
#define NUM_CHANNELS          16
#define NUM_REG_SETS          2

// Control register bits
#define CTRL_STEREO_POS       (0xf)
#define CTRL_INVERT_WAVE      (1 << 4)
#define CTRL_MODULATE_ADJ     (1 << 5)

// Data bits
#define DATA_SIGN             (1 << 7)
#define DATA_VALUE            (0x7f)
#define FREQ_DISABLE          (1 << 0)

// Channel register sets
#define REG_SET_NORMAL        0
#define REG_SET_ALT           1

typedef struct
{
	UINT8 FreqLo[NUM_CHANNELS];
	UINT8 FreqMed[NUM_CHANNELS];
	UINT8 FreqHi[NUM_CHANNELS];
	UINT8 Unused1[NUM_CHANNELS];
	UINT8 Unused2[NUM_CHANNELS];
	UINT8 WaveformReg[NUM_CHANNELS];
	UINT8 AmplitudeReg[NUM_CHANNELS];
	UINT8 ControlReg[NUM_CHANNELS];
} CHANNELREGS;

typedef struct
{
	UINT8 WaveTable[WAVE_TABLES][WAVE_TABLE_SIZE];
	CHANNELREGS ChannelRegs[NUM_REG_SETS];  // Normal & Alt
} WAVERAM;

static UINT8 WaveRam[RAM_SIZE];
static WAVERAM* pWaveRam = (WAVERAM*)WaveRam;
static UINT32 PhaseRam[NUM_CHANNELS];

static double ChordBase[8] = { 0, 8.25, 24.75, 57.75, 123.75, 255.75, 519.75, 1047.75 };
static double StepInc[8] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
static INT D2ATable[128];

static INT StereoLeft[16]  = { 100,100,100,100,100,100,100,100,  0,  0,  0, 17, 33, 50, 67, 83 };
static INT StereoRight[16] = {   0,  0,  0,  0,  0,  0,  0,  0,100,100,100, 83, 67, 50, 33, 17 };

// 6MHz clock, 128 cycles to update all channels
static UINT CycleCount = 0;

static INT16 *SampleBuf = NULL;
static UINT32 SampleBufSize = 0;
static UINT32 SampleWritePtr = 0;

static UINT CurCh;
static UINT ActiveRegSet;
static INT SampleLeft;
static INT SampleRight;

static SoundStreamer *pSoundStreamer = NULL;

void Music5000Init()
{
	UINT i;
	UINT chord;
	UINT step;

	if (sizeof(WAVERAM) != RAM_SIZE)
	{
		exit(-1);
	}

	memset(WaveRam, 0, sizeof(WaveRam));
	memset(PhaseRam, 0, sizeof(PhaseRam));
	CycleCount = 0;
	CurCh = 0;
	ActiveRegSet = REG_SET_NORMAL;
	SampleLeft = 0;
	SampleRight = 0;
	SampleWritePtr = 0;

	// Build the D2A table
	i = 0;
	for (chord = 0; chord < 8; chord++)
	{
		double val = ChordBase[chord];
		for (step = 0; step < 16; step++)
		{
			D2ATable[i] = (INT)(val * 4); // Multiply up to get an integer
			val += StepInc[chord];
			i++;
		}
	}

	// Init the streamer
	delete pSoundStreamer;
	pSoundStreamer = CreateSoundStreamer(46875, 16, 2);
	if (!pSoundStreamer)
	{
		Music5000Enabled = FALSE;
	}
	else
	{
		pSoundStreamer->Play();
		SampleBufSize = pSoundStreamer->BufferSize();
		if (SampleBuf)
			free(SampleBuf);
		SampleBuf = (INT16*)malloc(SampleBufSize * 4);
		if (SampleBuf == NULL)
		{
			Music5000Enabled = FALSE;
		}
	}
}

void Music5000Reset()
{
	delete pSoundStreamer;
	pSoundStreamer = NULL;
}

void Music5000Write(UINT8 page, UINT8 address, UINT8 value)
{
	if (!Music5000Enabled)
		return;

	if ((page & 0xf0) != 0x30)
		return;

	page &= 0xe; // Bit0 unused
	UINT offset = (page << 7) + address;
	WaveRam[offset] = value;

#if 0
	char str[200];
	sprintf(str, "M5KWrite %02x ?%02x=%02x\n", page, address, value);
	OutputDebugString(str);
#endif
}

UINT8 Music5000Read(UINT8 page, UINT8 address)
{
	if (!Music5000Enabled)
		return 0xFF;

	if ((page & 0xf0) != 0x30)
		return 0xFF;

	page &= 0xe; // Bit0 unused
	UINT offset = (page << 7) + address;
	UINT8 value = WaveRam[offset];

#if 0
	char str[200];
	sprintf(str, "M5KRead %02x ?%02x=%02x\n", page, address, value);
	OutputDebugString(str);
#endif

	return value;
}

void Music5000Update(UINT cycles)
{
	CHANNELREGS *pChRegs;
	UINT32 freq;
	UINT wavetable, offset;
	UINT amplitude;
	UINT control;
	UINT data;
	UINT sign;
	UINT pos;
	INT sample;

	// Convert 2MHz 6502 cycles to 6MHz Music5000 cycles
	CycleCount += cycles * 3;

	// Need 8 cycles to update a channel
	while (CycleCount >= 8)
	{
		// Update phase for active register set
		pChRegs = &pWaveRam->ChannelRegs[ActiveRegSet];

		freq = (pChRegs->FreqHi[CurCh] << 16) +
			(pChRegs->FreqMed[CurCh] << 8) + (pChRegs->FreqLo[CurCh] & ~FREQ_DISABLE);
		PhaseRam[CurCh] += freq;

		// Pull wave sample out for the active register set
		offset = (PhaseRam[CurCh] >> 17) & 0x7f;
		wavetable = (pChRegs->WaveformReg[CurCh] >> 4);
		data = pWaveRam->WaveTable[wavetable][offset];
		amplitude = pChRegs->AmplitudeReg[CurCh];
		control = pChRegs->ControlReg[CurCh];
		sign = data & DATA_SIGN;
		data &= DATA_VALUE;

		if (control & CTRL_INVERT_WAVE)
			sign ^= DATA_SIGN;

		if (!(pChRegs->FreqLo[CurCh] & FREQ_DISABLE))
		{
			if (amplitude > 0x80)
				amplitude = 0x80;
			data = data * amplitude / 0x80;

			sample = D2ATable[data];
			if (sign)
				sample = -sample;

#if 0
			if (sample > 0)
			{
				char str[200];
				sprintf(str, "S %d, amplitude %d, data %d\n", sample, amplitude, data);
				OutputDebugString(str);
			}
#endif

			// Stereo
			pos = (control & CTRL_STEREO_POS);
			SampleLeft += sample * StereoLeft[pos] / 100;
			SampleRight += sample * StereoRight[pos] / 100;
		}

		// Modulate the next channel?
		if ((control & CTRL_MODULATE_ADJ) && (sign))
			ActiveRegSet = REG_SET_ALT;
		else
			ActiveRegSet = REG_SET_NORMAL;

		CurCh++;
		if (CurCh == NUM_CHANNELS)
		{
			CurCh = 0;
			ActiveRegSet = REG_SET_NORMAL;

			// Apply volume
			SampleLeft = (SampleLeft * SoundVolume)/100;
			SampleRight = (SampleRight * SoundVolume)/100;

			// Range check
			if (SampleLeft < -32768)
				SampleLeft = -32768;
			else if (SampleLeft > 32767)
				SampleLeft = 32767;

			if (SampleRight < -32768)
				SampleRight = -32768;
			else if (SampleRight > 32767)
				SampleRight = 32767;

			SampleBuf[SampleWritePtr] = SampleLeft;
			SampleWritePtr++;
			SampleBuf[SampleWritePtr] = SampleRight;
			SampleWritePtr++;
			if (SampleWritePtr/2 >= SampleBufSize)
			{
				pSoundStreamer->Stream(SampleBuf);
				SampleWritePtr = 0;
			}

			SampleLeft = 0;
			SampleRight = 0;
		}

		CycleCount -= 8;
	}
}

void SaveMusic5000UEF(FILE *SUEF)
{
	if (!Music5000Enabled)
		return;

	unsigned int sz = RAM_SIZE + sizeof(PhaseRam) + 4 + 8;
	fput16(0x0477,SUEF);
	fput32(sz,SUEF);
	fwrite(WaveRam,1,RAM_SIZE,SUEF);
	fwrite(PhaseRam,1,sizeof(PhaseRam),SUEF);
	fputc(CurCh,SUEF);
	fputc(CycleCount,SUEF);
	fputc(ActiveRegSet,SUEF);
	fputc(0,SUEF);//Unused pad
	fput32(SampleLeft,SUEF);
	fput32(SampleRight,SUEF);
}

void LoadMusic5000UEF(FILE *SUEF)
{
	fread(WaveRam,1,RAM_SIZE,SUEF);
	fread(PhaseRam,1,sizeof(PhaseRam),SUEF);
	CurCh=fgetc(SUEF);
	CycleCount=fgetc(SUEF);
	ActiveRegSet=fgetc(SUEF);
	fgetc(SUEF);//Unused pad
	SampleLeft=fget32(SUEF);
	SampleRight=fget32(SUEF);
}
