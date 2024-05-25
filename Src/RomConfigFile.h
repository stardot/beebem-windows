/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2024  Chris Needham

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

#ifndef ROM_CONFIG_FILE_HEADER
#define ROM_CONFIG_FILE_HEADER

#include "Model.h"

#include <string>

constexpr int ROM_BANK_COUNT = 16;

class RomConfigFile
{
	public:
		void Reset();

		bool Load(const char *FileName);
		bool Save(const char *FileName);

		const std::string& GetFileName(Model model, int index) const;
		void SetFileName(Model model, int index, const std::string& FileName);

		void Swap(Model model, int First, int Second);

	private:
		std::string m_FileName[MODEL_COUNT][ROM_BANK_COUNT + 1];
};

extern const char *BANK_EMPTY;
extern const char *BANK_RAM;
extern const char *ROM_WRITABLE;

#endif
