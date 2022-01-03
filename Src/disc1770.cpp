/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2005  Greg Cook

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
WD1770 FDC Disc Support for BeebEm

Written by Richard Gellman - Feb 2001
*/

#include <stdio.h>
#include <stdlib.h>
#include "disc1770.h"
#include "6502core.h"
#include "log.h"
#include "main.h"
#include "uefstate.h"
#include "z80mem.h"
#include "beebsound.h"
#include "sysvia.h"

// Control/Status Register, Track, Sector, and Data Registers
static unsigned char FormatBuffer[2048];
static unsigned char *FormatPtr;
static unsigned char FormatState = 0;
static unsigned int FormatCount = 0;
static unsigned int FormatSize = 0;
static unsigned char Status = 0;
static unsigned char Data = 0;
static unsigned char Track = 0;
static unsigned char ATrack = 0;
static unsigned char Sector;
static bool HeadDir = true; // Head Movement direction - true to step in, false to step out
static unsigned char FDCommand = 0;
static unsigned char NextFDCommand = 0; // Next FDCommand during spin up/settling periods
static int LoadingCycles = 0; // Spin up/settle counter in CPU Cycles
static int SpinDown[2] = {0, 0}; // Spin down delay per drive
// The following are control bits
static bool UpdateTrack = false;
static bool MultiSect = false;
static const int StepRates[4] = {6, 12, 20, 30}; // Milliseconds
static int StepRate = StepRates[0];
static bool SpinUp = false;
static bool Verify = false;
static bool LightsOn[2] = { false, false };
static bool HeadLoaded[2] = { false,false };
// End of control bits
static int ByteCount = 0;
static long DataPos;

bool Disc1770Enabled = true;

/* File names of loaded disc images */
static char DscFileNames[2][256];

static FILE *Disc0; // File handlers for the disc drives 0 and 1
static FILE *Disc1;
static FILE *CurrentDisc; // Current Disc Handle

static unsigned char ExtControl; // FDC External Control Register
static int CurrentDrive = 0; // FDC Control drive setting
static long HeadPos[2]; // Position of Head on each drive for swapping
static unsigned char CurrentHead[2]; // Current Head on any drive
static int DiscStep[2]; // Single/Double sided disc step
static int DiscStrt[2]; // Single/Double sided disc start
static unsigned int SecSize[2];
static DiscType DscType[2];
static unsigned char MaxSects[2]; // Maximum sectors per track
static unsigned int DefStart[2]; // Starting point for head 1
static unsigned int TrkLen[2]; // Track Length in bytes
bool DWriteable[2] = { false, false }; // Write Protect

static bool DiskDensity[2];
static bool SelectedDensity;

static unsigned char RotSect = 0; // Sector counter to fool Opus DDOS on read address
bool InvertTR00; // Needed because the bloody stupid watford board inverts the input.

const int SPIN_DOWN_TIME = 4000000; // 2 seconds
const int SETTLE_TIME    = 30000; // 30 milliseconds
const int ONE_REV_TIME   = 500000; // 1 sixth of a second - used for density mismatch
const int SPIN_UP_TIME   = ONE_REV_TIME * 3; // Two Seconds

#define VERIFY_TIME (ONE_REV_TIME / MaxSects[CurrentDrive])
#define BYTE_TIME   (VERIFY_TIME / 256)

// WD1770 registers
const unsigned char WD1770_CONTROL_REGISTER = 0;
const unsigned char WD1770_STATUS_REGISTER  = 0;
const unsigned char WD1770_TRACK_REGISTER   = 1;
const unsigned char WD1770_SECTOR_REGISTER  = 2;
const unsigned char WD1770_DATA_REGISTER    = 3;

// WD1770 status register bits
const unsigned char WD1770_STATUS_MOTOR_ON         = 0x80;
const unsigned char WD1770_STATUS_WRITE_PROTECT    = 0x40;
const unsigned char WD1770_STATUS_SPIN_UP_COMPLETE = 0x20; // Type I commands
const unsigned char WD1770_STATUS_DELETED_DATA     = 0x20; // Type II and III commands
const unsigned char WD1770_STATUS_RECORD_NOT_FOUND = 0x10;
const unsigned char WD1770_STATUS_CRC_ERROR        = 0x08;
const unsigned char WD1770_STATUS_LOST_DATA        = 0x04;
const unsigned char WD1770_STATUS_NOT_TRACK0       = 0x04; // Type I commands
const unsigned char WD1770_STATUS_DATA_REQUEST     = 0x02;
const unsigned char WD1770_STATUS_INDEX            = 0x02; // Type I commands
const unsigned char WD1770_STATUS_BUSY             = 0x01;

// WD1770 commands
const int WD1770_COMMAND_RESTORE         = 0x00; // Type I
const int WD1770_COMMAND_SEEK            = 0x10; // Type I
const int WD1770_COMMAND_STEP            = 0x20; // Type I
const int WD1770_COMMAND_STEP_IN         = 0x40; // Type I
const int WD1770_COMMAND_STEP_OUT        = 0x60; // Type I
const int WD1770_COMMAND_READ_SECTOR     = 0x80; // Type II
const int WD1770_COMMAND_WRITE_SECTOR    = 0xa0; // Type II
const int WD1770_COMMAND_READ_ADDRESS    = 0xc0; // Type III
const int WD1770_COMMAND_READ_TRACK      = 0xe0; // Type III
const int WD1770_COMMAND_WRITE_TRACK     = 0xf0; // Type III
const int WD1770_COMMAND_FORCE_INTERRUPT = 0xd0; // Type IV

const int WD1770_CMD_FLAGS_MULTIPLE_SECTORS      = 0x10; // Type II commands
const int WD1770_CMD_FLAGS_DISABLE_SPIN_UP       = 0x08; // Type II commands
const int WD1770_CMD_FLAGS_ADD_DELAY             = 0x04; // Type II commands
const int WD1770_CMD_FLAGS_UPDATE_TRACK_REGISTER = 0x10; // Type I commands
const int WD1770_CMD_FLAGS_VERIFY                = 0x04; // Type I commands
const int WD1770_CMD_FLAGS_STEP_RATE             = 0x03; // Type I commands

// FDC control register
// See Hardware/Acorn1770/Acorn.cpp
const int DRIVE_CONTROL_SELECT_DRIVE_0 = 0x01;
const int DRIVE_CONTROL_SELECT_DRIVE_1 = 0x02;
const int DRIVE_CONTROL_SELECT_SIDE    = 0x10;
const int DRIVE_CONTROL_SELECT_DENSITY = 0x20;

static bool CurrentDiscOpen()
{
	return CurrentDisc != nullptr;
}

// Read 1770 Register. Note - NOT the FDC Control register at &FE24.

