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

#include "filedialog.h"

FileDialog::FileDialog(HWND hwndOwner, LPTSTR result, DWORD resultLength,
                       LPCTSTR initialFolder, LPCTSTR filter)
{
	memset(&m_ofn, 0, sizeof(m_ofn));

	m_ofn.lStructSize = sizeof(OPENFILENAME);
	m_ofn.hwndOwner = hwndOwner;
	m_ofn.lpstrFilter = filter;
	m_ofn.nFilterIndex = 1;
	m_ofn.lpstrFile = result;
	m_ofn.nMaxFile = resultLength;
	m_ofn.lpstrInitialDir = initialFolder;
	m_ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
}

bool FileDialog::ShowDialog(bool open)
{
	m_ofn.lpstrFile[0] = '\0';

	if (open)
	{
		return GetOpenFileName(&m_ofn) != 0;
	}
	else
	{
		m_ofn.Flags |= OFN_OVERWRITEPROMPT;

		return GetSaveFileName(&m_ofn) != 0;
	}
}
