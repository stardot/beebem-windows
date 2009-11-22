/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
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
// Serial/Cassette Support for BeebEm
// Written by Richard Gellman

#ifndef SERIAL_HEADER
#define SERIAL_HEADER

#define TAPECYCLES 357 // 2000000/5600 - 5600 is normal tape speed

#define MAX_MAP_LINES 4096
extern int map_lines;
extern char map_desc[MAX_MAP_LINES][40];
extern int map_time[MAX_MAP_LINES];
extern unsigned char Clk_Divide;

extern unsigned char ACIA_Control;
extern unsigned int Tx_Rate, Rx_Rate;

extern CycleCountT TapeTrigger;
extern CycleCountT IP232RxTrigger;
void Write_ACIA_Control(unsigned char CReg);
void Write_ACIA_Tx_Data(unsigned char Data);
void Write_SERPROC(unsigned char Data);
unsigned char Read_ACIA_Status(void);
unsigned char Read_ACIA_Rx_Data(void);
unsigned char Read_SERPROC(void);
extern unsigned char SerialPortEnabled;
extern unsigned char SerialPort;
void Serial_Poll(void);
void InitSerialPort(void);
void Kill_Serial(void);
void LoadUEF(char *UEFName);
void RewindTape(void);
extern int TapeClockSpeed;
extern void SerialThread(void *lpParam);
void StatThread(void *lpParam);
void InitThreads(void);
extern volatile bool bSerialStateChanged;
extern bool TapeControlEnabled;
extern char UEFTapeName[256];
extern int UnlockTape;
extern unsigned char TxD,RxD;
extern int TapeClock,OldClock;
extern int TapeClockSpeed;

void SetTapeSpeed(int speed);
void SetUnlockTape(int unlock);
void TapeControlOpenDialog(HINSTANCE hinst, HWND hwndMain);
void TapeControlOpenFile(char *UEFName);
void TapeControlCloseDialog(void);
void SaveSerialUEF(FILE *SUEF);
void LoadSerialUEF(FILE *SUEF);
void SetACIAStatus(unsigned char bit);
void ResetACIAStatus(unsigned char bit);
#endif
