/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell

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

#include "model.h"
#include <string>

class RomConfigFile
{
	public:
		void Reset();

		bool Load(const char *FileName);
		bool Save(const char *FileName);

		const std::string& GetFileName(Model model, int index) const;
		void SetFileName(Model model, int index, const std::string& FileName);

	private:
		std::string m_FileName[static_cast<int>(Model::Last)][17];
};

extern const char *BANK_EMPTY;
extern const char *BANK_RAM;
extern const char *ROM_WRITABLE;

#endif
