/*******************************************************************************
BeebEm - BBC Micro and Master 128 Emulator

TMS5220 interface

Based on code written by Frank Palazzolo and others

Copyright (c) 1997-2007, Nicola Salmoria and the MAME team
Copyright (C) 2005  John Welch
Copyright (C) 2023  Chris Needham

All rights reserved.

Redistribution and use of this code or any derivative works are permitted
provided that the following conditions are met:

* Redistributions may not be sold, nor may they be used in a commercial
product or activity.

* Redistributions that are modified from the original source must include the
complete source code, including the source code for all components used by a
binary built from the modified sources. However, as a special exception, the
source code distributed need not include anything that is normally distributed
(in either source or binary form) with the major components (compiler, kernel,
and so on) of the operating system on which the executable runs, unless that
component itself accompanies the executable.

* Redistributions must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or other
materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include <stdlib.h>

#include "Speech.h"
#include "beebsound.h"
#include "beebwin.h"
#include "Main.h"
#include "Log.h"

constexpr int FRAC_BITS = 14;
constexpr int FRAC_ONE  = 1 << FRAC_BITS;
constexpr int FRAC_MASK = FRAC_ONE - 1;

/* clock rate = 80 * output sample rate,     */
/* usually 640000 for 8000 Hz sample rate or */
/* usually 800000 for 10000 Hz sample rate.  */

constexpr int MAX_SAMPLE_CHUNK = 8000;

constexpr int FIFO_SIZE = 16;

struct TMS5220
{
	/* these contain data that describes the 128-bit data FIFO */
	uint8_t fifo[FIFO_SIZE];
	uint8_t fifo_head;
	uint8_t fifo_tail;
	uint8_t fifo_count;
	uint8_t fifo_bits_taken;
	uint8_t phrom_bits_taken;

	/* these contain global status bits */
	/*
	 R Nabet : speak_external is only set when a speak external command is going on.
	 tms5220_speaking is set whenever a speak or speak external command is going on.
	 Note that we really need to do anything in tms5220_process and play samples only when
	 tms5220_speaking is true.  Else, we can play nothing as well, which is a
	 speed-up...
	 */
	bool tms5220_speaking; /* Speak or Speak External command in progress */
	bool speak_external; /* Speak External command in progress */
	bool talk_status; /* tms5220 is really currently speaking */
	bool first_frame; /* we have just started speaking, and we are to parse the first frame */
	bool last_frame; /* we are doing the frame of sound */
	bool buffer_low; /* FIFO has less than 8 bytes in it */
	bool buffer_empty; /* FIFO is empty*/
	bool interrupt; /* true to interrupt (active high), convert to active low in SpeechInterrupt() */
	bool ready; /* true if ready (active high), convert to active low in SpeechReady() */
	int ready_count; /* countdown timer to reset the ready flag after read/write enable is asserted */

	/* these contain data describing the current and previous voice frames */
	uint16_t old_energy;
	uint16_t old_pitch;
	int old_k[10];

	uint16_t new_energy;
	uint16_t new_pitch;
	int new_k[10];

	/* these are all used to contain the current state of the sound generation */
	uint16_t current_energy;
	uint16_t current_pitch;
	int current_k[10];

	uint16_t target_energy;
	uint16_t target_pitch;
	int target_k[10];

	uint8_t interp_count; /* number of interp periods (0-7) */
	uint8_t sample_count; /* sample number within interp (0-24) */
	int pitch_count;

	int u[11];
	int x[10];

	int8_t randbit;

	int phrom_address;

	uint8_t data_register; /* data register, used by read command */
	bool RDB_flag; /* whether we should read data register or status register */
};

/* the state of the streamed output */
struct tms5220_info
{
	int last_sample;
	int curr_sample;
	int source_step;
	int source_pos;
	int clock;
	TMS5220 *chip;
};

/* TMS5220 ROM Tables */

/* This is the energy lookup table (4-bits -> 10-bits) */

const static unsigned short energytable[0x10] = {
	0x0000,0x00C0,0x0140,0x01C0,0x0280,0x0380,0x0500,0x0740,
	0x0A00,0x0E40,0x1440,0x1C80,0x2840,0x38C0,0x5040,0x7FC0
};

/* This is the pitch lookup table (6-bits -> 8-bits) */

const static unsigned short pitchtable[0x40] = {
	0x0000,0x1000,0x1100,0x1200,0x1300,0x1400,0x1500,0x1600,
	0x1700,0x1800,0x1900,0x1A00,0x1B00,0x1C00,0x1D00,0x1E00,
	0x1F00,0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2600,
	0x2700,0x2800,0x2900,0x2A00,0x2B00,0x2D00,0x2F00,0x3100,
	0x3300,0x3500,0x3600,0x3900,0x3B00,0x3D00,0x3F00,0x4200,
	0x4500,0x4700,0x4900,0x4D00,0x4F00,0x5100,0x5500,0x5700,
	0x5C00,0x5F00,0x6300,0x6600,0x6A00,0x6E00,0x7300,0x7700,
	0x7B00,0x8000,0x8500,0x8A00,0x8F00,0x9500,0x9A00,0xA000
};

