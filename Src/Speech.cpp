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

#include <fstream>

#include "Speech.h"
#include "6502core.h"
#include "BeebWin.h"
#include "FileUtils.h"
#include "Log.h"
#include "Main.h"
#include "Sound.h"
#include "StringUtils.h"

#if ENABLE_SPEECH

// TMS5220 ROM Tables

// This is the energy lookup table (4-bits -> 10-bits)

static const unsigned short energytable[0x10] = {
	0x0000,0x00C0,0x0140,0x01C0,0x0280,0x0380,0x0500,0x0740,
	0x0A00,0x0E40,0x1440,0x1C80,0x2840,0x38C0,0x5040,0x7FC0
};

// This is the pitch lookup table (6-bits -> 8-bits)

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

// These are the reflection coefficient lookup tables

// K1 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k1table[0x20] = {
	(short)0x82C0,(short)0x8380,(short)0x83C0,(short)0x8440,(short)0x84C0,(short)0x8540,(short)0x8600,(short)0x8780,
	(short)0x8880,(short)0x8980,(short)0x8AC0,(short)0x8C00,(short)0x8D40,(short)0x8F00,(short)0x90C0,(short)0x92C0,
	(short)0x9900,(short)0xA140,(short)0xAB80,(short)0xB840,(short)0xC740,(short)0xD8C0,(short)0xEBC0,0x0000,
	0x1440,0x2740,0x38C0,0x47C0,0x5480,0x5EC0,0x6700,0x6D40
};

// K2 is (5-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k2table[0x20] = {
	(short)0xAE00,(short)0xB480,(short)0xBB80,(short)0xC340,(short)0xCB80,(short)0xD440,(short)0xDDC0,(short)0xE780,
	(short)0xF180,(short)0xFBC0,0x0600,0x1040,0x1A40,0x2400,0x2D40,0x3600,
	0x3E40,0x45C0,0x4CC0,0x5300,0x5880,0x5DC0,0x6240,0x6640,
	0x69C0,0x6CC0,0x6F80,0x71C0,0x73C0,0x7580,0x7700,0x7E80
};

// K3 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k3table[0x10] = {
	(short)0x9200,(short)0x9F00,(short)0xAD00,(short)0xBA00,(short)0xC800,(short)0xD500,(short)0xE300,(short)0xF000,
	(short)0xFE00,0x0B00,0x1900,0x2600,0x3400,0x4100,0x4F00,0x5C00
};

// K4 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k4table[0x10] = {
	(short)0xAE00,(short)0xBC00,(short)0xCA00,(short)0xD800,(short)0xE600,(short)0xF400,0x0100,0x0F00,
	0x1D00,0x2B00,0x3900,0x4700,0x5500,0x6300,0x7100,0x7E00
};

// K5 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k5table[0x10] = {
	(short)0xAE00,(short)0xBA00,(short)0xC500,(short)0xD100,(short)0xDD00,(short)0xE800,(short)0xF400,(short)0xFF00,
	0x0B00,0x1700,0x2200,0x2E00,0x3900,0x4500,0x5100,0x5C00
};

// K6 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k6table[0x10] = {
	(short)0xC000,(short)0xCB00,(short)0xD600,(short)0xE100,(short)0xEC00,(short)0xF700,0x0300,0x0E00,
	0x1900,0x2400,0x2F00,0x3A00,0x4500,0x5000,0x5B00,0x6600
};

// K7 is (4-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k7table[0x10] = {
	(short)0xB300,(short)0xBF00,(short)0xCB00,(short)0xD700,(short)0xE300,(short)0xEF00,(short)0xFB00,0x0700,
	0x1300,0x1F00,0x2B00,0x3700,0x4300,0x4F00,0x5A00,0x6600
};

// K8 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k8table[0x08] = {
	(short)0xC000,(short)0xD800,(short)0xF000,0x0700,0x1F00,0x3700,0x4F00,0x6600
};

// K9 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k9table[0x08] = {
	(short)0xC000,(short)0xD400,(short)0xE800,(short)0xFC00,0x1000,0x2500,0x3900,0x4D00
};

// K10 is (3-bits -> 9 bits+sign, 2's comp. fractional (-1 < x < 1)

static const short k10table[0x08] = {
	(short)0xCD00,(short)0xDF00,(short)0xF100,0x0400,0x1600,0x2000,0x3B00,0x4D00
};

// Chirp table

static const char chirptable[51] = {
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
	0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00
};

// Interpolation coefficients (in rightshifts, as in actual chip)

