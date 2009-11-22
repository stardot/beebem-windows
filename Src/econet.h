/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2005  Mike Wyatt

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

// Econet for BeebEm
// Written by Rob O'Donnell. robert@irrelevant.com
// Mike Wyatt - further development, Dec 2005

#ifndef ECONET_HEADER
#define ECONET_HEADER

// Emulated ADLC control registers.
// control1_b0 is AC
// this splits register address 0x01 as control2 and control3
// and register address 0x03 as tx-data-last-data and control4
struct MC6854 {
	unsigned char control1;
	unsigned char control2;
	unsigned char control3;
	unsigned char control4;
	unsigned char txfifo[3];
	unsigned char rxfifo[3];
	unsigned char txfptr;		// first empty byte in fifo
	unsigned char rxfptr;		// first empty byte in fifo
	unsigned char txftl;		// tx fifo tx lst flags. (bits relate to subscripts)
	unsigned char rxffc;		// rx fifo fc flags bitss
	unsigned char rxap;			// rx fifo ap flags. (bits relate to subscripts)

	unsigned char status1;
	// b0 receiver data available
	// b1 status 2 read required (OR of all in s2)
	// b2 loop mode.
	// b3 flag detected
	// b4 clear to send
	// b5 transmitter underrun
	// b6 tx data reg avail / frame complete
	// b7 IRQ active

	unsigned char status2;
	// b0 Address present
	// b1 Frame Valid
	// b2 Rx idle
	// b3 Rx Abort
	// b4 Frame Check Sequence/Invalid Frame error
	// b5 Data Carrier Detect
	// b6 Overrun
	// b7 Receiver Data Avilable (see s1 b0 )

	int sr2pse;					// PSE level for SR2 rx bits
	// 0 = inactive
	// 1 = ERR, FV, DCD, OVRN, ABT
	// 2 = Idle
	// 3 = AP
	// 4 = RDA

	bool cts;		// signal up
	bool idle;
};

unsigned char Read_Econet_Station(void);
void EconetReset(void);
unsigned char ReadEconetRegister(unsigned char Register);
void WriteEconetRegister(unsigned char Register, unsigned char Value);
void ReadNetwork(void);
void debugADLCprint(void); 
void EconetError(const char *errstr);

bool EconetPoll(void);
bool EconetPoll_real(void);

extern char EconetEnabled;
extern bool EconetNMIenabled;
extern bool EconetStateChanged;
extern int EconetTrigger;
extern int EconetFlagFillTimeoutTrigger;
extern int EconetFlagFillTimeout;
volatile extern struct MC6854 ADLC;

extern unsigned char EconetStationNumber;
extern unsigned int EconetListenPort;
extern char EconetCfgPath[512];

extern WSADATA WsaDat;							// Windows sockets info

#endif
