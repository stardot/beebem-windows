/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023 Chris Needham

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

#ifndef FILE_UTILS_HEADER
#define FILE_UTILS_HEADER

#include <string>

bool FileExists(const char* PathName);
bool FolderExists(const char* PathName);

std::string AppendPath(const std::string& BasePath, const std::string& Path);

bool HasFileExt(const char* FileName, const char* Ext);
std::string ReplaceFileExt(const std::string& FileName, const char* Ext);
void GetPathFromFileName(const char* FileName, char* Path, size_t Size);
const char* GetFileNameFromPath(const char* PathName);
void MakeFileName(char* Path, size_t Size, const char* DirName, const char* FileName, ...);
void MakePreferredPath(char* PathName);
void AppendPath(char* pszPath, const char* pszPathToAppend);
bool IsRelativePath(const char* pszPath);

/*----------------------------------------------------------------------------*/

#endif
