/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt

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

#include "FileDialog.h"

/****************************************************************************/

FileDialog::FileDialog(HWND hwndOwner, LPTSTR Result, DWORD ResultLength,
                       LPCTSTR InitialFolder, LPCTSTR Filter)
{
	ZeroMemory(&m_ofn, sizeof(m_ofn));

	m_ofn.lStructSize = sizeof(OPENFILENAME);
	m_ofn.hwndOwner = hwndOwner;
	m_ofn.lpstrFilter = Filter;
	m_ofn.nFilterIndex = 1;
	m_ofn.lpstrFile = Result;
	m_ofn.nMaxFile = ResultLength;
	m_ofn.lpstrInitialDir = InitialFolder;
	m_ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
	              OFN_OVERWRITEPROMPT;
}

/****************************************************************************/

void FileDialog::SetFilterIndex(DWORD Index)
{
	m_ofn.nFilterIndex = Index;
}

void FileDialog::AllowMultiSelect()
{
	m_ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
}

void FileDialog::NoOverwritePrompt()
{
	m_ofn.Flags &= ~OFN_OVERWRITEPROMPT;
}

void FileDialog::SetTitle(LPCTSTR Title)
{
	m_ofn.lpstrTitle = Title;
}

/****************************************************************************/

bool FileDialog::Open()
{
	return ShowDialog(true);
}

bool FileDialog::Save()
{
	return ShowDialog(false);
}

/****************************************************************************/

DWORD FileDialog::GetFilterIndex() const
{
	return m_ofn.nFilterIndex;
}

/****************************************************************************/

bool FileDialog::ShowDialog(bool Open)
{
	if (Open)
	{
		return GetOpenFileName(&m_ofn) != 0;
	}
	else
	{
		return GetSaveFileName(&m_ofn) != 0;
	}
}

/****************************************************************************/
