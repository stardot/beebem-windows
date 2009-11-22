/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  David Sharp

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

//////////////////////////////////////////////////////////////////////
// TarmacGlobals.h: global declarations for Tarmac codebase
// Part of Tarmac
// By David Sharp
// http://www.davidsharp.com
//////////////////////////////////////////////////////////////////////

// prevent this being added to environment multiple times
#ifndef TARMAC_GLOBALS_H
#define TARMAC_GLOBALS_H

// global definitions for within tarmac

// define types
typedef	unsigned int uint32;
typedef unsigned int uint;			// assume any unsigned int larger than 8 bits
typedef	unsigned short uint16;
typedef	unsigned char uint8;
typedef	signed int int32;
// typedef	signed short int16;
typedef	signed char int8;

#ifndef TRUE
#define TRUE		1
#endif

#ifndef FALSE
#define FALSE		0
#endif

//
// return 1 if bit b is set in the supplied w, 0 otherwise
//

inline uint32 getBit(uint32 w, uint32 b)
{
	return (w >> b) & 1;
}

//
// return bitfield specified at bottom (least significant) end of uint32
//

inline uint32 getField(uint32 w, uint32 s, uint32 e)
{
	return ( (w >> s) & ((1 << ((e - s) + 1) ) - 1) ); 
}

//
// returns counts the number of set bits in the argument
//

inline uint countSetBits(uint32 value)
{
	uint total = 0;
	// while value has some set bits left in it
	while( value )
	{
		// add the value of the bit we're currently looking at
		total += (value & 1);
		// shift right and drop the current bit off the end
		value >>= 1;
	}
	return total;
}

//
// logical shift left operator
//

inline uint32 lslOperator(uint32 value, uint shiftAmount)
{
	return value << shiftAmount;
}

//
// logical shift right
//

inline uint32 lsrOperator(uint32 value, uint shiftAmount)
{
	return value >> shiftAmount;
}

//
// arithmetic shift right
//

inline uint32 asrOperator(uint32 value, uint shiftAmount)
{
	return (unsigned)( ((int)value) >> shiftAmount);
}

//
// rotate right
//

inline uint32 rorOperator(uint32 value, uint shiftAmount)
{
	return (value >> shiftAmount) | (value << (32 - shiftAmount));
}

#endif