/* These are the reflection coefficient lookup tables */

/* K1 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k1table[0x20] = {
	(short)0x82C0,(short)0x8380,(short)0x83C0,(short)0x8440,(short)0x84C0,(short)0x8540,(short)0x8600,(short)0x8780,
	(short)0x8880,(short)0x8980,(short)0x8AC0,(short)0x8C00,(short)0x8D40,(short)0x8F00,(short)0x90C0,(short)0x92C0,
	(short)0x9900,(short)0xA140,(short)0xAB80,(short)0xB840,(short)0xC740,(short)0xD8C0,(short)0xEBC0,0x0000,
	0x1440,0x2740,0x38C0,0x47C0,0x5480,0x5EC0,0x6700,0x6D40
};

/* K2 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k2table[0x20] = {
	(short)0xAE00,(short)0xB480,(short)0xBB80,(short)0xC340,(short)0xCB80,(short)0xD440,(short)0xDDC0,(short)0xE780,
	(short)0xF180,(short)0xFBC0,0x0600,0x1040,0x1A40,0x2400,0x2D40,0x3600,
	0x3E40,0x45C0,0x4CC0,0x5300,0x5880,0x5DC0,0x6240,0x6640,
	0x69C0,0x6CC0,0x6F80,0x71C0,0x73C0,0x7580,0x7700,0x7E80
};

/* K3 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k3table[0x10] = {
	(short)0x9200,(short)0x9F00,(short)0xAD00,(short)0xBA00,(short)0xC800,(short)0xD500,(short)0xE300,(short)0xF000,
	(short)0xFE00,0x0B00,0x1900,0x2600,0x3400,0x4100,0x4F00,0x5C00
};

/* K4 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k4table[0x10] = {
	(short)0xAE00,(short)0xBC00,(short)0xCA00,(short)0xD800,(short)0xE600,(short)0xF400,0x0100,0x0F00,
	0x1D00,0x2B00,0x3900,0x4700,0x5500,0x6300,0x7100,0x7E00
};

/* K5 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k5table[0x10] = {
	(short)0xAE00,(short)0xBA00,(short)0xC500,(short)0xD100,(short)0xDD00,(short)0xE800,(short)0xF400,(short)0xFF00,
	0x0B00,0x1700,0x2200,0x2E00,0x3900,0x4500,0x5100,0x5C00
};

/* K6 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k6table[0x10] = {
	(short)0xC000,(short)0xCB00,(short)0xD600,(short)0xE100,(short)0xEC00,(short)0xF700,0x0300,0x0E00,
	0x1900,0x2400,0x2F00,0x3A00,0x4500,0x5000,0x5B00,0x6600
};

/* K7 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k7table[0x10] = {
	(short)0xB300,(short)0xBF00,(short)0xCB00,(short)0xD700,(short)0xE300,(short)0xEF00,(short)0xFB00,0x0700,
	0x1300,0x1F00,0x2B00,0x3700,0x4300,0x4F00,0x5A00,0x6600
};

/* K8 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k8table[0x08] = {
	(short)0xC000,(short)0xD800,(short)0xF000,0x0700,0x1F00,0x3700,0x4F00,0x6600
};

/* K9 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k9table[0x08] = {
	(short)0xC000,(short)0xD400,(short)0xE800,(short)0xFC00,0x1000,0x2500,0x3900,0x4D00
};

/* K10 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

const static short k10table[0x08] = {
	(short)0xCD00,(short)0xDF00,(short)0xF100,0x0400,0x1600,0x2000,0x3B00,0x4D00
};

/* chirp table */

static const char chirptable[41] = {
	0x00, 0x2a, (char)0xd4, 0x32,
	(char)0xb2, 0x12, 0x25, 0x14,
	0x02, (char)0xe1, (char)0xc5, 0x02,
	0x5f, 0x5a, 0x05, 0x0f,
	0x26, (char)0xfc, (char)0xa5, (char)0xa5,
	(char)0xd6, (char)0xdd, (char)0xdc, (char)0xfc,
	0x25, 0x2b, 0x22, 0x21,
	0x0f, (char)0xff, (char)0xf8, (char)0xee,
	(char)0xed, (char)0xef, (char)0xf7, (char)0xf6,
	(char)0xfa, 0x00, 0x03, 0x02,
	0x01
};

/* interpolation coefficients */

static const char interp_coeff[8] = {
	8, 8, 8, 4, 4, 2, 2, 1
};

#define ENABLE_LOG 0

/* Static function prototypes */
static void process_command(TMS5220 *tms);
static int extract_bits(TMS5220 *tms, int count);
static int parse_frame(TMS5220 *tms, bool first_frame);
static void check_buffer_low(TMS5220 *tms);

static TMS5220 *tms5220_create();
static void tms5220_destroy(TMS5220 *tms);
static void tms5220_set_frequency(int frequency);
static void tms5220_process(TMS5220 *chip, short int *buffer, int size);
static void tms5220_reset(TMS5220 *tms);
static void tms5220_read_enable(TMS5220 *tms);
static void tms5220_data_write(TMS5220 *tms, unsigned char data);
static unsigned char tms5220_status_read(TMS5220 *tms);
static bool tms5220_int_read(TMS5220 *tms);

