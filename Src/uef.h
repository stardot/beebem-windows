/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Mike Wyatt

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
#ifndef _UEF_H
#define _UEF_H

extern int uef_errno;

/* some defines related to the status byte - these may change! */

#define UEF_MMASK			(3 << 16)
#define UEF_STARTBIT		(2 << 8)
#define UEF_STOPBIT			(1 << 8)
#define UEF_BYTEMASK		0xff

/* some macros for reading parts of the status byte */

#define UEF_HTONE			(0 << 16)
#define UEF_DATA			(1 << 16)
#define UEF_GAP				(2 << 16)
#define UEF_EOF				(3 << 16)

#define UEFRES_TYPE(x)		(x&UEF_MMASK)
#define UEFRES_BYTE(x)		(x&UEF_BYTEMASK)
#define UEFRES_10BIT(x)		(((x&UEF_BYTEMASK) << 1) | ((x&UEF_STARTBIT) ? 1 : 0) | ((x&UEF_STOPBIT) ? 0x200 : 0))
#define UEFRES_STARTBIT(x)	(x&UEF_STARTBIT)
#define UEFRES_STOPBIT(x)	(x&UEF_STOPBIT)

/* some possible return states */
#define UEF_OK				0
#define UEF_OPEN_NOTUEF		-1
#define UEF_OPEN_NOTTAPE	-2
#define UEF_OPEN_NOFILE		-3
#define UEF_OPEN_MEMERR		-4

/* setup */
extern "C" void uef_setclock(int beats);
extern "C" void uef_setunlock(int unlock);

/* poll mode */
extern "C" int uef_getdata(int time);

/* open & close */
extern "C" int uef_open(char *name);
extern "C" void uef_close(void);

/* writing */
extern "C" int uef_create(char *name);
extern "C" int uef_putdata(int data, int time);

#endif
