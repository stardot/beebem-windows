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

#include "IC32Latch.h"

// State of the 8-bit latch IC32
// bit 0 is WE for sound gen
// bit 1 is read select on speech proc
// bit 2 is write select on speech proc
// bit 4 and bit 5 screen start address offset
// bit 6 is caps lock LED
// bit 7 is shift lock LED

unsigned char IC32State = 0;