bool SpeechDefault;
bool SpeechEnabled;

static struct tms5220_info *tms5220;
static unsigned char phrom_rom[16][16384];

/*----------------------------------------------------------------------------*/

void BeebReadPhroms()
{
	/* Read all ROM files in the BeebFile directory */
	// This section rewritten for V.1.32 to take account of roms.cfg file.
	char TmpPath[256];
	strcpy(TmpPath, mainWin->GetUserDataPath());
	strcat(TmpPath, "phroms.cfg");

	FILE *RomCfg = fopen(TmpPath,"rt");

	if (RomCfg == nullptr)
	{
		mainWin->Report(MessageType::Error, "Cannot open PHROM Configuration file phroms.cfg");
		return;
	}

	// read phroms
	for (int romslot = 15; romslot >= 0; romslot--)
	{
		char RomName[80];
		fgets(RomName, 80, RomCfg);

		if (strchr(RomName, 13)) *strchr(RomName, 13) = 0;
		if (strchr(RomName, 10)) *strchr(RomName, 10) = 0;

		char fullname[256];
		strcpy(fullname, RomName);

		if (RomName[0] != '\\' && RomName[1] != ':')
		{
			strcpy(fullname, mainWin->GetUserDataPath());
			strcat(fullname, "Phroms/");
			strcat(fullname, RomName);
		}

		if (strncmp(RomName, "EMPTY", 5) != 0)
		{
			for (int sc = 0; fullname[sc]; sc++)
			{
				if (fullname[sc] == '\\')
					fullname[sc] = '/';
			}

			FILE *InFile = fopen(fullname,"rb");

			if (InFile != nullptr)
			{
				fread(phrom_rom[romslot], 1, 16384, InFile);
				fclose(InFile);
			}
			else
			{
				mainWin->Report(MessageType::Error, "Cannot open specified PHROM: %s", fullname);
			}
		}
	}

	fclose(RomCfg);
}

static int DoRead(TMS5220 *tms, int count)
{
	int phrom = (tms->phrom_address >> 14) & 0xf;

	// fprintf(stderr, "In my_read with addr = 0x%04x, phrom = 0x%02x, count = %d", addr, phrom, count);

	int val = 0;

	while (count--)
	{
		int addr = tms->phrom_address & 0x3fff;

		val = (val << 1) | ((phrom_rom[phrom][addr] >> tms->phrom_bits_taken) & 1);

		tms->phrom_bits_taken++;

		if (tms->phrom_bits_taken >= 8)
		{
			tms->phrom_address++;
			tms->phrom_bits_taken = 0;
		}
	}

	// fprintf(stderr, ", returning 0x%02x\n", val);
	return val;
}

/*--------------------------------------------------------------------------*/

// Allocate buffers and reset the 5220

void SpeechStart()
{
	int clock = 640000;

	SpeechEnabled = false;
	tms5220 = nullptr;

	BeebReadPhroms();

	tms5220 = (tms5220_info *)malloc(sizeof(*tms5220));
	memset(tms5220, 0, sizeof(*tms5220));
	tms5220->clock = clock;

	tms5220->chip = tms5220_create();
	if (tms5220->chip == nullptr)
	{
		free(tms5220);
		tms5220 = nullptr;
		return;
	}

	/* reset the 5220 */
	tms5220_reset(tms5220->chip);

	/* set the initial frequency */
	tms5220_set_frequency(clock);
	tms5220->source_pos = 0;
	tms5220->last_sample = tms5220->curr_sample = 0;

	SpeechEnabled = true;
}

/*--------------------------------------------------------------------------*/

// Free buffers

void SpeechStop()
{
	if (tms5220 != nullptr)
	{
		tms5220_destroy(tms5220->chip);
		free(tms5220);
		tms5220 = nullptr;
	}

	SpeechEnabled = false;
}

/*--------------------------------------------------------------------------*/

// Write data to the sound chip

void SpeechWrite(unsigned char Data)
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		tms5220_data_write(tms5220->chip, Data);
	}
}

void SpeechReadEnable()
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		tms5220_read_enable(tms5220->chip);
	}
}

/*--------------------------------------------------------------------------*/

// Read status or data from the sound chip

unsigned char SpeechRead()
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		unsigned char Value = tms5220_status_read(tms5220->chip);

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: read speech %02X\n", PrePC, Value);
		#endif

		return Value;

	}
	else
	{
		return 0;
	}
}

/*--------------------------------------------------------------------------*/

// Return the not ready status from the sound chip

bool SpeechReady()
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		return !tms5220->chip->ready;
	}
	else
	{
		return false;
	}
}

/*--------------------------------------------------------------------------*/

// Return the int status from the sound chip

bool SpeechInterrupt()
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		return tms5220_int_read(tms5220->chip);
	}
	else
	{
		return 0;
	}
}

/*--------------------------------------------------------------------------*/

// update the sound chip so that it is in sync with CPU execution

