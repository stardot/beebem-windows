/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2007  Mike Wyatt

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

#include <assert.h>
#include <fstream>
#include <string>

#include "Preferences.h"
#include "StringUtils.h"

//-----------------------------------------------------------------------------

// Token written to start of pref files
static const char* PREFS_TOKEN = "*** BeebEm Preferences ***";

//-----------------------------------------------------------------------------

static int Hex2Int(int hex)
{
	if (hex >= '0' && hex <= '9')
	{
		return hex - '0';
	}
	else if (hex >= 'A' && hex <= 'F')
	{
		return hex - 'A' + 10;
	}
	else if (hex >= 'a' && hex <= 'f')
	{
		return hex - 'a' + 10;
	}

	return 0;
}

//-----------------------------------------------------------------------------

Preferences::Result Preferences::Load(const char* FileName)
{
	std::ifstream Input(FileName);

	if (!Input)
	{
		return Result::Failed;
	}

	std::string Line;

	std::getline(Input, Line);

	if (!Input || Line != PREFS_TOKEN)
	{
		return Result::InvalidFormat;
	}

	while (std::getline(Input, Line))
	{
		trim(Line);

		// Skip blank lines and comments
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string::size_type Pos = Line.find('=');

		if (Pos != std::string::npos)
		{
			std::string Key(Line, 0, Pos);
			std::string Value(Line, Pos + 1, Line.size());

			trim(Key);
			trim(Value);

			m_Prefs[Key] = Value;
		}
		else
		{
			return Result::InvalidFormat;
		}
	}

	return Result::Success;
}

//-----------------------------------------------------------------------------

Preferences::Result Preferences::Save(const char* FileName)
{
	std::ofstream Output(FileName);

	if (!Output)
	{
		return Result::Failed;
	}

	Output.exceptions(std::ios::badbit | std::ios::failbit);

	try
	{
		Output << PREFS_TOKEN << "\n\n";

		for (const auto& i : m_Prefs)
		{
			Output << i.first << '=' << i.second << '\n';
		}
	}
	catch (const std::exception&)
	{
		return Result::Failed;
	}

	return Result::Success;
}

//-----------------------------------------------------------------------------

bool Preferences::GetBinaryValue(const char* id, void* Value, size_t Size) const
{
	bool found = true;

	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end())
	{
		const std::string& Data = i->second;

		if (Data.size() == Size * 2)
		{
			unsigned char* BinValue = reinterpret_cast<unsigned char *>(Value);

			for (size_t b = 0; b < Size; ++b)
			{
				BinValue[b] = (unsigned char)((Hex2Int(Data[b * 2]) << 4) | Hex2Int(Data[b * 2 + 1]));
			}
		}
		else
		{
			found = false;
		}
	}
	else
	{
		found = false;
	}

	return found;
}

//-----------------------------------------------------------------------------

void Preferences::SetBinaryValue(const char* id, const void* Value, size_t Size)
{
	assert(Size <= 512);

	char hx[1024];
	const unsigned char* BinValue = reinterpret_cast<const unsigned char *>(Value);

	for (size_t b = 0; b < Size; ++b)
	{
		sprintf(hx + b * 2, "%02x", static_cast<int>(BinValue[b]));
	}

	m_Prefs[id] = hx;
}

//-----------------------------------------------------------------------------

bool Preferences::GetStringValue(const char* id, char* Value) const
{
	bool found = true;

	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end())
	{
		strcpy(Value, i->second.c_str());
	}
	else
	{
		found = false;
	}

	return found;
}

//-----------------------------------------------------------------------------

void Preferences::SetStringValue(const char* id, const char* Value)
{
	m_Prefs[id] = Value;
}

//-----------------------------------------------------------------------------

bool Preferences::GetStringValue(const char* id, std::string& Value) const
{
	bool found = true;

	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end())
	{
		Value = i->second;
	}
	else
	{
		found = false;
	}

	return found;
}

//-----------------------------------------------------------------------------

bool Preferences::GetStringValue(const char* id, std::string& Value, const char* Default) const
{
	bool Found = GetStringValue(id, Value);

	if (!Found)
	{
		Value = Default;
	}

	return Found;
}

//-----------------------------------------------------------------------------

void Preferences::SetStringValue(const char* id, const std::string& Value)
{
	m_Prefs[id] = Value;
}

//-----------------------------------------------------------------------------

bool Preferences::GetDWORDValue(const char* id, DWORD& Value) const
{
	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i == m_Prefs.end())
	{
		return false;
	}

	try
	{
		unsigned long Num = std::stoul(i->second, nullptr, 16);

		if (Num <= std::numeric_limits<DWORD>::max())
		{
			Value = (DWORD)Num;
		}
		else
		{
			return false;
		}
	}
	catch (std::exception&)
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

bool Preferences::GetDWORDValue(const char* id, DWORD& Value, DWORD Default) const
{
	bool Found = GetDWORDValue(id, Value);

	if (!Found)
	{
		Value = Default;
	}

	return Found;
}

//-----------------------------------------------------------------------------

void Preferences::SetDWORDValue(const char* id, DWORD Value)
{
	char hx[10];
	sprintf(hx, "%08x", Value);
	m_Prefs[id] = hx;
}

//-----------------------------------------------------------------------------

bool Preferences::GetDecimalValue(const char* id, int& Value, int Default) const
{
	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i == m_Prefs.end())
	{
		Value = Default;
		return false;
	}

	if (ParseNumber(i->second, &Value))
	{
		return true;
	}

	Value = Default;

	return false;
}

//-----------------------------------------------------------------------------

bool Preferences::SetDecimalValue(const char* id, int Value)
{
	m_Prefs[id] = std::to_string(Value);

	return true;
}

//-----------------------------------------------------------------------------

bool Preferences::GetBoolValue(const char* id, bool& Value, bool Default) const
{
	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i == m_Prefs.end())
	{
		Value = Default;
		return false;
	}

	try
	{
		Value = std::stoi(i->second) != 0;
	}
	catch (std::exception&)
	{
		Value = Default;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

void Preferences::SetBoolValue(const char* id, bool Value)
{
	m_Prefs[id] = std::to_string(Value);
}

//-----------------------------------------------------------------------------

void Preferences::EraseValue(const char* id)
{
	m_Prefs.erase(id);
}

//-----------------------------------------------------------------------------

bool Preferences::HasValue(const char* id) const
{
	return m_Prefs.find(id) != m_Prefs.end();
}

//-----------------------------------------------------------------------------
