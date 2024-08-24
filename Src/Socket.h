/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2024  Chris Needham

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

#ifndef SOCKET_HEADER
#define SOCKET_HEADER

int CloseSocket(SOCKET Socket);
int GetLastSocketError();
bool SetSocketBlocking(SOCKET Socket, bool Blocking);
bool WouldBlock(int Error);

#define S_ADDR(s) (s).sin_addr.s_addr

#define IN_ADDR(addr) (addr).s_addr

#endif