void SpeechUpdate(unsigned char *buff, int length)
{
	int16_t sample_data[MAX_SAMPLE_CHUNK];
	int16_t *curr_data = sample_data;
	int prev = tms5220->last_sample;
	int curr = tms5220->curr_sample;
	unsigned char *buffer = buff;

	/* finish off the current sample */
	if (tms5220->source_pos > 0)
	{
		/* interpolate */
		while (length > 0 && tms5220->source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - tms5220->source_pos)) + (curr * tms5220->source_pos)) >> FRAC_BITS;
			// samp = ((samp + 32768) >> 8) & 255;
			// fprintf(stderr, "Sample = %d\n", samp);
			*buffer++ = samp;
			tms5220->source_pos += tms5220->source_step;
			length--;
		}

		/* if we're over, continue; otherwise, we're done */
		if (tms5220->source_pos >= FRAC_ONE)
		{
			tms5220->source_pos -= FRAC_ONE;
		}
		else
		{
			// if (buffer - buff != len)
			//	fprintf(stderr, "Here for some reason - mismatch in num of samples = %d, %d\n", len, buffer - buff);
			tms5220_process(tms5220->chip, sample_data, 0);
			return;
		}
	}

	/* compute how many new samples we need */
	int final_pos = tms5220->source_pos + length * tms5220->source_step;
	int new_samples = (final_pos + FRAC_ONE - 1) >> FRAC_BITS;
	if (new_samples > MAX_SAMPLE_CHUNK)
		new_samples = MAX_SAMPLE_CHUNK;

	/* generate them into our buffer */
	tms5220_process(tms5220->chip, sample_data, new_samples);
	prev = curr;
	curr = *curr_data++;

	/* then sample-rate convert with linear interpolation */
	while (length > 0)
	{
		/* interpolate */
		while (length > 0 && tms5220->source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - tms5220->source_pos)) + (curr * tms5220->source_pos)) >> FRAC_BITS;
			// samp = ((samp + 32768) >> 8) & 255;
			// fprintf(stderr, "Sample = %d\n", samp);
			*buffer++ = samp;
			tms5220->source_pos += tms5220->source_step;
			length--;
		}

		/* if we're over, grab the next samples */
		if (tms5220->source_pos >= FRAC_ONE)
		{
			tms5220->source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
			if (curr_data - sample_data > MAX_SAMPLE_CHUNK)
				curr_data = sample_data;
		}
	}

	/* remember the last samples */
	tms5220->last_sample = prev;
	tms5220->curr_sample = curr;

	// if (buffer - buff != len)
	//	fprintf(stderr, "At end of update - mismatch in num of samples = %d, %d\n", len, buffer - buff);
}

/**********************************************************************************************

tms5220_set_frequency -- adjusts the playback frequency

***********************************************************************************************/

void tms5220_set_frequency(int frequency)
{
	tms5220->source_step = (int)((double)(frequency / 80) * (double)FRAC_ONE / (double)SoundSampleRate);

	// fprintf(stderr, "Source Step = %d, FRAC_ONE = %d, FRAC_BITS = %d\n", tms5220->source_step, FRAC_ONE, FRAC_BITS);
}

TMS5220 *tms5220_create()
{
	TMS5220 *tms = (TMS5220 *)malloc(sizeof(*tms));
	memset(tms, 0, sizeof(*tms));
	return tms;
}

void tms5220_destroy(TMS5220 *tms)
{
	free(tms);
}

/*--------------------------------------------------------------------------*/

// Resets the TMS5220

static void tms5220_reset(TMS5220 *tms)
{
	/* initialize the FIFO */
	/*memset(tms->fifo, 0, sizeof(tms->fifo));*/
	tms->fifo_head = tms->fifo_tail = tms->fifo_count = tms->fifo_bits_taken = tms->phrom_bits_taken = 0;

	/* initialize the chip state */
	/* Note that we do not actually clear IRQ on start-up : IRQ is even raised if tms->buffer_empty or tms->buffer_low are false */
	tms->tms5220_speaking = tms->speak_external = tms->talk_status = tms->first_frame = tms->last_frame = tms->interrupt = false;
	tms->buffer_empty = tms->buffer_low = true;

	tms->RDB_flag = false;

	tms->ready = true;
	tms->ready_count = 0;

	/* initialize the energy/pitch/k states */
	tms->old_energy = tms->new_energy = tms->current_energy = tms->target_energy = 0;
	tms->old_pitch = tms->new_pitch = tms->current_pitch = tms->target_pitch = 0;
	memset(tms->old_k, 0, sizeof(tms->old_k));
	memset(tms->new_k, 0, sizeof(tms->new_k));
	memset(tms->current_k, 0, sizeof(tms->current_k));
	memset(tms->target_k, 0, sizeof(tms->target_k));

	/* initialize the sample generators */
	tms->interp_count = tms->sample_count = tms->pitch_count = 0;
	tms->randbit = 0;
	memset(tms->u, 0, sizeof(tms->u));
	memset(tms->x, 0, sizeof(tms->x));

	tms->phrom_address = 0;
}

static void tms5220_read_enable(TMS5220 *tms)
{
	tms->ready = false;
	tms->ready_count = 30;
}

/**********************************************************************************************

tms5220_data_write -- handle a write to the TMS5220

***********************************************************************************************/

