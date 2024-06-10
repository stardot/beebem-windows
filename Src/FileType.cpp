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

#include <string.h>

#include "FileType.h"

FileType GetFileTypeFromExtension(const char* FileName)
{
	FileType Type = FileType::None;

	const char *Ext = strrchr(FileName, '.');

	if (Ext != nullptr)
	{
		if (_stricmp(Ext + 1, "ssd") == 0)
		{
			Type = FileType::SSD;
		}
		else if (_stricmp(Ext + 1, "dsd") == 0)
		{
			Type = FileType::DSD;
		}
		else if (_stricmp(Ext + 1, "adl") == 0)
		{
			Type = FileType::ADFS;
		}
		else if (_stricmp(Ext + 1, "adf") == 0)
		{
			Type = FileType::ADFS;
		}
		else if (_stricmp(Ext + 1, "uef") == 0)
		{
			Type = FileType::UEF;
		}
		else if (_stricmp(Ext + 1, "uefstate") == 0)
		{
			Type = FileType::UEFState;
		}
		else if (_stricmp(Ext + 1, "csw") == 0)
		{
			Type = FileType::CSW;
		}
		else if (_stricmp(Ext + 1, "img") == 0)
		{
			Type = FileType::IMG;
		}
		else if (_stricmp(Ext + 1, "dos") == 0)
		{
			Type = FileType::DOS;
		}
		else if (_stricmp(Ext + 1, "fsd") == 0)
		{
			Type = FileType::FSD;
		}
	}

	return Type;
}
