/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt
Copyright (C) 1998  Robert Schmidt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2005  Jon Welch
Copyright (C) 2021  Chris Needham, Tadeusz Kijkowski

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

#ifndef JOYSTICKHANDLER_H
#define JOYSTICKHANDLER_H

#include <vector>

#include "beebwin.h"

/* Max number of joysticks in joystickapi */
#define MAX_JOYSTICK_DEVS       16
/* Max number of entries on joystick order list */
#define MAX_JOYSTICK_ORDER      16

struct JoystickId : std::pair<int, int>
{
    using std::pair<int, int>::pair;

    // Manufacturer ID aka Vendor ID
    int& mId() { return first; }
    // Product ID
    int& pId() { return second; }
};

struct JoystickOrderEntry : JoystickId
{
    std::string   Name{};
    int           JoyIndex{ -1 };

    JoystickOrderEntry() = default;
    JoystickOrderEntry(JoystickId id, const std::string& name, int joyIndex) :
	JoystickId(id), Name(name), JoyIndex(joyIndex) {}
    JoystickOrderEntry(int mid, int pid, const std::string& name) :
	JoystickId(mid, pid), Name(name) {}

    std::string to_string();
    bool from_string(const std::string&);
};

struct JoystickDev
{
    JOYCAPS      Caps{};
    JOYINFOEX    InfoEx{};
    int          Instance{ 0 };
    int          Order{ -1 };
    int          JoyIndex{ -1 };
    bool         Configured{ false };
    bool         Present{ false };

    JoystickId   Id() { return JoystickId{ Caps.wMid, Caps.wPid }; }
    std::string  DisplayString();
    bool         Update();
    DWORD        GetButtons();
    DWORD        GetAxesState(int threshold);
    void         GetAxesValue(int axesSet, int& x, int& y);
};

struct PCJoystickState
{
    JoystickDev* Dev{ nullptr };
    int           JoyIndex{ -1 };
    bool          Captured{ false };
    unsigned int  PrevAxes{ 0 };
    unsigned int  PrevBtns{ 0 };
    bool          JoystickToKeysActive{ false };
};

struct JoystickHandler
{
    JoystickDev       m_JoystickDevs[MAX_JOYSTICK_DEVS];
    PCJoystickState   m_PCJoystickState[NUM_PC_JOYSTICKS];
    std::vector<JoystickOrderEntry> m_JoystickOrder;

    void         ScanJoysticks(void);

    JoystickHandler() {}
    ~JoystickHandler() {}
};

#endif