void tms5220_data_write(TMS5220 *tms, unsigned char data)
{
	#if ENABLE_LOG
	WriteLog("%04X TMS5220: Write %02X\n", PrePC, data);
	#endif

	/* add this byte to the FIFO */
	if (tms->fifo_count < FIFO_SIZE)
	{
		tms->fifo[tms->fifo_tail] = data;
		tms->fifo_tail = (tms->fifo_tail + 1) % FIFO_SIZE;
		tms->fifo_count++;

		/* if we were speaking, then we're no longer empty */
		if (tms->speak_external)
		{
			tms->buffer_empty = false;
		}

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Added byte to FIFO (size=%d)\n", PrePC, tms->fifo_count);
		#endif
	}
	else
	{
		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Ran out of room in the FIFO!\n", PrePC);
		#endif
	}

	/* update the buffer low state */
	check_buffer_low(tms);

	if (!tms->speak_external)
	{
		/* R Nabet : we parse commands at once.  It is necessary for such commands as read. */
		process_command(tms);
	}

	tms->ready = false;
	tms->ready_count = 30;
}

/**********************************************************************************************

tms5220_status_read -- read status or data from the TMS5220

From the data sheet:
bit 0 = TS - Talk Status is active (high) when the VSP is processing speech data.
Talk Status goes active at the initiation of a Speak command or after nine
bytes of data are loaded into the FIFO following a Speak External command. It
goes inactive (low) when the stop code (Energy=1111) is processed, or
immediately by a buffer empty condition or a reset command.
bit 1 = BL - Buffer Low is active (high) when the FIFO buffer is more than half empty.
Buffer Low is set when the "Last-In" byte is shifted down past the half-full
boundary of the stack. Buffer Low is cleared when data is loaded to the stack
so that the "Last-In" byte lies above the half-full boundary and becomes the
ninth data byte of the stack.
bit 2 = BE - Buffer Empty is active (high) when the FIFO buffer has run out of data
while executing a Speak External command. Buffer Empty is set when the last bit
of the "Last-In" byte is shifted out to the Synthesis Section. This causes
Talk Status to be cleared. Speed is terminated at some abnormal point and the
Speak External command execution is terminated.

***********************************************************************************************/

static unsigned char tms5220_status_read(TMS5220 *tms)
{
	tms->ready = true;

	if (tms->RDB_flag)
	{
		/* if last command was read, return data register */
		tms->RDB_flag = false;
		return tms->data_register;
	}
	else
	{
		/* read status */

		/* clear the interrupt state */
		tms->interrupt = false;

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Status read: TS=%d BL=%d BE=%d\n", PrePC, tms->talk_status, tms->buffer_low, tms->buffer_empty);
		WriteLog("%04X TMS5220: Clear interrupt\n", PrePC);
		#endif

		return (tms->talk_status  ? 0x80 : 0x00) |
		       (tms->buffer_low   ? 0x40 : 0x00) |
		       (tms->buffer_empty ? 0x20 : 0x00);
	}
}

/*--------------------------------------------------------------------------*/

// Returns the interrupt state of the TMS5220 (active low)

static bool tms5220_int_read(TMS5220 *tms)
{
	return !tms->interrupt;
}

/*--------------------------------------------------------------------------*/

// Fill the buffer with a specific number of samples

