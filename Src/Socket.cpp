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

#include <windows.h>

#include "Socket.h"

#ifndef WIN32
#include <errno.h>
#include <unistd.h>
#endif

/****************************************************************************/

int CloseSocket(SOCKET Socket)
{
	#ifdef WIN32

	return closesocket(Socket);

	#else

	return close(Socket);

	#endif
}

/****************************************************************************/

int GetLastSocketError()
{
	#ifdef WIN32

	return WSAGetLastError();

	#else

	return errno;

	#endif
}

/****************************************************************************/

bool SetSocketBlocking(SOCKET Socket, bool Blocking)
{
	#ifdef WIN32

	unsigned long Mode = Blocking ? 0 : 1;
	return ioctlsocket(Socket, FIONBIO, &Mode) == 0;

	#else

	int Flags = fcntl(Socket, F_GETFL, 0);
	if (Flags == -1) return false;

	if (Blocking)
	{
		Flags &= ~O_NONBLOCK;
	}
	else
	{
		Flags |= O_NONBLOCK
	}

	return fcntl(Socket, F_SETFL, Flags) == 0;

	#endif
}

/****************************************************************************/

bool WouldBlock(int Error)
{
#ifdef WIN32

	return Error == WSAEWOULDBLOCK;

#else

	return Error == EWOULDBLOCK; // TODO: EAGAIN?

#endif
}

/****************************************************************************/