unsigned char Read1770Register(unsigned char Register) {
	if (!Disc1770Enabled)
		return 0xFF;

	// WriteLog("Disc1770: Read of Register %d - Status is %02X\n", Register, Status);

	// Fool anything reading the Index pulse signal by alternating it on each read.
	if (FDCommand < 6 && FDCommand != 0) {
		Status ^= WD1770_STATUS_INDEX;
	}

	if (Register == WD1770_STATUS_REGISTER) {
		NMIStatus &= ~(1 << nmi_floppy);
		return Status;
	}
	else if (Register == WD1770_TRACK_REGISTER) {
		return ATrack;
	}
	else if (Register == WD1770_SECTOR_REGISTER) {
		if (DscType[CurrentDrive] == DiscType::IMG || DscType[CurrentDrive] == DiscType::DOS)
			return Sector + 1;
		else
			return Sector;
	}
	else if (Register == WD1770_DATA_REGISTER) {
		if (FDCommand > 5)
		{
			Status &= ~WD1770_STATUS_DATA_REQUEST;
			NMIStatus &= ~(1 << nmi_floppy);
		}

		return Data;
	}

	return 0;
}

static void SetMotor(int Drive, bool State) {
	if (Drive == 0) {
		LEDs.Disc0 = State;
		if (MachineType == Model::FileStoreE01 || MachineType == Model::FileStoreE01S)
			mainWin->DrawFSLEDs(mainWin->m_hDC, 1);
	}
	else {
		LEDs.Disc1 = State;
		if (MachineType == Model::FileStoreE01 || MachineType == Model::FileStoreE01S)
			mainWin->DrawFSLEDs(mainWin->m_hDC, 1);
	}

	if (State) {
		Status |= WD1770_STATUS_MOTOR_ON;

		if (DiscDriveSoundEnabled && !HeadLoaded[Drive]) {
			PlaySoundSample(SAMPLE_DRIVE_MOTOR, true);
			PlaySoundSample(SAMPLE_HEAD_LOAD, false);
			HeadLoaded[Drive] = true;
		}
	}
	else {
		StopSoundSample(SAMPLE_DRIVE_MOTOR);
		StopSoundSample(SAMPLE_HEAD_SEEK);

		if (DiscDriveSoundEnabled && HeadLoaded[Drive]) {
			PlaySoundSample(SAMPLE_HEAD_UNLOAD, false);
			HeadLoaded[Drive] = false;
		}
	}
}

// Write 1770 Register - NOT the FDC Control register at &FE24

void Write1770Register(unsigned char Register, unsigned char Value) {
	if (!Disc1770Enabled)
		return;

	// WriteLog("Disc1770: Write of %02X to Register %d\n", Value, Register);

	if (Register == WD1770_CONTROL_REGISTER) {
		NMIStatus &= ~(1 << nmi_floppy); // reset INTRQ
		// Control Register - can only write if current drive is open
		// Changed, now command returns errors if no disc inserted
		const unsigned char ComBits = Value & 0xf0;
		const unsigned char HComBits = Value & 0xe0;

		if (HComBits < WD1770_COMMAND_READ_SECTOR) {
			// Type 1 Command
			Status |= WD1770_STATUS_BUSY;
			Status &= ~(WD1770_STATUS_RECORD_NOT_FOUND |
			            WD1770_STATUS_CRC_ERROR);

			if (HComBits == WD1770_COMMAND_STEP_IN) {
				FDCommand = 4;
				HeadDir = true;
				UpdateTrack = (Value & WD1770_CMD_FLAGS_UPDATE_TRACK_REGISTER) != 0;
			}
			else if (HComBits == WD1770_COMMAND_STEP_OUT) {
				FDCommand = 5;
				HeadDir = false;
				UpdateTrack = (Value & WD1770_CMD_FLAGS_UPDATE_TRACK_REGISTER) != 0;
			}
			else if (HComBits == WD1770_COMMAND_STEP) {
				FDCommand = 3;
				UpdateTrack = (Value & WD1770_CMD_FLAGS_UPDATE_TRACK_REGISTER) != 0;
			}
			else if (ComBits == WD1770_COMMAND_SEEK) {
				FDCommand = 2; // Seek
			}
			else if (ComBits == WD1770_COMMAND_RESTORE) {
				// Restore (Seek to Track 00)
				FDCommand = 1;
			}

			if (FDCommand < 6) {
				Status &= ~WD1770_STATUS_SPIN_UP_COMPLETE;
				Status |= WD1770_STATUS_BUSY;

				// Now set some control bits for Type 1 Commands
				SpinUp = (Value & WD1770_CMD_FLAGS_DISABLE_SPIN_UP) != 0;
				Verify = (Value & WD1770_CMD_FLAGS_VERIFY) != 0;
				StepRate = StepRates[Value & WD1770_CMD_FLAGS_STEP_RATE]; // Make sure the step rate time is added to the delay time.

				if (!(Status & WD1770_STATUS_MOTOR_ON)) {
					NextFDCommand = FDCommand;
					FDCommand = 11; /* Spin-Up delay */
					LoadingCycles = SPIN_UP_TIME;
					//if (!SpinUp) LoadingCycles=ONE_REV_TIME;
					SetMotor(CurrentDrive, true);
				}
				else {
					LoadingCycles = ONE_REV_TIME;
				}
			}
		}

		int SectorCycles = 0; // Number of cycles to wait for sector to come round

		if (CurrentDiscOpen() && Sector > (RotSect + 1))
			SectorCycles = (ONE_REV_TIME / MaxSects[CurrentDrive]) * ((RotSect + 1) - Sector);

		if (HComBits == WD1770_COMMAND_READ_SECTOR) {
			RotSect = Sector;
			Status |= WD1770_STATUS_BUSY;
			FDCommand = 8;
			MultiSect = (Value & WD1770_CMD_FLAGS_MULTIPLE_SECTORS) != 0;
			Status &= ~WD1770_STATUS_DATA_REQUEST;
		}
		else if (HComBits == WD1770_COMMAND_WRITE_SECTOR) {
			RotSect = Sector;
			Status |= WD1770_STATUS_BUSY;
			FDCommand = 9;
			MultiSect = (Value & WD1770_CMD_FLAGS_MULTIPLE_SECTORS) != 0;
		}
//		else if (ComBits == WD1770_COMMAND_READ_TRACK) { // Not implemented yet
//			Sector = 0;
//			Track = Data;
//			RotSect = Sector;
//			FDCommand = 20;
//			MultiSect = true;
//			Status &= ~WD1770_STATUS_DATA_REQUEST;
//		}
		else if (ComBits == WD1770_COMMAND_WRITE_TRACK) {
			Sector = 0;
			RotSect=Sector;
			Status |= WD1770_STATUS_BUSY;
			FDCommand = 21;
		}
		else if (ComBits == WD1770_COMMAND_FORCE_INTERRUPT) {
			if (FDCommand != 0) {
				Status &= ~WD1770_STATUS_BUSY;
			}
			else {
				Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
				            WD1770_STATUS_RECORD_NOT_FOUND |
				            WD1770_STATUS_CRC_ERROR |
				            WD1770_STATUS_DATA_REQUEST |
				            WD1770_STATUS_BUSY);
			}
			FDCommand = 12;
			LoadingCycles = 4000000;
			NextFDCommand = 0; // just in case

			// If this isn't done, the stupid Opus DDOS tries to use the last
			// byte of the last sector read in as a Track number for a seek command.
			Data = 0;

			if (Value & 0xf) {
				NMIStatus |= 1 << nmi_floppy;
			}
		}
		else if (ComBits == WD1770_COMMAND_READ_ADDRESS) {
			FDCommand = 14;
			Status |= WD1770_STATUS_BUSY;
			ByteCount = 6;
			if (!(Status & WD1770_STATUS_MOTOR_ON)) {
				NextFDCommand = FDCommand;
				FDCommand = 11; // Spin-Up delay
				LoadingCycles = SPIN_UP_TIME;
				//if (!SpinUp) LoadingCycles=ONE_REV_TIME; // Make it two seconds instead of one
				SetMotor(CurrentDrive, true);
			}
			else {
				LoadingCycles = SectorCycles;
			}

			if (DiskDensity[CurrentDrive] != SelectedDensity) {
				// Density mismatch
				FDCommand = 13; // "Confusion spin"
				SetMotor(CurrentDrive, true);
				Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
					WD1770_STATUS_RECORD_NOT_FOUND |
					WD1770_STATUS_CRC_ERROR);
				Status |= WD1770_STATUS_BUSY;
				LoadingCycles = ONE_REV_TIME;
			}
		}

		if (FDCommand == 8 || FDCommand == 9) {
			Status &= ~WD1770_STATUS_DATA_REQUEST;

			// Now set some control bits for Type 2 Commands
			SpinUp = (Value & WD1770_CMD_FLAGS_DISABLE_SPIN_UP) != 0;
			Verify = (Value & WD1770_CMD_FLAGS_VERIFY) != 0;

			if (!(Status & WD1770_STATUS_MOTOR_ON)) {
				NextFDCommand = FDCommand;
				FDCommand = 11; // Spin-Up delay
				LoadingCycles = SPIN_UP_TIME;
				//if (SpinUp) LoadingCycles=ONE_REV_TIME;
				SetMotor(CurrentDrive, true);
			}
			else {
				LoadingCycles = SectorCycles;
			}

			LoadingCycles += BYTE_TIME;

			if (DiskDensity[CurrentDrive] != SelectedDensity) {
				// Density mismatch
				FDCommand = 13; // "Confusion spin"
				SetMotor(CurrentDrive, true);
				Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
				            WD1770_STATUS_RECORD_NOT_FOUND |
				            WD1770_STATUS_CRC_ERROR);
				Status |= WD1770_STATUS_BUSY;
				LoadingCycles = ONE_REV_TIME;
			}
		}
	}
	else if (Register == WD1770_TRACK_REGISTER) {
		Track = Value;
		ATrack = Value;
	}
	else if (Register == WD1770_SECTOR_REGISTER) {
		if (DscType[CurrentDrive] == DiscType::IMG || DscType[CurrentDrive] == DiscType::DOS)
			Sector = Value - 1;
		else
			Sector = Value;
	}
	else if (Register == WD1770_DATA_REGISTER) {
		Data = Value;

		if (FDCommand > 5) {
			Status &= ~WD1770_STATUS_DATA_REQUEST;
			NMIStatus &= ~(1 << nmi_floppy);
		}
	}
}

