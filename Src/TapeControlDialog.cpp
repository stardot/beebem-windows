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
#include "Main.h"
#include "Resource.h"
#include "Serial.h"
#include "WindowUtils.h"

// Tape control dialog box variables
std::vector<TapeMapEntry> TapeMap;
bool TapeControlEnabled = false;
static HWND hwndTapeControl;
static HWND hwndMap;

static INT_PTR CALLBACK TapeControlDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

/****************************************************************************/

static void TapeControlNewTape();
static void TapeControlEjectTape();
static void UpdateState(HWND hwndDlg);

/****************************************************************************/

void TapeControlOpenDialog(HINSTANCE hinst, HWND /* hwndMain */)
{
	TapeControlEnabled = true;

	if (!IsWindow(hwndTapeControl))
	{
		hwndTapeControl = CreateDialog(hinst, MAKEINTRESOURCE(IDD_TAPECONTROL),
		                               nullptr, TapeControlDlgProc);
		hCurrentDialog = hwndTapeControl;

		DisableRoundedCorners(hwndTapeControl);
		ShowWindow(hwndTapeControl, SW_SHOW);

		hwndMap = GetDlgItem(hwndTapeControl, IDC_TAPE_CONTROL_MAP);
		SendMessage(hwndMap, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
		            (LPARAM)MAKELPARAM(FALSE,0));

		int Time = SerialGetTapeClock();
		TapeControlAddMapLines();
		TapeControlUpdateCounter(Time);
	}
}

/****************************************************************************/

void TapeControlCloseDialog()
{
	DestroyWindow(hwndTapeControl);
	hwndTapeControl = nullptr;
	hwndMap = nullptr;
	TapeControlEnabled = false;
	hCurrentDialog = nullptr;
}

/****************************************************************************/

void TapeControlAddMapLines()
{
	SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);

	for (const TapeMapEntry& line : TapeMap)
	{
		SendMessage(hwndMap, LB_ADDSTRING, 0, (LPARAM)line.desc.c_str());
	}

	UpdateState(hwndTapeControl);
}

/****************************************************************************/

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

/****************************************************************************/

static void EnableDlgItem(HWND hDlg, UINT nIDDlgItem, bool Enable)
{
	EnableWindow(GetDlgItem(hDlg, nIDDlgItem), Enable);
}

/****************************************************************************/

static bool IsDlgItemChecked(HWND hDlg, UINT nIDDlgItem)
{
	return SendDlgItemMessage(hDlg, nIDDlgItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/****************************************************************************/

static void SetDlgItemChecked(HWND hDlg, UINT nIDDlgItem, bool Checked)
{
	SendDlgItemMessage(hDlg, nIDDlgItem, BM_SETCHECK, Checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

/****************************************************************************/

static void UpdateState(HWND hwndDlg)
{
	SetFocus(hwndDlg);

	SerialTapeState State = SerialGetTapeState();

	UINT nIDCheckButton;

	switch (State)
	{
		case SerialTapeState::Playing:
			nIDCheckButton = IDC_PLAYING;

			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_PLAY, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_STOP, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_EJECT, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_REWIND, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_LOAD_TAPE, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_NEW_TAPE, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_RECORD, false);
			break;

		case SerialTapeState::Recording:
			nIDCheckButton = IDC_RECORDING;

			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_PLAY, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_STOP, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_EJECT, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_REWIND, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_LOAD_TAPE, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_NEW_TAPE, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_RECORD, false);
			break;

		case SerialTapeState::Stopped:
			nIDCheckButton = IDC_STOPPED;

			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_PLAY, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_STOP, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_EJECT, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_REWIND, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_LOAD_TAPE, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_NEW_TAPE, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_RECORD, SerialTapeIsUef());
			break;

		case SerialTapeState::NoTape:
		default:
			nIDCheckButton = IDC_STOPPED;

			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_PLAY, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_STOP, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_EJECT, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_REWIND, false);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_LOAD_TAPE, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_NEW_TAPE, true);
			EnableDlgItem(hwndDlg, IDC_TAPE_CONTROL_RECORD, false);
			break;
	}

	mainWin->EnableSaveState(State != SerialTapeState::Recording);

	CheckRadioButton(hwndDlg,
	                 IDC_PLAYING,
	                 IDC_STOPPED,
	                 nIDCheckButton);
}

/****************************************************************************/

INT_PTR CALLBACK TapeControlDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM /* lParam */)
{
	switch (message)
	{
		case WM_INITDIALOG:
			SetDlgItemText(hwndDlg, IDC_TAPE_FILENAME, TapeFileName);
			UpdateState(hwndDlg);
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
					SerialStopTapeRecording();
					SerialPlayTape();
					UpdateState(hwndDlg);
					return TRUE;

				case IDC_TAPE_CONTROL_STOP:
					if (TapeState.Recording)
					{
						SerialStopTapeRecording();
						SerialUpdateTapeClock();

						if (SerialTapeIsModified())
						{
							UEFResult Result = UEFFile.Save(TapeFileName);

							if (Result != UEFResult::Success)
							{
								mainWin->Report(MessageType::Error,
								                "Failed to write to tape file:\n %s", TapeFileName);
							}

							UEFFile.CreateTapeMap(TapeMap);
							TapeControlAddMapLines();
						}
					}

					SerialStopTape();
					UpdateState(hwndDlg);
					return TRUE;

				case IDC_TAPE_CONTROL_EJECT:
					TapeControlEjectTape();
					UpdateState(hwndDlg);
					return TRUE;

				case IDC_TAPE_CONTROL_REWIND:
					RewindTape();
					UpdateState(hwndDlg);
					return TRUE;

				case IDC_TAPE_CONTROL_LOAD_TAPE:
					mainWin->LoadTape();
					return TRUE;

				case IDC_TAPE_CONTROL_NEW_TAPE:
					TapeControlNewTape();
					TapeControlSetFileName("(Untitled)");
					UEFFile.CreateTapeMap(TapeMap);
					TapeControlAddMapLines();
					UpdateState(hwndDlg);
					return TRUE;

				case IDC_TAPE_CONTROL_RECORD:
					SerialRecordTape();
					UpdateState(hwndDlg);
					return TRUE;

				case IDC_TAPE_CONTROL_UNLOCK: {
					bool Unlock = IsDlgItemChecked(hwndDlg, IDC_TAPE_CONTROL_UNLOCK);
					mainWin->SetUnlockTape(Unlock);
					return TRUE;
				}

				case IDCANCEL:
					TapeControlCloseDialog();
					return TRUE;
			}
	}

	return FALSE;
}

/****************************************************************************/

static void TapeControlEjectTape()
{
	SerialEjectTape();
	TapeControlSetFileName("");
}

/****************************************************************************/

static void TapeControlNewTape()
{
	mainWin->NewTape(TapeFileName, sizeof(TapeFileName));
}

/****************************************************************************/

void TapeControlCloseTape()
{
	SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);
	UpdateState(hwndTapeControl);
}

/****************************************************************************/

void TapeControlSetFileName(const char *FileName)
{
	SetDlgItemText(hwndTapeControl, IDC_TAPE_FILENAME, FileName);
}

/****************************************************************************/

void TapeControlSetUnlock(bool Unlock)
{
	SetDlgItemChecked(hwndTapeControl, IDC_TAPE_CONTROL_UNLOCK, Unlock);
}

/****************************************************************************/
