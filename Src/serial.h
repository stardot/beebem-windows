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

#include <vector>

#include "csw.h"
#include "uef.h"

extern unsigned int Tx_Rate;
extern unsigned int Rx_Rate;
extern unsigned char Clk_Divide;

// MC6850 status register bits
constexpr unsigned char MC6850_STATUS_RDRF = 0x01;
constexpr unsigned char MC6850_STATUS_TDRE = 0x02;
constexpr unsigned char MC6850_STATUS_DCD  = 0x04;
constexpr unsigned char MC6850_STATUS_CTS  = 0x08;
constexpr unsigned char MC6850_STATUS_FE   = 0x10;
constexpr unsigned char MC6850_STATUS_OVRN = 0x20;
constexpr unsigned char MC6850_STATUS_PE   = 0x40;
constexpr unsigned char MC6850_STATUS_IRQ  = 0x80;

extern unsigned char ACIA_Status;
extern unsigned char ACIA_Control;

extern CycleCountT TapeTrigger;
extern CycleCountT IP232RxTrigger;

void SerialACIAWriteControl(unsigned char Value);
unsigned char SerialACIAReadStatus();

void SerialACIAWriteTxData(unsigned char Value);
unsigned char SerialACIAReadRxData();

void SerialULAWrite(unsigned char Value);
unsigned char SerialULARead();

extern bool SerialPortEnabled;
extern unsigned char SerialPort;

void SerialInit();
void SerialPoll();
void InitSerialPort();
void Kill_Serial();
UEFResult LoadUEFTape(const char *FileName);
CSWResult LoadCSWTape(const char *FileName);
void CloseTape();
void RewindTape();

extern volatile bool bSerialStateChanged;
extern bool TapeControlEnabled;
extern bool UnlockTape;
extern unsigned char TxD,RxD;
extern int TapeClock,OldClock;
extern int TapeClockSpeed;

void SetTapeSpeed(int speed);
void SetUnlockTape(bool unlock);
void TapeControlOpenDialog(HINSTANCE hinst, HWND hwndMain);
void TapeControlAddMapLines();
void TapeControlCloseDialog();

void SaveSerialUEF(FILE *SUEF);
void LoadSerialUEF(FILE *SUEF);

#endif
