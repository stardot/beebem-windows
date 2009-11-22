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
 *  serialdevices.cc
 *  BeebEm3
 *
 *  Created by Jon Welch on 28/08/2006.
 *

 Remote serial port & IP232 support by Rob O'Donnell Mar 2009.
 . Raw mode connects and disconnects on RTS going up and down.
 . CTS reflects connection status.
 . IP232 mode maintains contstant connection. CTS reflects DTR
 . on the modem; you would normally tie this to DCD depending
 . on your application.
 . Currently "custom destination" must be specified by editing
 . Preferences.cfg after saving settings once.

 */

#include <windows.h>
#include <stdio.h>
#include "serialdevices.h"
#include "6502core.h"
#include "uef.h"
#include "main.h"
#include "serial.h"
#include "beebsound.h"
#include "beebwin.h"
#include "debug.h"
#include "uefstate.h"
#include "csw.h"
#include "uservia.h"
#include "atodconv.h"


#define TS_BUFF_SIZE	10240
#define TS_DELAY		8192			// Cycles to wait for data to be TX'd or RX'd

//#define IP232_BUFF_SIZE	128
#define IP232_CXDELAY	8192			// Cycles to wait after connection

// IP232
extern WSADATA WsaDat;							// Windows sockets info
SOCKET mEthernetHandle = SOCKET_ERROR;		// Listen socket

// bool bEthernetSocketsCreated = FALSE;

struct sockaddr_in ip232_serv_addr; 

// This bit is the Serial Port stuff
unsigned char TouchScreenEnabled;
unsigned char EthernetPortEnabled;
unsigned char IP232localhost;
unsigned char IP232custom;
unsigned char IP232mode;
unsigned char IP232raw;
unsigned int IP232customport;
char IP232customip [20];
bool ip232_flag_received = FALSE;
char IPAddress[256];
int PortNo;

bool mStartAgain = false;

unsigned char ts_inbuff[TS_BUFF_SIZE];
unsigned char ts_outbuff[TS_BUFF_SIZE];
int ts_inhead, ts_intail, ts_inlen;
int ts_outhead, ts_outtail, ts_outlen;
int ts_delay;

CycleCountT IP232RxTrigger=CycleCountTMax;

void TouchScreenOpen(void)
{
	ts_inhead = ts_intail = ts_inlen = 0;
	ts_outhead = ts_outtail = ts_outlen = 0;
	ts_delay = 0;
}

bool TouchScreenPoll(void)
{
unsigned char data;
static int mode = 0;

	if (ts_delay > 0)
	{
		ts_delay--;
		return false;
	}
	
	if (ts_outlen > 0)		// Process data waiting to be received by BeebEm straight away
	{
		return true;
	}

	if (ts_inlen > 0)
	{
		data = ts_inbuff[ts_inhead];
		ts_inhead = (ts_inhead + 1) % TS_BUFF_SIZE;
		ts_inlen--;
		
		switch (data)
		{
			case 'M' :
				mode = 0;
				break;

			case '0' :
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
			case '8' :
			case '9' :
				mode = mode * 10 + data - '0';
				break;

			case '.' :

/*
 * Mode 1 seems to be polled, sends a '?' and we reply with data
 * Mode 129 seems to be send current values all time
 */
				
				WriteLog("Setting touch screen mode to %d\n", mode);
				break;

			case '?' :

				if (mode == 1)		// polled mode
				{
					TouchScreenReadScreen(false);
				}
				break;
		}
	}
	
	if (mode == 129)		// real time mode
	{
		TouchScreenReadScreen(true);
	}
		
	if (mode == 130)		// area mode - seems to be pressing with two fingers which we can't really do ??
	{
		TouchScreenReadScreen(true);
	}
	
	
	return false;
}

void TouchScreenWrite(unsigned char data)
{

//	WriteLog("TouchScreenWrite 0x%02x\n", data);
	
	if (ts_inlen != TS_BUFF_SIZE)
	{
		ts_inbuff[ts_intail] = data;
		ts_intail = (ts_intail + 1) % TS_BUFF_SIZE;
		ts_inlen++;
		ts_delay = TS_DELAY;
	}
	else
	{
		WriteLog("TouchScreenWrite input buffer full\n");
	}
}