static void UpdateTR00Status()
{
	bool Track0 = (Track == 0);

	if (InvertTR00) {
		Track0 = !Track0;
	}

	if (Track0) {
		Status &= ~WD1770_STATUS_NOT_TRACK0;
	}
	else {
		Status |= WD1770_STATUS_NOT_TRACK0;
	}
}


// This function is called from the 6502core to enable the chip to do stuff
// in the background.

void Poll1770(int NCycles) {
	for (int i = 0; i < 2; i++) {
		if (LightsOn[i]) {
			SpinDown[i] -= NCycles;
			if (SpinDown[i] <= 0) {
				SetMotor(i, false);
				LightsOn[i] = false;
				if (!LightsOn[0] && !LightsOn[1]) {
					Status &= ~WD1770_STATUS_MOTOR_ON;
				}
			}
		}
	}

	if ((Status & WD1770_STATUS_BUSY) && !NMILock) {
		if (FDCommand < 6 && CurrentDiscOpen() && (DscType[CurrentDrive] == DiscType::SSD && CurrentHead[CurrentDrive] == 1)) {
			// Single sided disk, disc not ready
			Status &= ~WD1770_STATUS_BUSY;
			Status |= WD1770_STATUS_RECORD_NOT_FOUND;
			NMIStatus |= 1 << nmi_floppy;
			FDCommand = 12;
			return;
		}

		if (FDCommand < 6 && CurrentDiscOpen()) {
			int TracksPassed = 0; // Holds the number of tracks passed over by the head during seek
			unsigned char OldTrack = (Track >= 80 ? 0 : Track);
			// Seek type commands
			Status &= ~(WD1770_STATUS_RECORD_NOT_FOUND |
			            WD1770_STATUS_CRC_ERROR);

			if (FDCommand == 1) {
				// Restore
				fseek(CurrentDisc, DiscStrt[CurrentDrive], SEEK_SET);
				Track = 0;
			}
			else if (FDCommand == 2) {
				// Seek
				fseek(CurrentDisc, DiscStrt[CurrentDrive] + (DiscStep[CurrentDrive] * Data), SEEK_SET);
				Track = Data;
			}
			else if (FDCommand == 4) {
				// Step In
				HeadDir = true;
				fseek(CurrentDisc, DiscStep[CurrentDrive], SEEK_CUR);
				Track++;
			}
			else if (FDCommand == 5) {
				// Step Out
				HeadDir = false;
				fseek(CurrentDisc, -DiscStep[CurrentDrive], SEEK_CUR);
				Track--;
			}
			else if (FDCommand == 3) {
				// Step
				fseek(CurrentDisc, HeadDir ? DiscStep[CurrentDrive] : -DiscStep[CurrentDrive], SEEK_CUR);
				Track = HeadDir ? Track + 1 : Track - 1;
			}

			if (UpdateTrack || FDCommand < 3) ATrack = Track;
			FDCommand = 15;
			NextFDCommand = 0;
			UpdateTrack = false; // This following bit calculates stepping time
			if (Track >= OldTrack) {
				TracksPassed = Track - OldTrack;
			}
			else {
				TracksPassed = OldTrack - Track;
			}
			OldTrack = Track;
			RotSect = 0;
			// Add track * (steprate * 1000) to LoadingCycles
			LoadingCycles = TracksPassed * (StepRate * 1000);
			LoadingCycles += Verify ? VERIFY_TIME : 0;

			if (DiscDriveSoundEnabled) {
				if (TracksPassed > 1) {
					PlaySoundSample(SAMPLE_HEAD_SEEK, true);
					LoadingCycles = TracksPassed * SAMPLE_HEAD_SEEK_CYCLES_PER_TRACK;
				}
				else if (TracksPassed == 1) {
					PlaySoundSample(SAMPLE_HEAD_STEP, false);
					LoadingCycles = SAMPLE_HEAD_STEP_CYCLES;
				}
			}
			return;
		}

		if (FDCommand == 15) {
			LoadingCycles -= NCycles;
			if (LoadingCycles <= 0) {
				StopSoundSample(SAMPLE_HEAD_SEEK);
				LoadingCycles = SPIN_DOWN_TIME;
				FDCommand = 12;
				NMIStatus |= 1 << nmi_floppy;

				UpdateTR00Status();

				Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
				            WD1770_STATUS_BUSY);
			}
			return;
		}

		if (FDCommand < 6 && !CurrentDiscOpen()) {
			// Disc not ready, return seek error.
			Status &= ~WD1770_STATUS_BUSY;
			Status |= WD1770_STATUS_RECORD_NOT_FOUND;
			NMIStatus |= 1 << nmi_floppy;
			FDCommand = 12;
			return;
		}

		if (FDCommand == 6) { // Read
			LoadingCycles -= NCycles;

			if (LoadingCycles > 0) {
				return;
			}

			if (!(Status & WD1770_STATUS_DATA_REQUEST)) {
				NextFDCommand = 0;
				Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
				            WD1770_STATUS_RECORD_NOT_FOUND |
				            WD1770_STATUS_CRC_ERROR |
				            WD1770_STATUS_LOST_DATA);

				ByteCount--;
				// If reading a single sector, and ByteCount== :-
				// 256..1: read + DRQ (256x)
				//      0: INTRQ + rotate disc
				// If reading multiple sectors, and ByteCount== :-
				// 256..2: read + DRQ (255x)
				//      1: read + DRQ + rotate disc + go back to 256
				if (ByteCount > 0 && !feof(CurrentDisc)) {
					Data = fgetc(CurrentDisc);
					Status |= WD1770_STATUS_DATA_REQUEST;
					NMIStatus |= 1 << nmi_floppy;
				}

				if (ByteCount == 0 || (ByteCount == 1 && MultiSect)) {
					RotSect++;

					if (RotSect > MaxSects[CurrentDrive]) {
						RotSect = 0;
					}
				}

				if (ByteCount == 0 && !MultiSect) {
					// End of sector
					Status &= ~(WD1770_STATUS_DATA_REQUEST |
					            WD1770_STATUS_BUSY);
					NMIStatus |= 1 << nmi_floppy;
					fseek(CurrentDisc, HeadPos[CurrentDrive], SEEK_SET);
					FDCommand = 10;
				}

				if (ByteCount == 1 && MultiSect) {
					ByteCount = SecSize[CurrentDrive] + 1;
					Sector++;

					if (Sector == MaxSects[CurrentDrive]) {
						MultiSect = false;
						// Sector = 0;
					}
				}

				LoadingCycles = BYTE_TIME; // Slow down the read a bit :)

				// Reset shift state if it was set by Run Disc
				if (mainWin->m_ShiftBooted) {
					mainWin->m_ShiftBooted = false;
					BeebKeyUp(0, 0);
				}
			}

			return;
		}

		if (FDCommand == 7 && DWriteable[CurrentDrive]) { // Write
			LoadingCycles -= NCycles;

			if (LoadingCycles > 0) {
				return;
			}

			if (!(Status & WD1770_STATUS_DATA_REQUEST)) {
				// DRQ already issued and answered
				NextFDCommand = 0;
				Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
				            WD1770_STATUS_RECORD_NOT_FOUND |
				            WD1770_STATUS_CRC_ERROR |
				            WD1770_STATUS_LOST_DATA);

				ByteCount--;
				// If writing a single sector, and ByteCount== :-
				// 256..2: write + next DRQ (255x)
				//      1: write + INTRQ + rotate disc
				// If writing multiple sectors, and ByteCount== :-
				// 256..2: write + next DRQ (255x)
				//      1: write + next DRQ + rotate disc + go back to 256
				fputc(Data, CurrentDisc);

				if (ByteCount > 1 || MultiSect) {
					Status |= WD1770_STATUS_DATA_REQUEST;
					NMIStatus |= 1 << nmi_floppy;
				}

				if (ByteCount <= 1) {
					RotSect++;

					if (RotSect > MaxSects[CurrentDrive]) {
						RotSect = 0;
					}
				}

				if (ByteCount <= 1 && !MultiSect) {
					Status &= ~(WD1770_STATUS_DATA_REQUEST |
					            WD1770_STATUS_BUSY);
					NMIStatus |= 1 << nmi_floppy;
					fseek(CurrentDisc, HeadPos[CurrentDrive], SEEK_SET);
					FDCommand = 10;
				}

				if (ByteCount <= 1 && MultiSect) {
					ByteCount = SecSize[CurrentDrive] + 1;
					Sector++;

					if (Sector == MaxSects[CurrentDrive]) {
						MultiSect = false;
						// Sector = 0;
					}
				}

				LoadingCycles = BYTE_TIME; // Bit longer for a write
			}

			return;
		}

		if (FDCommand == 7 && !DWriteable[CurrentDrive]) {
			// Status &= ~WD1770_STATUS_BUSY;
			Status |= WD1770_STATUS_WRITE_PROTECT;
			NMIStatus |= 1 << nmi_floppy;
			FDCommand = 0;
		}

		if (FDCommand >= 8 && CurrentDiscOpen() && FDCommand <= 10) { // Read/Write Prepare
			Status |= WD1770_STATUS_BUSY;
			Status &= ~(WD1770_STATUS_WRITE_PROTECT |
			            WD1770_STATUS_SPIN_UP_COMPLETE |
			            WD1770_STATUS_LOST_DATA);
			ByteCount = SecSize[CurrentDrive] + 1;
			DataPos = ftell(CurrentDisc);
			HeadPos[CurrentDrive] = DataPos;
			LoadingCycles = 45;
			fseek(CurrentDisc, DiscStrt[CurrentDrive] + (DiscStep[CurrentDrive] * Track) + (Sector * SecSize[CurrentDrive]), SEEK_SET);
		}

		if (FDCommand >= 8 && !CurrentDiscOpen() && FDCommand <= 9) {
			Status &= ~WD1770_STATUS_BUSY;
			Status |= WD1770_STATUS_RECORD_NOT_FOUND;
			NMIStatus |= 1 << nmi_floppy;
			FDCommand = 0;
		}

		if (FDCommand == 8 && CurrentDiscOpen()) FDCommand = 6;

		if (FDCommand == 9 && CurrentDiscOpen()) {
			Status |= WD1770_STATUS_DATA_REQUEST;
			NMIStatus |= 1 << nmi_floppy;
			FDCommand = 7;
		}
	}

