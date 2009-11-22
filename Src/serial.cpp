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

/*
Serial/Cassette Support for BeebEm
Written by Richard Gellman - March 2001
*/

// P.S. If anybody knows how to emulate this, do tell me - 16/03/2001 - Richard Gellman

// Need to comment out uef calls and remove uef.lib from project in
// order to use VC profiling.
//#define PROFILING

#include <windows.h>
#include <stdio.h>

#include "6502core.h"
#include "uef.h"
#include "main.h"
#include "serial.h"
#include "beebsound.h"
#include "beebwin.h"
#include "beebemrc.h"
#include "debug.h"
#include "uefstate.h"
#include "csw.h"
#include "serialdevices.h"
#include "debug.h"

#define CASSETTE 0  // Device in 
#define RS423 1		// use defines

unsigned char Cass_Relay=0; // Cassette Relay state
unsigned char SerialChannel=CASSETTE; // Device in use

unsigned char RDR,TDR; // Receive and Transmit Data Registers
unsigned char RDSR,TDSR; // Receive and Transmit Data Shift Registers (buffers)
unsigned int Tx_Rate=1200,Rx_Rate=1200; // Recieve and Transmit baud rates.
unsigned char Clk_Divide=1; // Clock divide rate

unsigned char ACIA_Status,ACIA_Control; // 6850 ACIA Status.& Control
unsigned char SP_Control; // SERPROC Control;

unsigned char CTS,RTS,FirstReset=1;
unsigned char DCD=0,DCDI=1,ODCDI=1,DCDClear=0; // count to clear DCD bit

unsigned char Parity,Stop_Bits,Data_Bits,RIE,TIE; // Receive Intterrupt Enable
												  // and Transmit Interrupt Enable
unsigned char TxD,RxD; // Transmit and Receive destinations (data or shift register)

char UEFTapeName[256]; // Filename of current tape file
unsigned char UEFOpen=0;

unsigned int Baud_Rates[8]={19200,1200,4800,150,9600,300,2400,75};

unsigned char OldRelayState=0;
CycleCountT TapeTrigger=CycleCountTMax;
#define TAPECYCLES 357 // 2000000/5600 - 5600 is normal tape speed

int UEF_BUF=0,NEW_UEF_BUF=0;
int TapeClock=0,OldClock=0;
int TapeClockSpeed = 5600;
int UnlockTape=1;

// Tape control variables
int map_lines;
char map_desc[MAX_MAP_LINES][40];
int map_time[MAX_MAP_LINES];
bool TapeControlEnabled = false;
bool TapePlaying = true;
bool TapeRecording = false;
static HWND hwndTapeControl;
static HWND hwndMap;
extern HWND hCurrentDialog;
void TapeControlOpenFile(char *file_name);
void TapeControlUpdateCounter(int tape_time);
void TapeControlStopRecording(bool RefreshControl);

// This bit is the Serial Port stuff
unsigned char SerialPortEnabled;
unsigned char SerialPort;

HANDLE hSerialPort=NULL; // Serial port handle
DCB dcbSerialPort; // Serial port device control block
char nSerialPort[5]; // Serial port name
char *pnSerialPort=nSerialPort;
unsigned char SerialPortOpen=0; // Indicates status of serial port (on the host)
unsigned int SerialBuffer=0,SerialWriteBuffer=0;
DWORD BytesIn,BytesOut;
DWORD LineStat;
DWORD dwCommEvent;
OVERLAPPED olSerialPort={0},olSerialWrite={0},olStatus={0};
volatile bool bSerialStateChanged=FALSE;
volatile bool bWaitingForData=FALSE;
volatile bool bWaitingForStat=FALSE;
volatile bool bCharReady=FALSE;
COMMTIMEOUTS ctSerialPort;
DWORD dwClrCommError;

FILE *serlog;

void SetACIAStatus(unsigned char bit) {
	ACIA_Status|=1<<bit;
}

void ResetACIAStatus(unsigned char bit) {
	ACIA_Status&=~(1<<bit);
}