void tms5220_process(TMS5220 *tms, int16_t *buffer, int size)
{
	int buf_count = 0;
	int i, interp_period;

tryagain:
	/* if we're not speaking, parse commands */
	/*while (!tms->speak_external && tms->fifo_count > 0)
	process_command(tms);*/

	/* if we're empty and still not speaking, fill with nothingness */
	if (!tms->tms5220_speaking && !tms->last_frame)
		goto empty;

	/* if we're to speak, but haven't started, wait for the 9th byte */
	if (!tms->talk_status && tms->speak_external)
	{
		if (tms->fifo_count < 9)
			goto empty;

		tms->talk_status = true;
		tms->first_frame = true; /* will cause the first frame to be parsed */
		tms->buffer_empty = false;
	}

#if 0
	/* we are to speak, yet we fill with 0s until start of next frame */
	if (tms->first_frame)
	{
		while (size > 0 && (tms->sample_count != 0 || tms->interp_count != 0))
		{
			tms->sample_count = (tms->sample_count + 1) % 200;
			tms->interp_count = (tms->interp_count + 1) % 25;
			buffer[buf_count] = 0x80; /* should be (-1 << 8) ??? (cf note in data sheet, p 10, table 4) */
			buf_count++;
			size--;
		}
	}
#endif

	/* loop until the buffer is full or we've stopped speaking */
	while (size > 0 && tms->talk_status)
	{
		int current_val;

		/* if we're ready for a new frame */
		if (tms->interp_count == 0 && tms->sample_count == 0)
		{
			/* Parse a new frame */
			if (!parse_frame(tms, tms->first_frame))
				break;

			tms->first_frame = false;

			/* Set old target as new start of frame */
			tms->current_energy = tms->old_energy;
			tms->current_pitch = tms->old_pitch;
			for (i = 0; i < 10; i++)
				tms->current_k[i] = tms->old_k[i];

			/* is this a zero energy frame? */
			if (tms->current_energy == 0)
			{
				/*printf("processing frame: zero energy\n");*/
				tms->target_energy = 0;
				tms->target_pitch = tms->current_pitch;
				for (i = 0; i < 10; i++)
					tms->target_k[i] = tms->current_k[i];
			}
			/* is this a stop frame? */
			else if (tms->current_energy == (energytable[15] >> 6))
			{
				/*printf("processing frame: stop frame\n");*/
				tms->current_energy = energytable[0] >> 6;
				tms->target_energy = tms->current_energy;
				/*tms->interp_count = tms->sample_count =*/ tms->pitch_count = 0;
				tms->last_frame = 0;
				if (tms->tms5220_speaking)
				{
					/* new speech command in progress */
					tms->first_frame = true;
				}
				else
				{
					/* really stop speaking */
					tms->talk_status = false;

					/* generate an interrupt if necessary */
					tms->interrupt = true;

					#if ENABLE_LOG
					WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
					#endif
				}

				/* try to fetch commands again */
				goto tryagain;
			}
			else
			{
				/* is this the ramp down frame? */
				if (tms->new_energy == (energytable[15] >> 6))
				{
					tms->target_energy = 0;
					tms->target_pitch = tms->current_pitch;
					for (i = 0; i < 10; i++)
						tms->target_k[i] = tms->current_k[i];
				}
				/* Reset the step size */
				else
				{
					tms->target_energy = tms->new_energy;
					tms->target_pitch = tms->new_pitch;

					for (i = 0; i < 4; i++)
					{
						tms->target_k[i] = tms->new_k[i];
					}

					if (tms->current_pitch == 0)
					{
						for (i = 4; i < 10; i++)
						{
							tms->target_k[i] = tms->current_k[i] = 0;
						}
					}
					else
					{
						for (i = 4; i < 10; i++)
						{
							tms->target_k[i] = tms->new_k[i];
						}
					}
				}
			}
		}
		else if (tms->interp_count == 0)
		{
			/* Update values based on step values */

			interp_period = tms->sample_count / 25;
			tms->current_energy += (tms->target_energy - tms->current_energy) / interp_coeff[interp_period];
			if (tms->old_pitch != 0)
				tms->current_pitch += (tms->target_pitch - tms->current_pitch) / interp_coeff[interp_period];

			for (i = 0; i < 10; i++)
			{
				tms->current_k[i] += (tms->target_k[i] - tms->current_k[i]) / interp_coeff[interp_period];
			}
		}

		current_val = 0x00;

		if (tms->old_energy == 0)
		{
			/* generate silent samples here */
			current_val = 0x00;
		}
		else if (tms->old_pitch == 0)
		{
			/* generate unvoiced samples here */
			tms->randbit = (rand() % 2) * 2 - 1;
			current_val = (tms->randbit * tms->current_energy) / 4;
		}
		else
		{
			/* generate voiced samples here */
			if (tms->pitch_count < sizeof(chirptable))
			{
				current_val = (chirptable[tms->pitch_count] * tms->current_energy) / 256;
			}
			else
			{
				current_val = 0x00;
			}
		}

		/* Lattice filter here */

		tms->u[10] = current_val;

		for (i = 9; i >= 0; i--)
		{
			tms->u[i] = tms->u[i+1] - ((tms->current_k[i] * tms->x[i]) / 32768);
		}

		for (i = 9; i >= 1; i--)
		{
			tms->x[i] = tms->x[i-1] + ((tms->current_k[i-1] * tms->u[i-1]) / 32768);
		}

		tms->x[0] = tms->u[0];

		/* clipping, just like the chip */

		if (tms->u[0] > 511)
		{
			buffer[buf_count] = 255;
		}
		else if (tms->u[0] < -512)
		{
			buffer[buf_count] = 0;
		}
		else
		{
			buffer[buf_count] = (tms->u[0] >> 2) + 128;
		}

		/* Update all counts */

		size--;
		tms->sample_count = (tms->sample_count + 1) % 200;

		if (tms->current_pitch != 0)
		{
			tms->pitch_count = (tms->pitch_count + 1) % tms->current_pitch;
		}
		else
		{
			tms->pitch_count = 0;
		}

		tms->interp_count = (tms->interp_count + 1) % 25;
		buf_count++;
	}

empty:
	while (size > 0)
	{
		tms->sample_count = (tms->sample_count + 1) % 200;
		tms->interp_count = (tms->interp_count + 1) % 25;
		buffer[buf_count] = 0x80; /* should be (-1 << 8) ??? (cf note in data sheet, p 10, table 4) */
		buf_count++;
		size--;
	}
}

/**********************************************************************************************

process_command -- extract a byte from the FIFO and interpret it as a command

***********************************************************************************************/

