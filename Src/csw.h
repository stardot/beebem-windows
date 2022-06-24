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

#ifndef INCLUDE_CSW
#define INCLUDE_CSW

enum class CSWResult {
	Success,
	OpenFailed,
	InvalidCSWFile,
	InvalidHeaderExtension
};

constexpr int BUFFER_LEN = 256;

CSWResult CSWOpen(const char *FileName);
void CSWClose();

int csw_data(void);
int csw_poll(int clock);
void map_csw_file();

enum class CSWState {
	WaitingForTone,
	Tone,
	Data,
	Undefined
};

extern CSWState csw_state;

extern int csw_bit;
extern int csw_pulselen;
extern int csw_ptr;
extern int csw_pulsecount;
extern bool CSWFileOpen;
extern int CSW_CYCLES;

#endif
