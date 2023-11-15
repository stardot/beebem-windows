/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Mike Wyatt

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

#include <string>

#include "TapeControlDialog.h"
#include "beebemrc.h"
#include "Main.h"
#include "Serial.h"

// Tape control dialog box variables
std::vector<TapeMapEntry> TapeMap;
bool TapeControlEnabled = false;
static HWND hwndTapeControl;
static HWND hwndMap;

static void TapeControlRecord();

static INT_PTR CALLBACK TapeControlDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

void TapeControlOpenDialog(HINSTANCE hinst, HWND /* hwndMain */)
{
	TapeControlEnabled = true;

	if (!IsWindow(hwndTapeControl))
	{
		hwndTapeControl = CreateDialog(hinst, MAKEINTRESOURCE(IDD_TAPECONTROL),
		                               NULL, TapeControlDlgProc);
		hCurrentDialog = hwndTapeControl;
		ShowWindow(hwndTapeControl, SW_SHOW);

		hwndMap = GetDlgItem(hwndTapeControl, IDC_TAPE_CONTROL_MAP);
		SendMessage(hwndMap, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
		            (LPARAM)MAKELPARAM(FALSE,0));

		int Time = SerialGetTapeClock();
		TapeControlAddMapLines(Time);
	}
}

void TapeControlCloseDialog()
{
	DestroyWindow(hwndTapeControl);
	hwndTapeControl = nullptr;
	hwndMap = nullptr;
	TapeControlEnabled = false;
	hCurrentDialog = nullptr;
}

void TapeControlAddMapLines(int Time)
{
	SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);

	for (const TapeMapEntry& line : TapeMap)
	{
		SendMessage(hwndMap, LB_ADDSTRING, 0, (LPARAM)line.desc.c_str());
	}

	TapeControlUpdateCounter(Time);
}

void TapeControlUpdateCounter(int tape_time)
{
	if (TapeControlEnabled)
	{
		size_t i = 0;

		while (i < TapeMap.size() && TapeMap[i].time <= tape_time)
			i++;

		if (i > 0)
			i--;

		SendMessage(hwndMap, LB_SETCURSEL, (WPARAM)i, 0);
	}
}

INT_PTR CALLBACK TapeControlDlgProc(HWND /* hwndDlg */, UINT message, WPARAM wParam, LPARAM /* lParam */)
{
	switch (message)
	{
		case WM_INITDIALOG:
			return TRUE;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				hCurrentDialog = nullptr;
			}
			else
			{
				hCurrentDialog = hwndTapeControl;
			}
			return FALSE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_TAPE_CONTROL_MAP:
					if (HIWORD(wParam) == LBN_SELCHANGE)
					{
						LRESULT s = SendMessage(hwndMap, LB_GETCURSEL, 0, 0);

						if (s != LB_ERR && s >= 0 && s < (int)TapeMap.size())
						{
							SetTapePosition(TapeMap[s].time);
						}
					}
					return FALSE;

				case IDC_TAPE_CONTROL_PLAY:
					SerialStopTapeRecording(true);
					SerialPlayTape();
					return TRUE;

				case IDC_TAPE_CONTROL_STOP:
					SerialStopTapeRecording(true);
					SerialStopTape();
					return TRUE;

				case IDC_TAPE_CONTROL_EJECT:
					SerialStopTapeRecording(false);
					SerialEjectTape();
					return TRUE;

				case IDC_TAPE_CONTROL_RECORD:
					TapeControlRecord();
					return TRUE;

				case IDCANCEL:
					TapeControlCloseDialog();
					return TRUE;
			}
	}

	return FALSE;
}

void TapeControlRecord()
{
	if (!TapeRecording)
	{
		// Query for new file name
		char FileName[_MAX_PATH];
		FileName[0] = '\0';

		if (mainWin->NewTapeImage(FileName, sizeof(FileName)))
		{
			CloseTape();

			// Create file
			if (!SerialRecordTape(FileName))
			{
				mainWin->Report(MessageType::Error,
				                "Error creating tape file:\n  %s", FileName);
			}
		}
	}
}

void TapeControlCloseTape()
{
	SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);
}
