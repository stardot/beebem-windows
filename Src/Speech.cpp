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
#include "6502core.h"
#include "beebsound.h"
#include "beebwin.h"
#include "Main.h"
#include "Log.h"

/* TMS5220 ROM Tables */

/* This is the energy lookup table (4-bits -> 10-bits) */

static const unsigned short energytable[0x10] = {
	0x0000,0x00C0,0x0140,0x01C0,0x0280,0x0380,0x0500,0x0740,
	0x0A00,0x0E40,0x1440,0x1C80,0x2840,0x38C0,0x5040,0x7FC0
};

/* This is the pitch lookup table (6-bits -> 8-bits) */

static const unsigned short pitchtable[0x40] = {
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

static const short k1table[0x20] = {
	(short)0x82C0,(short)0x8380,(short)0x83C0,(short)0x8440,(short)0x84C0,(short)0x8540,(short)0x8600,(short)0x8780,
	(short)0x8880,(short)0x8980,(short)0x8AC0,(short)0x8C00,(short)0x8D40,(short)0x8F00,(short)0x90C0,(short)0x92C0,
	(short)0x9900,(short)0xA140,(short)0xAB80,(short)0xB840,(short)0xC740,(short)0xD8C0,(short)0xEBC0,0x0000,
	0x1440,0x2740,0x38C0,0x47C0,0x5480,0x5EC0,0x6700,0x6D40
};

/* K2 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k2table[0x20] = {
	(short)0xAE00,(short)0xB480,(short)0xBB80,(short)0xC340,(short)0xCB80,(short)0xD440,(short)0xDDC0,(short)0xE780,
	(short)0xF180,(short)0xFBC0,0x0600,0x1040,0x1A40,0x2400,0x2D40,0x3600,
	0x3E40,0x45C0,0x4CC0,0x5300,0x5880,0x5DC0,0x6240,0x6640,
	0x69C0,0x6CC0,0x6F80,0x71C0,0x73C0,0x7580,0x7700,0x7E80
};

/* K3 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k3table[0x10] = {
	(short)0x9200,(short)0x9F00,(short)0xAD00,(short)0xBA00,(short)0xC800,(short)0xD500,(short)0xE300,(short)0xF000,
	(short)0xFE00,0x0B00,0x1900,0x2600,0x3400,0x4100,0x4F00,0x5C00
};

/* K4 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k4table[0x10] = {
	(short)0xAE00,(short)0xBC00,(short)0xCA00,(short)0xD800,(short)0xE600,(short)0xF400,0x0100,0x0F00,
	0x1D00,0x2B00,0x3900,0x4700,0x5500,0x6300,0x7100,0x7E00
};

/* K5 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k5table[0x10] = {
	(short)0xAE00,(short)0xBA00,(short)0xC500,(short)0xD100,(short)0xDD00,(short)0xE800,(short)0xF400,(short)0xFF00,
	0x0B00,0x1700,0x2200,0x2E00,0x3900,0x4500,0x5100,0x5C00
};

/* K6 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k6table[0x10] = {
	(short)0xC000,(short)0xCB00,(short)0xD600,(short)0xE100,(short)0xEC00,(short)0xF700,0x0300,0x0E00,
	0x1900,0x2400,0x2F00,0x3A00,0x4500,0x5000,0x5B00,0x6600
};

/* K7 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k7table[0x10] = {
	(short)0xB300,(short)0xBF00,(short)0xCB00,(short)0xD700,(short)0xE300,(short)0xEF00,(short)0xFB00,0x0700,
	0x1300,0x1F00,0x2B00,0x3700,0x4300,0x4F00,0x5A00,0x6600
};

/* K8 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k8table[0x08] = {
	(short)0xC000,(short)0xD800,(short)0xF000,0x0700,0x1F00,0x3700,0x4F00,0x6600
};

/* K9 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k9table[0x08] = {
	(short)0xC000,(short)0xD400,(short)0xE800,(short)0xFC00,0x1000,0x2500,0x3900,0x4D00
};

/* K10 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1) */

static const short k10table[0x08] = {
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

/*----------------------------------------------------------------------------*/

class TMS5220
{
	public:
		TMS5220();
		TMS5220(const TMS5220&) = delete;
		TMS5220& operator=(const TMS5220&) = delete;
		~TMS5220();

	public:
		void Reset();
		void ReadEnable();
		unsigned char ReadStatus();
		void WriteData(unsigned char data);
		bool ReadInt();
		bool ReadReady();

		void Poll(int Cycles);

		void ProcessSamples(short int *buffer, int size);

	private:
		void ProcessCommand();
		int ExtractBits(int count);
		int ParseFrame(bool first_frame);
		void CheckBufferLow();
		int ReadPhrom(int count);

	private:
		// These contain data that describes the 128-bit data FIFO
		static constexpr int FIFO_SIZE = 16;
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
		bool tms5220_speaking; // Speak or Speak External command in progress
		bool speak_external; // Speak External command in progress
		bool talk_status; // tms5220 is really currently speaking
		bool first_frame; // we have just started speaking, and we are to parse the first frame
		bool last_frame; // we are doing the frame of sound
		bool buffer_low; // FIFO has less than 8 bytes in it
		bool buffer_empty; // FIFO is empty
		bool interrupt; // true to interrupt (active high), convert to active low in SpeechInterrupt()
		bool ready; // true if ready (active high), convert to active low in SpeechReady()
		int ready_count; // countdown timer to reset the ready flag after read/write enable is asserted

		// These contain data describing the current and previous voice frames
		uint16_t old_energy;
		uint16_t old_pitch;
		int old_k[10];

		uint16_t new_energy;
		uint16_t new_pitch;
		int new_k[10];

		// These are all used to contain the current state of the sound generation
		uint16_t current_energy;
		uint16_t current_pitch;
		int current_k[10];

		uint16_t target_energy;
		uint16_t target_pitch;
		int target_k[10];

		uint8_t interp_count; // Number of interp periods (0-7)
		uint8_t sample_count; // Sample number within interp (0-24)
		int pitch_count;

		int u[11];
		int x[10];

		int8_t randbit;

		int phrom_address;

		uint8_t data_register; // Data register, used by read command
		bool RDB_flag; // Whether we should read data register or status register
};

// The state of the streamed output

class TMS5220StreamState
{
	public:
		TMS5220StreamState(int Clock);
		TMS5220StreamState(const TMS5220StreamState&) = delete;
		TMS5220StreamState& operator=(const TMS5220StreamState&) = delete;
		~TMS5220StreamState();

	public:
		void Update(unsigned char *buff, int length);

	public:
		TMS5220 chip;

	private:
		static constexpr int MAX_SAMPLE_CHUNK = 8000;

		static constexpr int FRAC_BITS = 14;
		static constexpr int FRAC_ONE  = 1 << FRAC_BITS;
		static constexpr int FRAC_MASK = FRAC_ONE - 1;

		int last_sample;
		int curr_sample;
		int source_step;
		int source_pos;
};

/*----------------------------------------------------------------------------*/

bool SpeechDefault;
bool SpeechEnabled;

static TMS5220StreamState *tms5220;
static unsigned char phrom_rom[16][16384];

#define ENABLE_LOG 0

/*----------------------------------------------------------------------------*/

TMS5220::TMS5220()
{
	memset(this, 0, sizeof(*this));
}

/*----------------------------------------------------------------------------*/

TMS5220::~TMS5220()
{
}

/*--------------------------------------------------------------------------*/

// Resets the TMS5220

void TMS5220::Reset()
{
	// Initialize the FIFO
	memset(fifo, 0, sizeof(fifo));
	fifo_head = fifo_tail = fifo_count = fifo_bits_taken = phrom_bits_taken = 0;

	// Initialize the chip state
	// Note that we do not actually clear IRQ on start-up : IRQ is even raised if buffer_empty or buffer_low are false
	tms5220_speaking = speak_external = talk_status = first_frame = last_frame = interrupt = false;
	buffer_empty = buffer_low = true;

	RDB_flag = false;

	ready = true;
	ready_count = 0;

	// Initialize the energy/pitch/k states
	old_energy = new_energy = current_energy = target_energy = 0;
	old_pitch = new_pitch = current_pitch = target_pitch = 0;

	memset(old_k, 0, sizeof(old_k));
	memset(new_k, 0, sizeof(new_k));
	memset(current_k, 0, sizeof(current_k));
	memset(target_k, 0, sizeof(target_k));

	// Initialize the sample generators
	interp_count = sample_count = pitch_count = 0;
	randbit = 0;
	memset(u, 0, sizeof(u));
	memset(x, 0, sizeof(x));

	phrom_address = 0;
}

/*----------------------------------------------------------------------------*/

void TMS5220::ReadEnable()
{
	ready = false;
	ready_count = 30;
}

/*--------------------------------------------------------------------------*/

// Read status or data from the TMS5220

// From the data sheet:
// bit 0 = TS - Talk Status is active (high) when the VSP is processing speech data.
// Talk Status goes active at the initiation of a Speak command or after nine
// bytes of data are loaded into the FIFO following a Speak External command. It
// goes inactive (low) when the stop code (Energy=1111) is processed, or
// immediately by a buffer empty condition or a reset command.
// bit 1 = BL - Buffer Low is active (high) when the FIFO buffer is more than half empty.
// Buffer Low is set when the "Last-In" byte is shifted down past the half-full
// boundary of the stack. Buffer Low is cleared when data is loaded to the stack
// so that the "Last-In" byte lies above the half-full boundary and becomes the
// ninth data byte of the stack.
// bit 2 = BE - Buffer Empty is active (high) when the FIFO buffer has run out of data
// while executing a Speak External command. Buffer Empty is set when the last bit
// of the "Last-In" byte is shifted out to the Synthesis Section. This causes
// Talk Status to be cleared. Speed is terminated at some abnormal point and the
// Speak External command execution is terminated.

unsigned char TMS5220::ReadStatus()
{
	ready = true;

	if (RDB_flag)
	{
		// If last command was read, return data register
		RDB_flag = false;
		return data_register;
	}
	else
	{
		// Read status

		// Clear the interrupt state
		interrupt = false;

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Status read: TS=%d BL=%d BE=%d\n", PrePC, talk_status, buffer_low, buffer_empty);
		WriteLog("%04X TMS5220: Clear interrupt\n", PrePC);
		#endif

		return (talk_status  ? 0x80 : 0x00) |
		       (buffer_low   ? 0x40 : 0x00) |
		       (buffer_empty ? 0x20 : 0x00);
	}
}

/*--------------------------------------------------------------------------*/

// Handle a write to the TMS5220

void TMS5220::WriteData(unsigned char data)
{
	#if ENABLE_LOG
	WriteLog("%04X TMS5220: Write %02X\n", PrePC, data);
	#endif

	// Add this byte to the FIFO
	if (fifo_count < FIFO_SIZE)
	{
		fifo[fifo_tail] = data;
		fifo_tail = (fifo_tail + 1) % FIFO_SIZE;
		fifo_count++;

		// If we were speaking, then we're no longer empty
		if (speak_external)
		{
			buffer_empty = false;
		}

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Added byte to FIFO (size=%d)\n", PrePC, fifo_count);
		#endif
	}
	else
	{
		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Ran out of room in the FIFO!\n", PrePC);
		#endif
	}

	// Update the buffer low state
	CheckBufferLow();

	if (!speak_external)
	{
		// R Nabet : we parse commands at once.  It is necessary for such commands as read.
		ProcessCommand();
	}

	ready = false;
	ready_count = 30;
}

/*--------------------------------------------------------------------------*/

// Returns the interrupt state of the TMS5220 (active low)

bool TMS5220::ReadInt()
{
	return !interrupt;
}

/*--------------------------------------------------------------------------*/

// Returns the ready state of the TMS5220

bool TMS5220::ReadReady()
{
	return !ready;
}

/*--------------------------------------------------------------------------*/

void TMS5220::Poll(int Cycles)
{
	if (!ready)
	{
		if (ready_count > 0)
		{
			ready_count -= Cycles;

			if (ready_count <= 0)
			{
				ready_count = 0;
				ready = true;
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

// Fill the buffer with a specific number of samples

void TMS5220::ProcessSamples(int16_t *buffer, int size)
{
	int buf_count = 0;
	int i, interp_period;

tryagain:
	// If we're not speaking, parse commands
	// while (!speak_external && fifo_count > 0)
	//   process_command(tms);

	// If we're empty and still not speaking, fill with nothingness
	if (!tms5220_speaking && !last_frame)
		goto empty;

	// If we're to speak, but haven't started, wait for the 9th byte
	if (!talk_status && speak_external)
	{
		if (fifo_count < 9)
			goto empty;

		talk_status = true;
		first_frame = true; // will cause the first frame to be parsed
		buffer_empty = false;
	}

#if 0
	// We are to speak, yet we fill with 0s until start of next frame
	if (first_frame)
	{
		while (size > 0 && (sample_count != 0 || interp_count != 0))
		{
			sample_count = (sample_count + 1) % 200;
			interp_count = (interp_count + 1) % 25;
			buffer[buf_count] = 0x80; // should be (-1 << 8) ??? (cf note in data sheet, p 10, table 4)
			buf_count++;
			size--;
		}
	}
#endif

	// Loop until the buffer is full or we've stopped speaking
	while (size > 0 && talk_status)
	{
		int current_val;

		// If we're ready for a new frame
		if (interp_count == 0 && sample_count == 0)
		{
			// Parse a new frame
			if (!ParseFrame(first_frame))
				break;

			first_frame = false;

			// Set old target as new start of frame
			current_energy = old_energy;
			current_pitch = old_pitch;

			for (i = 0; i < 10; i++)
				current_k[i] = old_k[i];

			// Is this a zero energy frame?
			if (current_energy == 0)
			{
				// WriteLog("processing frame: zero energy\n");
				target_energy = 0;
				target_pitch = current_pitch;

				for (i = 0; i < 10; i++)
					target_k[i] = current_k[i];
			}
			// Is this a stop frame?
			else if (current_energy == (energytable[15] >> 6))
			{
				// WriteLog("processing frame: stop frame\n");
				current_energy = energytable[0] >> 6;
				target_energy = current_energy;
				/* interp_count = sample_count = */ pitch_count = 0;
				last_frame = 0;

				if (tms5220_speaking)
				{
					// New speech command in progress
					first_frame = true;
				}
				else
				{
					// Really stop speaking
					talk_status = false;

					// Generate an interrupt if necessary
					interrupt = true;

					#if ENABLE_LOG
					WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
					#endif
				}

				// Try to fetch commands again
				goto tryagain;
			}
			else
			{
				// Is this the ramp down frame?
				if (new_energy == (energytable[15] >> 6))
				{
					target_energy = 0;
					target_pitch = current_pitch;

					for (i = 0; i < 10; i++)
						target_k[i] = current_k[i];
				}
				else
				{
					// Reset the step size
					target_energy = new_energy;
					target_pitch = new_pitch;

					for (i = 0; i < 4; i++)
					{
						target_k[i] = new_k[i];
					}

					if (current_pitch == 0)
					{
						for (i = 4; i < 10; i++)
						{
							target_k[i] = current_k[i] = 0;
						}
					}
					else
					{
						for (i = 4; i < 10; i++)
						{
							target_k[i] = new_k[i];
						}
					}
				}
			}
		}
		else if (interp_count == 0)
		{
			// Update values based on step values

			interp_period = sample_count / 25;

			current_energy += (target_energy - current_energy) / interp_coeff[interp_period];

			if (old_pitch != 0)
				current_pitch += (target_pitch - current_pitch) / interp_coeff[interp_period];

			for (i = 0; i < 10; i++)
			{
				current_k[i] += (target_k[i] - current_k[i]) / interp_coeff[interp_period];
			}
		}

		current_val = 0x00;

		if (old_energy == 0)
		{
			// Generate silent samples here
			current_val = 0x00;
		}
		else if (old_pitch == 0)
		{
			// Generate unvoiced samples here
			randbit = (rand() % 2) * 2 - 1;
			current_val = (randbit * current_energy) / 4;
		}
		else
		{
			// Generate voiced samples here
			if (pitch_count < sizeof(chirptable))
			{
				current_val = (chirptable[pitch_count] * current_energy) / 256;
			}
			else
			{
				current_val = 0x00;
			}
		}

		// Lattice filter here

		u[10] = current_val;

		for (i = 9; i >= 0; i--)
		{
			u[i] = u[i+1] - ((current_k[i] * x[i]) / 32768);
		}

		for (i = 9; i >= 1; i--)
		{
			x[i] = x[i-1] + ((current_k[i-1] * u[i-1]) / 32768);
		}

		x[0] = u[0];

		// Clipping, just like the chip

		if (u[0] > 511)
		{
			buffer[buf_count] = 255;
		}
		else if (u[0] < -512)
		{
			buffer[buf_count] = 0;
		}
		else
		{
			buffer[buf_count] = (u[0] >> 2) + 128;
		}

		// Update all counts

		size--;
		sample_count = (sample_count + 1) % 200;

		if (current_pitch != 0)
		{
			pitch_count = (pitch_count + 1) % current_pitch;
		}
		else
		{
			pitch_count = 0;
		}

		interp_count = (interp_count + 1) % 25;
		buf_count++;
	}

empty:
	while (size > 0)
	{
		sample_count = (sample_count + 1) % 200;
		interp_count = (interp_count + 1) % 25;
		buffer[buf_count] = 0x80; // should be (-1 << 8) ??? (cf note in data sheet, p 10, table 4)
		buf_count++;
		size--;
	}
}

/*--------------------------------------------------------------------------*/

// Extract a byte from the FIFO and interpret it as a command

void TMS5220::ProcessCommand()
{
	// If there are stray bits, ignore them
	if (fifo_bits_taken > 0)
	{
		fifo_bits_taken = 0;
		fifo_count--;
		fifo_head = (fifo_head + 1) % FIFO_SIZE;
	}

	// Grab a full byte from the FIFO
	if (fifo_count > 0)
	{
		unsigned char cmd = fifo[fifo_head];
		fifo_count--;
		fifo_head = (fifo_head + 1) % FIFO_SIZE;

		// Parse the command
		switch (cmd & 0x70)
		{
			case 0x10: // Read byte
				phrom_bits_taken = 0;
				data_register = ReadPhrom(8); /* read one byte from speech ROM... */
				RDB_flag = true;
				break;

			case 0x30: // Read and branch
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: read and branch\n", PrePC);
				#endif

				RDB_flag = false;

				// The Read and Branch command causes the VSP to initiate a Read
				// and Branch function on the VSM. The VSP is not able to access
				// the VSM for 240 microseconds after executing this command.
				break;

			case 0x40: { // Load address
				// TMS5220 data sheet says that if we load only one 4-bit nibble, it won't work.
				// This code does not care about this.
				unsigned char data = cmd & 0x0f;

				#if ENABLE_LOG
				WriteLog("%04X TMS5220: load address cmd with data = 0x%02x\n", PrePC, data);
				#endif

				phrom_address >>= 4;
				phrom_address &= 0x0ffff;
				phrom_address |= (data << 16);

				#if ENABLE_LOG
				WriteLog("%04X TMS5220: load address cmd with data = 0x%02x, new address = 0x%05x\n", PrePC, data, phrom_address);
				#endif

				break;
			}

			case 0x50: // Speak
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: speak\n", PrePC);
				#endif

				tms5220_speaking = true;
				speak_external = false;

				if (!last_frame)
				{
					first_frame = true;
				}

				talk_status = true; // Start immediately
				break;

			case 0x60: // Speak external
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: speak external\n", PrePC);
				#endif

				tms5220_speaking = speak_external = true;

				RDB_flag = false;

				// According to the datasheet, this will cause an interrupt due to a BE condition
				if (!buffer_empty)
				{
					buffer_empty = true;
					interrupt = true;

					#if ENABLE_LOG
					WriteLog("%04X TMS5220: Buffer empty set\n", PrePC);
					WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
					#endif
				}

				talk_status = false; // Wait to have 8 bytes in buffer before starting
				break;

			case 0x70: // Reset
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: reset\n", PrePC);
				#endif

				Reset();
				break;
		}
	}

	// Update the buffer low state
	CheckBufferLow();
}

/*--------------------------------------------------------------------------*/

// Extract a specific number of bits from the FIFO

int TMS5220::ExtractBits(int count)
{
	int val = 0;

	if (speak_external)
	{
		// Extract from FIFO
		while (count--)
		{
			val = (val << 1) | ((fifo[fifo_head] >> fifo_bits_taken) & 1);

			fifo_bits_taken++;

			if (fifo_bits_taken >= 8)
			{
				fifo_count--;
				fifo_head = (fifo_head + 1) % FIFO_SIZE;
				fifo_bits_taken = 0;
			}
		}
	}
	else
	{
		// Extract from speech ROM
		val = ReadPhrom(count);
	}

	return val;
}

/*--------------------------------------------------------------------------*/

// Parse a new frame's worth of data; returns 0 if not enough bits in buffer

int TMS5220::ParseFrame(bool first)
{
	int bits = 0; // Number of bits in FIFO (speak external only)

	if (!first)
	{
		// Remember previous frame
		old_energy = new_energy;
		old_pitch = new_pitch;
		for (int i = 0; i < 10; i++)
			old_k[i] = new_k[i];
	}

	// Clear out the new frame
	new_energy = 0;
	new_pitch = 0;
	for (int i = 0; i < 10; i++)
		new_k[i] = 0;

	// If the previous frame was a stop frame, don't do anything
	if (!first && (old_energy == (energytable[15] >> 6)))
	{
		buffer_empty = true;
		return 1;
	}

	if (speak_external)
	{
		// Count the total number of bits available
		bits = fifo_count * 8 - fifo_bits_taken;
	}

	// Attempt to extract the energy index
	if (speak_external)
	{
		bits -= 4;
		if (bits < 0)
			goto ranout;
	}

	int indx = ExtractBits(4);
	new_energy = energytable[indx] >> 6;

	// If the index is 0 or 15, we're done
	if (indx == 0 || indx == 15)
	{
		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (4-bit energy=%d frame)\n", PrePC, new_energy);
		#endif

		// Clear fifo if stop frame encountered
		if (indx == 15)
		{
			fifo_head = fifo_tail = fifo_count = fifo_bits_taken = phrom_bits_taken = 0;
			speak_external = tms5220_speaking = false;
			last_frame = true;
		}
		goto done;
	}

	// Attempt to extract the repeat flag
	if (speak_external)
	{
		bits -= 1;
		if (bits < 0)
			goto ranout;
	}

	int rep_flag = ExtractBits(1);

	// Attempt to extract the pitch
	if (speak_external)
	{
		bits -= 6;
		if (bits < 0)
			goto ranout;
	}

	indx = ExtractBits(6);
	new_pitch = pitchtable[indx] / 256;

	// If this is a repeat frame, just copy the k's
	if (rep_flag)
	{
		for (int i = 0; i < 10; i++)
			new_k[i] = old_k[i];

		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (11-bit energy=%d pitch=%d rep=%d frame)\n", PrePC, new_energy, new_pitch, rep_flag);
		#endif

		goto done;
	}

	// If the pitch index was zero, we need 4 k's
	if (indx == 0)
	{
		// Attempt to extract 4 K's
		if (speak_external)
		{
			bits -= 18;
			if (bits < 0)
				goto ranout;
		}

		new_k[0] = k1table[ExtractBits(5)];
		new_k[1] = k2table[ExtractBits(5)];
		new_k[2] = k3table[ExtractBits(4)];
		new_k[3] = k4table[ExtractBits(4)];

		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (29-bit energy=%d pitch=%d rep=%d 4K frame)\n", PrePC, new_energy, new_pitch, rep_flag);
		#endif

		goto done;
	}

	// Else we need 10 K's
	if (speak_external)
	{
		bits -= 39;
		if (bits < 0)
			goto ranout;
	}

	new_k[0] = k1table[ExtractBits(5)];
	new_k[1] = k2table[ExtractBits(5)];
	new_k[2] = k3table[ExtractBits(4)];
	new_k[3] = k4table[ExtractBits(4)];
	new_k[4] = k5table[ExtractBits(4)];
	new_k[5] = k6table[ExtractBits(4)];
	new_k[6] = k7table[ExtractBits(4)];
	new_k[7] = k8table[ExtractBits(3)];
	new_k[8] = k9table[ExtractBits(3)];
	new_k[9] = k10table[ExtractBits(3)];

	#if ENABLE_LOG
	WriteLog("%04X TMS5220:  (50-bit energy=%d pitch=%d rep=%d 10K frame)\n", PrePC, new_energy, new_pitch, rep_flag);
	#endif

done:
	#if ENABLE_LOG
	if (speak_external)
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
		// If this is the first frame, no previous frame to take as a starting point
		old_energy = new_energy;
		old_pitch = new_pitch;

		for (int i = 0; i < 10; i++)
			old_k[i] = new_k[i];
	}

	// Update the buffer_low status
	CheckBufferLow();
	return 1;

ranout:
	#if ENABLE_LOG
	WriteLog("%04X TMS5220: Ran out of bits on a parse!\n", PrePC);
	#endif

	// This is an error condition; mark the buffer empty and turn off speaking
	buffer_empty = true;
	talk_status = speak_external = tms5220_speaking = first_frame = last_frame = false;
	fifo_count = fifo_head = fifo_tail = 0;

	RDB_flag = false;

	// Generate an interrupt if necessary
	interrupt = true;

	#if ENABLE_LOG
	WriteLog("TMS5220: Interrupt set\n");
	#endif

	return 0;
}

/*--------------------------------------------------------------------------*/

// Check to see if the buffer low flag should be on or off

void TMS5220::CheckBufferLow()
{
	// Did we just become low?
	if (fifo_count <= 8)
	{
		// Generate an interrupt if necessary
		if (!buffer_low)
		{
			#if ENABLE_LOG
			WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
			WriteLog("%04X TMS5220: Buffer low set\n", PrePC);
			#endif

			interrupt = true;
		}

		buffer_low = true;
	}
	else
	{
		// Did we just become full?
		buffer_low = false;

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Buffer low cleared\n", PrePC);
		#endif
	}
}

/*----------------------------------------------------------------------------*/

int TMS5220::ReadPhrom(int count)
{
	int phrom = (phrom_address >> 14) & 0xf;

	#if ENABLE_LOG
	WriteLog("%04X TMS5220: ReadPhrom, addr = 0x%04x, phrom = 0x%02x, count = %d", PrePC, phrom_address, phrom, count);
	#endif

	int val = 0;

	while (count--)
	{
		int addr = phrom_address & 0x3fff;

		val = (val << 1) | ((phrom_rom[phrom][addr] >> phrom_bits_taken) & 1);

		if (++phrom_bits_taken == 8)
		{
			phrom_address++;
			phrom_bits_taken = 0;
		}
	}

	#if ENABLE_LOG
	WriteLog(", returning 0x%02x\n", val);
	#endif

	return val;
}

/*----------------------------------------------------------------------------*/

// clock rate = 80 * output sample rate,
// usually 640000 for 8000 Hz sample rate or
// usually 800000 for 10000 Hz sample rate.

TMS5220StreamState::TMS5220StreamState(int Clock) :
	last_sample(0),
	curr_sample(0),
	source_step((int)((double)(Clock / 80) * (double)FRAC_ONE / (double)SoundSampleRate)),
	source_pos(0)
{
	chip.Reset();
}

/*----------------------------------------------------------------------------*/

TMS5220StreamState::~TMS5220StreamState()
{
}

/*--------------------------------------------------------------------------*/

// Update the sound chip so that it is in sync with CPU execution

void TMS5220StreamState::Update(unsigned char *buff, int length)
{
	int16_t sample_data[MAX_SAMPLE_CHUNK];
	int16_t *curr_data = sample_data;
	int prev = last_sample;
	int curr = curr_sample;
	unsigned char *buffer = buff;

	// Finish off the current sample
	if (source_pos > 0)
	{
		// Interpolate
		while (length > 0 && source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - source_pos)) + (curr * source_pos)) >> FRAC_BITS;
			// samp = ((samp + 32768) >> 8) & 255;
			// fprintf(stderr, "Sample = %d\n", samp);
			*buffer++ = samp;
			source_pos += source_step;
			length--;
		}

		// If we're over, continue; otherwise, we're done
		if (source_pos >= FRAC_ONE)
		{
			source_pos -= FRAC_ONE;
		}
		else
		{
			#if ENABLE_LOG
			// if (buffer - buff != length)
			//	WriteLog("Here for some reason - mismatch in num of samples = %d, %d\n", length, buffer - buff);
			#endif

			chip.ProcessSamples(sample_data, 0);
			return;
		}
	}

	// Compute how many new samples we need
	int final_pos = source_pos + length * source_step;
	int new_samples = (final_pos + FRAC_ONE - 1) >> FRAC_BITS;
	if (new_samples > MAX_SAMPLE_CHUNK)
		new_samples = MAX_SAMPLE_CHUNK;

	// Generate them into our buffer
	chip.ProcessSamples(sample_data, new_samples);

	prev = curr;
	curr = *curr_data++;

	// Then sample-rate convert with linear interpolation
	while (length > 0)
	{
		// Interpolate
		while (length > 0 && source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - source_pos)) + (curr * source_pos)) >> FRAC_BITS;
			// samp = ((samp + 32768) >> 8) & 255;
			// fprintf(stderr, "Sample = %d\n", samp);
			*buffer++ = samp;
			source_pos += source_step;
			length--;
		}

		// If we're over, grab the next samples
		if (source_pos >= FRAC_ONE)
		{
			source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
			if (curr_data - sample_data > MAX_SAMPLE_CHUNK)
				curr_data = sample_data;
		}
	}

	// Remember the last samples
	last_sample = prev;
	curr_sample = curr;

	#if ENABLE_LOG
	// if (buffer - buff != length)
	//	WriteLog("At end of update - mismatch in num of samples = %d, %d\n", length, buffer - buff);
	#endif
}

