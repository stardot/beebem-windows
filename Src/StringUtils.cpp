/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2020  Chris Needham

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

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <locale>

#include "StringUtils.h"

static void trimLeft(std::string& str)
{
	auto pos = std::find_if(str.begin(), str.end(), [](int ch) {
		return !std::isspace(ch);
	});

	str.erase(str.begin(), pos);
}

static void trimRight(std::string& str)
{
	auto pos = std::find_if(str.rbegin(), str.rend(), [](int ch) {
		return !std::isspace(ch);
	});

	str.erase(pos.base(), str.end());
}

void trim(std::string& str)
{
	trimLeft(str);
	trimRight(str);
}

bool ParseNumber(const std::string& str, int* pValue)
{
	try
	{
		*pValue = std::stoi(str);
	}
	catch (std::exception&)
	{
		return false;
	}

	return true;
}

bool StringEndsWith(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() &&
		str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string WStr2Str(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> Converter;

	return Converter.to_bytes(str);
}

std::wstring Str2WStr(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> Converter;

	return Converter.from_bytes(str);
}
