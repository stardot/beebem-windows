/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  David Sharp
ARM7TDMI Co-Processor Emulator
Copyright (C) 2010 Kieran Mockford

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

extern "C"
{
#include "ARMulator\armdefs.h"
}

typedef	unsigned char uint8;

class CSprowCoPro
{
// functions
public:
	// construct / destruct
	CSprowCoPro();
	~CSprowCoPro();

	// bool keepRunning; // keep calling run()
	void exec(int count);
	void reset();

private:
	uint8 romMemory[0x80000]; // 512 KBytes of rom memory
	ARMul_State* state;
	int m_CycleCount;
};
