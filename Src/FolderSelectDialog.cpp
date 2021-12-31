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

#include <windows.h>

#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)

#include "FolderSelectDialog.h"

int CALLBACK FolderSelectDialog::BrowseCallbackProc(
	HWND hWnd,
	UINT uMsg,
	LPARAM /* lParam */,
	LPARAM lpData)
{
	FolderSelectDialog* pDialog = reinterpret_cast<FolderSelectDialog *>(lpData);

	if (pDialog != nullptr)	{
		switch (uMsg) {
			case BFFM_INITIALIZED:
				if (!pDialog->m_InitialFolder.empty()) {
					SendMessage(
						hWnd,
						BFFM_SETEXPANDED,
						TRUE, // lParam contains a path name, not a pidl.
						(LPARAM)pDialog->m_InitialFolder.c_str()
					);

					SendMessage(
						hWnd,
						BFFM_SETSELECTION,
						TRUE, // lParam contains a path name, not a pidl.
						(LPARAM)pDialog->m_InitialFolder.c_str()
					);
				}
				break;
		}
	}

	return 0;
}

FolderSelectDialog::FolderSelectDialog(
	HWND hwndOwner,
	const char *Title,
	const char *InitialFolder) :
	m_Title(Title),
	m_InitialFolder(InitialFolder)
{
	m_BrowseInfo.hwndOwner      = hwndOwner;
	m_BrowseInfo.pidlRoot       = NULL;
	m_BrowseInfo.pszDisplayName = m_Buffer;
	m_BrowseInfo.lpszTitle      = m_Title.empty() ? NULL : m_Title.c_str();
	m_BrowseInfo.ulFlags        = BIF_EDITBOX | BIF_NEWDIALOGSTYLE |
	                              BIF_RETURNONLYFSDIRS | BIF_RETURNFSANCESTORS;
	m_BrowseInfo.lpfn           = BrowseCallbackProc;
	m_BrowseInfo.lParam         = reinterpret_cast<LPARAM>(this);

	m_Buffer[0] = 0;
}

FolderSelectDialog::Result FolderSelectDialog::DoModal()
{
	Result result = Result::Cancel;

	LPITEMIDLIST pidl = SHBrowseForFolder(&m_BrowseInfo);

	if (pidl != nullptr) {
		if (SHGetPathFromIDList(pidl, m_Buffer)) {
			result = Result::OK;
		}
		else {
			m_Buffer[0] = 0;
			result = Result::InvalidFolder;
		}

		CoTaskMemFree(pidl);
	}

	return result;
}

std::string FolderSelectDialog::GetFolder() const
{
	return m_Buffer;
}
