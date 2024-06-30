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

#ifndef MESSAGES_HEADER
#define MESSAGES_HEADER

#include <windows.h>

constexpr UINT WM_REINITDX                         = WM_APP;
constexpr UINT WM_USER_KEYBOARD_DIALOG_CLOSED      = WM_APP + 1;
constexpr UINT WM_SELECT_KEY_DIALOG_CLOSED         = WM_APP + 2;
constexpr UINT WM_CLEAR_KEY_MAPPING                = WM_APP + 3;
constexpr UINT WM_USER_PORT_BREAKOUT_DIALOG_CLOSED = WM_APP + 4;
constexpr UINT WM_DIRECTX9_DEVICE_LOST             = WM_APP + 5;
constexpr UINT WM_IP232_ERROR                      = WM_APP + 6;

// Menu item IDs
constexpr UINT IDM_TEXT_TO_SPEECH_VOICE_BASE = 50000;

#endif
