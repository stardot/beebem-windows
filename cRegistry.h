// This class is an easier way of accessing the registry from a c++ application.
// By David Overton (david@overton.org.uk (NO SPAM)).
// See cRegistry.cpp for more comments and stuff.

#if !defined(_cRegistry_H_)
#define _cRegistry_H_

#include <windows.h>
#define MAX_BUFF_LENGTH 1024 // maximum length of data (in bytes) that you may read in.

class cRegistry {
public:
	bool CreateKey(HKEY hKeyRoot, LPSTR lpSubKey);
	bool DeleteKey(HKEY hKeyRoot, LPSTR lpSubKey);

	bool DeleteValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValueName);

	bool GetBinaryValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, PVOID pData, int* pnSize);
	bool GetDWORDValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, DWORD &dwBuffer);
	bool GetStringValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, LPSTR lpBuffer);

	bool SetBinaryValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, PVOID pData, int* pnSize);
	bool SetDWORDValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, DWORD dwValue);
	bool SetStringValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, LPSTR lpData);
};


#endif