/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch
Copyright (C) 2009  Rob O'Donnell

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

/*
 *  serialdevices.h
 *  BeebEm3
 *
 *  Created by Jon Welch on 28/08/2006.
 */

#ifndef SERIALDEVICES_HEADER
#define SERIALDEVICES_HEADER

extern bool TouchScreenEnabled;
void TouchScreenOpen(void);
bool TouchScreenPoll(void);
void TouchScreenClose(void);
void TouchScreenWrite(unsigned char data);
unsigned char TouchScreenRead(void);
void TouchScreenStore(unsigned char data);
void TouchScreenReadScreen(bool check);

extern bool EthernetPortEnabled;
extern bool IP232localhost;
extern bool IP232custom;
extern bool IP232mode;
extern bool IP232raw;
extern unsigned int IP232customport;
extern char IP232customip [20];

bool IP232Open(void);
bool IP232Poll(void);
void IP232Close(void);
void IP232Write(unsigned char data);
unsigned char IP232Read(void);
extern char IPAddress[256];
extern int PortNo;

unsigned char EthernetPortGet(void);
void EthernetPortStore(unsigned char data);

#endif