// Not implemented Read Track yet, perhaps don't really need it

//	if (FDCommand == 22) { // Read Track
//		LoadingCycles -= NCycles; if (LoadingCycles>0) return;
//		if ((dStatus & 2)==0) {
//			NextFDCommand = 0;
//			ResetStatus(4); ResetStatus(5); ResetStatus(3); ResetStatus(2);
//			if (!feof(CurrentDisc)) { Data=fgetc(CurrentDisc); SetStatus(1); NMIStatus |= 1 << nmi_floppy; } // DRQ
//			dByteCount--;
//			if (dByteCount==0) RotSect++; if (RotSect>MaxSects[CurrentDrive]) RotSect=0;
//			if (dByteCount == 0 && !MultiSect) { ResetStatus(0); NMIStatus |= 1 << nmi_floppy; fseek(CurrentDisc, HeadPos[CurrentDrive], SEEK_SET); FDCommand = 10; } // End of sector
//			if (dByteCount == 0 && MultiSect) {
//				dByteCount = 257; Sector++;
//				if (Sector == MaxSects[CurrentDrive]) { MultiSect = false; /* Sector = 0; */ }
//			}
//			LoadingCycles = BYTE_TIME; // Slow down the read a bit :)
//		}
//		return;
//	}

	if (FDCommand == 23 && DWriteable[CurrentDrive]) { // Write Track
		LoadingCycles -= NCycles;
		if (LoadingCycles > 0) {
			return;
		}

		if (!(Status & WD1770_STATUS_DATA_REQUEST)) {

			// if (ByteCount == 0)
			//	WriteLog("Formatting Track %d, Sector %d\n", Track, Sector);

			NextFDCommand = 0;
			Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
			            WD1770_STATUS_RECORD_NOT_FOUND |
			            WD1770_STATUS_CRC_ERROR |
			            WD1770_STATUS_LOST_DATA);
			Status |= WD1770_STATUS_DATA_REQUEST;
			NMIStatus |= 1 << nmi_floppy;

			switch (FormatState)
			{
				case 0x00:
					FormatPtr = FormatBuffer;
					*FormatPtr++ = Data;
					FormatState++;
					break;

				case 0x01:
					*FormatPtr++ = Data;

					if (Data == 0xfb) // Data Marker
					{
						FormatState++;
						FormatCount = 0;
						FormatSize = 256; // Assume default
						unsigned char *ptr = FormatBuffer;
						while (*ptr != 0xfe) ptr++;
						ptr++;

						switch (ptr[3])
						{
							case 0:
								FormatSize = 128;
								break;

							case 1:
								FormatSize = 256;
								break;

							case 2:
								FormatSize = 512;
								MaxSects[CurrentDrive] = 8;
								SecSize[CurrentDrive] = 512;
								DiskDensity[CurrentDrive] = 0;
								DiscStep[CurrentDrive] = 512 * 9 * 2;
								DiscStrt[CurrentDrive] = ptr[1] * 512 * 9; // Head number 0 or 1
								DefStart[CurrentDrive] = 512 * 9;
								TrkLen[CurrentDrive] = 512 * 9;
								DscType[CurrentDrive] = DiscType::DOS;
								break;

							case 3:
								FormatSize = 1024;
								MaxSects[CurrentDrive] = 4;
								SecSize[CurrentDrive] = 1024;
								DiskDensity[CurrentDrive] = 0;
								DiscStep[CurrentDrive] = 1024 * 5 * 2;
								DiscStrt[CurrentDrive] = ptr[1] * 1024 * 5; // Head number 0 or 1
								DefStart[CurrentDrive] = 1024 * 5;
								TrkLen[CurrentDrive] = 1024 * 5;
								DscType[CurrentDrive] = DiscType::IMG;
								break;
						}
					}
					break;

				case 0x02: // Sector contents
					if (FormatCount < FormatSize)
					{
						fputc(Data,CurrentDisc);
						FormatCount++;
					}
					else
					{
						FormatState++;
						FormatCount = 0;
						Sector++;

						if (Sector > MaxSects[CurrentDrive])
						{
							Status &= ~(WD1770_STATUS_DATA_REQUEST |
							            WD1770_STATUS_BUSY);
							NMIStatus |= 1 << nmi_floppy;
							fseek(CurrentDisc, HeadPos[CurrentDrive], SEEK_SET);
							FDCommand = 10;
						}
					}
					break;

				case 0x03: // 0xF7 - write CRC
					FormatState = 0;
					break;
			}

			LoadingCycles = BYTE_TIME; // Bit longer for a write
		}
		return;
	}

	if (FDCommand == 23 && !DWriteable[CurrentDrive]) {
		// WriteLog(tlog, "Disc Write Protected\n");
		Status |= WD1770_STATUS_WRITE_PROTECT;
		NMIStatus |= 1 << nmi_floppy;
		FDCommand = 0;
	}

	if (FDCommand >= 20 && CurrentDiscOpen() && FDCommand <= 21) { // Read/Write Track Prepare
		Status |= WD1770_STATUS_BUSY;
		Status &= ~(WD1770_STATUS_WRITE_PROTECT |
		            WD1770_STATUS_SPIN_UP_COMPLETE |
		            WD1770_STATUS_LOST_DATA);
		LoadingCycles = 45;
		fseek(CurrentDisc, DiscStrt[CurrentDrive] + (DiscStep[CurrentDrive] * Track), SEEK_SET);
		Sector = 0;
		ByteCount = 0;
		DataPos = ftell(CurrentDisc);
		HeadPos[CurrentDrive] = DataPos;
		// WriteLog("Read/Write Track Prepare - Disc = %d, Track = %d\n", CurrentDrive, Track);
	}

	if (FDCommand >= 20 && !CurrentDiscOpen() && FDCommand <= 21) {
		// WriteLog("ResetStatus(0) Here 8\n");
		Status &= ~WD1770_STATUS_BUSY;
		Status |= WD1770_STATUS_RECORD_NOT_FOUND;
		NMIStatus |= 1 << nmi_floppy;
		FDCommand = 0;
	}

	if (FDCommand == 20 && CurrentDiscOpen()) {
		FDCommand = 22;
	}

	if (FDCommand == 21 && CurrentDiscOpen()) {
		FDCommand = 23;
		FormatState = 0;
		Status |= WD1770_STATUS_DATA_REQUEST;
		NMIStatus |= 1 << nmi_floppy;
	}

	if (FDCommand == 10) {
		Status &= ~(WD1770_STATUS_WRITE_PROTECT |
		            WD1770_STATUS_SPIN_UP_COMPLETE |
		            WD1770_STATUS_RECORD_NOT_FOUND |
		            WD1770_STATUS_BUSY);

		if (NextFDCommand == 255) {
			// Error during access
			UpdateTR00Status();

			Status |= WD1770_STATUS_RECORD_NOT_FOUND;
			Status &= ~WD1770_STATUS_CRC_ERROR;

		}
		NMIStatus |= 1 << nmi_floppy;
		FDCommand = 12;
		LoadingCycles = SPIN_DOWN_TIME; // Spin-down delay
		return;
	}

	if (FDCommand == 11) {
		// Spin-up Delay
		LoadingCycles -= NCycles;
		if (LoadingCycles <= 0) {
			FDCommand = NextFDCommand;
			if (NextFDCommand < 6) {
				Status |= WD1770_STATUS_SPIN_UP_COMPLETE;
			}
			else {
				Status &= ~WD1770_STATUS_SPIN_UP_COMPLETE;
			}
		}
		return;
	}

	if (FDCommand == 12) {
		// Spin Down delay
		if (Verify) LoadingCycles += SETTLE_TIME;
		LightsOn[CurrentDrive] = true;
		SpinDown[CurrentDrive] = SPIN_DOWN_TIME;
		RotSect = 0;
		FDCommand = 0;
		Status &= ~WD1770_STATUS_BUSY;
		return;
	}

	if (FDCommand == 13) {
		// Confusion spin
		LoadingCycles -= NCycles;

		if (LoadingCycles < 2000 && !(Status & WD1770_STATUS_SPIN_UP_COMPLETE)) {
			// Complete spin-up, but continuing whirring
			Status |= WD1770_STATUS_SPIN_UP_COMPLETE;
		}

		if (LoadingCycles <= 0) FDCommand = 10; NextFDCommand = 255; // Go to spin down, but report error.
		SpinDown[CurrentDrive] = SPIN_DOWN_TIME;
		LightsOn[CurrentDrive] = true;
		return;
	}

	if (FDCommand == 14) {
		// Read Address - just 6 bytes
		LoadingCycles -= NCycles;
		if (LoadingCycles > 0) {
			return;
		}

		if (!(Status & WD1770_STATUS_DATA_REQUEST)) {
			NextFDCommand = 0;
			Status &= ~(WD1770_STATUS_SPIN_UP_COMPLETE |
			            WD1770_STATUS_RECORD_NOT_FOUND |
			            WD1770_STATUS_CRC_ERROR |
			            WD1770_STATUS_LOST_DATA);

			if (ByteCount == 6) {
				Data = Track;
			}
			else if (ByteCount == 5) {
				Data = CurrentHead[CurrentDrive];
			}
			else if (ByteCount == 4) {
				Data = RotSect + 1;
			}
			else if (ByteCount == 3) {
				if (DscType[CurrentDrive] == DiscType::SSD)  Data = 1; // 256
				if (DscType[CurrentDrive] == DiscType::DSD)  Data = 1; // 256
				if (DscType[CurrentDrive] == DiscType::ADFS) Data = 1; // 256
				if (DscType[CurrentDrive] == DiscType::IMG)  Data = 3; // 1024
				if (DscType[CurrentDrive] == DiscType::DOS)  Data = 2; // 512
			}
			else if (ByteCount == 2) {
				Data = 0;
			}
			else if (ByteCount == 1) {
				Data = 0;
			}
			else if (ByteCount == 0) {
				FDCommand = 0;
				Status &= ~WD1770_STATUS_BUSY;
				RotSect++;
				if (RotSect == MaxSects[CurrentDrive] + 1) {
					RotSect = 0;
				}
				FDCommand = 10;
				return;
			}

			Status |= WD1770_STATUS_DATA_REQUEST;
			ByteCount--;
			NMIStatus |= 1 << nmi_floppy;
			LoadingCycles = BYTE_TIME; // Slow down the read a bit :)
		}

		return;
	}
}

