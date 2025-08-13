/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2024  Chris Needham

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

#include "WindowUtils.h"

/****************************************************************************/

typedef HRESULT(STDAPICALLTYPE* DWM_SET_WINDOW_ATTRIBUTE)(HWND, DWORD, LPCVOID, DWORD);

static HMODULE hDwmApi = nullptr;
static DWM_SET_WINDOW_ATTRIBUTE DwmSetWindowAttribute = nullptr;

/****************************************************************************/

void InitWindowUtils()
{
	hDwmApi = LoadLibrary("dwmapi.dll");

	if (hDwmApi != nullptr)
	{
		DwmSetWindowAttribute = reinterpret_cast<DWM_SET_WINDOW_ATTRIBUTE>(
			GetProcAddress(hDwmApi, "DwmSetWindowAttribute")
		);
	}
}

/****************************************************************************/

void ExitWindowUtils()
{
	if (hDwmApi != nullptr)
	{
		FreeLibrary(hDwmApi);

		hDwmApi = nullptr;
		DwmSetWindowAttribute = nullptr;
	}
}

/****************************************************************************/

void CentreWindow(HWND hWndParent, HWND hWnd)
{
	if (hWndParent == nullptr || hWnd == nullptr)
	{
		return;
	}

	RECT rcParent;
	GetWindowRect(hWndParent, &rcParent);

	RECT rcWindow;
	GetWindowRect(hWnd, &rcWindow);

	RECT rc;
	CopyRect(&rc, &rcParent);

	// Offset the owner and dialog box rectangles so that right and bottom
	// values represent the width and height, and then offset the owner again
	// to discard space taken up by the dialog box.

	OffsetRect(&rcWindow, -rcWindow.left, -rcWindow.top);
	OffsetRect(&rc, -rc.left, -rc.top);
	OffsetRect(&rc, -rcWindow.right, -rcWindow.bottom);

	// The new position is the sum of half the remaining space and the owner's
	// original position.

	SetWindowPos(hWnd,
	             HWND_TOP,
	             rcParent.left + (rc.right / 2),
	             rcParent.top + (rc.bottom / 2),
	             0, 0, // Ignores size arguments.
	             SWP_NOSIZE);
}

/****************************************************************************/

void DisableRoundedCorners(HWND hWnd)
{
	if (DwmSetWindowAttribute != nullptr)
	{
		const DWORD DWMWCP_DONOTROUND = 1;
		const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
		const DWORD CornerPreference = DWMWCP_DONOTROUND;

		DwmSetWindowAttribute(hWnd,
		                      DWMWA_WINDOW_CORNER_PREFERENCE,
		                      &CornerPreference,
		                      sizeof(CornerPreference));
	}
}

/****************************************************************************/

// Resizes a window to a given client area width and height, and optionally
// sets the window position.
//
// Returns true if the window was resized to the given client area width and
// height, or false otherwise. The return value may be false if the menu now
// has a different number of rows, the client area size may not be right,
// so call SetWindowClientSize() again.

bool SetWindowClientSize(HWND hWnd, const WindowPos* pWindowPos)
{
	// Get current client area size.
	RECT ClientRect;
	GetClientRect(hWnd, &ClientRect);

	int ClientWidth = ClientRect.right - ClientRect.left;
	int ClientHeight = ClientRect.bottom - ClientRect.top;

	if (pWindowPos->UseCurrentWindowPos)
	{
		if (ClientWidth == pWindowPos->ClientWidth &&
		    ClientHeight == pWindowPos->ClientHeight)
		{
			return true; // Nothing to do.
		}
	}

	// Get window size.
	RECT WindowRect;
	GetWindowRect(hWnd, &WindowRect);

	int WindowWidth = WindowRect.right - WindowRect.left;
	int WindowHeight = WindowRect.bottom - WindowRect.top;

	// Compute the difference (non-client area)
	int ExtraWidth = WindowWidth - ClientWidth;
	int ExtraHeight = WindowHeight - ClientHeight;

	// Compute new window size to match desired client area
	int NewWindowWidth = pWindowPos->ClientWidth + ExtraWidth;
	int NewWindowHeight = pWindowPos->ClientHeight + ExtraHeight;

	int WindowX = pWindowPos->UseCurrentWindowPos ? WindowRect.left : pWindowPos->WindowX;
	int WindowY = pWindowPos->UseCurrentWindowPos ? WindowRect.top : pWindowPos->WindowY;

	// Resize the window
	SetWindowPos(hWnd,
	             HWND_NOTOPMOST,
	             WindowX,
	             WindowY,
	             NewWindowWidth,
	             NewWindowHeight,
	             SWP_NOZORDER | SWP_NOACTIVATE);

	// Now check to see if the target client area size was achieved.
	GetClientRect(hWnd, &ClientRect);

	ClientWidth = ClientRect.right - ClientRect.left;
	ClientHeight = ClientRect.bottom - ClientRect.top;

	return ClientWidth == pWindowPos->ClientWidth &&
	       ClientHeight == pWindowPos->ClientHeight;
}

/****************************************************************************/
