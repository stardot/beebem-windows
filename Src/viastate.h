/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert

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

#ifndef VS_HEADER
#define VS_HEADER

typedef struct VIAState {
  unsigned char ora,orb;
  unsigned char ira,irb;
  unsigned char ddra,ddrb;
  unsigned char acr,pcr;
  unsigned char ifr,ier;
  int timer1c,timer2c; /* NOTE: Timers descrement at 2MHz and values are */
  int timer1l,timer2l; /*   fixed up on read/write - latches hold 1MHz values*/
  int timer1hasshot; /* True if we have already caused an interrupt for one shot mode */
  int timer2hasshot; /* True if we have already caused an interrupt for one shot mode */
  int timer1adjust; // Adjustment for 1.5 cycle counts, every other interrupt, it becomes 2 cycles instead of one
  int timer2adjust; // Adjustment for 1.5 cycle counts, every other interrupt, it becomes 2 cycles instead of one
} VIAState;

#endif
