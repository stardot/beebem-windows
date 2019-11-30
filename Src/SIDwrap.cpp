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

#include "SIDwrap.h"
#include "sid.h"

bool		SIDEnabled = false;
extern int	SoundVolume;

static INT16 *SampleBuf = NULL;
static UINT32 SampleBufSize = 0;
static UINT32 SampleWritePtr = 0;

static SoundStreamer *pSoundStreamerPaula = NULL;

#define SID_SAMPLE_RATE 48000

static SIDChip *pSID;

static int nCyclesAcc = 0;

void SIDInit()
{

	// Init the SID emulation
	delete pSID;
	pSID = new SIDChip();
	pSID->set_chip_model(MOS6581);
	pSID->set_sampling_parameters(1000000, SAMPLE_FAST, SID_SAMPLE_RATE);
	
	// Init the streamer
	delete pSoundStreamerPaula;
	pSoundStreamerPaula = CreateSoundStreamer(SID_SAMPLE_RATE, 16, 1);
	if (!pSoundStreamerPaula)
	{
		SIDEnabled = false;
	}
	else
	{
		pSoundStreamerPaula->Play();
		SampleBufSize = (UINT32)pSoundStreamerPaula->BufferSize();
		if (SampleBuf)
			free(SampleBuf);
		SampleBuf = (INT16*)malloc(SampleBufSize * 2);
		if (SampleBuf == NULL)
		{
			SIDEnabled = false;
		}
		SampleWritePtr = 0;
	}
}

void SIDReset()
{
	delete pSoundStreamerPaula;
	pSoundStreamerPaula = NULL;
	delete pSID;
	pSID = NULL;
	
}

void SIDWrite(UINT16 address, UINT8 value)
{
	if (!SIDEnabled)
		return;
	if (pSID == NULL)
		return;

	if ((address & 0xFFE0) == 0xFC20) {
		pSID->write(address & 0x1F, value);
	}
}

bool SIDRead(UINT16 address, UINT8 *value)
{
	if (!SIDEnabled)
		return false;
	if (pSID == NULL)
		return false;

	if ((address & 0xFFE0) == 0xFC20) {
		*value = pSID->read(address);
	}
	
	return false;
}

void SIDUpdate(UINT cycles)
{
	if (!SIDEnabled || pSID == NULL)
		return;

	nCyclesAcc += cycles;
	
	while (nCyclesAcc >= 2)
	{
		int cycles1M = nCyclesAcc >> 1;

		while (cycles1M > 0) {
			int n = SampleBufSize - SampleWritePtr;

			SampleWritePtr += pSID->clock(cycles1M, &SampleBuf[SampleWritePtr], n, 0);

			if (SampleWritePtr == SampleBufSize)
			{
				pSoundStreamerPaula->Stream(SampleBuf);
				SampleWritePtr = 0;
			}
		}

		nCyclesAcc = nCyclesAcc & 1;
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