unsigned char TouchScreenRead(void)
{
unsigned char data;

	data = 0;
	if (ts_outlen > 0)
	{
		data = ts_outbuff[ts_outhead];
		ts_outhead = (ts_outhead + 1) % TS_BUFF_SIZE;
		ts_outlen--;
		ts_delay = TS_DELAY;
	}
	else
	{
		WriteLog("TouchScreenRead output buffer empty\n");
	}
	
//	WriteLog("TouchScreenRead 0x%02x\n", data);

	return data;
}

void TouchScreenClose(void)
{
}

void TouchScreenStore(unsigned char data)
{
	if (ts_outlen != TS_BUFF_SIZE)
	{
		ts_outbuff[ts_outtail] = data;
		ts_outtail = (ts_outtail + 1) % TS_BUFF_SIZE;
		ts_outlen++;
	}
	else
	{
		WriteLog("TouchScreenStore output buffer full\n");
	}
}

void TouchScreenReadScreen(bool check)
{
int x, y;
static int last_x = -1, last_y = -1, last_m = -1;

	x = (65535 - JoystickX) / (65536 / 120) + 1;
	y = JoystickY / (65536 / 90) + 1;

	if ( (last_x != x) || (last_y != y) || (last_m != AMXButtons) || (check == false))
	{

//		WriteLog("JoystickX = %d, JoystickY = %d, last_x = %d, last_y = %d\n", JoystickX, JoystickY, last_x, last_y);
		
		if (AMXButtons & AMX_LEFT_BUTTON)
		{
			TouchScreenStore( 64 + ((x & 0xf0) >> 4) );
			TouchScreenStore( 64 + (x & 0x0f) );
			TouchScreenStore( 64 + ((y & 0xf0) >> 4) );
			TouchScreenStore( 64 + (y & 0x0f) );
			TouchScreenStore('.');
//			WriteLog("Sending X = %d, Y = %d\n", x, y);
		} else {
			TouchScreenStore( 64 + 0x0f);
			TouchScreenStore( 64 + 0x0f);
			TouchScreenStore( 64 + 0x0f);
			TouchScreenStore( 64 + 0x0f);
			TouchScreenStore('.');
//			WriteLog("Screen not touched\n");
		}
		last_x = x;
		last_y = y;
		last_m = AMXButtons;
	}
}



/******************************************************************
**                                                               **
**  Note - touchscreen routines use ts_in* for data into the ts  **
**  and ts_out* as data from the touchscreen.                    **
**  IP232 routines use ts_in* as data coming in from the modem,  **
**  and ts-out* for data to be sent out to the modem.            **
**  i.e. the opposite way around!!       watch out!              **
**                                                               **
******************************************************************/