static void process_command(TMS5220 *tms)
{
	/* if there are stray bits, ignore them */
	if (tms->fifo_bits_taken > 0)
	{
		tms->fifo_bits_taken = 0;
		tms->fifo_count--;
		tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
	}

	/* grab a full byte from the FIFO */
	if (tms->fifo_count > 0)
	{
		unsigned char cmd = tms->fifo[tms->fifo_head];
		tms->fifo_count--;
		tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;

		/* parse the command */
		switch (cmd & 0x70)
		{
			case 0x10: /* read byte */
				tms->phrom_bits_taken = 0;
				tms->data_register = DoRead(tms, 8); /* read one byte from speech ROM... */
				tms->RDB_flag = true;
				break;

			case 0x30: /* read and branch */
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: read and branch\n", PrePC);
				#endif

				tms->RDB_flag = false;

				// The Read and Branch command causes the VSP to initiate a Read
				// and Branch function on the VSM. The VSP is not able to access
				// the VSM for 240 microseconds after executing this command.
				break;

			case 0x40: { /* load address */
				/* tms5220 data sheet says that if we load only one 4-bit nibble, it won't work.
				This code does not care about this. */
				unsigned char data = cmd & 0x0f;

				#if ENABLE_LOG
				WriteLog("%04X TMS5220: load address cmd with data = 0x%02x\n", PrePC, data);
				#endif

				tms->phrom_address >>= 4;
				tms->phrom_address &= 0x0ffff;
				tms->phrom_address |= (data << 16);

				#if ENABLE_LOG
				WriteLog("%04X TMS5220: load address cmd with data = 0x%02x, new address = 0x%05x\n", PrePC, data, tms->phrom_address);
				#endif

				break;
			}

			case 0x50: /* speak */
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: speak\n", PrePC);
				#endif

				tms->tms5220_speaking = true;
				tms->speak_external = false;

				if (!tms->last_frame)
				{
					tms->first_frame = true;
				}

				tms->talk_status = true; /* start immediately */
				break;

			case 0x60: /* speak external */
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: speak external\n", PrePC);
				#endif

				tms->tms5220_speaking = tms->speak_external = true;

				tms->RDB_flag = false;

				/* according to the datasheet, this will cause an interrupt due to a BE condition */
				if (!tms->buffer_empty)
				{
					tms->buffer_empty = true;
					tms->interrupt = true;

					#if ENABLE_LOG
					WriteLog("%04X TMS5220: Buffer empty set\n", PrePC);
					WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
					#endif
				}

				tms->talk_status = false; /* wait to have 8 bytes in buffer before starting */
				break;

			case 0x70: /* reset */
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: reset\n", PrePC);
				#endif

				tms5220_reset(tms);
				break;
		}
	}

	/* update the buffer low state */
	check_buffer_low(tms);
}

/**********************************************************************************************

extract_bits -- extract a specific number of bits from the FIFO

***********************************************************************************************/

static int extract_bits(TMS5220 *tms, int count)
{
	int val = 0;

	if (tms->speak_external)
	{
		/* extract from FIFO */
		while (count--)
		{
			val = (val << 1) | ((tms->fifo[tms->fifo_head] >> tms->fifo_bits_taken) & 1);

			tms->fifo_bits_taken++;

			if (tms->fifo_bits_taken >= 8)
			{
				tms->fifo_count--;
				tms->fifo_head = (tms->fifo_head + 1) % FIFO_SIZE;
				tms->fifo_bits_taken = 0;
			}
		}
	}
	else
	{
		/* extract from speech ROM */
		val = DoRead(tms, count);
	}

	return val;
}

/**********************************************************************************************

parse_frame -- parse a new frame's worth of data; returns 0 if not enough bits in buffer

***********************************************************************************************/