/*----------------------------------------------------------------------------*/

void SpeechInit()
{
	// Read all ROM files in the BeebFile directory
	// This section rewritten for V.1.32 to take account of roms.cfg file.
	char Path[256];
	strcpy(Path, mainWin->GetUserDataPath());
	strcat(Path, "Phroms.cfg");

	FILE *RomCfg = fopen(Path,"rt");

	if (RomCfg == nullptr)
	{
		mainWin->Report(MessageType::Error, "Cannot open PHROM configuration file:\n  %s", Path);
		return;
	}

	// Read phroms
	for (int romslot = 15; romslot >= 0; romslot--)
	{
		char RomName[80];
		fgets(RomName, 80, RomCfg);

		if (strchr(RomName, 13)) *strchr(RomName, 13) = 0;
		if (strchr(RomName, 10)) *strchr(RomName, 10) = 0;

		char PhromPath[256];
		strcpy(PhromPath, RomName);

		if (RomName[0] != '\\' && RomName[1] != ':')
		{
			strcpy(PhromPath, mainWin->GetUserDataPath());
			strcat(PhromPath, "Phroms/");
			strcat(PhromPath, RomName);
		}

		if (strncmp(RomName, "EMPTY", 5) != 0)
		{
			FILE *InFile = fopen(PhromPath, "rb");

			if (InFile != nullptr)
			{
				fread(phrom_rom[romslot], 1, 16384, InFile);
				fclose(InFile);
			}
			else
			{
				mainWin->Report(MessageType::Error, "Cannot open specified PHROM:\n  %s", PhromPath);
			}
		}
	}

	fclose(RomCfg);
}

