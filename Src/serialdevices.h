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
 *
 */

extern unsigned char TouchScreenEnabled;
void TouchScreenOpen(void);
bool TouchScreenPoll(void);
void TouchScreenClose(void);
void TouchScreenWrite(unsigned char data);
unsigned char TouchScreenRead(void);
void TouchScreenStore(unsigned char data);
void TouchScreenReadScreen(bool check);



extern unsigned char EthernetPortEnabled;
extern unsigned char IP232localhost;
extern unsigned char IP232custom;
extern unsigned char IP232mode;
extern unsigned char IP232raw;
extern unsigned int IP232customport;
extern char IP232customip [20];

bool IP232Open(void);
bool IP232Poll(void);
void IP232Close(void);
void IP232Write(unsigned char data);
unsigned char IP232Read(void);
extern char IPAddress[256];
extern int PortNo;

extern DWORD mEthernetPortReadTaskID;
void MyEthernetPortReadThread(void *parameter);
extern DWORD mEthernetPortStatusTaskID;
void MyEthernetPortStatusThread(void *parameter);
unsigned char EthernetPortGet(void);
void EthernetPortStore(unsigned char data);
extern SOCKET mEthernetHandle;