Disc1770Result Load1770DiscImage(const char *DscFileName, int DscDrive, DiscType Type) {
	Disc1770Result Result = Disc1770Result::Failed;

	FILE* DiscLoaded = nullptr;

	if (DscDrive == 0) {
		if (Disc0 != nullptr) {
			fclose(Disc0);
		}

		Disc0 = fopen(DscFileName, "rb+");

		if (Disc0 != nullptr) {
			Result = Disc1770Result::OpenedReadWrite;
		}
		else {
			Disc0 = fopen(DscFileName, "rb");

			if (Disc0 != nullptr) {
				Result = Disc1770Result::OpenedReadOnly;
			}
		}

		if (Disc0 != nullptr) {
			if (CurrentDrive == 0) {
				CurrentDisc = Disc0;
			}

			DiscLoaded = Disc0;
		}
	}
	else if (DscDrive == 1) {
		if (Disc1 != nullptr) {
			fclose(Disc1);
		}

		Disc1 = fopen(DscFileName, "rb+");

		if (Disc1 != nullptr) {
			Result = Disc1770Result::OpenedReadWrite;
		}
		else {
			Disc1 = fopen(DscFileName, "rb");

			if (Disc1 != nullptr) {
				Result = Disc1770Result::OpenedReadOnly;
			}
		}

		if (Disc1 != nullptr) {
			if (CurrentDrive == 1) {
				CurrentDisc = Disc1;
			}

			DiscLoaded = Disc1;
		}
	}

	if (Result == Disc1770Result::Failed) {
		return Result;
	}

	strcpy(DscFileNames[DscDrive], DscFileName);

	// if (discType = DiscType::SSD) CurrentHead[DscDrive]=0;
	// Feb 14th 2001 - Valentines Day - Bah Humbug - ADFS Support added here
	if (Type == DiscType::SSD) {
		SecSize[DscDrive] = 256;
		DiskDensity[DscDrive] = 1;
		DiscStep[DscDrive] = 2560;
		DiscStrt[DscDrive] = 0;
		DefStart[DscDrive] = 80 * 10 * 256; // 0;
		TrkLen[DscDrive] = 2560;
	}
	else if (Type == DiscType::DSD) {
		SecSize[DscDrive] = 256;
		DiskDensity[DscDrive] = 1;
		DiscStep[DscDrive] = 5120;
		DiscStrt[DscDrive] = CurrentHead[DscDrive] * 2560;
		DefStart[DscDrive] = 2560;
		TrkLen[DscDrive] = 2560;
	}
	else if (Type == DiscType::IMG) {
		SecSize[DscDrive] = 1024;
		DiskDensity[DscDrive] = 0;
		DiscStep[DscDrive] = 1024 * 5 * 2;
		DiscStrt[DscDrive] = CurrentHead[DscDrive] * 1024 * 5;
		DefStart[DscDrive] = 1024 * 5;
		TrkLen[DscDrive] = 1024 * 5;
	}
	else if (Type == DiscType::DOS) {
		SecSize[DscDrive] = 512;
		DiskDensity[DscDrive] = 0;
		DiscStep[DscDrive] = 512 * 9 * 2;
		DiscStrt[DscDrive] = CurrentHead[DscDrive] * 512 * 9;
		DefStart[DscDrive] = 512 * 9;
		TrkLen[DscDrive] = 512 * 9;
	}
	else if (Type == DiscType::ADFS) {
		SecSize[DscDrive] = 256;
		DiskDensity[DscDrive] = 0;
		DiscStep[DscDrive] = 8192;
		DiscStrt[DscDrive] = CurrentHead[DscDrive] * 4096;
		DefStart[DscDrive] = 4096;
		TrkLen[DscDrive] = 4096;
		// This is a quick check to see what type of disc the ADFS disc is.
		// Bytes 0xfc - 0xfe is the total number of sectors.
		// In an ADFS L disc, this is 0xa00 (160 Tracks)
		// for and ADFS M disc, this is 0x500 (80 Tracks)
		// and for the dreaded ADFS S disc, this is 0x280
		long HeadStore = ftell(DiscLoaded);
		fseek(DiscLoaded, 0xfc, SEEK_SET);
		long TotalSectors = fgetc(DiscLoaded);
		TotalSectors |= fgetc(DiscLoaded) << 8;
		TotalSectors |= fgetc(DiscLoaded) << 16;
		fseek(DiscLoaded, HeadStore, SEEK_SET);
		if (TotalSectors == 0x500 || TotalSectors == 0x280) { // Just so 1024 sector mixed mode ADFS/NET discs can be recognised as dbl sided
			DiscStep[DscDrive] = 4096;
			DiscStrt[DscDrive] = 0;
			DefStart[DscDrive] = 0;
			TrkLen[DscDrive] = 4096;
		}
	}

	DscType[DscDrive] = Type;
	MaxSects[DscDrive] = (Type == DiscType::SSD || Type == DiscType::DSD) ? 9 : 15;
	if (Type == DiscType::IMG) MaxSects[DscDrive] = 4;
	if (Type == DiscType::DOS) MaxSects[DscDrive] = 8;

	return Result;
}

