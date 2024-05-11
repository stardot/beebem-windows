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

#include <windows.h>

#include "RomConfigFile.h"
#include "BeebWin.h"
#include "FileUtils.h"
#include "Main.h"
#include "StringUtils.h"

#include <stdio.h>
#include <fstream>
#include <string>

/****************************************************************************/

const char *BANK_EMPTY = "EMPTY";
const char *BANK_RAM = "RAM";
const char *ROM_WRITABLE = ":RAM";

static const char* DefaultRoms[MODEL_COUNT][ROM_BANK_COUNT + 1] =
{
	// BBC B
	{
		"BBC\\OS12.rom",
		"BBC\\BASIC2.rom",
		"BBC\\DNFS.rom",
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY
	},
	// BBC Model B + Integra-B
	{
		"BBCINT\\OS12.rom",
		"BBCINT\\IBOS126.rom",
		"BBCINT\\BASIC2.rom",
		"BBCINT\\DNFS.rom",
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY
	},
	// BBC Model B Plus
	{
		"BPLUS\\B+MOS.rom",
		"BPLUS\\BASIC2.rom",
		"BPLUS\\DFS-2.26.rom",
		"BPLUS\\ADFS-1.30.rom",
		BANK_RAM,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_RAM,
		BANK_RAM
	},
	// BBC Master 128
	{
		"M128\\MOS.rom",
		"M128\\Terminal.rom",
		"M128\\View.rom",
		"M128\\ADFS.rom",
		"M128\\BASIC4.rom",
		"M128\\Edit.rom",
		"M128\\ViewSheet.rom",
		"M128\\DFS.rom",
		"M128\\ANFS-4.25-2201351.rom",
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
	},
	// BBC Master ET
	{
		"Master ET\\MOS400.rom",
		"Master ET\\Terminal.rom",
		"Master ET\\ANFS425.rom",
		"Master ET\\BASIC4.rom",
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_RAM,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY,
		BANK_EMPTY
	}
};

/****************************************************************************/

void RomConfigFile::Reset()
{
	for (int model = 0; model < MODEL_COUNT; model++)
	{
		for (int index = 0; index < ROM_BANK_COUNT + 1; index++)
		{
			m_FileName[model][index].clear();
		}
	}
}

/****************************************************************************/

bool RomConfigFile::Load(const char *FileName)
{
	bool Success = true;

	std::ifstream File(FileName);

	if (!File)
	{
		mainWin->Report(MessageType::Error,
		                "Cannot open ROM configuration file:\n  %s", FileName);

		return false;
	}

	std::string Line;

	int Bank = 0;
	int Model = 0;

	while (std::getline(File, Line))
	{
		trim(Line);

		// Skip blank lines and comments
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		if (Line.size() >= _MAX_PATH)
		{
			Success = false;
			break;
		}

		m_FileName[Model][Bank] = Line;

		if (++Bank == ROM_BANK_COUNT + 1)
		{
			Model++;
			Bank = 0;

			if (Model == MODEL_COUNT)
			{
				break;
			}
		}
	}

	// For backwards compatibility with existing config files from earlier
	// BeebEm versions, set default ROMs for any models not in the config file.

	while (Model < MODEL_COUNT)
	{
		for (Bank = 0; Bank < ROM_BANK_COUNT + 1; Bank++)
		{
			m_FileName[Model][Bank] = DefaultRoms[Model][Bank];
		}

		Model++;
	}

	if (!Success)
	{
		mainWin->Report(MessageType::Error,
		                "Invalid ROM configuration file:\n  %s", FileName);

		Reset();
	}

	return Success;
}

/****************************************************************************/

bool RomConfigFile::Save(const char *FileName)
{
	if (FileExists(FileName))
	{
		if (mainWin->Report(MessageType::Question,
		                    "File already exists:\n  %s\n\nOverwrite file?",
		                    FileName) != MessageResult::Yes)
		{
			return false;
		}
	}

	FILE *fd = fopen(FileName, "w");

	if (fd == nullptr)
	{
		mainWin->Report(MessageType::Error,
		                "Failed to write ROM configuration file:\n  %s", FileName);
		return false;
	}

	for (int model = 0; model < MODEL_COUNT; model++)
	{
		fprintf(fd, "# %s\n\n", GetModelName(static_cast<Model>(model)));

		for (int bank = 0; bank < ROM_BANK_COUNT + 1; ++bank)
		{
			fprintf(fd, "%s\n", GetFileName(static_cast<Model>(model), bank).c_str());
		}

		if (model < MODEL_COUNT - 1)
		{
			fprintf(fd, "\n");
		}
	}

	fclose(fd);

	return true;
}

/****************************************************************************/

const std::string& RomConfigFile::GetFileName(Model model, int index) const
{
	return m_FileName[static_cast<int>(model)][index];
}

void RomConfigFile::SetFileName(Model model, int index, const std::string& FileName)
{
	m_FileName[static_cast<int>(model)][index] = FileName;
}

/****************************************************************************/

void RomConfigFile::Swap(Model model, int First, int Second)
{
	std::swap(
		m_FileName[static_cast<int>(model)][First],
		m_FileName[static_cast<int>(model)][Second]
	);
}

/****************************************************************************/