void Write_ACIA_Control(unsigned char CReg) {
	unsigned char bit;
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Serial: Write ACIA control %02X", (int)CReg);
		DebugDisplayTrace(DEBUG_SERIAL, true, info);
	}

	ACIA_Control=CReg; // This is done for safe keeping
	// Master reset
	if ((CReg & 3)==3) {
		ACIA_Status&=8; ResetACIAStatus(7); SetACIAStatus(2);
		intStatus&=~(1<<serial); // Master reset clears IRQ
		if (FirstReset==1) { 
			CTS=1; SetACIAStatus(3);
			FirstReset=0; RTS=1; 
		} // RTS High on first Master reset.
		ResetACIAStatus(2); DCD=0; DCDI=0; DCDClear=0;
		SetACIAStatus(1); // Xmit data register empty
		TapeTrigger=TotalCycles+TAPECYCLES;
	}
	// Clock Divide
	if ((CReg & 3)==0) Clk_Divide=1;
	if ((CReg & 3)==1) Clk_Divide=16;
	if ((CReg & 3)==2) Clk_Divide=64;
	// Parity, Data, and Stop Bits.
	Parity=2-((CReg & 4)>>2);
	Stop_Bits=2-((CReg & 8)>>3);
	Data_Bits=7+((CReg & 16)>>4);
	if ((CReg & 28)==16) { Stop_Bits=2; Parity=0; }
	if ((CReg & 28)==20) { Stop_Bits=1; Parity=0; }
	// Transmission control
	TIE=(CReg & 32)>>5;
	RTS=(CReg & 64)>>6;
	RIE=(CReg & 128)>>7;
	bit=(CReg & 96)>>5;
	if (bit==3) { RTS=0; TIE=0; }
	// Seem to need an interrupt immediately for tape writing when TIE set
	if ( (SerialChannel==CASSETTE) && TIE && (Cass_Relay == 1) ) {
		intStatus|=1<<serial;
		SetACIAStatus(7);
	}
	// Change serial port settings
	if ((SerialChannel==RS423) && (SerialPortEnabled) && (!TouchScreenEnabled) && (!EthernetPortEnabled)) {
		GetCommState(hSerialPort,&dcbSerialPort);
		dcbSerialPort.ByteSize=Data_Bits;
		dcbSerialPort.StopBits=(Stop_Bits==1)?ONESTOPBIT:TWOSTOPBITS;
		dcbSerialPort.Parity=(Parity==0)?NOPARITY:((Parity==1)?ODDPARITY:EVENPARITY);
		dcbSerialPort.DCBlength=sizeof(dcbSerialPort);
		SetCommState(hSerialPort,&dcbSerialPort);
		// Check RTS
		EscapeCommFunction(hSerialPort,(RTS==1)?CLRRTS:SETRTS);
	}
}

void Write_ACIA_Tx_Data(unsigned char Data) {
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Serial: Write ACIA Tx %02X", (int)Data);
		DebugDisplayTrace(DEBUG_SERIAL, true, info);
	}

//	WriteLog("Serial: Write ACIA Tx %02X, SerialChannel = %d\n", (int)Data, SerialChannel);

	intStatus&=~(1<<serial);
	ResetACIAStatus(7);

/*
 * 10/09/06
 * JW - A bug in swarm loader overwrites the rs423 output buffer counter
 * Unless we do something with the data, the loader hangs so just swallow it (see below)
 */

	if ( (SerialChannel==CASSETTE) || ((SerialChannel==RS423) && (!SerialPortEnabled)) )
	{
		ResetACIAStatus(1);
		TDR=Data;
		TxD=1;
		int baud = Tx_Rate * ((Clk_Divide==1) ? 64 : (Clk_Divide==64) ? 1 : 4);
		TapeTrigger=TotalCycles + 2000000/(baud/8) * TapeClockSpeed/5600;
	}
	if ((SerialChannel==RS423) && (SerialPortEnabled)) {
		if (ACIA_Status & 2) {
			ResetACIAStatus(1);
			SerialWriteBuffer=Data;

			if (TouchScreenEnabled)
			{
				TouchScreenWrite(Data);
			}
			else if (EthernetPortEnabled)
				{
//					if (Data_Bits==7) {
//						IP232Write(Data & 127 );
//					} else {
						IP232Write(Data );
						if (!IP232raw && Data == 255) IP232Write(Data);
//					}
				} 
				else
				{
					WriteFile(hSerialPort,&SerialWriteBuffer,1,&BytesOut,&olSerialWrite);
				}
			
			SetACIAStatus(1);
		}
	}
}

void Write_SERPROC(unsigned char Data) {
	unsigned int HigherRate;
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Serial: Write serial ULA %02X", (int)Data);
		DebugDisplayTrace(DEBUG_SERIAL, true, info);
	}

	SP_Control=Data;
	// Slightly easier this time.
	// just the Rx and Tx baud rates, and the selectors.
	Cass_Relay=(Data & 128)>>7;
	TapeAudio.Enabled=(Cass_Relay && (TapePlaying||TapeRecording))?TRUE:FALSE;
	LEDs.Motor=(Cass_Relay==1);
	if (Cass_Relay)
		TapeTrigger=TotalCycles+TAPECYCLES;
	if (Cass_Relay!=OldRelayState) {
		OldRelayState=Cass_Relay;
		ClickRelay(Cass_Relay);
	}
	SerialChannel=(Data & 64)>>6;
	Tx_Rate=Baud_Rates[(Data & 7)];
	Rx_Rate=Baud_Rates[(Data & 56)>>3];
	// Note, the PC serial port (or at least win32) does not allow different transmit/receive rates
	// So we will use the higher of the two
	if (SerialChannel==RS423) {
		HigherRate=Tx_Rate;
		if (Rx_Rate>Tx_Rate) HigherRate=Rx_Rate;
		GetCommState(hSerialPort,&dcbSerialPort);
		dcbSerialPort.BaudRate=HigherRate;
		dcbSerialPort.DCBlength=sizeof(dcbSerialPort);
		SetCommState(hSerialPort,&dcbSerialPort);
	}
}

