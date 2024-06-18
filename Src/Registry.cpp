/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  David Overton

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

#include "Registry.h"

/****************************************************************************/

bool RegCreateKey(HKEY hKeyRoot, LPCSTR lpSubKey)
{
	bool Result = false;
	HKEY hKeyResult;

	if ((RegCreateKeyEx(hKeyRoot, lpSubKey, 0, NULL, 0, KEY_ALL_ACCESS,
	                    NULL, &hKeyResult, NULL)) == ERROR_SUCCESS)
	{
		RegCloseKey(hKeyResult);
		Result = true;
	}

	return Result;
}

/****************************************************************************/

bool RegGetBinaryValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                       void* pData, int* pnSize)
{
	bool Result = false;
	HKEY hKeyResult;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		DWORD dwType = REG_BINARY;
		DWORD dwSize = *pnSize;

		LONG lRes = RegQueryValueEx(hKeyResult, lpValue, NULL, &dwType, (LPBYTE)pData, &dwSize);

		if (lRes == ERROR_SUCCESS && dwType == REG_BINARY)
		{
			*pnSize = dwSize;
			Result = true;
		}

		RegCloseKey(hKeyResult);
	}

	return Result;
}

/****************************************************************************/

bool RegSetBinaryValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                       const void* pData, int* pnSize)
{
	bool Result = false;
	HKEY hKeyResult;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		DWORD dwSize = *pnSize;

		LONG lRes = RegSetValueEx(hKeyResult, lpValue, 0, REG_BINARY,
		                          reinterpret_cast<const BYTE*>(pData), dwSize);

		if (lRes == ERROR_SUCCESS)
		{
			*pnSize = dwSize;
			Result = true;
		}

		RegCloseKey(hKeyResult);
	}

	return Result;
}

/****************************************************************************/

bool RegGetStringValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                       LPSTR pData, DWORD dwSize)
{
	bool Result = false;
	HKEY hKeyResult;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		DWORD dwType = REG_SZ;

		LONG lRes = RegQueryValueEx(hKeyResult, lpValue, NULL, &dwType,
		                            (LPBYTE)pData, &dwSize);

		if (lRes == ERROR_SUCCESS && dwType == REG_SZ)
		{
			Result = true;
		}

		RegCloseKey(hKeyResult);
	}

	return Result;
}

/****************************************************************************/

bool RegSetStringValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                       LPCSTR pData)
{
	bool Result = false;
	HKEY hKeyResult;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		DWORD dwSize = (DWORD)strlen(pData);

		LONG lRes = RegSetValueEx(hKeyResult, lpValue, 0, REG_SZ,
		                          reinterpret_cast<const BYTE*>(pData), dwSize);

		if (lRes == ERROR_SUCCESS)
		{
			Result = true;
		}

		RegCloseKey(hKeyResult);
	}

	return Result;
}

/****************************************************************************/
