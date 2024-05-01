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

#include "Preferences.h"

//-----------------------------------------------------------------------------

static const int MAX_PREFS_LINE_LEN = 1024;

// Token written to start of pref files
#define PREFS_TOKEN "*** BeebEm Preferences ***"

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

Preferences::Result Preferences::Load(const char *fileName)
{
	FILE *fd = fopen(fileName, "r");

	if (fd == nullptr) {
		return Result::Failed;
	}

	char buf[MAX_PREFS_LINE_LEN];

	if (fgets(buf, MAX_PREFS_LINE_LEN - 1, fd) != nullptr) {
		if (strcmp(buf, PREFS_TOKEN "\n") != 0) {
			fclose(fd);
			return Result::InvalidFormat;
		}
		else {
			while (fgets(buf, MAX_PREFS_LINE_LEN - 1, fd) != nullptr) {
				char *val = strchr(buf, '=');

				if (val) {
					*val = 0;
					++val;
					if (val[strlen(val) - 1] == '\n')
						val[strlen(val) - 1] = 0;
					m_Prefs[buf] = val;
				}
			}
		}

		fclose(fd);
	}

	return Result::Success;
}

//-----------------------------------------------------------------------------

Preferences::Result Preferences::Save(const char *fileName)
{
	// Write the file
	FILE *fd = fopen(fileName, "w");

	if (fd == nullptr) {
		return Result::Failed;
	}

	fprintf(fd, PREFS_TOKEN "\n\n");

	for (const auto& i : m_Prefs) {
		fprintf(fd, "%s=%s\n", i.first.c_str(), i.second.c_str());
	}

	fclose(fd);

	return Result::Success;
}

//-----------------------------------------------------------------------------

bool Preferences::GetBinaryValue(const char *id, void *bin, size_t binsize)
{
	bool found = true;

	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end())
	{
		const std::string& value = i->second;

		if (value.size() == binsize * 2)
		{
			unsigned char *binc = reinterpret_cast<unsigned char *>(bin);

			for (size_t b = 0; b < binsize; ++b)
			{
				binc[b] = (unsigned char)((Hex2Int(value[b * 2]) << 4) | Hex2Int(value[b * 2 + 1]));
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

void Preferences::SetBinaryValue(const char *id, const void *bin, size_t binsize)
{
	char hx[MAX_PREFS_LINE_LEN];
	const unsigned char *binc = reinterpret_cast<const unsigned char *>(bin);

	for (size_t b = 0; b < binsize; ++b) {
		sprintf(hx + b * 2, "%02x", static_cast<int>(binc[b]));
	}

	m_Prefs[id] = hx;
}

//-----------------------------------------------------------------------------

bool Preferences::GetStringValue(const char *id, char *str)
{
	bool found = true;

	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end()) {
		strcpy(str, i->second.c_str());
	}
	else {
		found = false;
	}

	return found;
}

//-----------------------------------------------------------------------------

void Preferences::SetStringValue(const char *id, const char *str)
{
	m_Prefs[id] = str;
}

//-----------------------------------------------------------------------------

bool Preferences::GetStringValue(const char *id, std::string& str)
{
	bool found = true;

	PrefsMap::const_iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end())
	{
		str = i->second;
	}
	else
	{
		found = false;
	}

	return found;
}

//-----------------------------------------------------------------------------

void Preferences::SetStringValue(const char *id, const std::string& str)
{
	m_Prefs[id] = str;
}

//-----------------------------------------------------------------------------

bool Preferences::GetDWORDValue(const char *id, DWORD &dw)
{
	bool found = true;

	PrefsMap::iterator i = m_Prefs.find(id);

	if (i != m_Prefs.end()) {
		sscanf(i->second.c_str(), "%x", &dw);
	}
	else {
		found = false;
	}

	return found;
}

//-----------------------------------------------------------------------------

void Preferences::SetDWORDValue(const char *id, DWORD dw)
{
	char hx[MAX_PREFS_LINE_LEN];
	sprintf(hx, "%08x", dw);
	m_Prefs[id] = hx;
}

//-----------------------------------------------------------------------------

bool Preferences::GetBoolValue(const char *id, bool &b)
{
	unsigned char c = 0;
	bool found = GetBinaryValue(id, &c, sizeof(c));

	b = c != 0;

	return found;
}

//-----------------------------------------------------------------------------

void Preferences::SetBoolValue(const char *id, bool b)
{
	unsigned char c = b;
	SetBinaryValue(id, &c, sizeof(c));
}

//-----------------------------------------------------------------------------

void Preferences::EraseValue(const char* id)
{
	m_Prefs.erase(id);
}

//-----------------------------------------------------------------------------

bool Preferences::HasValue(const char* id)
{
	return m_Prefs.find(id) != m_Prefs.end();
}

//-----------------------------------------------------------------------------
