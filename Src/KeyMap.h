/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997  Laurie Whiffen

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

#ifndef KEYMAP_HEADER
#define KEYMAP_HEADER

#include <string>

#include "BeebWin.h"

struct KeyMapping {
	int row;    // Beeb row
	int col;    // Beeb col
	bool shift; // Beeb shift state
};

constexpr int KEYMAP_SIZE = 256;

typedef KeyMapping KeyMap[KEYMAP_SIZE][2]; // Indices are: [Virt key][shift state]

void InitKeyMap();

bool ReadKeyMap(const char *filename, KeyMap *keymap);
bool WriteKeyMap(const char *filename, KeyMap *keymap);

const char* GetPCKeyName(int PCKey);

void SetUserKeyMapping(int Row, int Column, bool BBCShift, int PCKey, bool PCShift);
void ClearUserKeyMapping(int Row, int Column, bool Shift);
std::string GetKeysUsed(int Row, int Column, bool Shift);

extern KeyMap DefaultKeyMap;
extern KeyMap LogicalKeyMap;
extern KeyMap UserKeyMap;

extern const KeyMap *transTable;

#endif
