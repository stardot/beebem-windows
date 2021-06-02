/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2009  Mike Wyatt

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

#include <windows.h>
#include "RomConfigFile.h"
#include "beebwin.h"
#include "main.h"

#include <stdio.h>
#include <fstream>
#include <string>

const char *BANK_EMPTY = "EMPTY";
const char *BANK_RAM = "RAM";
const char *ROM_WRITABLE = ":RAM";

void RomConfigFile::Reset()
{
	for (int model = 0; model < static_cast<int>(Model::Last); model++)
	{
		for (int index = 0; index < 17; index++)
		{
			m_FileName[model][index].clear();
		}
	}
}

bool RomConfigFile::Load(const char *filename)
{
	bool success = true;
	int model;
	int bank;
	std::string line;

	std::ifstream file(filename);
	if (!file.is_open())
	{
		mainWin->Report(MessageType::Error,
		                "Cannot open ROM configuration file:\n  %s", filename);
		return false;
	}

	Reset();

	for (model = 0; model < 4 && success; ++model)
	{
		for (bank = 0; bank < 17 && success; ++bank)
		{
			if (std::getline(file, line))
			{
				m_FileName[model][bank] = line;
			}
			else
			{
				success = false;
			}
		}
	}

	// Now get Filestore E01 OS & FS

	for (bank = 0; bank < 2 && success; ++bank)
	{
		if (std::getline(file, line))
		{
			SetFileName(Model::FileStoreE01, bank, line);
		}
		else
		{
			break;
		}
	}

	// and Filestore E01S OS

	if (std::getline(file, line))
	{
		SetFileName(Model::FileStoreE01S, 0, line);
	}

	if (!success)
	{
		mainWin->Report(MessageType::Error,
		                "Invalid ROM configuration file:\n  %s", filename);
		Reset();
	}

	return success;
}

bool RomConfigFile::Save(const char *filename)
{
	FILE *fd = fopen(filename, "r");

	if (fd)
	{
		fclose(fd);

		if (mainWin->Report(MessageType::Question,
		                    "File already exists:\n  %s\n\nOverwrite file?",
		                    filename) != MessageResult::Yes)
		{
			return false;
		}
	}

	fd = fopen(filename, "w");
	if (!fd)
	{
		mainWin->Report(MessageType::Error,
		                "Failed to write ROM configuration file:\n  %s", filename);
		return false;
	}

	for (int model = 0; model <= static_cast<int>(Model::Master128); ++model)
	{
		for (int bank = 0; bank < 17; ++bank)
		{
			fprintf(fd, "%s\n", GetFileName(static_cast<Model>(model), bank).c_str());
		}
	}

	// add the Filestore E01 files
	fprintf(fd, "%s\n", GetFileName(Model::FileStoreE01, 0).c_str()); // OS
	fprintf(fd, "%s\n", GetFileName(Model::FileStoreE01, 1).c_str()); // FS

	// add the Filestore E01S files
	fprintf(fd, "%s\n", GetFileName(Model::FileStoreE01S, 0).c_str()); // OS & FS combined

	fclose(fd);

	return true;
}

const std::string& RomConfigFile::GetFileName(Model model, int index) const
{
	return m_FileName[static_cast<int>(model)][index];
}

void RomConfigFile::SetFileName(Model model, int index, const std::string& FileName)
{
	m_FileName[static_cast<int>(model)][index] = FileName;
}
