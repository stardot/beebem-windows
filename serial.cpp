/*
Serial/Cassette Support for BeebEm

Written by Richard Gellman - March 2001

You may not distribute this entire file separate from the whole BeebEm distribution.

You may use sections or all of this code for your own uses, provided that:

1) It is not a separate file in itself.
2) This copyright message is included
3) Acknowledgement is made to the author, and any aubsequent authors of additional code.

The code may be modified as required, and freely distributed as such authors see fit,
provided that:

1) Acknowledgement is made to the author, and any subsequent authors of additional code
2) Indication is made of modification.

Absolutely no warranties/guarantees are made by using this code. You use and/or run this code
at your own risk. The code is classed by the author as "unlikely to cause problems as far as
can be determined under normal use".

-- end of legal crap - Richard Gellman :) */

// P.S. If anybody knows how to emulate this, do tell me - 16/03/2001 - Richard Gellman

#include "serial.h"
#include "6502core.h"
#include "uef.h"
#include "main.h"
#include "beebsound.h"
#include "beebwin.h"
#include <stdio.h>
#include <windows.h>

#define CASSETTE 0  // Device in 
#define RS423 1		// use defines

unsigned char Cass_Relay=0; // Cassette Relay state
unsigned char SerialChannel=CASSETTE; // Device in use

unsigned char RDRE=0,TDRF=0; // RX/TX Data Reg in use specifiers
unsigned char RDR,TDR; // Receive and Transmit Data Registers
unsigned char RDSR,TDSR; // Receive and Transmit Data Shift Registers (buffers)
unsigned int Tx_Rate,Rx_Rate; // Recieve and Transmit baud rates.
unsigned char Clk_Divide; // Clock divide rate

unsigned char ACIA_Status,ACIA_Control; // 6850 ACIA Status.& Control
unsigned char SP_Control; // SERPROC Control;

unsigned char CTS,RTS,FirstReset=1;
unsigned char DCD=0,DCDI=1,ODCDI=1,DCDClear=0; // count to clear DCD bit

unsigned char Parity,Stop_Bits,Data_Bits,RIE,TIE; // Receive Intterrupt Enable
												  // and Transmit Interrupt Enable
unsigned char TxD,RxD; // Transmit and Receive destinations (data or shift register)
#define EVEN 0;
#define ODD 1;

UEFFILE TapeFile; // UEF Tape file handle for Thomas Harte's UEF-DLL

unsigned int Baud_Rates[8]={19200,1200,4800,150,9600,300,2400,75};

unsigned char INTONE=0; // Dummy byte bug
unsigned char OldRelayState=0;

#define DCDLOWLIMIT 20
#define DCDCOUNTDOWN 200
unsigned int DCDClock=DCDCOUNTDOWN;
unsigned char UEFOpen=0;
unsigned int TapeCount=100;

int UEF_BUF=0,NEW_UEF_BUF=0;
int TapeClock=0,OldClock=0;

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
int TapeClockSpeed;

FILE *serlog;

void SetACIAStatus(unsigned char bit) {
	ACIA_Status|=1<<bit;
}

void ResetACIAStatus(unsigned char bit) {
	ACIA_Status&=~(1<<bit);
}

