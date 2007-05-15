/*
 *  serialdevices.cc
 *  BeebEm3
 *
 *  Created by Jon Welch on 28/08/2006.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
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


#define TS_BUFF_SIZE	128
#define TS_DELAY		8192			// Cycles to wait for data to be TX'd or RX'd
	
// This bit is the Serial Port stuff
unsigned char TouchScreenEnabled;
unsigned char ts_inbuff[TS_BUFF_SIZE];
unsigned char ts_outbuff[TS_BUFF_SIZE];
int ts_inhead, ts_intail, ts_inlen;
int ts_outhead, ts_outtail, ts_outlen;
int ts_delay;

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
