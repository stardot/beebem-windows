/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
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

#ifndef PORT_HEADER
#define PORT_HEADER

#include <limits.h>

/* Used for accelerating copies */
#ifdef WIN32
typedef __int64 EightByteType;	// $NRM for MSVC. Will it work though?
#else
typedef long long EightByteType;
#endif

/* Used to keep a count of total number of cycles executed */
typedef int CycleCountT;

#define CycleCountTMax INT_MAX
#define CycleCountWrap (INT_MAX / 2)

#define DEFAULTSAMPLERATE 40000

#endif