bool IP232Open(void)
{

	ts_inhead = ts_intail = ts_outlen = 0;
	ts_outhead = ts_outtail = ts_inlen = 0;

	//----------------------
	// Let's prepare some IP sockets
	if (WSAStartup(MAKEWORD(1, 1), &WsaDat) != 0) {
		WriteLog("IP232: WSA initialisation failed");
		if (DebugEnabled) 
			DebugDisplayTrace(DEBUG_REMSER, true, "IP232: WSA initialisation failed");
		

//		return false;
	}

	mEthernetHandle = socket(AF_INET, SOCK_STREAM, 0);
	if (mEthernetHandle == INVALID_SOCKET)
	{
		WriteLog("Unable to create IP232 socket");
		if (DebugEnabled)
			DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Unable to create socket");

		return false ; //Couldn't create the socket

	}
	if (DebugEnabled)
		DebugDisplayTrace(DEBUG_REMSER, true, "IP232: socket created");

//	bEthernetSocketsCreated = true;
	
	IP232RxTrigger = TotalCycles + IP232_CXDELAY;

	ip232_serv_addr.sin_family = AF_INET; // address family Internet
	ip232_serv_addr.sin_port = htons (PortNo); //Port to connect on
	ip232_serv_addr.sin_addr.s_addr = inet_addr (IPAddress); //Target IP

	if (connect(mEthernetHandle, (SOCKADDR *)&ip232_serv_addr, sizeof(ip232_serv_addr)) == SOCKET_ERROR)
	{
		WriteLog("Unable to connect to IP232 server %s",IPAddress);
		if (DebugEnabled) {
			char info[200];
			sprintf(info, "IP232: Unable to connect to server  %s",IPAddress);
			DebugDisplayTrace(DEBUG_REMSER, true, info);
		}
		IP232Close();
		mEthernetHandle = 0;

		return false; //Couldn't connect
	}
	if (DebugEnabled)
		DebugDisplayTrace(DEBUG_REMSER, true, "IP232: connected to server");

	// TEMP! 
	ResetACIAStatus(3); // CTS input low,
	SetACIAStatus(1); // TDRE high
	if (DebugEnabled) 
		DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Init, CTS low");


	if (mEthernetPortReadTaskID == NULL)
	{
		CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) MyEthernetPortReadThread,NULL,0,&mEthernetPortReadTaskID);
		CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) MyEthernetPortStatusThread,NULL,0,&mEthernetPortStatusTaskID);
	}
	return true;
}

bool IP232Poll(void)
{
	
//	fd_set	fds;
//	timeval tv;
//	int i;

/*	if (mStartAgain == true)
	{
		WriteLog("Closing Comms\n");
		if (DebugEnabled) 
				DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Comms Close");
//		mStartAgain = false;
//		MessageBox(GETHWND,"Could not connect to specified address",WindowTitle,MB_OK|MB_ICONERROR);
		bSerialStateChanged=TRUE;
		SerialPortEnabled=FALSE;
		mainWin->ExternUpdateSerialMenu();
		
		IP232Close();
//		IP232Open();
		return false;
	}
*/

	if (TotalCycles > IP232RxTrigger && ts_inlen > 0)
	{
		return true;
	}

	return false;
}

void IP232Close(void)
{
	if (mEthernetHandle > 0) {
		WriteLog("Closing IP232 socket");
		if (DebugEnabled) 
				DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Closing Sockets");

		closesocket(mEthernetHandle);
		WSACleanup();
		mEthernetHandle = 0;
//		bEthernetSocketsCreated = false;
	}

/*
	if (mEthernetPortReadTaskID != NULL)
	{
		MPTerminateTask(mEthernetPortReadTaskID, 0);
	}
	if (mEthernetPortStatusTaskID != NULL)
	{
		MPTerminateTask(mEthernetPortStatusTaskID, 0);
	}
	
	if (mListenTaskID != NULL)
	{
		MPTerminateTask(mListenTaskID, 0);
	}

	mListenHandle = NULL;
	mEthernetPortStatusTaskID = NULL;
	mEthernetPortReadTaskID = NULL;
*/
	

}

void IP232Write(unsigned char data)
{

//	WriteLog("TouchScreenWrite 0x%02x\n", data);
	
	if (ts_outlen != TS_BUFF_SIZE)
	{
		ts_outbuff[ts_outtail] = data;
		ts_outtail = (ts_outtail + 1) % TS_BUFF_SIZE;
		ts_outlen++;
	}
	else
	{
		WriteLog("IP232Write send buffer full\n");
		if (DebugEnabled) 
				DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Write send Buffer Full");

	}
}

unsigned char IP232Read(void)
{	
	unsigned char data;

	data = 0;
	if (ts_inlen > 0)
	{
		data = ts_inbuff[ts_inhead];
		ts_inhead = (ts_inhead + 1) % TS_BUFF_SIZE;
		ts_inlen--;
		// Simulated baud rate delay between bytes..
		IP232RxTrigger = TotalCycles + 2000000 / (Rx_Rate / 9);
	}
	else
	{
		WriteLog("IP23 receive buffer empty\n");
		if (DebugEnabled) 
				DebugDisplayTrace(DEBUG_REMSER, true, "IP232: receive buffer empty");

	}
	
	return data;
}




