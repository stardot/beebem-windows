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

#include "ARMulator/armdefs.h"

class CSprowCoPro
{
	public:
		enum class InitResult
		{
			Success,
			FileNotFound,
			InvalidROM
		};

	public:
		CSprowCoPro();
		~CSprowCoPro();
		CSprowCoPro(const CSprowCoPro&) = delete;
		CSprowCoPro& operator=(CSprowCoPro&) = delete;

		InitResult Init(const char *ROMPath);
		void Execute(int Count);
		void Reset();

		void SaveState(FILE* SUEF);
		void LoadState(FILE* SUEF);

	private:
		static constexpr int ROM_SIZE = 0x80000; // 512 KBytes of rom memory

		unsigned char m_ROMMemory[ROM_SIZE];
		ARMul_State *m_State;
		// bool keepRunning; // keep calling run()
		int m_CycleCount;
};