// This function writes the control register at &FE24.

void WriteFDCControlReg(unsigned char Value) {
	// WriteLog("Disc1770: CTRL REG write of %02X\n", Value);

	ExtControl = Value;

	if (ExtControl & DRIVE_CONTROL_SELECT_DRIVE_0) {
		CurrentDisc = Disc0;
		CurrentDrive = 0;
	}

	if (ExtControl & DRIVE_CONTROL_SELECT_DRIVE_1) {
		CurrentDisc = Disc1;
		CurrentDrive = 1;
	}

	if ((ExtControl & DRIVE_CONTROL_SELECT_SIDE) && CurrentHead[CurrentDrive] == 0) {
		// Select side 1
		CurrentHead[CurrentDrive] = 1;
		if (CurrentDisc != nullptr) {
			fseek(CurrentDisc, TrkLen[CurrentDrive], SEEK_CUR);
		}
		DiscStrt[CurrentDrive] = DefStart[CurrentDrive];
	}

	if (!(ExtControl & DRIVE_CONTROL_SELECT_SIDE) && CurrentHead[CurrentDrive] == 1) {
		// Select side 0
		CurrentHead[CurrentDrive] = 0;
		if (CurrentDisc != nullptr) {
			fseek(CurrentDisc, 0 - TrkLen[CurrentDrive], SEEK_CUR);
		}
		DiscStrt[CurrentDrive] = 0;
	}

	// Density Select: 0 = Double, 1 = Single
	SelectedDensity = (Value & DRIVE_CONTROL_SELECT_DENSITY) != 0;
	// SelectedDensity = 1;
}