void Write_ACIA_Control(unsigned char CReg) {
	unsigned char bit;
	ACIA_Control=CReg; // This is done for safe keeping
	// Master reset
	if ((CReg & 3)==3) {
		ACIA_Status&=8; ResetACIAStatus(7); SetACIAStatus(2);
		intStatus&=~(1<<serial); // Master reset clears IRQ
		if (FirstReset==1) { 
			CTS=1; SetACIAStatus(3);
			FirstReset=0; RTS=1; 
		} // RTS High on first Master reset.
		if (DCDI==0) ResetACIAStatus(2); // clear DCD Bit if DCD input is low
		ResetACIAStatus(2); DCDI=0; DCDClear=0;
		SetACIAStatus(1); // Xmit data register empty
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
	// Change serial port settings
	if ((SerialChannel==RS423) && (SerialPortEnabled)) {
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
	intStatus&=~(1<<serial);
	ResetACIAStatus(7);
	if ((SerialChannel=RS423) && (SerialPortEnabled)) {
		if (ACIA_Status & 2) {
			ResetACIAStatus(1);
			SerialWriteBuffer=Data;
			WriteFile(hSerialPort,&SerialWriteBuffer,1,&BytesOut,&olSerialWrite);
			SetACIAStatus(1);
		}
	}
}

void Write_SERPROC(unsigned char Data) {
	unsigned int HigherRate;
	SP_Control=Data;
	// Slightly easier this time.
	// just the Rx and Tx baud rates, and the selectors.
	Cass_Relay=(Data & 128)>>7;
	TapeAudio.Enabled=(Cass_Relay)?TRUE:FALSE;
	LEDs.Motor=(Cass_Relay==1);
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
	DCDClear=2;
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
	intStatus&=~(1<<serial);
	ResetACIAStatus(7);
	if (DCDI==0) { DCDClear=2; }
	TData=RDR; RDR=RDSR; RDSR=0;
	if (RxD>0) RxD--; 
	if (RxD==0) ResetACIAStatus(0);
	if ((RxD>0) && (RIE)) { intStatus|=1<<serial; SetACIAStatus(7); }
	if (Data_Bits==7) TData&=127;
	return(TData);
}

unsigned char Read_SERPROC(void) {
return(SP_Control);
}

void Serial_Poll(void) {
  if (SerialChannel==CASSETTE) {
	// Cassette port
	if (Cass_Relay==1 && UEFOpen && TapeClock!=OldClock) {
		NEW_UEF_BUF=uef_getdata(TapeFile,TapeClock);
		OldClock=TapeClock;
	}
	if ((NEW_UEF_BUF!=UEF_BUF || UEFRES_TYPE(NEW_UEF_BUF)==UEF_HTONE || UEFRES_TYPE(NEW_UEF_BUF)==UEF_GAP) && (Cass_Relay==1) && (UEFOpen)) {
		UEF_BUF=NEW_UEF_BUF;
		// New data read in, so do something about it
		if (UEFRES_TYPE(UEF_BUF) == UEF_HTONE) { 
			if (DCDClock<DCDLOWLIMIT) DCDI=0; else DCDI=1;
			if (DCDClock>0) DCDClock--;
			TapeAudio.Signal=2;
			//TapeAudio.Samples=0;
			TapeAudio.BytePos=11;
		}
		if (UEFRES_TYPE(UEF_BUF) == UEF_GAP) { DCDI=1; DCDClock=DCDCOUNTDOWN; INTONE=0; TapeAudio.Signal=0; }
		if (UEFRES_TYPE(UEF_BUF) == UEF_DATA) {
			HandleData(UEFRES_BYTE(UEF_BUF));
			DCDClock=DCDCOUNTDOWN;
				TapeAudio.Data=(UEFRES_BYTE(UEF_BUF)<<1)|1;
				TapeAudio.BytePos=1;
				TapeAudio.CurrentBit=0;
				TapeAudio.Signal=1;
				TapeAudio.ByteCount=3;
			}
	}
	if ((Cass_Relay==1) && (RxD<2) && UEFOpen) {
		TapeCount--;
		if (TapeCount==0){
			TapeClock++;
			TapeCount=100;
		}
	}
	if (DCDI==1 && ODCDI==0) {
		// low to high transition on the DCD line
		if (RIE) { intStatus|=1<<serial; SetACIAStatus(7); } // cause interrupt if RIE set
		DCD=1; SetACIAStatus(2); //ResetACIAStatus(0);
		DCDClear=0;
	}
	if (DCDI==0 && ODCDI==1) {
		DCD=0; ResetACIAStatus(2);
		DCDClear=0;
	}
	if (DCDI!=ODCDI) ODCDI=DCDI;
  }
	if ((SerialChannel==RS423) && (SerialPortEnabled)) {
		if  ((!bWaitingForStat) && (!bSerialStateChanged)) {
			WaitCommEvent(hSerialPort,&dwCommEvent,&olStatus);
			bWaitingForStat=TRUE;
		}
		if ((!bSerialStateChanged) && (bCharReady) && (!bWaitingForData) && (RxD<2)) {
			if (!ReadFile(hSerialPort,&SerialBuffer,1,&BytesIn,&olSerialPort)) {
				if (GetLastError()==ERROR_IO_PENDING) {
					bWaitingForData=TRUE;
				} else {
					MessageBox(GETHWND,"Serial Port Error","BBC Emulator",MB_OK|MB_ICONERROR);
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

void InitThreads(void) {
	if (hSerialPort) { CloseHandle(hSerialPort); hSerialPort=NULL; }
	bWaitingForData=FALSE;
	bWaitingForStat=FALSE;
	if (SerialPortEnabled) {
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
	if ((WaitForSingleObject(olStatus.hEvent,10)==WAIT_OBJECT_0) && (SerialPortEnabled)) {
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
		if ((!bSerialStateChanged) && (SerialPortEnabled) && (bWaitingForData)) {
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
	Sleep(0);
	} while (1);
}

void InitSerialPort(void) {
	BOOL bPortStat;
	// Initialise COM port
	if (SerialPortEnabled) {
		if (SerialPort==1) pnSerialPort="Com1";
		if (SerialPort==2) pnSerialPort="Com2";
		if (SerialPort==3) pnSerialPort="Com3";
		if (SerialPort==4) pnSerialPort="Com4";
		hSerialPort=CreateFile(pnSerialPort,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
		if (hSerialPort==INVALID_HANDLE_VALUE) {
			MessageBox(GETHWND,"Could not open specified serial port","BBC Emulator",MB_OK|MB_ICONERROR);
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

void Kill_Serial(void) {
	if (UEFOpen) uef_close(TapeFile);
	if (SerialPortOpen) CloseHandle(hSerialPort);
}

void LoadUEF(char *UEFName) {
	if (UEFOpen) uef_close(TapeFile);
	uef_setmode(UEFMode_Poll);
	uef_setclock(TapeClockSpeed);
	// Clock values:
	// 5600 - Normal speed - anything higher is a bit slow
	// 750 - Recommended minium settings, fastest reliable load
	TapeFile=uef_open(UEFName);
	if (TapeFile!=NULL) {
		UEFOpen=1;
		TapeClock=0;
		TapeCount=100;
		OldClock=0;
	}
}

void RewindTape(void) {
	TapeClock=0;
	OldClock=0;
	TapeCount=100;
}