unsigned char Read_ACIA_Status(void) {
//	if (DCDI==0 && DCD!=0)
//	{
//		DCDClear++;
//		if (DCDClear > 1) {
//			DCD=0; ResetACIAStatus(2);
//			DCDClear=0;
//		}
//	}
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Serial: Read ACIA status %02X", (int)ACIA_Status);
		DebugDisplayTrace(DEBUG_SERIAL, true, info);
	}

//	WriteLog("Serial: Read ACIA status %02X\n", (int)ACIA_Status);

	return(ACIA_Status);
}

void HandleData(unsigned char AData) {
	//fprintf(serlog,"%d %02X\n",RxD,AData);
	// This proc has to dump data into the serial chip's registers

	if (RxD==0) { RDR=AData; SetACIAStatus(0); } // Rx Reg full
	if (RxD==1) { RDSR=AData; SetACIAStatus(0); }
	ResetACIAStatus(5);
	if (RxD==2) { RDR=RDSR; RDSR=AData; SetACIAStatus(5); } // overrun
	if (RIE) { intStatus|=1<<serial; SetACIAStatus(7); } // interrupt on receive/overun
	if (RxD<2) RxD++; 	
}


unsigned char Read_ACIA_Rx_Data(void) {
	unsigned char TData;
//	if (DCDI==0 && DCD!=0)
//	{
//		DCDClear++;
//		if (DCDClear > 1) {
//			DCD=0; ResetACIAStatus(2);
//			DCDClear=0;
//		}
//	}
	intStatus&=~(1<<serial);
	ResetACIAStatus(7);
	TData=RDR; RDR=RDSR; RDSR=0;
	if (RxD>0) RxD--; 
	if (RxD==0) ResetACIAStatus(0);
	if ((RxD>0) && (RIE)) { intStatus|=1<<serial; SetACIAStatus(7); }
	if (Data_Bits==7) TData&=127;
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Serial: Read ACIA Rx %02X", (int)TData);
		DebugDisplayTrace(DEBUG_SERIAL, true, info);
	}

//	WriteLog("Serial: Read ACIA Rx %02X, ACIA_Status = %02x\n", (int)TData, (int) ACIA_Status);

	return(TData);
}

unsigned char Read_SERPROC(void) {
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Serial: Read serial ULA %02X", (int)SP_Control);
		DebugDisplayTrace(DEBUG_SERIAL, true, info);
	}

	return(SP_Control);
}

