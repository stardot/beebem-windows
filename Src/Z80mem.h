/* Header file for the Memory and the Memory Management Unit (MMU).
   Module MEM_MMU.[hc] Copyright (C) 1998/1999 by Andreas Gerlich (agl)

This file is part of yaze-ag - yet another Z80 emulator by ag.

Yaze-ag is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <limits.h>

#if UCHAR_MAX == 255
typedef unsigned char	BYTE;
#else
#error Need to find an 8-bit type for BYTE
#endif

#if USHRT_MAX == 65535
typedef unsigned short	WORD;
#else
#error Need to find an 16-bit type for WORD
#endif

/* FASTREG needs to be at least 16 bits wide and efficient for the
   host architecture */
#if UINT_MAX >= 65535
typedef unsigned int	FASTREG;
#else
typedef unsigned long	FASTREG;
#endif

/* FASTWORK needs to be wider than 16 bits and efficient for the host
   architecture */
#if UINT_MAX > 65535
typedef unsigned int	FASTWORK;
#else
typedef unsigned long	FASTWORK;
#endif

/* Some important macros. They are the interface between an access from
   the simz80-/yaze-Modules and the method of the memory access: */

#define GetBYTE(a)		ReadZ80Mem(a)
#define GetBYTE_pp(a)	ReadZ80Mem( (a++) )
#define GetBYTE_mm(a)	ReadZ80Mem( (a--) )
#define PutBYTE(a, v)	WriteZ80Mem(a, v)
#define PutBYTE_pp(a,v)	WriteZ80Mem( (a++) , v)
#define PutBYTE_mm(a,v)	WriteZ80Mem( (a--) , v)
#define mm_PutBYTE(a,v)	WriteZ80Mem( (--a) , v)
#define GetWORD(a)		(ReadZ80Mem(a) | ( ReadZ80Mem( (a) + 1) << 8) )


/* don't work: #define GetWORD_pppp(a)	(RAM_pp(a) + (RAM_pp(a) << 8)) */
/* make once more a try at 18.10.1999/21:45 ... with the following macro:  */
/* works also not #define GetWORD_pppp(a) (RAM_pp(a) | (RAM_pp(a) << 8))  */
/* I dont know what the optimizer do with such macro.
   If someone knows about it - I'am very interessed to that knowledge.
 */

#define PutWORD(a, v) \
	do { \
		WriteZ80Mem((a), (BYTE)v); \
		WriteZ80Mem(((a) + 1), (BYTE)((v) >> 8)); \
	} while (0)