static const char interp_coeff[8] = {
	3, 3, 3, 2, 2, 1, 1, 0
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
		int16_t LatticeFilter();
		void ProcessCommand();
		int ExtractBits(int count);
		bool ParseFrame(bool first_frame);
		void CheckBufferLow();
		int ReadPhrom(int count);

	private:
		// These contain data that describes the 128-bit data FIFO
		static constexpr int FIFO_SIZE = 16;
		uint8_t m_fifo[FIFO_SIZE];
		uint8_t m_fifo_head;
		uint8_t m_fifo_tail;
		uint8_t m_fifo_count;
		uint8_t m_fifo_bits_taken;
		uint8_t m_phrom_bits_taken;

		// These contain global status bits

		// R Nabet : m_speak_external is only set when a speak external command is going on.
		// m_tms5220_speaking is set whenever a speak or speak external command is going on.
		// Note that we really need to do anything in tms5220_process and play samples only when
		// m_tms5220_speaking is true.  Else, we can play nothing as well, which is a
		// speed-up...

		bool m_tms5220_speaking; // Speak or Speak External command in progress
		bool m_speak_external; // Speak External command in progress
		bool m_talk_status; // tms5220 is really currently speaking
		bool m_first_frame; // we have just started speaking, and we are to parse the first frame
		bool m_last_frame; // we are doing the frame of sound
		bool m_buffer_low; // If true, FIFO has less than 8 bytes in it
		bool m_buffer_empty; // If true, FIFO is empty
		bool m_interrupt; // true to interrupt (active high), convert to active low in SpeechInterrupt()
		bool m_ready; // true if ready (active high), convert to active low in SpeechReady()
		int m_ready_count; // countdown timer to reset the ready flag after read/write enable is asserted

		// These contain data describing the current and previous voice frames
		uint16_t m_old_energy;
		uint16_t m_old_pitch;
		int m_old_k[10];

		uint16_t m_new_energy;
		uint16_t m_new_pitch;
		int m_new_k[10];

		// These are all used to contain the current state of the sound generation
		uint16_t m_current_energy;
		uint16_t m_current_pitch;
		int m_current_k[10];

		uint16_t m_target_energy;
		uint16_t m_target_pitch;
		int m_target_k[10];

		uint16_t m_previous_energy; // Needed for lattice filter to match patent

		uint8_t m_interp_count; // Number of interp periods (0-7)
		uint8_t m_sample_count; // Sample number within interp (0-24)
		int m_pitch_count;

		int m_u[11];
		int m_x[10];

		int m_rng; // the random noise generator configuration is: 1 + x + x^3 + x^4 + x^13

		int8_t m_excitation_data;

		int m_phrom_address;

		// Set after each load address, so that next read operation
		// is preceded by a dummy read
		// bool m_schedule_dummy_read;

		uint8_t m_data_register; // Data register, used by read command
		bool m_RDB_flag; // Whether we should read data register or status register
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

		int m_last_sample;
		int m_curr_sample;
		int m_source_step;
		int m_source_pos;
};

/*----------------------------------------------------------------------------*/

bool SpeechEnabled;
bool SpeechStarted;

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
	memset(m_fifo, 0, sizeof(m_fifo));
	m_fifo_head = m_fifo_tail = m_fifo_count = m_fifo_bits_taken = m_phrom_bits_taken = 0;

	// Initialize the chip state
	// Note that we do not actually clear IRQ on start-up : IRQ is even raised if m_buffer_empty or buffer_low are false
	m_tms5220_speaking = m_speak_external = m_talk_status = m_first_frame = m_last_frame = m_interrupt = false;
	m_buffer_empty = m_buffer_low = true;

	m_RDB_flag = false;

	m_ready = true;
	m_ready_count = 0;

	// Initialize the energy/pitch/k states
	m_old_energy = m_new_energy = m_current_energy = m_target_energy = 0;
	m_old_pitch = m_new_pitch = m_current_pitch = m_target_pitch = 0;

	memset(m_old_k, 0, sizeof(m_old_k));
	memset(m_new_k, 0, sizeof(m_new_k));
	memset(m_current_k, 0, sizeof(m_current_k));
	memset(m_target_k, 0, sizeof(m_target_k));

	m_previous_energy = 0;

	// Initialize the sample generators
	m_interp_count = 0;
	m_sample_count = 0;
	m_pitch_count = 0;

	memset(m_u, 0, sizeof(m_u));
	memset(m_x, 0, sizeof(m_x));

	m_rng = 0x1FFF;

	m_excitation_data = 0;

	m_phrom_address = 0;

	// m_schedule_dummy_read = true;
}

/*----------------------------------------------------------------------------*/

void TMS5220::ReadEnable()
{
	m_ready = false;
	m_ready_count = 30;
}

/*--------------------------------------------------------------------------*/

// Read status or data from the TMS5220

