/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt
Copyright (C) 1998  Robert Schmidt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2005  Jon Welch

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

#ifndef PREFERENCES_HEADER
#define PREFERENCES_HEADER

#include <map>
#include <string>

class Preferences
{
	public:
		enum class Result {
			Success,
			Failed,
			InvalidFormat
		};

	public:
		Result Load(const char* FileName);
		Result Save(const char* FileName);

		bool GetBinaryValue(const char* id, void* Value, size_t Size) const;
		void SetBinaryValue(const char* id, const void* Value, size_t Size);
		bool GetStringValue(const char* id, char* Value) const;
		void SetStringValue(const char* id, const char* Value);
		bool GetStringValue(const char* id, std::string& Value) const;
		bool GetStringValue(const char* id, std::string& Value, const char* Default) const;
		void SetStringValue(const char* id, const std::string& Value);
		bool GetDWORDValue(const char* id, DWORD& Value) const;
		bool GetDWORDValue(const char* id, DWORD& Value, DWORD Default) const;
		void SetDWORDValue(const char* id, DWORD Value);
		bool GetDecimalValue(const char* id, int& Value, int Default) const;
		bool SetDecimalValue(const char* id, int Value);
		bool GetBoolValue(const char* id, bool& Value, bool Default) const;
		void SetBoolValue(const char* id, bool Value);
		void EraseValue(const char* id);
		bool HasValue(const char* id) const;

	private:
		typedef std::map<std::string, std::string> PrefsMap;

		PrefsMap m_Prefs;
};

#endif