/*--------------------------------------------------------------------------*/

// Allocate buffers and reset the 5220

void SpeechStart()
{
	int clock = 640000;

	SpeechStop();

	tms5220 = new(std::nothrow) TMS5220StreamState(clock);

	if (tms5220 == nullptr)
	{
		return;
	}

	SpeechEnabled = true;
}

/*--------------------------------------------------------------------------*/

// Free buffers

void SpeechStop()
{
	if (tms5220 != nullptr)
	{
		delete tms5220;
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
		tms5220->chip.WriteData(Data);
	}
}

/*--------------------------------------------------------------------------*/

void SpeechReadEnable()
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		tms5220->chip.ReadEnable();
	}
}

/*--------------------------------------------------------------------------*/

// Read status or data from the sound chip

unsigned char SpeechRead()
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		unsigned char Value = tms5220->chip.ReadStatus();

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
		return tms5220->chip.ReadReady();
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
		return tms5220->chip.ReadInt();
	}
	else
	{
		return 0;
	}
}

/*--------------------------------------------------------------------------*/

void SpeechUpdate(unsigned char *buff, int length)
{
	if (SpeechEnabled && tms5220 != nullptr)
	{
		tms5220->Update(buff, length);
	}
}

/*--------------------------------------------------------------------------*/

void SpeechPoll(int Cycles)
{
	if (tms5220 != nullptr)
	{
		tms5220->chip.Poll(Cycles);
	}
}

/*--------------------------------------------------------------------------*/