// From the data sheet:
//
// bit D0(bit 7) = TS - Talk Status is active (high) when the VSP is processing speech data.
//         Talk Status goes active at the initiation of a Speak command or after nine
//         bytes of data are loaded into the FIFO following a Speak External command. It
//         goes inactive (low) when the stop code (Energy=1111) is processed, or
//         immediately by a buffer empty condition or a reset command.
// bit D1(bit 6) = BL - Buffer Low is active (high) when the FIFO buffer is more than half empty.
//         Buffer Low is set when the "Last-In" byte is shifted down past the half-full
//         boundary of the stack. Buffer Low is cleared when data is loaded to the stack
//         so that the "Last-In" byte lies above the half-full boundary and becomes the
//         ninth data byte of the stack.
// bit D2(bit 5) = BE - Buffer Empty is active (high) when the FIFO buffer has run out of data
//         while executing a Speak External command. Buffer Empty is set when the last bit
//         of the "Last-In" byte is shifted out to the Synthesis Section. This causes
//         Talk Status to be cleared. Speed is terminated at some abnormal point and the
//         Speak External command execution is terminated.

unsigned char TMS5220::ReadStatus()
{
	m_ready = true;

	if (m_RDB_flag)
	{
		// If last command was read, return data register
		m_RDB_flag = false;

		return m_data_register;
	}
	else
	{
		// Read status

		// Clear the interrupt pin on status read
		m_interrupt = false;

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Status read: TS=%d BL=%d BE=%d\n", PrePC, m_talk_status, m_buffer_low, m_buffer_empty);
		WriteLog("%04X TMS5220: Clear interrupt\n", PrePC);
		#endif

		return (m_talk_status  ? 0x80 : 0x00) |
		       (m_buffer_low   ? 0x40 : 0x00) |
		       (m_buffer_empty ? 0x20 : 0x00);
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
	if (m_fifo_count < FIFO_SIZE)
	{
		m_fifo[m_fifo_tail] = data;
		m_fifo_tail = (m_fifo_tail + 1) % FIFO_SIZE;
		m_fifo_count++;

		// If we were speaking, then we're no longer empty
		if (m_speak_external)
		{
			m_buffer_empty = false;
		}

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Added byte to FIFO (size=%d)\n", PrePC, m_fifo_count);
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

	if (!m_speak_external)
	{
		// R Nabet : we parse commands at once.  It is necessary for such
		// commands as read.
		ProcessCommand();
	}

	// How long does /READY stay inactive, when /WS is pulled low?
	// This depends ENTIRELY on the command written, or whether the chip
	// is in speak external mode or not...
	//
	// Speak external mode: ~16 cycles
	// Command Mode:
	// SPK: ? cycles
	// SPKEXT: ? cycles
	// RDBY: between 60 and 140 cycles
	// RB: ? cycles (80?)
	// RST: between 60 and 140 cycles
	//
	//
	// TODO: actually HANDLE the timing differences! currently just assuming
	// always 16 cycles
	// This should take around 10-16 (closer to ~15) cycles to complete for
	// fifo writes
	// TODO: but actually depends on what command is written if in command mode

	m_ready = false;
	m_ready_count = 30;
}

/*--------------------------------------------------------------------------*/

// Returns the interrupt state of the TMS5220 (active low)

bool TMS5220::ReadInt()
{
	return !m_interrupt;
}

/*--------------------------------------------------------------------------*/

// Returns the ready state of the TMS5220 (active low)

bool TMS5220::ReadReady()
{
	return !m_ready;
}

/*--------------------------------------------------------------------------*/

void TMS5220::Poll(int Cycles)
{
	if (!m_ready)
	{
		if (m_ready_count > 0)
		{
			m_ready_count -= Cycles;

			if (m_ready_count <= 0)
			{
				m_ready_count = 0;
				m_ready = true;
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

// Clips and wraps the 14 bit return value from the lattice filter to its
// final 10 bit value (-512 to 511), and upshifts this to 16 bits

static int16_t ClipAndWrap(int16_t cliptemp)
{
	// clipping & wrapping, just like the patent shows

	if (cliptemp > 2047)
	{
		cliptemp = -2048 + (cliptemp - 2047);
	}
	else if (cliptemp < -2048)
	{
		cliptemp = 2047 - (cliptemp + 2048);
	}

	if (cliptemp > 511)
	{
		return 127 << 8;
	}
	else if (cliptemp < -512)
	{
		return -128 << 8;
	}
	else
	{
		return cliptemp << 6;
	}
}

/*--------------------------------------------------------------------------*/

// Executes one 'full run' of the lattice filter on a specific byte of
// excitation data, and specific values of all the current k constants,
// and returns the resulting sample.
//
// Note: the current_k processing here by dividing the result by 32768 is
// necessary, as the stored parameters in the lookup table are the 10 bit
// coefficients but are pre-multiplied by 512 for ease of storage. This is
// undone on the real chip by a shifter here, after the multiply.

int16_t TMS5220::LatticeFilter()
{
	// Lattice filter here
	//
	// Aug/05/07: redone as unrolled loop, for clarity - LN
	// Copied verbatim from table I in US patent 4,209,804:
	// notation equivalencies from table:
	// Yn(i) = m_u[n-1]
	// Kn = m_current_k[n-1]
	// bn = m_x[n-1]

	m_u[10] = (m_excitation_data * m_previous_energy) >> 8; // Y(11)
	m_u[9]  = m_u[10] - (m_current_k[9] * m_x[9]) / 32768;
	m_u[8]  = m_u[9]  - (m_current_k[8] * m_x[8]) / 32768;
	m_x[9]  = m_x[8]  + (m_current_k[8] * m_u[8]) / 32768;
	m_u[7]  = m_u[8]  - (m_current_k[7] * m_x[7]) / 32768;
	m_x[8]  = m_x[7]  + (m_current_k[7] * m_u[7]) / 32768;
	m_u[6]  = m_u[7]  - (m_current_k[6] * m_x[6]) / 32768;
	m_x[7]  = m_x[6]  + (m_current_k[6] * m_u[6]) / 32768;
	m_u[5]  = m_u[6]  - (m_current_k[5] * m_x[5]) / 32768;
	m_x[6]  = m_x[5]  + (m_current_k[5] * m_u[5]) / 32768;
	m_u[4]  = m_u[5]  - (m_current_k[4] * m_x[4]) / 32768;
	m_x[5]  = m_x[4]  + (m_current_k[4] * m_u[4]) / 32768;
	m_u[3]  = m_u[4]  - (m_current_k[3] * m_x[3]) / 32768;
	m_x[4]  = m_x[3]  + (m_current_k[3] * m_u[3]) / 32768;
	m_u[2]  = m_u[3]  - (m_current_k[2] * m_x[2]) / 32768;
	m_x[3]  = m_x[2]  + (m_current_k[2] * m_u[2]) / 32768;
	m_u[1]  = m_u[2]  - (m_current_k[1] * m_x[1]) / 32768;
	m_x[2]  = m_x[1]  + (m_current_k[1] * m_u[1]) / 32768;
	m_u[0]  = m_u[1]  - (m_current_k[0] * m_x[0]) / 32768;
	m_x[1]  = m_x[0]  + (m_current_k[0] * m_u[0]) / 32768;
	m_x[0]  = m_u[0];

	m_previous_energy = m_current_energy;

	return m_u[0];
}

/*--------------------------------------------------------------------------*/

// Fill the buffer with a specific number of samples

void TMS5220::ProcessSamples(int16_t *buffer, int size)
{
	int buf_count = 0;
	int i, interp_period;

tryagain:
	// If we're not speaking, parse commands
	// while (!m_speak_external && m_fifo_count > 0)
	//   process_command(tms);

	// If we're empty and still not speaking, fill with nothingness
	if (!m_tms5220_speaking && !m_last_frame)
		goto empty;

	// If we're to speak, but haven't started, wait for the 9th byte
	if (!m_talk_status && m_speak_external)
	{
		if (m_fifo_count < 9)
			goto empty;

		m_talk_status = true;
		m_first_frame = true; // will cause the first frame to be parsed
		m_buffer_empty = false;
	}

	// Loop until the buffer is full or we've stopped speaking
	while (size > 0 && m_talk_status)
	{
		// If we're ready for a new frame
		if (m_interp_count == 0 && m_sample_count == 0)
		{
			// Parse a new frame
			if (!ParseFrame(m_first_frame))
				break;

			m_first_frame = false;

			// Set old target as new start of frame
			m_current_energy = m_old_energy;
			m_current_pitch = m_old_pitch;

			for (i = 0; i < 10; i++)
				m_current_k[i] = m_old_k[i];

			// Is this a zero energy frame?
			if (m_current_energy == 0)
			{
				// WriteLog("processing frame: zero energy\n");
				m_target_energy = 0;
				m_target_pitch = m_current_pitch;

				for (i = 0; i < 10; i++)
					m_target_k[i] = m_current_k[i];
			}
			// Is this a stop frame?
			else if (m_current_energy == (energytable[15] >> 6))
			{
				// WriteLog("processing frame: stop frame\n");
				m_current_energy = energytable[0] >> 6;
				m_target_energy = m_current_energy;
				/* m_interp_count = m_sample_count = */ m_pitch_count = 0;
				m_last_frame = 0;

				if (m_tms5220_speaking)
				{
					// New speech command in progress
					m_first_frame = true;
				}
				else
				{
					// Really stop speaking
					m_talk_status = false;

					// Generate an interrupt if necessary
					m_interrupt = true;

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
				if (m_new_energy == (energytable[15] >> 6))
				{
					m_target_energy = 0;
					m_target_pitch = m_current_pitch;

					for (i = 0; i < 10; i++)
						m_target_k[i] = m_current_k[i];
				}
				else
				{
					// Reset the step size
					m_target_energy = m_new_energy;
					m_target_pitch = m_new_pitch;

					for (i = 0; i < 4; i++)
					{
						m_target_k[i] = m_new_k[i];
					}

					if (m_current_pitch == 0)
					{
						for (i = 4; i < 10; i++)
						{
							m_target_k[i] = m_current_k[i] = 0;
						}
					}
					else
					{
						for (i = 4; i < 10; i++)
						{
							m_target_k[i] = m_new_k[i];
						}
					}
				}
			}
		}
		else if (m_interp_count == 0)
		{
			// Update values based on step values

			interp_period = m_sample_count / 25;

			m_current_energy += (m_target_energy - m_current_energy) >> interp_coeff[interp_period];

			if (m_old_pitch != 0)
			{
				m_current_pitch += (m_target_pitch - m_current_pitch) >> interp_coeff[interp_period];
			}

			for (i = 0; i < 10; i++)
			{
				m_current_k[i] += (m_target_k[i] - m_current_k[i]) >> interp_coeff[interp_period];
			}
		}

		if (m_old_energy == 0)
		{
			// Generate silent samples here

			// This is NOT correct, the current_energy is forced to zero when we
			// just passed a zero energy frame because thats what the tables hold
			// for that value. However, this code does no harm. Will be removed later.
			m_excitation_data = 0;
		}
		else if (m_old_pitch == 0)
		{
			// Generate unvoiced samples here

			if (m_rng & 1)
			{
				// According to the patent it is (either + or -) half of the maximum value
				// in the chirp table, so +-64
				m_excitation_data = -64;
			}
			else
			{
				m_excitation_data = 64;
			}
		}
		else
		{
			// Generate voiced samples here

			// US patent 4331836 Figure 14B shows, and logic would hold,
			// that a pitch based chirp function has a chirp/peak and then a
			// long chain of zeroes. The last entry of the chirp rom is at
			// address 0b110011 (50d), the 51st sample, and if the address
			// reaches that point the ADDRESS incrementer is disabled, forcing
			// all samples beyond 50d to be == 50d (address 50d holds zeroes)

			if (m_pitch_count > 50)
			{
				m_excitation_data = chirptable[50];
			}
			else
			{
				m_excitation_data = chirptable[m_pitch_count];
			}
		}

		int bitout = ((m_rng >> 12) & 1) ^
		             ((m_rng >> 10) & 1) ^
		             ((m_rng >>  9) & 1) ^
		             ((m_rng >>  0) & 1);

		m_rng >>= 1;
		m_rng |= bitout << 12;

		// Execute lattice filter and clipping/wrapping
		buffer[buf_count] = ClipAndWrap(LatticeFilter());

		// Update all counts

		size--;
		m_sample_count = (m_sample_count + 1) % 200;

		if (m_current_pitch != 0)
		{
			m_pitch_count = (m_pitch_count + 1) % m_current_pitch;
		}
		else
		{
			m_pitch_count = 0;
		}

		m_interp_count = (m_interp_count + 1) % 25;
		buf_count++;
	}

empty:
	while (size > 0)
	{
		m_sample_count = (m_sample_count + 1) % 200;
		m_interp_count = (m_interp_count + 1) % 25;
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
	if (m_fifo_bits_taken > 0)
	{
		m_fifo_bits_taken = 0;
		m_fifo_count--;
		m_fifo_head = (m_fifo_head + 1) % FIFO_SIZE;
	}

	// Grab a full byte from the FIFO
	if (m_fifo_count > 0)
	{
		unsigned char cmd = m_fifo[m_fifo_head];
		m_fifo_count--;
		m_fifo_head = (m_fifo_head + 1) % FIFO_SIZE;

		// Parse the command
		switch (cmd & 0x70)
		{
			case 0x10: // Read byte
				/* if (m_schedule_dummy_read)
				{
					m_schedule_dummy_read = false;

					ReadPhrom(1);
				} */

				m_phrom_bits_taken = 0;
				m_data_register = ReadPhrom(8); /* read one byte from speech ROM... */
				m_RDB_flag = true;
				break;

			case 0x30: // Read and branch
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: read and branch\n", PrePC);
				#endif

				m_RDB_flag = false;

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

				m_phrom_address >>= 4;
				m_phrom_address &= 0x0ffff;
				m_phrom_address |= (data << 16);

				#if ENABLE_LOG
				WriteLog("%04X TMS5220: load address cmd with data = 0x%02x, new address = 0x%05x\n", PrePC, data, m_phrom_address);
				#endif

				// m_schedule_dummy_read = true;
				break;
			}

			case 0x50: // Speak
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: speak\n", PrePC);
				#endif

				/* if (m_schedule_dummy_read)
				{
					m_schedule_dummy_read = false;
					ReadPhrom(1);
				} */

				m_tms5220_speaking = true;
				m_speak_external = false;

				if (!m_last_frame)
				{
					m_first_frame = true;
				}

				m_talk_status = true; // Start immediately
				break;

			case 0x60: // Speak external
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: speak external\n", PrePC);
				#endif

				// SPKEXT going active activates SPKEE which clears the fifo
				m_fifo_head = m_fifo_tail = m_fifo_count = m_fifo_bits_taken = 0;
				// SPEN is enabled when the fifo passes half full (falling edge of BL signal)

				m_tms5220_speaking = m_speak_external = true;

				m_RDB_flag = false;

				// According to the datasheet, this will cause an interrupt due to a BE condition
				if (!m_buffer_empty)
				{
					m_buffer_empty = true;
					m_interrupt = true;

					#if ENABLE_LOG
					WriteLog("%04X TMS5220: Buffer empty set\n", PrePC);
					WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
					#endif
				}

				m_talk_status = false; // Wait to have 8 bytes in buffer before starting
				break;

			case 0x70: // Reset
				#if ENABLE_LOG
				WriteLog("%04X TMS5220: reset\n", PrePC);
				#endif

				/* if (m_schedule_dummy_read)
				{
					m_schedule_dummy_read = false;

					ReadPhrom(1);
				} */

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

	if (m_speak_external)
	{
		// Extract from FIFO
		while (count--)
		{
			val = (val << 1) | ((m_fifo[m_fifo_head] >> m_fifo_bits_taken) & 1);

			m_fifo_bits_taken++;

			if (m_fifo_bits_taken >= 8)
			{
				m_fifo_count--;
				m_fifo[m_fifo_head] = 0; // Zero the newly depleted fifo head byte
				m_fifo_head = (m_fifo_head + 1) % FIFO_SIZE;
				m_fifo_bits_taken = 0;
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

// Parse a new frame's worth of data; returns false if not enough bits in buffer

bool TMS5220::ParseFrame(bool first_frame)
{
	int bits = 0; // Number of bits in FIFO (speak external only)

	if (!first_frame)
	{
		// Remember previous frame
		m_old_energy = m_new_energy;
		m_old_pitch = m_new_pitch;
		for (int i = 0; i < 10; i++)
			m_old_k[i] = m_new_k[i];
	}

	// Clear out the new frame
	m_new_energy = 0;
	m_new_pitch = 0;
	for (int i = 0; i < 10; i++)
		m_new_k[i] = 0;

	// If the previous frame was a stop frame, don't do anything
	if (!first_frame && (m_old_energy == (energytable[15] >> 6)))
	{
		m_buffer_empty = true;
		return true;
	}

	if (m_speak_external)
	{
		// Count the total number of bits available
		bits = m_fifo_count * 8 - m_fifo_bits_taken;
	}

	int rep_flag;
	int indx;

	// Attempt to extract the energy index
	if (m_speak_external)
	{
		bits -= 4;
		if (bits < 0)
			goto ranout;
	}

	indx = ExtractBits(4);
	m_new_energy = energytable[indx] >> 6;

	// If the index is 0 or 15, we're done
	if (indx == 0 || indx == 15)
	{
		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (4-bit energy=%d frame)\n", PrePC, m_new_energy);
		#endif

		// Clear fifo if stop frame encountered
		if (indx == 15)
		{
			m_fifo_head = m_fifo_tail = m_fifo_count = m_fifo_bits_taken = m_phrom_bits_taken = 0;
			m_speak_external = m_tms5220_speaking = false;
			m_last_frame = true;
		}
		goto done;
	}

	// Attempt to extract the repeat flag
	if (m_speak_external)
	{
		bits -= 1;
		if (bits < 0)
			goto ranout;
	}

	rep_flag = ExtractBits(1);

	// Attempt to extract the pitch
	if (m_speak_external)
	{
		bits -= 6;
		if (bits < 0)
			goto ranout;
	}

	indx = ExtractBits(6);
	m_new_pitch = pitchtable[indx] / 256;

	// If this is a repeat frame, just copy the k's
	if (rep_flag)
	{
		for (int i = 0; i < 10; i++)
			m_new_k[i] = m_old_k[i];

		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (11-bit energy=%d pitch=%d rep=%d frame)\n", PrePC, m_new_energy, m_new_pitch, rep_flag);
		#endif

		goto done;
	}

	// If the pitch index was zero, we need 4 k's
	if (indx == 0)
	{
		// Attempt to extract 4 K's
		if (m_speak_external)
		{
			bits -= 18;
			if (bits < 0)
				goto ranout;
		}

		m_new_k[0] = k1table[ExtractBits(5)];
		m_new_k[1] = k2table[ExtractBits(5)];
		m_new_k[2] = k3table[ExtractBits(4)];
		m_new_k[3] = k4table[ExtractBits(4)];

		#if ENABLE_LOG
		WriteLog("%04X TMS5220:  (29-bit energy=%d pitch=%d rep=%d 4K frame)\n", PrePC, m_new_energy, m_new_pitch, rep_flag);
		#endif

		goto done;
	}

	// Else we need 10 K's
	if (m_speak_external)
	{
		bits -= 39;
		if (bits < 0)
			goto ranout;
	}

	m_new_k[0] = k1table[ExtractBits(5)];
	m_new_k[1] = k2table[ExtractBits(5)];
	m_new_k[2] = k3table[ExtractBits(4)];
	m_new_k[3] = k4table[ExtractBits(4)];
	m_new_k[4] = k5table[ExtractBits(4)];
	m_new_k[5] = k6table[ExtractBits(4)];
	m_new_k[6] = k7table[ExtractBits(4)];
	m_new_k[7] = k8table[ExtractBits(3)];
	m_new_k[8] = k9table[ExtractBits(3)];
	m_new_k[9] = k10table[ExtractBits(3)];

	#if ENABLE_LOG
	WriteLog("%04X TMS5220:  (50-bit energy=%d pitch=%d rep=%d 10K frame)\n", PrePC, m_new_energy, m_new_pitch, rep_flag);
	#endif

done:
	#if ENABLE_LOG
	if (m_speak_external)
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
		m_old_energy = m_new_energy;
		m_old_pitch = m_new_pitch;

		for (int i = 0; i < 10; i++)
			m_old_k[i] = m_new_k[i];
	}

	// Update the buffer_low state
	CheckBufferLow();
	return true;

ranout:
	#if ENABLE_LOG
	WriteLog("%04X TMS5220: Ran out of bits on a parse!\n", PrePC);
	#endif

	// This is an error condition; mark the buffer empty and turn off speaking
	m_buffer_empty = true;
	m_talk_status = m_speak_external = m_tms5220_speaking = m_first_frame = m_last_frame = false;
	m_fifo_count = m_fifo_head = m_fifo_tail = 0;

	m_RDB_flag = false;

	// Generate an interrupt if necessary
	m_interrupt = true;

	#if ENABLE_LOG
	WriteLog("TMS5220: Interrupt set\n");
	#endif

	return false;
}

/*--------------------------------------------------------------------------*/

// Check to see if the buffer low flag should be on or off

void TMS5220::CheckBufferLow()
{
	if (m_fifo_count <= 8)
	{
		// Generate an interrupt if necessary; if /BL was inactive and
		// is now active, set int.

		if (!m_buffer_low)
		{
			#if ENABLE_LOG
			WriteLog("%04X TMS5220: Interrupt set\n", PrePC);
			WriteLog("%04X TMS5220: Buffer low set\n", PrePC);
			#endif

			m_interrupt = true;
		}

		m_buffer_low = true;
	}
	else
	{
		m_buffer_low = false;

		#if ENABLE_LOG
		WriteLog("%04X TMS5220: Buffer low cleared\n", PrePC);
		#endif
	}
}

/*----------------------------------------------------------------------------*/

int TMS5220::ReadPhrom(int count)
{
	int phrom = (m_phrom_address >> 14) & 0xf;

	#if ENABLE_LOG
	WriteLog("%04X TMS5220: ReadPhrom, addr = 0x%04x, phrom = 0x%02x, count = %d", PrePC, m_phrom_address, phrom, count);
	#endif

	int val = 0;

	while (count-- > 0)
	{
		int addr = m_phrom_address & 0x3fff;

		val = (val << 1) | ((phrom_rom[phrom][addr] >> m_phrom_bits_taken) & 1);

		if (++m_phrom_bits_taken == 8)
		{
			m_phrom_address++;
			m_phrom_bits_taken = 0;
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
	m_last_sample(0),
	m_curr_sample(0),
	m_source_step((int)((double)(Clock / 80) * (double)FRAC_ONE / (double)SoundSampleRate)),
	m_source_pos(0)
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
	int prev = m_last_sample;
	int curr = m_curr_sample;
	unsigned char *buffer = buff;

	// Finish off the current sample
	if (m_source_pos > 0)
	{
		// Interpolate
		while (length > 0 && m_source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - m_source_pos)) + (curr * m_source_pos)) >> FRAC_BITS;
			*buffer++ = ((samp + 32768) >> 8) & 0xff;
			m_source_pos += m_source_step;
			length--;
		}

		// If we're over, continue; otherwise, we're done
		if (m_source_pos >= FRAC_ONE)
		{
			m_source_pos -= FRAC_ONE;
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
	int final_pos = m_source_pos + length * m_source_step;
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
		while (length > 0 && m_source_pos < FRAC_ONE)
		{
			int samp = ((prev * (FRAC_ONE - m_source_pos)) + (curr * m_source_pos)) >> FRAC_BITS;
			*buffer++ = ((samp + 32768) >> 8) & 0xff;
			m_source_pos += m_source_step;
			length--;
		}

		// If we're over, grab the next samples
		if (m_source_pos >= FRAC_ONE)
		{
			m_source_pos -= FRAC_ONE;
			prev = curr;
			curr = *curr_data++;
			if (curr_data - sample_data > MAX_SAMPLE_CHUNK)
				curr_data = sample_data;
		}
	}

	// Remember the last samples
	m_last_sample = prev;
	m_curr_sample = curr;

	#if ENABLE_LOG
	// if (buffer - buff != length)
	//	WriteLog("At end of update - mismatch in num of samples = %d, %d\n", length, buffer - buff);
	#endif
}

/*----------------------------------------------------------------------------*/

bool SpeechInit()
{
	memset(phrom_rom, 0, sizeof(phrom_rom));

	// Read the PHROM files listed in Phroms.cfg.
	char Path[MAX_PATH];
	strcpy(Path, mainWin->GetUserDataPath());
	AppendPath(Path, "Phroms.cfg");

	std::ifstream RomCfg(Path);

	if (!RomCfg)
	{
		mainWin->Report(MessageType::Error, "Cannot open PHROM configuration file:\n  %s", Path);
		return false;
	}

	bool Success = true;

	// Read phroms
	int RomSlot = 15;

	std::string Line;

	while (std::getline(RomCfg, Line))
	{
		trim(Line);

		// Skip blank lines and comments
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		if (Line == "EMPTY")
		{
			continue;
		}

		if (Line.size() >= MAX_PATH)
		{
			mainWin->Report(MessageType::Error, "Invalid PHROM configuration file:\n  %s", Path);
			Success = false;
			break;
		}

		char PhromPath[MAX_PATH];
		strcpy(PhromPath, Line.c_str());

		if (IsRelativePath(Line.c_str()))
		{
			strcpy(PhromPath, mainWin->GetUserDataPath());
			AppendPath(PhromPath, "Phroms");
			AppendPath(PhromPath, Line.c_str());
		}

		FILE *InFile = fopen(PhromPath, "rb");

		if (InFile != nullptr)
		{
			size_t BytesRead = fread(phrom_rom[RomSlot], 1, 16384, InFile);

			fclose(InFile);

			if (BytesRead == 0 || BytesRead % 1024 != 0)
			{
				mainWin->Report(MessageType::Error,
				                "Invalid PHROM file size:\n  %s",
				                PhromPath);
			}
		}
		else
		{
			mainWin->Report(MessageType::Error, "Cannot open specified PHROM:\n\n%s", PhromPath);
			Success = false;
		}

		if (RomSlot == 0)
		{
			break;
		}
		else
		{
			RomSlot--;
		}
	}

	return Success;
}

/*--------------------------------------------------------------------------*/

// Allocate buffers and reset the 5220

void SpeechStart()
{
	int clock = 640000; // 640 kHz

	SpeechStop();

	tms5220 = new(std::nothrow) TMS5220StreamState(clock);

	if (tms5220 == nullptr)
	{
		return;
	}

	SpeechStarted = true;
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

	SpeechStarted = false;
}

/*--------------------------------------------------------------------------*/

// Write data to the sound chip

void SpeechWrite(unsigned char Data)
{
	if (SpeechStarted)
	{
		tms5220->chip.WriteData(Data);
	}
}

/*--------------------------------------------------------------------------*/

void SpeechReadEnable()
{
	if (SpeechStarted)
	{
		tms5220->chip.ReadEnable();
	}
}

/*--------------------------------------------------------------------------*/

// Read status or data from the sound chip

unsigned char SpeechRead()
{
	if (SpeechStarted)
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
	if (SpeechStarted)
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
	if (SpeechStarted)
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
	if (SpeechStarted)
	{
		tms5220->Update(buff, length);
	}
}

/*--------------------------------------------------------------------------*/

void SpeechPoll(int Cycles)
{
	if (SpeechStarted)
	{
		tms5220->chip.Poll(Cycles);
	}
}

/*--------------------------------------------------------------------------*/

#endif