void Serial_Poll(void)
{

//	if (trace == 1) WriteLog("Here - SerialChannel = %d, TapeRecording = %d\n", SerialChannel, TapeRecording);

	if (SerialChannel==CASSETTE)
	{
		if (TapeRecording)
		{
		
//			if (trace == 1) WriteLog("Here - TapeRecording\n");

			if (Cass_Relay==1 && UEFOpen && TotalCycles >= TapeTrigger)
			{
				if (TxD > 0)
				{
					// Writing data
					if (!uef_putdata(TDR|UEF_DATA, TapeClock))
					{
						char errstr[256];
						sprintf(errstr, "Error writing to UEF file:\n  %s", UEFTapeName);
						MessageBox(GETHWND,errstr,"BeebEm",MB_ICONERROR|MB_OK);
						TapeControlStopRecording(true);
					}
					TxD=0;
					SetACIAStatus(1);
					if (TIE)
					{
						intStatus|=1<<serial;
						SetACIAStatus(7);
					}
					TapeAudio.Data=(TDR<<1)|1;
					TapeAudio.BytePos=1;
					TapeAudio.CurrentBit=0;
					TapeAudio.Signal=1;
					TapeAudio.ByteCount=3;
				}
				else
				{
					// Tone
					if (!uef_putdata(UEF_HTONE, TapeClock))
					{
						char errstr[256];
						sprintf(errstr, "Error writing to UEF file:\n  %s", UEFTapeName);
						MessageBox(GETHWND,errstr,"BeebEm",MB_ICONERROR|MB_OK);
						TapeControlStopRecording(true);
					}
					TapeAudio.Signal=2;
					TapeAudio.BytePos=11;

				}

				TapeTrigger=TotalCycles+TAPECYCLES;
				TapeClock++;
			}
		}
		else // Playing or stopped
		{
//			if (trace == 1) WriteLog("In Serial Poll - Cass_Relay = %d, UEFOpen = %d, TapeClock = %d, OldClock = %d\n", Cass_Relay, UEFOpen, TapeClock, OldClock);


/*
 * 10/09/06
 * JW - If trying to write data when not recording, just ignore
 */

			if ( (TxD > 0) && (TotalCycles >= TapeTrigger) )
			{

//				WriteLog("Ignoring Writes\n");

				TxD=0;
				SetACIAStatus(1);
				if (TIE)
				{
					intStatus|=1<<serial;
					SetACIAStatus(7);
				}
			}

			if (Cass_Relay==1 && UEFOpen && TapeClock!=OldClock)
			{
#ifndef PROFILING

				NEW_UEF_BUF=uef_getdata(TapeClock);

#endif
				OldClock=TapeClock;
			}

			if ((NEW_UEF_BUF!=UEF_BUF || UEFRES_TYPE(NEW_UEF_BUF)==UEF_HTONE || UEFRES_TYPE(NEW_UEF_BUF)==UEF_GAP) &&
				(Cass_Relay==1) && (UEFOpen))
			{
				if (UEFRES_TYPE(UEF_BUF) != UEFRES_TYPE(NEW_UEF_BUF))
					TapeControlUpdateCounter(TapeClock);
		
				UEF_BUF=NEW_UEF_BUF;

				// New data read in, so do something about it
				if (UEFRES_TYPE(UEF_BUF) == UEF_HTONE)
				{
					DCDI=1;
					TapeAudio.Signal=2;
					//TapeAudio.Samples=0;
					TapeAudio.BytePos=11;
				}
				if (UEFRES_TYPE(UEF_BUF) == UEF_GAP)
				{
					DCDI=1;
					TapeAudio.Signal=0;
				}
				if (UEFRES_TYPE(UEF_BUF) == UEF_DATA)
				{
					DCDI=0;
					HandleData(UEFRES_BYTE(UEF_BUF));
					TapeAudio.Data=(UEFRES_BYTE(UEF_BUF)<<1)|1;
					TapeAudio.BytePos=1;
					TapeAudio.CurrentBit=0;
					TapeAudio.Signal=1;
					TapeAudio.ByteCount=3;
				}
			}
			if ((Cass_Relay==1) && (RxD<2) && UEFOpen)
			{
				if (TotalCycles >= TapeTrigger)
				{
					if (TapePlaying)
						TapeClock++;
					TapeTrigger=TotalCycles+TAPECYCLES;
				}
			}

// CSW stuff			

			if (Cass_Relay == 1 && CSWOpen && TapeClock != OldClock)
			{
				int last_state = csw_state;
				
				CSW_BUF = csw_poll(TapeClock);
				OldClock = TapeClock;
			
				if (last_state != csw_state)
					TapeControlUpdateCounter(csw_ptr);

				if (csw_state == 0)		// Waiting for tone
				{
					DCDI=1;
					TapeAudio.Signal=0;
				}
				
				// New data read in, so do something about it
				if (csw_state == 1)		// In tone
				{
					DCDI=1;
					TapeAudio.Signal=2;
					TapeAudio.BytePos=11;
				}

				if ( (CSW_BUF >= 0) && (csw_state == 2) )
				{
					DCDI=0;
					HandleData(CSW_BUF);
					TapeAudio.Data=(CSW_BUF<<1)|1;
					TapeAudio.BytePos=1;
					TapeAudio.CurrentBit=0;
					TapeAudio.Signal=1;
					TapeAudio.ByteCount=3;
				}
			}
			if ((Cass_Relay==1) && (RxD<2) && CSWOpen)
			{
				if (TotalCycles >= TapeTrigger)
				{
					if (TapePlaying)
						TapeClock++;
					
					TapeTrigger = TotalCycles + CSW_CYCLES;
					
				}
			}
			
			if (DCDI==1 && ODCDI==0)
			{
				// low to high transition on the DCD line
				if (RIE)
				{
					intStatus|=1<<serial;
					SetACIAStatus(7);
				}
				DCD=1; SetACIAStatus(2); //ResetACIAStatus(0);
				DCDClear=0;
			}
			if (DCDI==0 && ODCDI==1)
			{
				DCD=0; ResetACIAStatus(2);
				DCDClear=0;
			}
			if (DCDI!=ODCDI)
				ODCDI=DCDI;
		}
	}

	if ((SerialChannel==RS423) && (SerialPortEnabled))
	{

		if (TouchScreenEnabled)
		{
			if (TouchScreenPoll() == true)
			{
				if (RxD<2)
					HandleData(TouchScreenRead());
			}
		}
		else if (EthernetPortEnabled)
		{	
			if (IP232Poll() == true)
			{
				if (RxD<2)
					HandleData(IP232Read());
			}
		}
		else
		{
			if  ((!bWaitingForStat) && (!bSerialStateChanged)) {
				WaitCommEvent(hSerialPort,&dwCommEvent,&olStatus);
				bWaitingForStat=TRUE;
			}
			if ((!bSerialStateChanged) && (bCharReady) && (!bWaitingForData) && (RxD<2)) {
				if (!ReadFile(hSerialPort,&SerialBuffer,1,&BytesIn,&olSerialPort)) {
					if (GetLastError()==ERROR_IO_PENDING) {
						bWaitingForData=TRUE;
					} else {
						MessageBox(GETHWND,"Serial Port Error",WindowTitle,MB_OK|MB_ICONERROR);
					}
				} else {
					if (BytesIn>0) {
						HandleData((unsigned char)SerialBuffer);
					} else {
						ClearCommError(hSerialPort, &dwClrCommError,NULL);
						bCharReady=FALSE;
					}
				}
			}
		} 
	}
}