unsigned char ReadFDCControlReg() {
	return ExtControl;
}

void Reset1770() {
	CurrentDisc = Disc0;
	CurrentDrive = 0;
	CurrentHead[0] = 0;
	CurrentHead[1] = 0;
	DiscStrt[0] = 0;
	DiscStrt[1] = 0;
	if (Disc0) fseek(Disc0, 0, SEEK_SET);
	if (Disc1) fseek(Disc1, 0, SEEK_SET);
	SetMotor(0, false);
	SetMotor(1, false);
	Status = 0;
	ExtControl = DRIVE_CONTROL_SELECT_DRIVE_0; // Drive 0 selected, single density, side 0
	MaxSects[0] = (DscType[0] == DiscType::SSD || DscType[0] == DiscType::DSD) ? 9 : 15;
	MaxSects[1] = (DscType[1] == DiscType::SSD || DscType[1] == DiscType::DSD) ? 9 : 15;
	if (DscType[0] == DiscType::IMG) MaxSects[0] = 4;
	if (DscType[1] == DiscType::IMG) MaxSects[1] = 4;
	if (DscType[0] == DiscType::DOS) MaxSects[0] = 8;
	if (DscType[1] == DiscType::DOS) MaxSects[1] = 8;
}

void Close1770Disc(int Drive)
{
	if (Drive == 0 && Disc0 != nullptr) {
		fclose(Disc0);
		Disc0 = nullptr;
		DscFileNames[0][0] = 0;
	}

	if (Drive == 1 && Disc1 != nullptr) {
		fclose(Disc1);
		Disc1 = nullptr;
		DscFileNames[1][0] = 0;
	}
}

#define BPUT(a) fputc(a, file); CheckSum = (CheckSum + a) & 0xff

// This function creates a blank ADFS disc image.

bool CreateADFSImage(const char *FileName, int Tracks) {
	int ent;
	const int sectors = (Tracks * 16);
	FILE *file = fopen(FileName, "wb");
	if (file != nullptr) {
		// Now write the data - this is complex
		// Free space map - T0S0 - start sectors
		int CheckSum = 0;
		BPUT(7); BPUT(0); BPUT(0);
		for (ent = 0; ent < 0xf9; ent++) BPUT(0);
		BPUT(sectors & 0xff); BPUT((sectors >> 8) & 0xff); BPUT(0); // Total sectors
		BPUT(CheckSum & 0xff); // Checksum Byte
		CheckSum = 0;
		// Free space map - T0S1 - lengths
		BPUT((sectors - 7) & 0xff); BPUT(((sectors - 7) >> 8) & 0xff); BPUT(0); // Length of first free space
		for (ent = 0; ent < 0xfb; ent++) BPUT(0);
		BPUT(3); BPUT(CheckSum);
		// Root Catalogue - T0S2-T0S7
		BPUT(1);
		BPUT('H'); BPUT('u'); BPUT('g'); BPUT('o'); // Hugo
		for (ent = 0; ent < 47; ent++) {
			// 47 catalogue entries
			for (int bcount = 5; bcount < 0x1e; bcount++) BPUT(0);
			BPUT(ent);
		}
		BPUT(0);
		BPUT('$'); // Set root directory name
		BPUT(13);
		for (ent = 0x0; ent < 11; ent++) BPUT(0);
		BPUT('$'); // Set root title name
		BPUT(13);
		for (ent = 0x0; ent < 31; ent++) BPUT(0);
		BPUT(1);
		BPUT('H'); BPUT('u'); BPUT('g'); BPUT('o'); // Hugo
		BPUT(0);
		fclose(file);

		return true;
	}

	return false;
}

