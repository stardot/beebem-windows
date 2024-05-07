/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2017  Chris Needham

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

#ifndef MODEL_HEADER
#define MODEL_HEADER

enum class Model : unsigned char {
	B,         // 0: BBC B
	IntegraB,  // 1: BBC B with Integra B
	BPlus,     // 2: BBC B+
	Master128, // 3: BBC Master 128
	Last
};

const char* GetModelName(Model model);

#endif
