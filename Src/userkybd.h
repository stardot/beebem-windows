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

#ifndef USERKYBD_HEADER
#define USERKYBD_HEADER

#include <windows.h>

// Public declarations.

struct BBCKey
{
    unsigned int ctrlId;
    const char* name;
    int row;
    int column;
};

const BBCKey* GetBBCKeyByName(const std::string& name);
const BBCKey* GetBBCKeyByRowAndCol(int row, int col);

bool UserKeyboardDialog(HWND hwndParent, bool joystick);

extern KeyMap UserKeymap;
extern JoyMap JoystickMap;

#endif
