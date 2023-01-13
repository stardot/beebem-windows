/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2009  Mike Wyatt

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
#include <commctrl.h>

#include "ListView.h"

/****************************************************************************/

int LVInsertColumn(HWND hWnd, UINT uCol, const char* pszText, int iAlignment, UINT uWidth)
{
	LVCOLUMN lc = { 0 };
	lc.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_FMT | LVCF_WIDTH;
	lc.fmt = iAlignment;
	lc.pszText = const_cast<char*>(pszText);
	lc.iSubItem = uCol;
	lc.cx = uWidth;
	return ListView_InsertColumn(hWnd, uCol, &lc);
}

int LVInsertItem(HWND hWnd, UINT uRow, UINT uCol, const char* pszText, LPARAM lParam)
{
	LVITEM li = { 0 };
	li.mask = LVIF_TEXT | LVIF_PARAM;
	li.iItem = uRow;
	li.iSubItem = uCol;
	li.pszText = const_cast<char*>(pszText);
	li.lParam = (lParam ? lParam : uRow);
	return ListView_InsertItem(hWnd, &li);
}

LPARAM LVGetItemData(HWND hWnd, UINT uRow)
{
	LVITEM li = { 0 };
	li.mask = LVIF_PARAM;
	li.iItem = uRow;
	ListView_GetItem(hWnd, &li);

	return li.lParam;
}

void LVSetItemText(HWND hWnd, UINT uRow, UINT uCol, const LPTSTR pszText)
{
	ListView_SetItemText(hWnd, uRow, uCol, pszText);
}

void LVSetFocus(HWND hWnd)
{
	int row = ListView_GetSelectionMark(hWnd);

	ListView_SetItemState(hWnd,
	                      row,
	                      LVIS_SELECTED | LVIS_FOCUSED,
	                      LVIS_SELECTED | LVIS_FOCUSED);

	SetFocus(hWnd);
}