void InitThreads(void) {
	if (hSerialPort) { CloseHandle(hSerialPort); hSerialPort=NULL; }
	bWaitingForData=FALSE;
	bWaitingForStat=FALSE;
	if ( (SerialPortEnabled) && (SerialPort > 0)) {
		InitSerialPort(); // Set up the serial port if its enabled.
		if (olSerialPort.hEvent) { CloseHandle(olSerialPort.hEvent); olSerialPort.hEvent=NULL; }
		olSerialPort.hEvent=CreateEvent(NULL,TRUE,FALSE,NULL); // Create the serial port event signal
		if (olSerialWrite.hEvent) { CloseHandle(olSerialWrite.hEvent); olSerialWrite.hEvent=NULL; }
		olSerialWrite.hEvent=CreateEvent(NULL,TRUE,FALSE,NULL); // Write event, not actually used.
		if (olStatus.hEvent) { CloseHandle(olStatus.hEvent); olStatus.hEvent=NULL; }
		olStatus.hEvent=CreateEvent(NULL,TRUE,FALSE,NULL); // Status event, for WaitCommEvent
	}
	bSerialStateChanged=FALSE;
}

void StatThread(void *lpParam) {
	DWORD dwOvRes=0;
	do {
		if ((!TouchScreenEnabled) && (!EthernetPortEnabled) &&
		    (WaitForSingleObject(olStatus.hEvent,10)==WAIT_OBJECT_0) && (SerialPortEnabled) ) {
			if (GetOverlappedResult(hSerialPort,&olStatus,&dwOvRes,FALSE)) {
				// Event waiting in dwCommEvent
				if ((dwCommEvent & EV_RXCHAR) && (!bWaitingForData)) { 
					bCharReady=TRUE;
				}
				if (dwCommEvent & EV_CTS) {
					// CTS line change
					GetCommModemStatus(hSerialPort,&LineStat);
					if (LineStat & MS_CTS_ON) ResetACIAStatus(3); else SetACIAStatus(3); // Invert for CTS bit
					if (LineStat & MS_CTS_ON) SetACIAStatus(1); else ResetACIAStatus(1); // Keep for TDRE bit
				}
			}
			bWaitingForStat=FALSE;
		}
		else
		{
			Sleep(100); // Don't hog CPU if nothing happening
		}
		if ((bSerialStateChanged) && (!bWaitingForData)) {
			// Shut off serial port, and re-initialise
			InitThreads();
			bSerialStateChanged=FALSE;
		}
		Sleep(0);
	} while(1);
}

void SerialThread(void *lpParam) {
	// New Serial port thread - 7:35pm 16/10/2001 GMT
	// This sorta runs as a seperate process in effect, checking
	// enable status, and doing the monitoring.
	DWORD spResult;
	do {
		if ((!bSerialStateChanged) && (SerialPortEnabled) && (!TouchScreenEnabled) && (!EthernetPortEnabled) && (bWaitingForData)) {
			spResult=WaitForSingleObject(olSerialPort.hEvent,INFINITE); // 10ms to respond
			if (spResult==WAIT_OBJECT_0) {
				if (GetOverlappedResult(hSerialPort,&olSerialPort,&BytesIn,FALSE)) {
					// sucessful read, screw any errors.
					if ((SerialChannel==RS423) && (BytesIn>0)) HandleData((unsigned char)SerialBuffer);
					if (BytesIn==0) {
						bCharReady=FALSE;
						ClearCommError(hSerialPort, &dwClrCommError,NULL);
					}
					bWaitingForData=FALSE;
				}
			}
		}
		else
		{
			Sleep(100); // Don't hog CPU if nothing happening
		}
		Sleep(0);
	} while (1);
}

void InitSerialPort(void) {
	BOOL bPortStat;
	// Initialise COM port
	if ( (SerialPortEnabled) && (SerialPort > 0 ) ) {
		if (SerialPort==1) pnSerialPort="Com1";
		if (SerialPort==2) pnSerialPort="Com2";
		if (SerialPort==3) pnSerialPort="Com3";
		if (SerialPort==4) pnSerialPort="Com4";
		hSerialPort=CreateFile(pnSerialPort,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
		if (hSerialPort==INVALID_HANDLE_VALUE) {
			MessageBox(GETHWND,"Could not open specified serial port",WindowTitle,MB_OK|MB_ICONERROR);
			bSerialStateChanged=TRUE;
			SerialPortEnabled=FALSE;
			mainWin->ExternUpdateSerialMenu();
		}
		else {
			bPortStat=SetupComm(hSerialPort, 1280, 1280);
			bPortStat=GetCommState(hSerialPort, &dcbSerialPort);
			dcbSerialPort.fBinary=TRUE;
			dcbSerialPort.BaudRate=9600;
			dcbSerialPort.Parity=NOPARITY;
			dcbSerialPort.StopBits=ONESTOPBIT;
			dcbSerialPort.ByteSize=8;
			dcbSerialPort.fDtrControl=DTR_CONTROL_DISABLE;
			dcbSerialPort.fOutxCtsFlow=FALSE;
			dcbSerialPort.fOutxDsrFlow=FALSE;
			dcbSerialPort.fOutX=FALSE;
			dcbSerialPort.fDsrSensitivity=FALSE;
			dcbSerialPort.fInX=FALSE;
			dcbSerialPort.fRtsControl=RTS_CONTROL_DISABLE; // Leave it low (do not send) for now
			dcbSerialPort.DCBlength=sizeof(dcbSerialPort);
			bPortStat=SetCommState(hSerialPort, &dcbSerialPort);
			ctSerialPort.ReadIntervalTimeout=MAXDWORD;
			ctSerialPort.ReadTotalTimeoutConstant=0;
			ctSerialPort.ReadTotalTimeoutMultiplier=0;
			ctSerialPort.WriteTotalTimeoutConstant=0;
			ctSerialPort.WriteTotalTimeoutMultiplier=0;
			SetCommTimeouts(hSerialPort,&ctSerialPort); 
			SetCommMask(hSerialPort,EV_CTS | EV_RXCHAR | EV_ERR);
		}
	}
	//serlog=fopen("/ser.log","wb");
}

void CloseUEF(void) {
#ifndef PROFILING
	if (UEFOpen) {
		TapeControlStopRecording(false);
		uef_close();
		UEFOpen=0;
		TxD=0;
		RxD=0;
		if (TapeControlEnabled) 
			SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);
	}
#endif
}

