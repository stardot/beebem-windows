/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023  Chris Needham

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

#ifndef IC32LATCH_HEADER
#define IC32LATCH_HEADER

constexpr unsigned char IC32_SOUND_WRITE     = 0x01;
constexpr unsigned char IC32_SPEECH_READ     = 0x02; // BBC B only
constexpr unsigned char IC32_SPEECH_WRITE    = 0x04; // BBC B only
constexpr unsigned char IC32_RTC_READ        = 0x02; // Master only
constexpr unsigned char IC32_RTC_DATA_STROBE = 0x04; // Master only
constexpr unsigned char IC32_KEYBOARD_WRITE  = 0x08;
constexpr unsigned char IC32_SCREEN_ADDRESS  = 0x30;
constexpr unsigned char IC32_CAPS_LOCK       = 0x40;
constexpr unsigned char IC32_SHIFT_LOCK      = 0x80;

extern unsigned char IC32State;

#endif
