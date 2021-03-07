/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2021  Chris Needham

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
#include <stdarg.h>
#include <stdio.h>

#include "DebugTrace.h"

#if !defined(NDEBUG)

void DebugTrace(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	// Calculate required length, +1 is for NUL terminator
	const int length = _vscprintf(format, args) + 1;

	char *buffer = (char*)malloc(length);

	if (buffer != nullptr)
	{
		vsprintf(buffer, format, args);
		OutputDebugString(buffer);
		free(buffer);
	}

	va_end(args);
}

#endif