void Kill_Serial(void) {
	CloseUEF();
	CloseCSW();
	if (SerialPortOpen)
		CloseHandle(hSerialPort);
}

void LoadUEF(char *UEFName) {
#ifndef PROFILING
	CloseUEF();

	strcpy(UEFTapeName, UEFName);

	if (TapeControlEnabled)
		TapeControlOpenFile(UEFName);

	// Clock values:
	// 5600 - Normal speed - anything higher is a bit slow
	// 750 - Recommended minium settings, fastest reliable load
	uef_setclock(TapeClockSpeed);
	SetUnlockTape(UnlockTape);

	if (uef_open(UEFName)) {
		UEFOpen=1;
		UEF_BUF=0;
		TxD=0;
		RxD=0;
		TapeClock=0;
		OldClock=0;
		TapeTrigger=TotalCycles+TAPECYCLES;
		TapeControlUpdateCounter(TapeClock);
	}
	else {
		UEFTapeName[0]=0;
	}
#endif
}

void RewindTape(void) {
	TapeControlStopRecording(true);
	UEF_BUF=0;
	TapeClock=0;
	OldClock=0;
	TapeTrigger=TotalCycles+TAPECYCLES;
	TapeControlUpdateCounter(TapeClock);

	csw_state = 0;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;

}

void SetTapeSpeed(int speed) {
	int NewClock = (int)((double)TapeClock * ((double)speed / TapeClockSpeed));
	TapeClockSpeed=speed;
	if (UEFOpen)
		LoadUEF(UEFTapeName);
	TapeClock=NewClock;
}

void SetUnlockTape(int unlock) {
	uef_setunlock(unlock);
}

//*******************************************************************

bool map_file(char *file_name)
{
	bool done=false;
	int file;
	int i;
	int start_time;
	int n;
	int data;
	int last_data;
	int blk;
	int blk_num;
	char block[500];
	bool std_last_block=true;
	char name[11];

	uef_setclock(TapeClockSpeed);

	file = uef_open(file_name);
	if (file == NULL)
	{
		return(false);
	}

	i=0;
	start_time=0;
	map_lines=0;
	last_data=0;
	blk_num=0;

	memset(map_desc, 0, sizeof(map_desc));
	memset(map_time, 0, sizeof(map_time));

	while (!done && map_lines < MAX_MAP_LINES)
	{
		data = uef_getdata(i);
		if (data != last_data)
		{
			if (UEFRES_TYPE(data) != UEFRES_TYPE(last_data))
			{
				if (UEFRES_TYPE(last_data) == UEF_DATA)
				{
					// End of block, standard header?
					if (blk > 20 && block[0] == 0x2A)
					{
						if (!std_last_block)
						{
							// Change of block type, must be first block
							blk_num=0;
							if (map_lines > 0 && map_desc[map_lines-1][0] != 0)
							{
								strcpy(map_desc[map_lines], "");
								map_time[map_lines]=start_time;
								map_lines++;
							}
						}

						// Pull file name from block
						n = 1;
						while (block[n] != 0 && block[n] >= 32 && block[n] <= 127 && n <= 10)
						{
							name[n-1] = block[n];
							n++;
						}
						name[n-1] = 0;
						if (name[0] != 0)
							sprintf(map_desc[map_lines], "%-12s %02X  Length %04X", name, blk_num, blk);
						else
							sprintf(map_desc[map_lines], "<No name>    %02X  Length %04X", blk_num, blk);

						map_time[map_lines]=start_time;

						// Is this the last block for this file?
						if (block[strlen(name) + 14] & 0x80)
						{
							blk_num=-1;
							++map_lines;
							strcpy(map_desc[map_lines], "");
							map_time[map_lines]=start_time;
						}
						std_last_block=true;
					}
					else
					{
						sprintf(map_desc[map_lines], "Non-standard %02X  Length %04X", blk_num, blk);
						map_time[map_lines]=start_time;
						std_last_block=false;
					}

					// Replace time counter in previous blank lines
					if (map_lines > 0 && map_desc[map_lines-1][0] == 0)
						map_time[map_lines-1]=start_time;

					// Data block recorded
					map_lines++;
					blk_num++;
				}
					
				if (UEFRES_TYPE(data) == UEF_HTONE)
				{
					// strcpy(map_desc[map_lines++], "Tone");
					start_time=i;
				}
				else if (UEFRES_TYPE(data) == UEF_GAP)
				{
					if (map_lines > 0 && map_desc[map_lines-1][0] != 0)
						strcpy(map_desc[map_lines++], "");
					start_time=i;
					blk_num=0;
				}
				else if (UEFRES_TYPE(data) == UEF_DATA)
				{
					blk=0;
					block[blk++]=UEFRES_BYTE(data);
				}
				else if (UEFRES_TYPE(data) == UEF_EOF)
				{
					done=true;
				}
			}
			else
			{
				if (UEFRES_TYPE(data) == UEF_DATA)
				{
					if (blk < 500)
						block[blk++]=UEFRES_BYTE(data);
					else
						blk++;
				}
			}
		}
		last_data=data;
		i++;
	}

	uef_close();

	return(true);
}