static int parse_frame(TMS5220 *tms, bool first_frame)
{
	int bits = 0; /* number of bits in FIFO (speak external only) */

	if (!first_frame)
	{
		/* remember previous frame */
		tms->old_energy = tms->new_energy;
		tms->old_pitch = tms->new_pitch;
		for (int i = 0; i < 10; i++)
			tms->old_k[i] = tms->new_k[i];
	}

	/* clear out the new frame */
	tms->new_energy = 0;
	tms->new_pitch = 0;
	for (int i = 0; i < 10; i++)
		tms->new_k[i] = 0;

	/* if the previous frame was a stop frame, don't do anything */
	if (!first_frame && (tms->old_energy == (energytable[15] >> 6)))
		/*return 1;*/
	{
		tms->buffer_empty = true;
		return 1;
	}

	if (tms->speak_external)
	{
		/* count the total number of bits available */
		bits = tms->fifo_count * 8 - tms->fifo_bits_taken;
	}

	/* attempt to extract the energy index */
	if (tms->speak_external)
	{
		bits -= 4;
		if (bits < 0)
			goto ranout;
	}

	int indx = extract_bits(tms, 4);
	tms->new_energy = energytable[indx] >> 6;

	/* if the index is 0 or 15, we're done */
	if (indx == 0 || indx == 15)
	{
		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (4-bit energy=%d frame)\n", PrePC, tms->new_energy);
		#endif

		/* clear tms->fifo if stop frame encountered */
		if (indx == 15)
		{
			tms->fifo_head = tms->fifo_tail = tms->fifo_count = tms->fifo_bits_taken = tms->phrom_bits_taken = 0;
			tms->speak_external = tms->tms5220_speaking = false;
			tms->last_frame = true;
		}
		goto done;
	}

	/* attempt to extract the repeat flag */
	if (tms->speak_external)
	{
		bits -= 1;
		if (bits < 0)
			goto ranout;
	}

	int rep_flag = extract_bits(tms, 1);

	/* attempt to extract the pitch */
	if (tms->speak_external)
	{
		bits -= 6;
		if (bits < 0)
			goto ranout;
	}
	indx = extract_bits(tms, 6);
	tms->new_pitch = pitchtable[indx] / 256;

	/* if this is a repeat frame, just copy the k's */
	if (rep_flag)
	{
		for (int i = 0; i < 10; i++)
			tms->new_k[i] = tms->old_k[i];

		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (11-bit energy=%d pitch=%d rep=%d frame)\n", PrePC, tms->new_energy, tms->new_pitch, rep_flag);
		#endif

		goto done;
	}

	/* if the pitch index was zero, we need 4 k's */
	if (indx == 0)
	{
		/* attempt to extract 4 K's */
		if (tms->speak_external)
		{
			bits -= 18;
			if (bits < 0)
				goto ranout;
		}
		tms->new_k[0] = k1table[extract_bits(tms, 5)];
		tms->new_k[1] = k2table[extract_bits(tms, 5)];
		tms->new_k[2] = k3table[extract_bits(tms, 4)];
		tms->new_k[3] = k4table[extract_bits(tms, 4)];

		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (29-bit energy=%d pitch=%d rep=%d 4K frame)\n", PrePC, tms->new_energy, tms->new_pitch, rep_flag);
		#endif

		goto done;
	}

	/* else we need 10 K's */
	if (tms->speak_external)
	{
		bits -= 39;
		if (bits < 0)
			goto ranout;
	}

	tms->new_k[0] = k1table[extract_bits(tms, 5)];
	tms->new_k[1] = k2table[extract_bits(tms, 5)];
	tms->new_k[2] = k3table[extract_bits(tms, 4)];
	tms->new_k[3] = k4table[extract_bits(tms, 4)];
	tms->new_k[4] = k5table[extract_bits(tms, 4)];
	tms->new_k[5] = k6table[extract_bits(tms, 4)];
	tms->new_k[6] = k7table[extract_bits(tms, 4)];
	tms->new_k[7] = k8table[extract_bits(tms, 3)];
	tms->new_k[8] = k9table[extract_bits(tms, 3)];
	tms->new_k[9] = k10table[extract_bits(tms, 3)];

	#if ENABLE_LOG
	WriteLog("%04X TMS5220:  (50-bit energy=%d pitch=%d rep=%d 10K frame)\n", PrePC, tms->new_energy, tms->new_pitch, rep_flag);
	#endif

done:
	#if ENABLE_LOG
	if (tms->speak_external)
	{
		WriteLog("%04X TMS5220: Parsed a frame successfully in FIFO - %d bits remaining\n", PrePC, bits);
	}
	else
	{
		WriteLog("%04X TMS5220: Parsed a frame successfully in ROM\n", PrePC);
	}
	#endif

	if (first_frame)
	{
		/* if this is the first frame, no previous frame to take as a starting point */
		tms->old_energy = tms->new_energy;
		tms->old_pitch = tms->new_pitch;

		for (int i = 0; i < 10; i++)
			tms->old_k[i] = tms->new_k[i];
	}

	/* update the tms->buffer_low status */
	check_buffer_low(tms);
	return 1;

ranout:
	#if ENABLE_LOG
	WriteLog("%04X TMS5220: Ran out of bits on a parse!\n", PrePC);
	#endif

	/* this is an error condition; mark the buffer empty and turn off speaking */
	tms->buffer_empty = true;
	tms->talk_status = tms->speak_external = tms->tms5220_speaking = first_frame = tms->last_frame = false;
	tms->fifo_count = tms->fifo_head = tms->fifo_tail = 0;

	tms->RDB_flag = false;

	/* generate an interrupt if necessary */
	tms->interrupt = true;

	#if ENABLE_LOG
	WriteLog("TMS5220: Interrupt set\n");
	#endif

	return 0;
}

/**********************************************************************************************

check_buffer_low -- check to see if the buffer low flag should be on or off

***********************************************************************************************/

static void check_buffer_low(TMS5220 *tms)
{
	/* did we just become low? */
	if (tms->fifo_count <= 8)
	{
		/* generate an interrupt if necessary */
		if (!tms->buffer_low)
		{
			#if ENABLE_LOG
			WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
			WriteLog("%04X TMS5220: Buffer low set\n", PrePC);
			#endif

			tms->interrupt = true;
		}

		tms->buffer_low = true;
	}
	else
	{
		/* did we just become full? */
		tms->buffer_low = false;

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Buffer low cleared\n", PrePC);
		#endif
	}
}

void SpeechPoll(int Cycles)
{
	if (!tms5220->chip->ready)
	{
		if (tms5220->chip->ready_count > 0)
		{
			tms5220->chip->ready_count -= Cycles;

			if (tms5220->chip->ready_count <= 0)
			{
				tms5220->chip->ready_count = 0;
				tms5220->chip->ready = true;
			}
		}
	}
}
