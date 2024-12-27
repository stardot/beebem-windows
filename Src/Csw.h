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

#ifndef CSW_HEADER
#define CSW_HEADER

#include <vector>

#include "TapeMap.h"

enum class CSWResult {
	Success,
	OpenFailed,
	ReadFailed,
	InvalidCSWFile,
	InvalidHeaderExtension
};

CSWResult CSWOpen(const char *FileName);
void CSWClose();

int CSWPoll();
void CSWCreateTapeMap(std::vector<TapeMapEntry>& TapeMap);

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
extern int CSWPollCycles;

void SaveCSWState(FILE* SUEF);
void LoadCSWState(FILE* SUEF);

#endif
