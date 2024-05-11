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

#include "Model.h"

static const char* const szModel[] = {
	"BBC Model B",
	"BBC Model B + Integra-B",
	"BBC Model B Plus",
	"BBC Master 128",
	"BBC Master ET",
	"BBC Master Compact",
	"Filestore E01",
	"Filestore E01S",
};

const char* GetModelName(Model model)
{
	return szModel[static_cast<int>(model)];
}