void Save1770UEF(FILE *SUEF)
{
	extern char FDCDLL[256];
	extern char CDiscName[2][256];
	char blank[256];
	memset(blank,0,256);

	fput16(0x046F,SUEF);
	fput32(857,SUEF);
	fputc(static_cast<int>(DscType[0]), SUEF);
	fputc(static_cast<int>(DscType[1]), SUEF);

	if (Disc0 != nullptr) {
		fwrite(CDiscName[0],1,256,SUEF);
	}
	else {
		// No disc in drive 0
		fwrite(blank,1,256,SUEF);
	}

	if (Disc1 != nullptr) {
		fwrite(CDiscName[1],1,256,SUEF);
	}
	else {
		// No disc in drive 1
		fwrite(blank,1,256,SUEF);
	}

	fputc(Status,SUEF);
	fputc(Data,SUEF);
	fputc(Track,SUEF);
	fputc(ATrack,SUEF);
	fputc(Sector,SUEF);
	fputc(HeadDir,SUEF);
	fputc(FDCommand,SUEF);
	fputc(NextFDCommand,SUEF);
	fput32(LoadingCycles,SUEF);
	fput32(SpinDown[0],SUEF);
	fput32(SpinDown[1],SUEF);
	fputc(UpdateTrack,SUEF);
	fputc(MultiSect,SUEF);
	fputc(StepRate,SUEF);
	fputc(SpinUp,SUEF);
	fputc(Verify,SUEF);
	fputc(LightsOn[0],SUEF);
	fputc(LightsOn[1],SUEF);
	fput32(ByteCount,SUEF);
	fput32(DataPos,SUEF);
	fputc(ExtControl,SUEF);
	fputc(CurrentDrive,SUEF);
	fput32(HeadPos[0],SUEF);
	fput32(HeadPos[1],SUEF);
	fputc(CurrentHead[0],SUEF);
	fputc(CurrentHead[1],SUEF);
	fput32(DiscStep[0],SUEF);
	fput32(DiscStep[1],SUEF);
	fput32(DiscStrt[0],SUEF);
	fput32(DiscStrt[1],SUEF);
	fputc(MaxSects[0],SUEF);
	fputc(MaxSects[1],SUEF);
	fput32(DefStart[0],SUEF);
	fput32(DefStart[1],SUEF);
	fput32(TrkLen[0],SUEF);
	fput32(TrkLen[1],SUEF);
	fputc(DWriteable[0],SUEF);
	fputc(DWriteable[1],SUEF);
	fputc(DiskDensity[0],SUEF);
	fputc(DiskDensity[1],SUEF);
	fputc(SelectedDensity,SUEF);
	fputc(RotSect,SUEF);
	fwrite(FDCDLL,1,256,SUEF);
}

void Load1770UEF(FILE *SUEF, int Version)
{
	extern char FDCDLL[256];
	extern bool DiscLoaded[2];
	char FileName[256];
	bool Loaded = false;
	bool LoadFailed = false;

	// Close current images, don't want them corrupted if
	// saved state was in middle of writing to disc.
	Close1770Disc(0);
	Close1770Disc(1);
	DiscLoaded[0] = false;
	DiscLoaded[1] = false;

	DscType[0] = static_cast<DiscType>(fgetc(SUEF));
	DscType[1] = static_cast<DiscType>(fgetc(SUEF));

	fread(FileName,1,256,SUEF);
	if (FileName[0]) {
		// Load drive 0
		Loaded = true;
		mainWin->Load1770DiscImage(FileName, 0, DscType[0]);
		if (Disc0 == nullptr)
			LoadFailed = true;
	}

	fread(FileName,1,256,SUEF);
	if (FileName[0]) {
		// Load drive 1
		Loaded = true;
		mainWin->Load1770DiscImage(FileName, 1, DscType[1]);
		if (Disc1 == nullptr)
			LoadFailed = true;
	}

	if (Loaded && !LoadFailed) {
		Status = fget8(SUEF);
		Data = fget8(SUEF);
		Track = fget8(SUEF);
		ATrack = fget8(SUEF);
		Sector = fget8(SUEF);
		HeadDir = fgetbool(SUEF);
		FDCommand = fget8(SUEF);
		NextFDCommand = fget8(SUEF);
		LoadingCycles = fget32(SUEF);
		SpinDown[0] = fget32(SUEF);
		SpinDown[1] = fget32(SUEF);
		UpdateTrack = fgetbool(SUEF);
		MultiSect = fgetbool(SUEF);
		StepRate = fget8(SUEF);
		SpinUp = fgetbool(SUEF);
		Verify = fgetbool(SUEF);
		LightsOn[0] = fgetbool(SUEF);
		LightsOn[1] = fgetbool(SUEF);
		ByteCount = fget32(SUEF);
		DataPos = fget32(SUEF);
		ExtControl = fget8(SUEF);
		CurrentDrive = fget8(SUEF);
		HeadPos[0] = fget32(SUEF);
		HeadPos[1] = fget32(SUEF);
		CurrentHead[0] = fget8(SUEF);
		CurrentHead[1] = fget8(SUEF);
		DiscStep[0] = fget32(SUEF);
		DiscStep[1] = fget32(SUEF);
		DiscStrt[0] = fget32(SUEF);
		DiscStrt[1] = fget32(SUEF);
		MaxSects[0] = fget8(SUEF);
		MaxSects[1] = fget8(SUEF);
		DefStart[0] = fget32(SUEF);
		DefStart[1] = fget32(SUEF);
		TrkLen[0] = fget32(SUEF);
		TrkLen[1] = fget32(SUEF);
		DWriteable[0] = fgetbool(SUEF);
		DWriteable[1] = fgetbool(SUEF);
		DiskDensity[0] = fgetbool(SUEF);
		if (Version <= 9)
			DiskDensity[1] = DiskDensity[0];
		else
			DiskDensity[1] = fgetbool(SUEF);
		SelectedDensity = fgetbool(SUEF);
		RotSect = fget8(SUEF);
		fread(FDCDLL,1,256,SUEF);

		if (MachineType != Model::Master128) {
			mainWin->LoadFDC(FDCDLL, false);
		}
	}
}

void Get1770DiscInfo(int DscDrive, DiscType *Type, char *pFileName)
{
	*Type = DscType[DscDrive];
	strcpy(pFileName, DscFileNames[DscDrive]);
}