void MyEthernetPortReadThread(void *parameter)
{ // Much taken from Mac version by Jon Welch

fd_set	fds;
timeval tv;
int i, j;
unsigned char buff[256];
int space, bufflen;
	
	Sleep (3000);

	while (1)
	{
		
		if (mEthernetHandle > 0)
		{
			
			space = TS_BUFF_SIZE - ts_inlen;

			if (space > 256)
//			if (ts_inlen == 0)
			{
				
				FD_ZERO(&fds);
				tv.tv_sec = 0;
				tv.tv_usec = 0;
			
				FD_SET(mEthernetHandle, &fds);
			
				i = select(32, &fds, NULL, NULL, &tv);		// Read
				if (i > 0)
				{

					i = recv(mEthernetHandle, (char *) buff, 256, 0);
					if (i > 0)
					{
					
//						WriteLog("Read %d bytes\n%s\n", i, buff);
						if (DebugEnabled) {
							char info[514];
							sprintf(info, "IP232: Read %d bytes from server",i);
							DebugDisplayTrace(DEBUG_REMSER, true, info);
							char hexval[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
							for(j = 0; j < i; j++){
								info[j*2] = hexval[((buff[j] >> 4) & 0xF)];
								info[(j*2) + 1] = hexval[(buff[j]) & 0x0F];
							}
							info[j*2] = 0;
							DebugDisplayTrace(DEBUG_REMSER, true, info);

						}
					
						for (j = 0; j < i; j++)
						{
							if (ip232_flag_received) 
							{
								ip232_flag_received = false;
								if (buff[j] == 1) {
										if (DebugEnabled) 
											DebugDisplayTrace(DEBUG_REMSER, true, "Flag,1 DCD True, CTS");
										// dtr on modem high
										ResetACIAStatus(3);  // CTS goes active low
										SetACIAStatus(1);  // so TDRE goes high ??
								}
								else if (buff[j] == 0) {
										if (DebugEnabled) 
											DebugDisplayTrace(DEBUG_REMSER, true, "Flag,0 DCD False, clear CTS");
										// dtr on modem low
										SetACIAStatus(3);  // CTS goes inactive high
										ResetACIAStatus(1);  // so TDRE goes low
								}
								else if (buff[j] == 255) {
										if (DebugEnabled) 
											DebugDisplayTrace(DEBUG_REMSER, true, "Flag,Flag =255");

										EthernetPortStore(255);
								}
							}
							else
							{
								if (buff[j] == 255 && IP232raw == 0)
								{
									ip232_flag_received = true;
								}
								else
								{
									EthernetPortStore(buff[j]);
								}
							}

						}
					}
					else
					{
						// Should really check what the error was ...

//						WriteLog("Read error %d\n", i);

						WriteLog("Remote session disconnected\n");
						if (DebugEnabled) 
							DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Remote session disconnected");
//						mEthernetPortReadTaskID = NULL;
						bSerialStateChanged=TRUE;
						SerialPortEnabled=FALSE;
						mainWin->ExternUpdateSerialMenu();
						IP232Close();
						MessageBox(GETHWND,"Lost connection; serial port has been disabled.",WindowTitle,MB_OK|MB_ICONERROR);
						return ; // noErr;
						
					}
				}
				else
				{
//					WriteLog("Nothing to read %d\n", i);
				}
			}		


			if (ts_outlen > 0 && mEthernetHandle > 0)
			{
				WriteLog("Sending to IP232 server");
				if (DebugEnabled) 
					DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Sending to remote server");

				bufflen=0;
				while (ts_outlen)
				{
					buff[bufflen++] = EthernetPortGet();
				}

				FD_ZERO(&fds);
				tv.tv_sec = 0;
				tv.tv_usec = 0;
				
				FD_SET(mEthernetHandle, &fds);
				
				i = select(32, NULL, &fds, NULL, &tv);		// Write
				if (i <= 0)
				{
					WriteLog("Select Error %i\n", i);
					if (DebugEnabled) 
						DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Select error on send");
				}
				else
				{
					i = send(mEthernetHandle, (char *) buff, bufflen, 0);

					if (i < bufflen)
					{
						// Should really check what the error was ...
						WriteLog("Send Error %i\n", i);
						if (DebugEnabled) 
								DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Send Error");
						bSerialStateChanged=TRUE;
						SerialPortEnabled=FALSE;
						mainWin->ExternUpdateSerialMenu();
						
						IP232Close();
						MessageBox(GETHWND,"Lost connection; serial port has been disabled",WindowTitle,MB_OK|MB_ICONERROR);

					}
					
				}
			}
		}
			
		//		Sleep(1000 * 50);		// sleep for 50 msec
		Sleep (50); //+10*J); //100ms
	}
	
	return ; //noErr;
}


void EthernetPortStore(unsigned char data)
{ // much taken from Mac version by Jon Welch
	if (ts_inlen != TS_BUFF_SIZE)
	{
		ts_inbuff[ts_intail] = data;
		ts_intail = (ts_intail + 1) % TS_BUFF_SIZE;
		ts_inlen++;
	}
	else
	{
		WriteLog("EthernetPortStore output buffer full\n");
		if (DebugEnabled) 
			DebugDisplayTrace(DEBUG_REMSER, true, "IP232: EthernetPortStore output buffer full");
	}
}
unsigned char EthernetPortGet(void)
{ // much taken from Mac version by Jon Welch

	unsigned char data;
	
	data = 0;
	
	if (ts_outlen > 0)
	{
		data = ts_outbuff[ts_outhead];
		ts_outhead = (ts_outhead + 1) % TS_BUFF_SIZE;
		ts_outlen--;
	}
	
	return data;
}


void MyEthernetPortStatusThread(void *parameter)
{ // much taken from Mac version by Jon Welch
	int dcd, odcd, rts, orts;

	dcd = 0;
	odcd = 0;
	rts = orts = 0;
	
// Put bits in here for DCD when got active IP connection
	
	Sleep (2000);

	while (1)
	{
		if (IP232mode == 0) {  
			if (mEthernetHandle > 0)
			{
				dcd = 1;

			}
			else
			{
				dcd = 0;
			}
			
			if ( (dcd == 1) && (odcd ==0) )
			{
//				RaiseDCD();
				ResetACIAStatus(3);  // CTS goes Low
				SetACIAStatus(1);  // so TDRE goes high
				if (DebugEnabled) 
					DebugDisplayTrace(DEBUG_REMSER, true, "IP232: StatusThread DCD up, set CTS low");

			}

			if ( (dcd == 0) && (odcd == 1) )
			{
//				LowerDCD();
				SetACIAStatus(3);  // CTS goes Hgh
				ResetACIAStatus(1);  // so TDRE goes low
				if (DebugEnabled) 
					DebugDisplayTrace(DEBUG_REMSER, true, "IP232: StatusThread lost DCD, set CTS high");
			}
		}
		else //IP232mode == 1
		{
			if ((ACIA_Control & 96) == 64) {
				rts = 1;
			}
			else
			{
				rts = 0;
			}
			if (rts != orts)
			{
				if (mEthernetHandle > 0)
				{
					if (DebugEnabled) {
						char info[200];
						sprintf(info, "IP232: Sending RTS status of %i",rts);
						DebugDisplayTrace(DEBUG_REMSER, true, info);
					}
					
					IP232Write(255);
					IP232Write(rts);
				}
				orts = rts;
			}


		}


		
		if (dcd != odcd)
			odcd = dcd;
		
		//usleep(1000 * 50);		// sleep for 50 milli seconds
		Sleep (50);
	}
	
	//	fprintf(stderr, "Exited MySerialStatusThread\n");
	
	return; // noErr;
}
