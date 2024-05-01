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

class Preferences {

	public:
		enum class Result {
			Success,
			Failed,
			InvalidFormat
		};

	public:
		Result Load(const char *fileName);
		Result Save(const char *fileName);

		bool GetBinaryValue(const char *id, void *bin, size_t binsize);
		void SetBinaryValue(const char *id, const void *bin, size_t binsize);
		bool GetStringValue(const char *id, char *str);
		void SetStringValue(const char *id, const char *str);
		bool GetStringValue(const char *id, std::string& str);
		void SetStringValue(const char *id, const std::string& str);
		bool GetDWORDValue(const char *id, DWORD &dw);
		void SetDWORDValue(const char *id, DWORD dw);
		bool GetBoolValue(const char *id, bool &b);
		void SetBoolValue(const char *id, bool b);
		void EraseValue(const char *id);
		bool HasValue(const char *id);

	private:
		typedef std::map<std::string, std::string> PrefsMap;

		PrefsMap m_Prefs;
};

#endif
