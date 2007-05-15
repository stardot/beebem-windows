// Implementation of cRegistry class (I like lower case c's).
// This class provides easier way of accessing the registry from a c++ application.
// Written by David Overton (david@overton.org.uk -- NO SPAM).
// Use this code if you want to, i found it much easier to use the registry with it.
// Please don't remove this section from here, or the section in the header.
// 
// This code is not commented (as you can see) but it works, and it is pretty 
// self-evident anyway.
//
// Problems/Improvements: 
//
// * Not really a problem, but this code could be modified to return a more
//   meaningful value than true or false for each function.
// 
// * The string/binary functions need to be modified to allow more than MAX_BUFFER_SIZE
//   bytes to be read in at any one time (possibly pass buffer size as function param).
//
// * Doesn't provide network registry support.

#include "cRegistry.h"


bool cRegistry::CreateKey(HKEY hKeyRoot, LPSTR lpSubKey)
{
	HKEY hKeyResult;
	if(RegCreateKey(hKeyRoot, lpSubKey, &hKeyResult)==ERROR_SUCCESS)
		return true;
	return false;
}

bool cRegistry::DeleteKey(HKEY hKeyRoot, LPSTR lpSubKey)
{
	if(RegDeleteKey(hKeyRoot,lpSubKey)==ERROR_SUCCESS)
		return true;	
	return false;
}

bool cRegistry::DeleteValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValueName)
{
	return false;
}

bool cRegistry::GetBinaryValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, PVOID pData, int* pnSize)
{
	HKEY hKeyResult;
	DWORD dwType	= REG_BINARY;
	DWORD dwSize	= *pnSize;
	LONG  lRes		= 0;
	if((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult))==ERROR_SUCCESS) {
		if((lRes=RegQueryValueEx(hKeyResult,lpValue,NULL,&dwType,(LPBYTE)pData,&dwSize))==ERROR_SUCCESS) {
			if(dwType==REG_BINARY) {
				*pnSize=dwSize;
				RegCloseKey(hKeyResult);
				return true;
			}
		}
		RegCloseKey(hKeyResult);
	}
	return false;
}

bool cRegistry::GetDWORDValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, DWORD &dwBuffer)
{
	HKEY hKeyResult;
	DWORD dwType = REG_DWORD;
	DWORD dwBufferSize = MAX_BUFF_LENGTH;
	LONG  lRes;
	BYTE* pBytes = new BYTE[MAX_BUFF_LENGTH];
	ZeroMemory((void*)pBytes, MAX_BUFF_LENGTH );
	if((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult))==ERROR_SUCCESS) {
		if((lRes=RegQueryValueEx(hKeyResult,lpValue, NULL, &dwType, pBytes, &dwBufferSize))==ERROR_SUCCESS) {
			if(dwType==REG_DWORD) {
				DWORD* pDW = reinterpret_cast<DWORD*>(pBytes);
				dwBuffer = (*pDW);
				RegCloseKey(hKeyResult);
				delete[] pBytes;
				return 1;
			}
		}
		RegCloseKey(hKeyResult);
	}
	delete[] pBytes;
	return 0;
}

bool cRegistry::GetStringValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, LPSTR lpBuffer)
{
	HKEY hKeyResult;
	DWORD dwType = REG_SZ;
	DWORD dwBufferSize = MAX_BUFF_LENGTH;
	LONG  lRes;
	BYTE* pBytes = new BYTE[MAX_BUFF_LENGTH];
	ZeroMemory((void*)pBytes, MAX_BUFF_LENGTH );
	if((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult))==ERROR_SUCCESS) {
		if((lRes=RegQueryValueEx(hKeyResult,lpValue, NULL, &dwType, pBytes, &dwBufferSize))==ERROR_SUCCESS) {
			if(dwType==REG_SZ) {
				strcpy(lpBuffer,(char*)pBytes);
				RegCloseKey(hKeyResult);
				delete[] pBytes;
				return 1;
			}
		}
		RegCloseKey(hKeyResult);
	}
	delete[] pBytes;
	return 0;
}

bool cRegistry::SetBinaryValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, PVOID pData, int* pnSize)
{
	HKEY hKeyResult;
	DWORD dwType	= REG_BINARY;
    DWORD dwSize	= *pnSize;
	LONG  lRes		= 0;
	if((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult))==ERROR_SUCCESS) {
		if((lRes=RegSetValueEx(hKeyResult,lpValue,0,REG_BINARY,reinterpret_cast<BYTE*>(pData),dwSize))==ERROR_SUCCESS) {
			RegCloseKey(hKeyResult);
			*pnSize = dwSize;
			return true;
		}
		RegCloseKey(hKeyResult);
	}
	return false;
}

bool cRegistry::SetDWORDValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, DWORD dwValue)
{
	HKEY hKeyResult;
	DWORD dwType = REG_DWORD;
	DWORD dwBufferSize = MAX_BUFF_LENGTH;
	LONG  lRes;
	if((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult))==ERROR_SUCCESS) {
		if((lRes=RegSetValueEx(hKeyResult,lpValue,0,REG_DWORD,reinterpret_cast<BYTE*>(&dwValue),sizeof(DWORD)))==ERROR_SUCCESS) {
			RegCloseKey(hKeyResult);
			return true;
		}
		RegCloseKey(hKeyResult);
	}
	return false;
}

bool cRegistry::SetStringValue(HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, LPSTR lpData)
{
	HKEY hKeyResult;
	DWORD dwLength = (DWORD)(strlen(lpData) * sizeof(char));
	DWORD dwType = REG_SZ;
	DWORD dwBufferSize = MAX_BUFF_LENGTH;
	LONG  lRes;
	if((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult))==ERROR_SUCCESS) {
		if((lRes=RegSetValueEx(hKeyResult,lpValue,0,REG_SZ,reinterpret_cast<BYTE*>(lpData),dwLength))==ERROR_SUCCESS) {
			RegCloseKey(hKeyResult);
			return true;
		}
	}
	return false;
}