//*******************************************************************

BOOL CALLBACK TapeControlDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

void TapeControlOpenDialog(HINSTANCE hinst, HWND hwndMain)
{
	int Clock;

	TapeControlEnabled = TRUE;

	if (!IsWindow(hwndTapeControl)) 
	{ 
		hwndTapeControl = CreateDialog(hinst, MAKEINTRESOURCE(IDD_TAPECONTROL),
										NULL, (DLGPROC)TapeControlDlgProc);
		hCurrentDialog = hwndTapeControl;
		ShowWindow(hwndTapeControl, SW_SHOW);

		hwndMap = GetDlgItem(hwndTapeControl, IDC_TCMAP);
		SendMessage(hwndMap, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
						(LPARAM)MAKELPARAM(FALSE,0));

		if (UEFOpen)
		{
			Clock = TapeClock;
			LoadUEF(UEFTapeName);
			TapeClock = Clock;
			TapeControlUpdateCounter(TapeClock);
		}

		if (CSWOpen)
		{
			Clock = csw_ptr;
			LoadCSW(UEFTapeName);
			csw_ptr = Clock;
			TapeControlUpdateCounter(csw_ptr);
		}
	}
}

void TapeControlCloseDialog()
{
	DestroyWindow(hwndTapeControl);
	hwndTapeControl = NULL;
	hwndMap = NULL;
	TapeControlEnabled = FALSE;
	hCurrentDialog = NULL;
	map_lines = 0;
	TapePlaying=true;
	TapeRecording=false;
}

void TapeControlOpenFile(char *UEFName)
{
	int i;

	if (TapeControlEnabled) 
	{
		if (CSWOpen == 0)
		{
			if (!map_file(UEFName))
			{
				char errstr[256];
				sprintf(errstr, "Cannot open UEF file:\n  %s", UEFName);
				MessageBox(GETHWND,errstr,"BeebEm",MB_ICONERROR|MB_OK);
				return;
			}
		}

		SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);
		for (i = 0; i < map_lines; ++i)
			SendMessage(hwndMap, LB_ADDSTRING, 0, (LPARAM)map_desc[i]);

		TapeControlUpdateCounter(0);
	}
}

void TapeControlUpdateCounter(int tape_time)
{
	int i;

	if (TapeControlEnabled) 
	{
		i = 0;
		while (i < map_lines && map_time[i] <= tape_time)
			i++;

		if (i > 0)
			i--;

		SendMessage(hwndMap, LB_SETCURSEL, (WPARAM)i, 0);
	}
}

