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

#ifndef FOLDER_SELECT_DIALOG_HEADER
#define FOLDER_SELECT_DIALOG_HEADER

#include <string>

class FolderSelectDialog
{
	public:
		enum class Result {
			OK,
			Cancel,
			InvalidFolder
		};

	public:
		FolderSelectDialog(
			HWND hwndOwner,
			const char *Title,
			const char *InitialFolder
		);

	public:
		Result DoModal();

		std::string GetFolder() const;

	private:
		static int CALLBACK BrowseCallbackProc(
			HWND hWnd,
			UINT uMsg,
			LPARAM lParam,
			LPARAM lpData
		);

	private:
		BROWSEINFO m_BrowseInfo;
		char m_Buffer[MAX_PATH];
		std::string m_InitialFolder;
		std::string m_Title;
};

#endif