BOOL CALLBACK TapeControlDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	char str[256];
	int s;
	int r;

	switch (message)
	{
		case WM_INITDIALOG:
			return TRUE;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
				hCurrentDialog = NULL;
			else
				hCurrentDialog = hwndTapeControl;
			return FALSE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_TCMAP:
					if (HIWORD(wParam) == LBN_SELCHANGE)
					{
						s = (int)SendMessage(hwndMap, LB_GETCURSEL, 0, 0);
						if (s != LB_ERR && s >= 0 && s < map_lines)
						{
							if (CSWOpen)
							{
								csw_ptr = map_time[s];
							}
							else
							{
								TapeClock=map_time[s];
							}
							OldClock=0;
							TapeTrigger=TotalCycles+TAPECYCLES;
						}
					}
					return FALSE;

				case IDC_TCPLAY:
					TapePlaying=true;
					TapeControlStopRecording(true);
					TapeAudio.Enabled=Cass_Relay?TRUE:FALSE;
					return TRUE;

				case IDC_TCSTOP:
					TapePlaying=false;
					TapeControlStopRecording(true);
					TapeAudio.Enabled=FALSE;
					return TRUE;

				case IDC_TCEJECT:
					TapeControlStopRecording(false);
					TapeAudio.Enabled=FALSE;
					CloseUEF();
					CloseCSW();
					return TRUE;

				case IDC_TCRECORD:
					if (CSWOpen) break;
					if (!TapeRecording)
					{
						r = IDCANCEL;
						if (UEFOpen)
						{
							sprintf(str, "Append to current tape file:\n  %s", UEFTapeName);
							r = MessageBox(GETHWND,str,"BeebEm",MB_ICONWARNING|MB_OKCANCEL);
							if (r == IDOK)
							{
								SendMessage(hwndMap, LB_SETCURSEL, (WPARAM)map_lines-1, 0);
							}
						}
						else
						{
							// Query for new file name
							CloseUEF();
							mainWin->NewTapeImage(UEFTapeName);
							if (UEFTapeName[0])
							{
								r = IDOK;
								FILE *fd = fopen(UEFTapeName,"rb");
								if (fd != NULL)
								{
									fclose(fd);
									sprintf(str, "File already exists:\n  %s\n\nOverwrite file?", UEFTapeName);
									if (MessageBox(GETHWND,str,"BeebEm",MB_YESNO|MB_ICONQUESTION) != IDYES)
										r = IDCANCEL;
								}

								if (r == IDOK)
								{
									// Create file
									if (uef_create(UEFTapeName))
									{
										UEFOpen=1;
									}
									else
									{
										sprintf(str, "Error creating tape file:\n  %s", UEFTapeName);
										MessageBox(GETHWND,str,"BeebEm",MB_ICONERROR|MB_OK);
										UEFTapeName[0] = 0;
										r = IDCANCEL;
									}
								}
							}
						}

						if (r == IDOK)
						{
							TapeRecording=true;
							TapePlaying=false;
							TapeAudio.Enabled=Cass_Relay?TRUE:FALSE;
						}
					}
					return TRUE;

				case IDCANCEL:
					TapeControlCloseDialog();
					return TRUE;
			}
	}
	return FALSE;
}

void TapeControlStopRecording(bool RefreshControl)
{
	if (TapeRecording)
	{
		uef_putdata(UEF_EOF, 0);
		TapeRecording = false;

		if (RefreshControl)
		{
			LoadUEF(UEFTapeName);
		}
	}
}

//*******************************************************************

void SaveSerialUEF(FILE *SUEF)
{
	if (UEFOpen)
	{
		fput16(0x0473,SUEF);
		fput32(293,SUEF);
		fputc(SerialChannel,SUEF);
		fwrite(UEFTapeName,1,256,SUEF);
		fputc(Cass_Relay,SUEF);
		fput32(Tx_Rate,SUEF);
		fput32(Rx_Rate,SUEF);
		fputc(Clk_Divide,SUEF);
		fputc(Parity,SUEF);
		fputc(Stop_Bits,SUEF);
		fputc(Data_Bits,SUEF);
		fputc(RIE,SUEF);
		fputc(TIE,SUEF);
		fputc(TxD,SUEF);
		fputc(RxD,SUEF);
		fputc(RDR,SUEF);
		fputc(TDR,SUEF);
		fputc(RDSR,SUEF);
		fputc(TDSR,SUEF);
		fputc(ACIA_Status,SUEF);
		fputc(ACIA_Control,SUEF);
		fputc(SP_Control,SUEF);
		fputc(DCD,SUEF);
		fputc(DCDI,SUEF);
		fputc(ODCDI,SUEF);
		fputc(DCDClear,SUEF);
		fput32(TapeClock,SUEF);
		fput32(TapeClockSpeed,SUEF);
	}
}

void LoadSerialUEF(FILE *SUEF)
{
	char errstr[200];
	char FileName[256];
	int sp;

	CloseUEF();

	SerialChannel=fgetc(SUEF);
	fread(FileName,1,256,SUEF);
	if (FileName[0])
	{
		LoadUEF(FileName);
		if (!UEFOpen)
		{
			if (!TapeControlEnabled)
			{
				sprintf(errstr, "Cannot open UEF file:\n  %s", FileName);
				MessageBox(GETHWND,errstr,"BeebEm",MB_OK|MB_ICONERROR);
			}
		}
		else
		{
			Cass_Relay=fgetc(SUEF);
			Tx_Rate=fget32(SUEF);
			Rx_Rate=fget32(SUEF);
			Clk_Divide=fgetc(SUEF);
			Parity=fgetc(SUEF);
			Stop_Bits=fgetc(SUEF);
			Data_Bits=fgetc(SUEF);
			RIE=fgetc(SUEF);
			TIE=fgetc(SUEF);
			TxD=fgetc(SUEF);
			RxD=fgetc(SUEF);
			RDR=fgetc(SUEF);
			TDR=fgetc(SUEF);
			RDSR=fgetc(SUEF);
			TDSR=fgetc(SUEF);
			ACIA_Status=fgetc(SUEF);
			ACIA_Control=fgetc(SUEF);
			SP_Control=fgetc(SUEF);
			DCD=fgetc(SUEF);
			DCDI=fgetc(SUEF);
			ODCDI=fgetc(SUEF);
			DCDClear=fgetc(SUEF);
			TapeClock=fget32(SUEF);
			sp=fget32(SUEF);
			if (sp != TapeClockSpeed)
			{
				TapeClock = (int)((double)TapeClock * ((double)TapeClockSpeed / sp));
			}
			TapeControlUpdateCounter(TapeClock);
		}
	}
}
