/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt

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

/* 04/12/1994 David Alan Gilbert: 8271 disc emulation  */
/* 30/08/1997 Mike Wyatt: Added disc write and format support */
/* 27/12/2011 J.G.Harston: Double-sided SSD supported */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "6502core.h"
#include "disc8271.h"
#include "UEFState.h"
#include "beebsound.h"
#include "sysvia.h"
#include "tube.h"
#include "Log.h"
#include "DebugTrace.h"

#ifdef WIN32
#include <windows.h>
#include "Main.h"
#endif

#define ENABLE_LOG 0

// 8271 Status register
const unsigned char STATUS_REG_COMMAND_BUSY       = 0x80;
const unsigned char STATUS_REG_COMMAND_FULL       = 0x40;
const unsigned char STATUS_REG_PARAMETER_FULL     = 0x20;
const unsigned char STATUS_REG_RESULT_FULL        = 0x10;
const unsigned char STATUS_REG_INTERRUPT_REQUEST  = 0x08;
const unsigned char STATUS_REG_NON_DMA_MODE       = 0x04;

// 8271 Result register
const unsigned char RESULT_REG_SUCCESS            = 0x00;
const unsigned char RESULT_REG_SCAN_NOT_MET       = 0x00;
const unsigned char RESULT_REG_SCAN_MET_EQUAL     = 0x02;
const unsigned char RESULT_REG_SCAN_MET_NOT_EQUAL = 0x04;
const unsigned char RESULT_REG_CLOCK_ERROR        = 0x08;
const unsigned char RESULT_REG_LATE_DMA           = 0x0A;
const unsigned char RESULT_REG_ID_CRC_ERROR       = 0x0C;
const unsigned char RESULT_REG_DATA_CRC_ERROR     = 0x0E;
const unsigned char RESULT_REG_DRIVE_NOT_READY    = 0x10;
const unsigned char RESULT_REG_WRITE_PROTECT      = 0x12;
const unsigned char RESULT_REG_TRACK_0_NOT_FOUND  = 0x14;
const unsigned char RESULT_REG_WRITE_FAULT        = 0x16;
const unsigned char RESULT_REG_SECTOR_NOT_FOUND   = 0x18;
const unsigned char RESULT_REG_DRIVE_NOT_PRESENT  = 0x1E; // Undocumented, see http://beebwiki.mdfs.net/OSWORD_%267F
const unsigned char RESULT_REG_DELETED_DATA_FOUND = 0x20;

// 8271 special registers
const unsigned char SPECIAL_REG_SCAN_SECTOR_NUMBER        = 0x06;
const unsigned char SPECIAL_REG_SCAN_COUNT_MSB            = 0x14;
const unsigned char SPECIAL_REG_SCAN_COUNT_LSB            = 0x13;
const unsigned char SPECIAL_REG_SURFACE_0_CURRENT_TRACK   = 0x12;
const unsigned char SPECIAL_REG_SURFACE_1_CURRENT_TRACK   = 0x1A;
const unsigned char SPECIAL_REG_MODE_REGISTER             = 0x17;
const unsigned char SPECIAL_REG_DRIVE_CONTROL_OUTPUT_PORT = 0x23;
const unsigned char SPECIAL_REG_DRIVE_CONTROL_INPUT_PORT  = 0x22;
const unsigned char SPECIAL_REG_SURFACE_0_BAD_TRACK_1     = 0x10;
const unsigned char SPECIAL_REG_SURFACE_0_BAD_TRACK_2     = 0x11;
const unsigned char SPECIAL_REG_SURFACE_1_BAD_TRACK_1     = 0x18;
const unsigned char SPECIAL_REG_SURFACE_1_BAD_TRACK_2     = 0x19;

bool Disc8271Enabled = true;
int Disc8271Trigger; /* Cycle based time Disc8271Trigger */
static unsigned char ResultReg;
static unsigned char StatusReg;
static unsigned char DataReg;
static unsigned char Internal_Scan_SectorNum;
static unsigned int Internal_Scan_Count; /* Read as two bytes */
static unsigned char Internal_ModeReg;
static unsigned char Internal_CurrentTrack[2]; /* 0/1 for surface number */
static unsigned char Internal_DriveControlOutputPort;
static unsigned char Internal_DriveControlInputPort;
static unsigned char Internal_BadTracks[2][2]; /* 1st subscript is surface 0/1 and second subscript is badtrack 0/1 */

// State set by the Specify (initialisation) command
// See Intel 8271 data sheet, page 15, ADUG page 39-40
static int StepRate; // In 2ms steps
static int HeadSettlingTime; // In 2ms steps
static int IndexCountBeforeHeadUnload; // Number of revolutions (0 to 14), or 15 to keep loaded
static int HeadLoadTime; // In 8ms steps

static int DriveHeadPosition[2]={0};
static bool DriveHeadLoaded=false;
static bool DriveHeadUnloadPending=false;

static int ThisCommand;
static int NParamsInThisCommand;
static int PresentParam; /* From 0 */
static unsigned char Params[16]; /* Wildly more than we need */

// These bools indicate which drives the last command selected.
// They also act as "drive ready" bits which are reset when the motor stops.
static bool Selects[2]; /* Drive selects */
static bool Writeable[2]={false,false}; /* True if the drives are writeable */

static bool FirstWriteInt; // Indicates the start of a write operation

static unsigned char NextInterruptIsErr; // non-zero causes error and drops this value into result reg

#define TRACKSPERDRIVE 80

/* Note Head select is done from bit 5 of the drive output register */
#define CURRENTHEAD ((Internal_DriveControlOutputPort>>5) & 1)

/* Note: reads/writes one byte every 80us */
#define TIMEBETWEENBYTES (160)

struct SectorType {

  struct IDFieldType {
    unsigned int CylinderNum:7;
    unsigned int RecordNum:5;
    unsigned int HeadNum:1;
    unsigned int PhysRecLength;
  } IDField;

  bool Deleted; // If true the sector is deleted
  unsigned char *Data;
};

struct TrackType {
  int LogicalSectors; /* Number of sectors stated in format command */
  int NSectors; /* i.e. the number of records we have - not anything physical */
  SectorType *Sectors;
  int Gap1Size,Gap3Size,Gap5Size; /* From format command */
};

/* All data on the disc - first param is drive number, then head. then physical track id */
TrackType DiscStore[2][2][TRACKSPERDRIVE];

/* File names of loaded disc images */
static char FileNames[2][256];

/* Number of sides of loaded disc images */
static int NumHeads[2];

static bool SaveTrackImage(int DriveNum, int HeadNum, int TrackNum);
static void DriveHeadScheduleUnload(void);

typedef void (*CommandFunc)(void);

#define UPDATENMISTATUS                           \
  if (StatusReg & STATUS_REG_INTERRUPT_REQUEST) { \
    NMIStatus |= 1 << nmi_floppy;                 \
  }                                               \
  else {                                          \
    NMIStatus &= ~(1 << nmi_floppy);              \
  }

/*--------------------------------------------------------------------------*/

struct CommandStatusType {
  int TrackAddr;
  int CurrentSector;
  int SectorLength; /* In bytes */
  int SectorsToGo;

  SectorType *CurrentSectorPtr;
  TrackType *CurrentTrackPtr;

  int ByteWithinSector; /* Next byte in sector or ID field */
};

static CommandStatusType CommandStatus;

/*--------------------------------------------------------------------------*/

struct PrimaryCommandLookupType {
  unsigned char CommandNum;
  unsigned char Mask; /* Mask command with this before comparing with CommandNum - allows drive ID to be removed */
  int NParams; /* Number of parameters to follow */
  CommandFunc ToCall; /* Called after all paameters have arrived */
  CommandFunc IntHandler; /* Called when interrupt requested by command is about to happen */
  const char *Ident; /* Mainly for debugging */
};

/*--------------------------------------------------------------------------*/
/* For appropriate commands checks the select bits in the command code and  */
/* selects the appropriate drive.                                           */
static void DoSelects(void) {
  Selects[0]=(ThisCommand & 0x40)!=0;
  Selects[1]=(ThisCommand & 0x80)!=0;
  Internal_DriveControlOutputPort&=0x3f;
  if (Selects[0]) Internal_DriveControlOutputPort|=0x40;
  if (Selects[1]) Internal_DriveControlOutputPort|=0x80;
}

/*--------------------------------------------------------------------------*/
static void NotImp(const char *NotImpCom) {
  mainWin->Report(MessageType::Error,
                  "Disc operation '%s' not supported", NotImpCom);
}

/*--------------------------------------------------------------------------*/
/* Load the head - ignore for the moment                                    */
static void DoLoadHead(void) {
}

/*--------------------------------------------------------------------------*/
/* Initialise our disc structures                                           */
static void InitDiscStore(void) {
  int head,track,drive;
  TrackType blank={0,0,NULL,0,0,0};

  for(drive=0;drive<2;drive++)
    for(head=0;head<2;head++)
      for(track=0;track<TRACKSPERDRIVE;track++)
        DiscStore[drive][head][track]=blank;
}

/*--------------------------------------------------------------------------*/
/* Given a logical track number accounts for bad tracks                     */
static int SkipBadTracks(int Unit, int trackin) {
  int offset=0;
  if (TubeType != Tube::TorchZ80) // If running under Torch Z80, ignore bad tracks
  {
    if (Internal_BadTracks[Unit][0]<=trackin) offset++;
    if (Internal_BadTracks[Unit][1]<=trackin) offset++;
  }
  return(trackin+offset);
}

/*--------------------------------------------------------------------------*/

static int GetSelectedDrive() {
  if (Selects[0]) {
    return 0;
  }

  if (Selects[1]) {
    return 1;
  }

  return -1;
}

/*--------------------------------------------------------------------------*/
/* Returns a pointer to the data structure for a particular track.  You     */
/* pass the logical track number, it takes into account bad tracks and the  */
/* drive select and head select etc.  It always returns a valid ptr - if    */
/* there aren't that many tracks then it uses the last one.                 */
/* The one exception!!!! is that if no drives are selected it returns NULL  */
static TrackType *GetTrackPtr(int LogicalTrackID) {
  int Drive = GetSelectedDrive();

  if (Drive < 0) {
    return nullptr;
  }

  LogicalTrackID = SkipBadTracks(Drive, LogicalTrackID);

  if (LogicalTrackID>=TRACKSPERDRIVE) LogicalTrackID=TRACKSPERDRIVE-1;

  return &DiscStore[Drive][CURRENTHEAD][LogicalTrackID];
}

/*--------------------------------------------------------------------------*/
/* Returns a pointer to the data structure for a particular sector. Returns */
/* NULL for Sector not found. Doesn't check cylinder/head ID                */
static SectorType *GetSectorPtr(const TrackType *Track, int LogicalSectorID, bool FindDeleted) {
  int CurrentSector;

  if (Track->Sectors==NULL) return(NULL);

  for(CurrentSector=0;CurrentSector<Track->NSectors;CurrentSector++)
    if ((Track->Sectors[CurrentSector].IDField.RecordNum==LogicalSectorID) && ((!Track->Sectors[CurrentSector].Deleted) || (!FindDeleted)))
      return(&(Track->Sectors[CurrentSector]));

  return(NULL);
}

/*--------------------------------------------------------------------------*/

// Cause an error - pass err num

static void DoErr(unsigned char ErrNum) {
  SetTrigger(50, Disc8271Trigger); // Give it a bit of time
  NextInterruptIsErr = ErrNum;
  StatusReg = STATUS_REG_COMMAND_BUSY; // Command is busy - come back when I have an interrupt
  UPDATENMISTATUS;
}

/*--------------------------------------------------------------------------*/

// Checks a few things in the sector - returns true if OK

static bool ValidateSector(const SectorType *Sector, int Track, int SecLength) {
  if (Sector->IDField.CylinderNum != Track) {
    return false;
  }

  if (Sector->IDField.PhysRecLength != SecLength) {
    return false;
  }

  return true;
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_ScanDataCommand(void) {
  DoSelects();
  NotImp("DoVarLength_ScanDataCommand");
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_ScanDataAndDeldCommand(void) {
  DoSelects();
  NotImp("DoVarLength_ScanDataAndDeldCommand");
}

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_WriteDataCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_WriteDataCommand");
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_WriteDataCommand(void) {
  DoSelects();
  DoLoadHead();

  const int Drive = GetSelectedDrive();

  if (Drive < 0) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  if (!Writeable[Drive]) {
    DoErr(RESULT_REG_WRITE_PROTECT);
    return;
  }

  Internal_CurrentTrack[Drive]=Params[0];
  CommandStatus.CurrentTrackPtr=GetTrackPtr(Params[0]);
  if (CommandStatus.CurrentTrackPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr, Params[1], false);
  if (CommandStatus.CurrentSectorPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
    return;
  }

  CommandStatus.TrackAddr=Params[0];
  CommandStatus.CurrentSector=Params[1];
  CommandStatus.SectorsToGo=Params[2] & 31;
  CommandStatus.SectorLength=1<<(7+((Params[2] >> 5) & 7));

  if (ValidateSector(CommandStatus.CurrentSectorPtr,CommandStatus.TrackAddr,CommandStatus.SectorLength)) {
    CommandStatus.ByteWithinSector=0;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
    StatusReg = STATUS_REG_COMMAND_BUSY;
    UPDATENMISTATUS;
    CommandStatus.ByteWithinSector=0;
    FirstWriteInt = true;
  } else {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
  }
}

/*--------------------------------------------------------------------------*/
static void WriteInterrupt(void) {
  bool LastByte = false;

  if (CommandStatus.SectorsToGo < 0) {
    StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
    UPDATENMISTATUS;
    return;
  }

  if (!FirstWriteInt)
    CommandStatus.CurrentSectorPtr->Data[CommandStatus.ByteWithinSector++]=DataReg;
  else
    FirstWriteInt = false;

  ResultReg=0;
  if (CommandStatus.ByteWithinSector>=CommandStatus.SectorLength) {
    CommandStatus.ByteWithinSector=0;
    if (--CommandStatus.SectorsToGo) {
      CommandStatus.CurrentSector++;
      CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr,
                                                    CommandStatus.CurrentSector,
                                                    false);
      if (CommandStatus.CurrentSectorPtr==NULL) {
        DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
        return;
      }
    } else {
      /* Last sector done, write the track back to disc */
      if (SaveTrackImage(Selects[0] ? 0 : 1, CURRENTHEAD, CommandStatus.TrackAddr)) {
        StatusReg = STATUS_REG_RESULT_FULL;
        UPDATENMISTATUS;
        LastByte = true;
        CommandStatus.SectorsToGo=-1; /* To let us bail out */
        SetTrigger(0,Disc8271Trigger); /* To pick up result */
      }
      else {
        DoErr(RESULT_REG_WRITE_PROTECT);
      }
    }
  }

  if (!LastByte) {
    StatusReg = STATUS_REG_COMMAND_BUSY |
                STATUS_REG_INTERRUPT_REQUEST |
                STATUS_REG_NON_DMA_MODE;
    UPDATENMISTATUS;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
  }
}

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_WriteDeletedDataCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_WriteDeletedDataCommand");
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_WriteDeletedDataCommand(void) {
  DoSelects();
  NotImp("DoVarLength_WriteDeletedDataCommand");
}

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_ReadDataCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_ReadDataCommand");
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_ReadDataCommand(void) {
  DoSelects();
  DoLoadHead();

  const int Drive = GetSelectedDrive();

  if (Drive < 0) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  // Reset shift state if it was set by Run Disc
  if (mainWin->m_ShiftBooted) {
    mainWin->m_ShiftBooted = false;
    BeebKeyUp(0, 0);
  }

  Internal_CurrentTrack[Drive]=Params[0];
  CommandStatus.CurrentTrackPtr=GetTrackPtr(Params[0]);
  if (CommandStatus.CurrentTrackPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr, Params[1], false);
  if (CommandStatus.CurrentSectorPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT);
    return;
  }

  CommandStatus.TrackAddr=Params[0];
  CommandStatus.CurrentSector=Params[1];
  CommandStatus.SectorsToGo=Params[2] & 31;
  CommandStatus.SectorLength=1<<(7+((Params[2] >> 5) & 7));

  if (ValidateSector(CommandStatus.CurrentSectorPtr,CommandStatus.TrackAddr,CommandStatus.SectorLength)) {
    CommandStatus.ByteWithinSector=0;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
    StatusReg = STATUS_REG_COMMAND_BUSY;
    UPDATENMISTATUS;
  } else {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT);
  }
}

/*--------------------------------------------------------------------------*/
static void ReadInterrupt(void) {
  bool LastByte = false;

  if (CommandStatus.SectorsToGo < 0) {
    StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
    UPDATENMISTATUS;
    return;
  }

  DataReg=CommandStatus.CurrentSectorPtr->Data[CommandStatus.ByteWithinSector++];

  #if ENABLE_LOG
  WriteLog("ReadInterrupt called - DataReg=0x%02X ByteWithinSector=%d\n", DataReg, CommandStatus.ByteWithinSector);
  #endif

  ResultReg=0;
  if (CommandStatus.ByteWithinSector>=CommandStatus.SectorLength) {
    CommandStatus.ByteWithinSector=0;
    /* I don't know if this can cause the thing to step - I presume not for the moment */
    if (--CommandStatus.SectorsToGo) {
      CommandStatus.CurrentSector++;
      CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr,
                                                    CommandStatus.CurrentSector,
                                                    false);
      if (CommandStatus.CurrentSectorPtr == NULL) {
        DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
        return;
      }
    } else {
      /* Last sector done */
      StatusReg = STATUS_REG_COMMAND_BUSY |
                  STATUS_REG_RESULT_FULL |
                  STATUS_REG_INTERRUPT_REQUEST |
                  STATUS_REG_NON_DMA_MODE;
      UPDATENMISTATUS;
      LastByte = true;
      CommandStatus.SectorsToGo=-1; /* To let us bail out */
      SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger); /* To pick up result */
    }
  }

  if (!LastByte) {
    StatusReg = STATUS_REG_COMMAND_BUSY |
                STATUS_REG_INTERRUPT_REQUEST |
                STATUS_REG_NON_DMA_MODE;
    UPDATENMISTATUS;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
  }
}

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_ReadDataAndDeldCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_ReadDataAndDeldCommand");
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_ReadDataAndDeldCommand(void) {
  /* Use normal read command for now - deleted data not supported */
  DoVarLength_ReadDataCommand();
}

/*--------------------------------------------------------------------------*/
static void DoReadIDCommand(void) {
  DoSelects();
  DoLoadHead();

  const int Drive = GetSelectedDrive();

  if (Drive < 0) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  Internal_CurrentTrack[Drive]=Params[0];
  CommandStatus.CurrentTrackPtr=GetTrackPtr(Params[0]);
  if (CommandStatus.CurrentTrackPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr, 0, false);
  if (CommandStatus.CurrentSectorPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
    return;
  }

  CommandStatus.TrackAddr=Params[0];
  CommandStatus.CurrentSector=0;
  CommandStatus.SectorsToGo=Params[2];

  CommandStatus.ByteWithinSector=0;
  SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
  StatusReg = STATUS_REG_COMMAND_BUSY;
  UPDATENMISTATUS;
}

/*--------------------------------------------------------------------------*/
static void ReadIDInterrupt(void) {
  bool LastByte = false;

  if (CommandStatus.SectorsToGo<0) {
    StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
    UPDATENMISTATUS;
    return;
  }

  if (CommandStatus.ByteWithinSector==0)
    DataReg=CommandStatus.CurrentSectorPtr->IDField.CylinderNum;
  else if (CommandStatus.ByteWithinSector==1)
    DataReg=CommandStatus.CurrentSectorPtr->IDField.HeadNum;
  else if (CommandStatus.ByteWithinSector==2)
    DataReg=CommandStatus.CurrentSectorPtr->IDField.RecordNum;
  else
    DataReg=1; /* 1=256 byte sector length */

  CommandStatus.ByteWithinSector++;

  ResultReg=0;
  if (CommandStatus.ByteWithinSector>=4) {
    CommandStatus.ByteWithinSector=0;
    if (--CommandStatus.SectorsToGo) {
      CommandStatus.CurrentSector++;
      CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr,
                                                    CommandStatus.CurrentSector,
                                                    false);
      if (CommandStatus.CurrentSectorPtr==NULL) {
        DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
        return;
      }
    } else {
      /* Last sector done */
      StatusReg = STATUS_REG_COMMAND_BUSY |
                  STATUS_REG_INTERRUPT_REQUEST |
                  STATUS_REG_NON_DMA_MODE;
      UPDATENMISTATUS;
      LastByte = true;
      CommandStatus.SectorsToGo=-1; /* To let us bail out */
      SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger); /* To pick up result */
    }
  }

  if (!LastByte) {
    StatusReg = STATUS_REG_COMMAND_BUSY |
                STATUS_REG_INTERRUPT_REQUEST |
                STATUS_REG_NON_DMA_MODE;
    UPDATENMISTATUS;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
  }
}

/*--------------------------------------------------------------------------*/
static void Do128ByteSR_VerifyDataAndDeldCommand(void) {
  DoSelects();
  NotImp("Do128ByteSR_VerifyDataAndDeldCommand");
}

/*--------------------------------------------------------------------------*/
static void DoVarLength_VerifyDataAndDeldCommand(void) {
  DoSelects();

  const int Drive = GetSelectedDrive();

  if (Drive < 0) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  Internal_CurrentTrack[Drive]=Params[0];
  CommandStatus.CurrentTrackPtr=GetTrackPtr(Params[0]);
  if (CommandStatus.CurrentTrackPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr, Params[1], false);
  if (CommandStatus.CurrentSectorPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
    return;
  }

  StatusReg = STATUS_REG_COMMAND_BUSY;
  UPDATENMISTATUS;
  SetTrigger(100,Disc8271Trigger); /* A short delay to causing an interrupt */
}

/*--------------------------------------------------------------------------*/
static void VerifyInterrupt(void) {
  StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
  UPDATENMISTATUS;
  ResultReg = RESULT_REG_SUCCESS; // All OK
}

/*--------------------------------------------------------------------------*/

static void DoFormatCommand(void) {
  DoSelects();

  DoLoadHead();

  const int Drive = GetSelectedDrive();

  if (Drive < 0) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  if (!Writeable[Drive]) {
    DoErr(RESULT_REG_WRITE_PROTECT);
    return;
  }

  Internal_CurrentTrack[Drive]=Params[0];
  CommandStatus.CurrentTrackPtr=GetTrackPtr(Params[0]);
  if (CommandStatus.CurrentTrackPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr, 0, false);
  if (CommandStatus.CurrentSectorPtr==NULL) {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
    return;
  }

  CommandStatus.TrackAddr=Params[0];
  CommandStatus.CurrentSector=0;
  CommandStatus.SectorsToGo=Params[2] & 31;
  CommandStatus.SectorLength=1<<(7+((Params[2] >> 5) & 7));

  if (CommandStatus.SectorsToGo==10 && CommandStatus.SectorLength==256) {
    CommandStatus.ByteWithinSector=0;
    SetTrigger(TIMEBETWEENBYTES,Disc8271Trigger);
    StatusReg = STATUS_REG_COMMAND_BUSY;
    UPDATENMISTATUS;
    FirstWriteInt = true;
  } else {
    DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
  }
}

/*--------------------------------------------------------------------------*/
static void FormatInterrupt(void) {
  bool LastByte = false;

  if (CommandStatus.SectorsToGo<0) {
    StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
    UPDATENMISTATUS;
    return;
  }

  if (!FirstWriteInt) {
    /* Ignore the ID data for now - just count the bytes */
    CommandStatus.ByteWithinSector++;
  }
  else
    FirstWriteInt = false;

  ResultReg=0;
  if (CommandStatus.ByteWithinSector>=4) {
    /* Fill sector with 0xe5 chars */
    for (int i = 0; i < 256; ++i) {
      CommandStatus.CurrentSectorPtr->Data[i]=(unsigned char)0xe5;
    }

    CommandStatus.ByteWithinSector=0;
    if (--CommandStatus.SectorsToGo) {
      CommandStatus.CurrentSector++;
      CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr,
                                                    CommandStatus.CurrentSector,
                                                    false);
      if (CommandStatus.CurrentSectorPtr==NULL) {
        DoErr(RESULT_REG_DRIVE_NOT_PRESENT); // Sector not found
        return;
      }
    } else {
      /* Last sector done, write the track back to disc */
      if (SaveTrackImage(Selects[0] ? 0 : 1, CURRENTHEAD, CommandStatus.TrackAddr)) {
        StatusReg = STATUS_REG_RESULT_FULL;
        UPDATENMISTATUS;
        LastByte = true;
        CommandStatus.SectorsToGo=-1; /* To let us bail out */
        SetTrigger(0,Disc8271Trigger); /* To pick up result */
      }
      else {
        DoErr(RESULT_REG_WRITE_PROTECT);
      }
    }
  }

  if (!LastByte) {
    StatusReg = STATUS_REG_COMMAND_BUSY |
                STATUS_REG_INTERRUPT_REQUEST |
                STATUS_REG_NON_DMA_MODE;
    UPDATENMISTATUS;
    SetTrigger(TIMEBETWEENBYTES * 256,Disc8271Trigger);
  }
}

/*--------------------------------------------------------------------------*/

static void SeekInterrupt() {
  StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
  UPDATENMISTATUS;
  ResultReg = RESULT_REG_SUCCESS; // All OK
}

/*--------------------------------------------------------------------------*/
static void DoSeekCommand(void) {
  DoSelects();

  DoLoadHead();

  int Drive = GetSelectedDrive();

  if (Drive<0) {
    DoErr(RESULT_REG_DRIVE_NOT_READY);
    return;
  }

  Internal_CurrentTrack[Drive]=Params[0];

  StatusReg = STATUS_REG_COMMAND_BUSY;
  UPDATENMISTATUS;
  SetTrigger(100,Disc8271Trigger); /* A short delay to causing an interrupt */
}

/*--------------------------------------------------------------------------*/
static void DoReadDriveStatusCommand(void) {
  bool Track0 = false;
  bool WriteProt = false;

  if (ThisCommand & 0x40) {
    Track0=(Internal_CurrentTrack[0]==0);
    WriteProt=(!Writeable[0]);
  }

  if (ThisCommand & 0x80) {
    Track0=(Internal_CurrentTrack[1]==0);
    WriteProt=(!Writeable[1]);
  }

  ResultReg=0x80 | (Selects[1]?0x40:0) | (Selects[0]?0x4:0) | (Track0?2:0) | (WriteProt?8:0);
  StatusReg |= STATUS_REG_RESULT_FULL;
  UPDATENMISTATUS;
}

/*--------------------------------------------------------------------------*/

// See Intel 8271 data sheet, page 15, ADUG page 39-40

static void DoSpecifyCommand(void) {
  switch (Params[0]) {
    case 0x0D: // Initialisation
      StepRate = Params[1];
      HeadSettlingTime = Params[2];
      IndexCountBeforeHeadUnload = (Params[3] & 0xf0) >> 4;
      HeadLoadTime = Params[3] & 0x0f;
      break;

    case 0x10: // Load bad tracks, surface 0
      Internal_BadTracks[0][0] = Params[1];
      Internal_BadTracks[0][1] = Params[2];
      Internal_CurrentTrack[0] = Params[3];
      break;

    case 0x18: // Load bad tracks, surface 1
      Internal_BadTracks[1][0] = Params[1];
      Internal_BadTracks[1][1] = Params[2];
      Internal_CurrentTrack[1] = Params[3];
      break;
  }
}

/*--------------------------------------------------------------------------*/
static void DoWriteSpecialCommand(void) {
  DoSelects();

  switch (Params[0]) {
    case SPECIAL_REG_SCAN_SECTOR_NUMBER:
      Internal_Scan_SectorNum = Params[1];
      break;

    case SPECIAL_REG_SCAN_COUNT_MSB:
      Internal_Scan_Count &= 0xff;
      Internal_Scan_Count |= Params[1] << 8;
      break;

    case SPECIAL_REG_SCAN_COUNT_LSB:
      Internal_Scan_Count &= 0xff00;
      Internal_Scan_Count |= Params[1];
      break;

    case SPECIAL_REG_SURFACE_0_CURRENT_TRACK:
      Internal_CurrentTrack[0] = Params[1];
      break;

    case SPECIAL_REG_SURFACE_1_CURRENT_TRACK:
      Internal_CurrentTrack[1] = Params[1];
      break;

    case SPECIAL_REG_MODE_REGISTER:
      Internal_ModeReg = Params[1];
      break;

    case SPECIAL_REG_DRIVE_CONTROL_OUTPUT_PORT:
      Internal_DriveControlOutputPort = Params[1];
      Selects[0] = (Params[1] & 0x40) != 0;
      Selects[1] = (Params[1] & 0x80) != 0;
      break;

    case SPECIAL_REG_DRIVE_CONTROL_INPUT_PORT:
      Internal_DriveControlInputPort = Params[1];
      break;

    case SPECIAL_REG_SURFACE_0_BAD_TRACK_1:
      Internal_BadTracks[0][0] = Params[1];
      break;

    case SPECIAL_REG_SURFACE_0_BAD_TRACK_2:
      Internal_BadTracks[0][1] = Params[1];
      break;

    case SPECIAL_REG_SURFACE_1_BAD_TRACK_1:
      Internal_BadTracks[1][0] = Params[1];
      break;

    case SPECIAL_REG_SURFACE_1_BAD_TRACK_2:
      Internal_BadTracks[1][1] = Params[1];
      break;

    default:
      #if ENABLE_LOG
      WriteLog("Write to bad special register\n");
      #endif
      break;
  }
}

/*--------------------------------------------------------------------------*/
static void DoReadSpecialCommand(void) {
  DoSelects();

  switch (Params[0]) {
    case SPECIAL_REG_SCAN_SECTOR_NUMBER:
      ResultReg = Internal_Scan_SectorNum;
      break;

    case SPECIAL_REG_SCAN_COUNT_MSB:
      ResultReg = (Internal_Scan_Count >> 8) & 0xff;
      break;

    case SPECIAL_REG_SCAN_COUNT_LSB:
      ResultReg = Internal_Scan_Count & 0xff;
      break;

    case SPECIAL_REG_SURFACE_0_CURRENT_TRACK:
      ResultReg = Internal_CurrentTrack[0];
      break;

    case SPECIAL_REG_SURFACE_1_CURRENT_TRACK:
      ResultReg = Internal_CurrentTrack[1];
      break;

    case SPECIAL_REG_MODE_REGISTER:
      ResultReg = Internal_ModeReg;
      break;

    case SPECIAL_REG_DRIVE_CONTROL_OUTPUT_PORT:
      ResultReg = Internal_DriveControlOutputPort;
      break;

    case SPECIAL_REG_DRIVE_CONTROL_INPUT_PORT:
      ResultReg = Internal_DriveControlInputPort;
      break;

    case SPECIAL_REG_SURFACE_0_BAD_TRACK_1:
      ResultReg = Internal_BadTracks[0][0];
      break;

    case SPECIAL_REG_SURFACE_0_BAD_TRACK_2:
      ResultReg = Internal_BadTracks[0][1];
      break;

    case SPECIAL_REG_SURFACE_1_BAD_TRACK_1:
      ResultReg = Internal_BadTracks[1][0];
      break;

    case SPECIAL_REG_SURFACE_1_BAD_TRACK_2:
      ResultReg = Internal_BadTracks[1][1];
      break;

    default:
      #if ENABLE_LOG
      WriteLog("Read of bad special register\n");
      #endif
      return;
  }

  StatusReg |= STATUS_REG_RESULT_FULL;
  UPDATENMISTATUS;
}

/*--------------------------------------------------------------------------*/
static void DoBadCommand(void) {
}

/*--------------------------------------------------------------------------*/
/* The following table is used to parse commands from the command number written into
the command register - it can't distinguish between subcommands selected from the
first parameter */
static const PrimaryCommandLookupType PrimaryCommandLookup[]={
  {0x00, 0x3f, 3, DoVarLength_ScanDataCommand, NULL,  "Scan Data (Variable Length/Multi-Record)"},
  {0x04, 0x3f, 3, DoVarLength_ScanDataAndDeldCommand, NULL,  "Scan Data & deleted data (Variable Length/Multi-Record)"},
  {0x0a, 0x3f, 2, Do128ByteSR_WriteDataCommand, NULL, "Write Data (128 byte/single record)"},
  {0x0b, 0x3f, 3, DoVarLength_WriteDataCommand, WriteInterrupt, "Write Data (Variable Length/Multi-Record)"},
  {0x0e, 0x3f, 2, Do128ByteSR_WriteDeletedDataCommand, NULL, "Write Deleted Data (128 byte/single record)"},
  {0x0f, 0x3f, 3, DoVarLength_WriteDeletedDataCommand, NULL, "Write Deleted Data (Variable Length/Multi-Record)"},
  {0x12, 0x3f, 2, Do128ByteSR_ReadDataCommand, NULL, "Read Data (128 byte/single record)"},
  {0x13, 0x3f, 3, DoVarLength_ReadDataCommand, ReadInterrupt, "Read Data (Variable Length/Multi-Record)"},
  {0x16, 0x3f, 2, Do128ByteSR_ReadDataAndDeldCommand, NULL, "Read Data & deleted data (128 byte/single record)"},
  {0x17, 0x3f, 3, DoVarLength_ReadDataAndDeldCommand, ReadInterrupt, "Read Data & deleted data (Variable Length/Multi-Record)"},
  {0x1b, 0x3f, 3, DoReadIDCommand, ReadIDInterrupt, "ReadID" },
  {0x1e, 0x3f, 2, Do128ByteSR_VerifyDataAndDeldCommand, NULL, "Verify Data and Deleted Data (128 byte/single record)"},
  {0x1f, 0x3f, 3, DoVarLength_VerifyDataAndDeldCommand, VerifyInterrupt, "Verify Data and Deleted Data (Variable Length/Multi-Record)"},
  {0x23, 0x3f, 5, DoFormatCommand, FormatInterrupt, "Format"},
  {0x29, 0x3f, 1, DoSeekCommand, SeekInterrupt, "Seek"},
  {0x2c, 0x3f, 0, DoReadDriveStatusCommand, NULL, "Read drive status"},
  {0x35, 0xff, 4, DoSpecifyCommand, NULL, "Specify" },
  {0x3a, 0x3f, 2, DoWriteSpecialCommand, NULL, "Write special registers" },
  {0x3d, 0x3f, 1, DoReadSpecialCommand, NULL, "Read special registers" },
  {0,    0,    0, DoBadCommand, NULL, "Unknown command"} /* Terminator due to 0 mask matching all */
};

/*--------------------------------------------------------------------------*/
/* returns a pointer to the data structure for the given command            */
/* If no matching command is given, the pointer points to an entry with a 0 */
/* mask, with a sensible function to call.                                  */
static const PrimaryCommandLookupType *CommandPtrFromNumber(int CommandNumber) {
  const PrimaryCommandLookupType *presptr=PrimaryCommandLookup;

  for(;presptr->CommandNum!=(presptr->Mask & CommandNumber);presptr++);

  return(presptr);
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-7 - with the fe80 etc stripped out

unsigned char Disc8271Read(int Address) {
  unsigned char Value = 0;

  if (!Disc8271Enabled)
    return 0xFF;

  switch (Address) {
    case 0:
      #if ENABLE_LOG
      WriteLog("8271 Status register read (0x%0X)\n", StatusReg);
      #endif

      Value=StatusReg;
      break;

    case 1:
      #if ENABLE_LOG
      WriteLog("8271 Result register read (0x%02X)\n", ResultReg);
      #endif

      // Clear interrupt request and result reg full flag
      StatusReg &= ~(STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST);
      UPDATENMISTATUS;
      Value=ResultReg;
      ResultReg = RESULT_REG_SUCCESS; // Register goes to 0 after its read
      break;

    case 4:
      #if ENABLE_LOG
      WriteLog("8271 data register read\n");
      #endif

      // Clear interrupt and non-dma request - not stated but DFS never looks at result reg!
      StatusReg &= ~(STATUS_REG_INTERRUPT_REQUEST | STATUS_REG_NON_DMA_MODE);
      UPDATENMISTATUS;
      Value=DataReg;
      break;

    default:
      #if ENABLE_LOG
      WriteLog("8271: Read to unknown register address=%04X\n", Address);
      #endif
      break;
  }

  return(Value);
}

/*--------------------------------------------------------------------------*/
static void CommandRegWrite(int Value) {
  const PrimaryCommandLookupType *ptr = CommandPtrFromNumber(Value);

  #if ENABLE_LOG
  WriteLog("8271: Command register write value=0x%02X (Name=%s)\n", Value, ptr->Ident);
  #endif

  ThisCommand=Value;
  NParamsInThisCommand=ptr->NParams;
  PresentParam=0;

  StatusReg |= STATUS_REG_COMMAND_BUSY | STATUS_REG_RESULT_FULL; // Observed on beeb for read special
  UPDATENMISTATUS;

  // No parameters then call routine immediately
  if (NParamsInThisCommand==0) {
    StatusReg&=0x7e;
    UPDATENMISTATUS;
    ptr->ToCall();
  }
}

/*--------------------------------------------------------------------------*/

static void ParamRegWrite(unsigned char Value) {
  // Parameter wanted ?
  if (PresentParam>=NParamsInThisCommand) {
    #if ENABLE_LOG
    WriteLog("8271: Unwanted parameter register write value=0x%02X\n", Value);
    #endif
  } else {
    Params[PresentParam++]=Value;

    StatusReg&=0xfe; /* Observed on beeb */
    UPDATENMISTATUS;

    // Got all params yet?
    if (PresentParam>=NParamsInThisCommand) {

      StatusReg&=0x7e; /* Observed on beeb */
      UPDATENMISTATUS;

      const PrimaryCommandLookupType *ptr = CommandPtrFromNumber(ThisCommand);

      #if ENABLE_LOG
      WriteLog("<Disc access> 8271: All parameters arrived for '%s':", ptr->Ident);

      for (int i = 0; i < PresentParam; i++) {
        WriteLog(" %02X", Params[i]);
      }

      WriteLog("\n");
      #endif

      ptr->ToCall();
    }
  }
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-7 - with the fe80 etc stripped out

void Disc8271Write(int Address, unsigned char Value) {
  if (!Disc8271Enabled)
    return;

  // Clear a pending head unload
  if (DriveHeadUnloadPending) {
    DriveHeadUnloadPending = false;
    ClearTrigger(Disc8271Trigger);
  }

  switch (Address) {
    case 0:
      CommandRegWrite(Value);
      break;

    case 1:
      ParamRegWrite(Value);
      break;

    case 2:
      // DebugTrace("8271: Reset register write, value=0x%02X\n", Value);

      // The caller should write a 1 and then >11 cycles later a 0 - but I'm just going
      // to reset on both edges
      Disc8271Reset();
      break;

    case 4:
      // DebugTrace("8271: Data register write, value=0x%02X\n", Value);

      StatusReg &= ~(STATUS_REG_INTERRUPT_REQUEST | STATUS_REG_NON_DMA_MODE);
      UPDATENMISTATUS;
      DataReg=Value;
      break;

    default:
      // DebugTrace("8271: Write to unknown register address=%04X, value=%02X\n", Address, Value);
      break;
  }

  DriveHeadScheduleUnload();
}

/*--------------------------------------------------------------------------*/
static void DriveHeadScheduleUnload(void) {
	// Schedule head unload when nothing else is pending.
	// This is mainly for the sound effects, but it also marks the drives as
	// not ready when the motor stops.
	if (DriveHeadLoaded && Disc8271Trigger==CycleCountTMax) {
		SetTrigger(4000000,Disc8271Trigger); // 2s delay to unload
		DriveHeadUnloadPending = true;
	}
}

/*--------------------------------------------------------------------------*/
static bool DriveHeadMotorUpdate(void) {
	// This is mainly for the sound effects, but it also marks the drives as
	// not ready when the motor stops.
	int Drive=0;
	int Tracks=0;

	if (DriveHeadUnloadPending) {
		// Mark drives as not ready
		Selects[0] = false;
		Selects[1] = false;
		DriveHeadUnloadPending = false;
		if (DriveHeadLoaded && DiscDriveSoundEnabled)
			PlaySoundSample(SAMPLE_HEAD_UNLOAD, false);
		DriveHeadLoaded = false;
		StopSoundSample(SAMPLE_DRIVE_MOTOR);
		StopSoundSample(SAMPLE_HEAD_SEEK);

		LEDs.Disc0 = false;
		LEDs.Disc1 = false;
		return true;
	}

	if (!DiscDriveSoundEnabled)
	{
		DriveHeadLoaded = true;
		return false;
	}

	if (!DriveHeadLoaded) {
		if (Selects[0]) LEDs.Disc0 = true;
		if (Selects[1]) LEDs.Disc1 = true;

		PlaySoundSample(SAMPLE_DRIVE_MOTOR, true);
		DriveHeadLoaded = true;
		PlaySoundSample(SAMPLE_HEAD_LOAD, false);
		SetTrigger(SAMPLE_HEAD_LOAD_CYCLES, Disc8271Trigger);
		return true;
	}

	if (Selects[0]) Drive = 0;
	if (Selects[1]) Drive = 1;

	StopSoundSample(SAMPLE_HEAD_SEEK);

	if (DriveHeadPosition[Drive] != Internal_CurrentTrack[Drive]) {
		Tracks = abs(DriveHeadPosition[Drive] - Internal_CurrentTrack[Drive]);
		if (Tracks > 1) {
			PlaySoundSample(SAMPLE_HEAD_SEEK, true);
			SetTrigger(Tracks * SAMPLE_HEAD_SEEK_CYCLES_PER_TRACK, Disc8271Trigger);
		}
		else {
			PlaySoundSample(SAMPLE_HEAD_STEP, false);
			SetTrigger(SAMPLE_HEAD_STEP_CYCLES, Disc8271Trigger);
		}
		if (DriveHeadPosition[Drive] < Internal_CurrentTrack[Drive])
			DriveHeadPosition[Drive] += Tracks;
		else
			DriveHeadPosition[Drive] -= Tracks;

		return true;
	}
	return false;
}

/*--------------------------------------------------------------------------*/

void Disc8271_poll_real() {
  ClearTrigger(Disc8271Trigger);

  if (DriveHeadMotorUpdate())
    return;

  // Set the interrupt flag in the status register
  StatusReg |= STATUS_REG_INTERRUPT_REQUEST;
  UPDATENMISTATUS;

  if (NextInterruptIsErr != 0) {
    ResultReg=NextInterruptIsErr;
    StatusReg = STATUS_REG_RESULT_FULL | STATUS_REG_INTERRUPT_REQUEST;
    UPDATENMISTATUS;
    NextInterruptIsErr=0;
  } else {
    /* Should only happen while a command is still active */
    const PrimaryCommandLookupType *comptr = CommandPtrFromNumber(ThisCommand);
    if (comptr->IntHandler!=NULL) comptr->IntHandler();
  }

  DriveHeadScheduleUnload();
}

/*--------------------------------------------------------------------------*/

void FreeDiscImage(int DriveNum) {
  for (int Track = 0; Track < TRACKSPERDRIVE; Track++) {
    for (int Head = 0; Head < 2; Head++) {
      SectorType *SecPtr = DiscStore[DriveNum][Head][Track].Sectors;

      if (SecPtr != NULL) {
        for (int Sector = 0; Sector < 10; Sector++) {
          if (SecPtr[Sector].Data != NULL) {
            free(SecPtr[Sector].Data);
            SecPtr[Sector].Data = NULL;
          }
        }
        free(SecPtr);
        DiscStore[DriveNum][Head][Track].Sectors = NULL;
      }
    }
  }
}

/*--------------------------------------------------------------------------*/
void LoadSimpleDiscImage(const char *FileName, int DriveNum, int HeadNum, int Tracks) {
  FILE *infile=fopen(FileName,"rb");
  if (!infile) {
    mainWin->Report(MessageType::Error,
                    "Could not open disc file:\n  %s", FileName);

    return;
  }

  mainWin->SetImageName(FileName, DriveNum, DiscType::SSD);

  // JGH, 26-Dec-2011
  NumHeads[DriveNum] = 1; // 1 = TRACKSPERDRIVE SSD image
                          // 2 = 2 * TRACKSPERDRIVE DSD image
  int Heads = 1;
  fseek(infile, 0L, SEEK_END);
  if (ftell(infile)>0x40000) {
    Heads = 2; // Long sequential image continues onto side 1
    NumHeads[DriveNum] = 0; // 0 = 2 * TRACKSPERDRIVE SSD image
  }
  fseek(infile, 0L, SEEK_SET);
  // JGH

  strcpy(FileNames[DriveNum], FileName);
  FreeDiscImage(DriveNum);

  for (int Head = HeadNum; Head < Heads; Head++) {
    for (int CurrentTrack = 0; CurrentTrack < Tracks; CurrentTrack++) {
      DiscStore[DriveNum][Head][CurrentTrack].LogicalSectors=10;
      DiscStore[DriveNum][Head][CurrentTrack].NSectors=10;
      SectorType *SecPtr = DiscStore[DriveNum][Head][CurrentTrack].Sectors = (SectorType*)calloc(10, sizeof(SectorType));
      DiscStore[DriveNum][Head][CurrentTrack].Gap1Size=0; /* Don't bother for the mo */
      DiscStore[DriveNum][Head][CurrentTrack].Gap3Size=0;
      DiscStore[DriveNum][Head][CurrentTrack].Gap5Size=0;

      for (int CurrentSector = 0; CurrentSector < 10; CurrentSector++) {
        SecPtr[CurrentSector].IDField.CylinderNum=CurrentTrack;
        SecPtr[CurrentSector].IDField.RecordNum=CurrentSector;
        SecPtr[CurrentSector].IDField.HeadNum=HeadNum;
        SecPtr[CurrentSector].IDField.PhysRecLength=256;
        SecPtr[CurrentSector].Deleted = false;
        SecPtr[CurrentSector].Data=(unsigned char *)calloc(1,256);
        fread(SecPtr[CurrentSector].Data,1,256,infile);
      }
    }
  }

  fclose(infile);
}

/*--------------------------------------------------------------------------*/
void LoadSimpleDSDiscImage(const char *FileName, int DriveNum, int Tracks) {
  FILE *infile=fopen(FileName,"rb");

  if (!infile) {
    mainWin->Report(MessageType::Error,
                    "Could not open disc file:\n  %s", FileName);

    return;
  }

  mainWin->SetImageName(FileName, DriveNum, DiscType::DSD);

  strcpy(FileNames[DriveNum], FileName);
  NumHeads[DriveNum] = 2;		/* 2 = 2*TRACKSPERDRIVE DSD image */

  FreeDiscImage(DriveNum);

  for (int CurrentTrack = 0; CurrentTrack < Tracks; CurrentTrack++) {
    for (int HeadNum = 0; HeadNum < 2; HeadNum++) {
      DiscStore[DriveNum][HeadNum][CurrentTrack].LogicalSectors=10;
      DiscStore[DriveNum][HeadNum][CurrentTrack].NSectors=10;
      SectorType *SecPtr = DiscStore[DriveNum][HeadNum][CurrentTrack].Sectors = (SectorType *)calloc(10,sizeof(SectorType));
      DiscStore[DriveNum][HeadNum][CurrentTrack].Gap1Size=0; /* Don't bother for the mo */
      DiscStore[DriveNum][HeadNum][CurrentTrack].Gap3Size=0;
      DiscStore[DriveNum][HeadNum][CurrentTrack].Gap5Size=0;

      for (int CurrentSector = 0; CurrentSector < 10; CurrentSector++) {
        SecPtr[CurrentSector].IDField.CylinderNum=CurrentTrack;
        SecPtr[CurrentSector].IDField.RecordNum=CurrentSector;
        SecPtr[CurrentSector].IDField.HeadNum=HeadNum;
        SecPtr[CurrentSector].IDField.PhysRecLength=256;
        SecPtr[CurrentSector].Deleted = false;
        SecPtr[CurrentSector].Data=(unsigned char *)calloc(1,256);
        fread(SecPtr[CurrentSector].Data,1,256,infile);
      }
    }
  }

  fclose(infile);
}

/*--------------------------------------------------------------------------*/
void Eject8271DiscImage(int DriveNum) {
  strcpy(FileNames[DriveNum], "");
  FreeDiscImage(DriveNum);
}

/*--------------------------------------------------------------------------*/

static bool SaveTrackImage(int DriveNum, int HeadNum, int TrackNum) {
  bool Success = true;

  FILE *outfile=fopen(FileNames[DriveNum],"r+b");

  if (!outfile) {
    mainWin->Report(MessageType::Error,
                    "Could not open disc file for write:\n  %s", FileNames[DriveNum]);

    return false;
  }

  long FileOffset;

  if(NumHeads[DriveNum]) {
    FileOffset = (NumHeads[DriveNum] * TrackNum + HeadNum) * 2560; /* 1=SSD, 2=DSD */
  }
  else {
    FileOffset = (TrackNum + HeadNum * TRACKSPERDRIVE) * 2560; /* 0=2-sided SSD */
  }

  /* Get the file length to check if the file needs extending */
  long FileLength = 0;

  Success = fseek(outfile, 0L, SEEK_END) == 0;
  if (Success)
  {
    FileLength=ftell(outfile);
    if (FileLength == -1L) {
      Success = false;
    }
  }

  while (Success && FileOffset > FileLength)
  {
    if (fputc(0, outfile) == EOF)
      Success = false;
    FileLength++;
  }

  if (Success)
  {
    Success = fseek(outfile, FileOffset, SEEK_SET) == 0;

    SectorType *SecPtr = DiscStore[DriveNum][HeadNum][TrackNum].Sectors;

    for (int CurrentSector = 0; Success && CurrentSector < 10; CurrentSector++) {
      if (fwrite(SecPtr[CurrentSector].Data,1,256,outfile) != 256) {
        Success = false;
      }
    }
  }

  if (fclose(outfile) != 0) {
    Success = false;
  }

  if (!Success) {
    mainWin->Report(MessageType::Error,
                    "Failed writing to disc file:\n  %s", FileNames[DriveNum]);
  }

  return Success;
}

/*--------------------------------------------------------------------------*/
bool IsDiscWritable(int DriveNum) {
  return Writeable[DriveNum];
}

/*--------------------------------------------------------------------------*/
void DiscWriteEnable(int DriveNum, bool WriteEnable) {
  int HeadNum;
  SectorType *SecPtr;
  unsigned char *Data;
  int File;
  int Catalogue, NumCatalogues;
  int NumSecs;
  int StartSec, LastSec;
  bool DiscOK = true;

  Writeable[DriveNum] = WriteEnable;

  /* If disc is being made writable then check that the disc catalogue will
     not get corrupted if new files are added.  The files in the disc catalogue
     must be in descending sector order otherwise the DFS ROMs write over
     files at the start of the disc.  The sector count in the catalogue must
     also be correct. */
  if (WriteEnable) {
    for(HeadNum=0; DiscOK && HeadNum<NumHeads[DriveNum]; HeadNum++) {
      SecPtr=DiscStore[DriveNum][HeadNum][0].Sectors;
      if (SecPtr==NULL)
        return; /* No disc image! */

      Data=SecPtr[1].Data;

      /* Check for a Watford DFS 62 file catalogue */
      NumCatalogues=2;
      Data=SecPtr[2].Data;
      for (int i=0; i<8; ++i)
        if (Data[i]!=(unsigned char)0xaa) {
          NumCatalogues=1;
          break;
        }

      for (Catalogue=0; DiscOK && Catalogue<NumCatalogues; ++Catalogue) {
        Data=SecPtr[Catalogue*2+1].Data;

        /* First check the number of sectors */
        NumSecs=((Data[6]&3)<<8)+Data[7];
        if (NumSecs != 0x320 && NumSecs != 0x190) {
          DiscOK = false;
        } else {

          /* Now check the start sectors of each file */
          LastSec=0x320;
          for (File=0; DiscOK && File<Data[5]/8; ++File) {
            StartSec=((Data[File*8+14]&3)<<8)+Data[File*8+15];
            if (LastSec < StartSec)
              DiscOK = false;
            LastSec=StartSec;
          }
        } /* if num sectors OK */
      } /* for catalogue */
    } /* for disc head */

    if (!DiscOK)
    {
      mainWin->Report(MessageType::Error,
                      "WARNING - Invalid Disc Catalogue\n\n"
                      "This disc image will get corrupted if files are written to it.\n"
                      "Copy all the files to a new image to fix it.");
    }
  }
}

/*--------------------------------------------------------------------------*/

void Disc8271Reset() {
  static bool InitialInit = true;

  ResultReg=0;
  StatusReg=0;

  UPDATENMISTATUS;

  Internal_Scan_SectorNum=0;
  Internal_Scan_Count=0; /* Read as two bytes */
  Internal_ModeReg=0;
  Internal_CurrentTrack[0]=Internal_CurrentTrack[1]=0; /* 0/1 for surface number */
  Internal_DriveControlOutputPort=0;
  Internal_DriveControlInputPort=0;
  Internal_BadTracks[0][0]=Internal_BadTracks[0][1]=Internal_BadTracks[1][0]=Internal_BadTracks[1][1]=0xff; /* 1st subscript is surface 0/1 and second subscript is badtrack 0/1 */

  // Default values set by Acorn DFS:
  StepRate = 12;
  HeadSettlingTime = 10;
  IndexCountBeforeHeadUnload = 12;
  HeadLoadTime = 8;

  if (DriveHeadLoaded) {
    DriveHeadUnloadPending = true;
    DriveHeadMotorUpdate();
  }

  ClearTrigger(Disc8271Trigger); /* No Disc8271Triggered events yet */

  ThisCommand=-1;
  NParamsInThisCommand=0;
  PresentParam=0;
  Selects[0]=Selects[1]=false;

  if (InitialInit) {
    InitialInit = false;
    InitDiscStore();
  }
}

/*--------------------------------------------------------------------------*/

void Save8271UEF(FILE *SUEF)
{
	char blank[256];
	memset(blank,0,256);

	fput16(0x046E,SUEF);
	fput32(613,SUEF);

	if (DiscStore[0][0][0].Sectors == NULL) {
		// No disc in drive 0
		fwrite(blank,1,256,SUEF);
	}
	else {
		fwrite(FileNames[0],1,256,SUEF);
	}
	if (DiscStore[1][0][0].Sectors == NULL) {
		// No disc in drive 1
		fwrite(blank,1,256,SUEF);
	}
	else {
		fwrite(FileNames[1],1,256,SUEF);
	}

	if (Disc8271Trigger == CycleCountTMax)
		fput32(Disc8271Trigger,SUEF);
	else
		fput32(Disc8271Trigger - TotalCycles,SUEF);
	fputc(ResultReg,SUEF);
	fputc(StatusReg,SUEF);
	fputc(DataReg,SUEF);
	fputc(Internal_Scan_SectorNum,SUEF);
	fput32(Internal_Scan_Count,SUEF);
	fputc(Internal_ModeReg,SUEF);
	fputc(Internal_CurrentTrack[0],SUEF);
	fputc(Internal_CurrentTrack[1],SUEF);
	fputc(Internal_DriveControlOutputPort,SUEF);
	fputc(Internal_DriveControlInputPort,SUEF);
	fputc(Internal_BadTracks[0][0],SUEF);
	fputc(Internal_BadTracks[0][1],SUEF);
	fputc(Internal_BadTracks[1][0],SUEF);
	fputc(Internal_BadTracks[1][1],SUEF);
	fput32(ThisCommand,SUEF);
	fput32(NParamsInThisCommand,SUEF);
	fput32(PresentParam,SUEF);
	fwrite(Params,1,16,SUEF);
	fput32(NumHeads[0],SUEF);
	fput32(NumHeads[1],SUEF);
	fput32(Selects[0]?1:0,SUEF);
	fput32(Selects[1]?1:0,SUEF);
	fput32(Writeable[0]?1:0,SUEF);
	fput32(Writeable[1]?1:0,SUEF);
	fput32(FirstWriteInt ? 1 : 0,SUEF);
	fput32(NextInterruptIsErr,SUEF);
	fput32(CommandStatus.TrackAddr,SUEF);
	fput32(CommandStatus.CurrentSector,SUEF);
	fput32(CommandStatus.SectorLength,SUEF);
	fput32(CommandStatus.SectorsToGo,SUEF);
	fput32(CommandStatus.ByteWithinSector,SUEF);
}

void Load8271UEF(FILE *SUEF)
{
	extern bool DiscLoaded[2];
	char FileName[256];
	char *ext;
	bool Loaded = false;
	bool LoadFailed = false;

	// Clear out current images, don't want them corrupted if
	// saved state was in middle of writing to disc.
	FreeDiscImage(0);
	FreeDiscImage(1);
	DiscLoaded[0] = false;
	DiscLoaded[1] = false;

	fread(FileName,1,256,SUEF);
	if (FileName[0]) {
		// Load drive 0
		Loaded = true;
		ext = strrchr(FileName, '.');
		if (ext != NULL && _stricmp(ext+1, "dsd") == 0)
			LoadSimpleDSDiscImage(FileName, 0, 80);
		else
			LoadSimpleDiscImage(FileName, 0, 0, 80);

		if (DiscStore[0][0][0].Sectors == NULL)
			LoadFailed = true;
	}

	fread(FileName,1,256,SUEF);
	if (FileName[0]) {
		// Load drive 1
		Loaded = true;
		ext = strrchr(FileName, '.');
		if (ext != NULL && _stricmp(ext+1, "dsd") == 0)
			LoadSimpleDSDiscImage(FileName, 1, 80);
		else
			LoadSimpleDiscImage(FileName, 1, 0, 80);

		if (DiscStore[1][0][0].Sectors == NULL)
			LoadFailed = true;
	}

	if (Loaded && !LoadFailed)
	{
		Disc8271Trigger=fget32(SUEF);
		if (Disc8271Trigger != CycleCountTMax)
			Disc8271Trigger+=TotalCycles;

		ResultReg = fget8(SUEF);
		StatusReg = fget8(SUEF);
		DataReg = fget8(SUEF);
		Internal_Scan_SectorNum = fget8(SUEF);
		Internal_Scan_Count=fget32(SUEF);
		Internal_ModeReg = fget8(SUEF);
		Internal_CurrentTrack[0] = fget8(SUEF);
		Internal_CurrentTrack[1] = fget8(SUEF);
		Internal_DriveControlOutputPort = fget8(SUEF);
		Internal_DriveControlInputPort = fget8(SUEF);
		Internal_BadTracks[0][0] = fget8(SUEF);
		Internal_BadTracks[0][1] = fget8(SUEF);
		Internal_BadTracks[1][0] = fget8(SUEF);
		Internal_BadTracks[1][1] = fget8(SUEF);
		ThisCommand=fget32(SUEF);
		NParamsInThisCommand=fget32(SUEF);
		PresentParam=fget32(SUEF);
		fread(Params,1,16,SUEF);
		NumHeads[0]=fget32(SUEF);
		NumHeads[1]=fget32(SUEF);
		Selects[0]=fget32(SUEF) != 0;
		Selects[1]=fget32(SUEF) != 0;
		Writeable[0]=fget32(SUEF) != 0;
		Writeable[1]=fget32(SUEF) != 0;
		FirstWriteInt=fget32(SUEF) != 0;
		NextInterruptIsErr=fget32(SUEF);
		CommandStatus.TrackAddr=fget32(SUEF);
		CommandStatus.CurrentSector=fget32(SUEF);
		CommandStatus.SectorLength=fget32(SUEF);
		CommandStatus.SectorsToGo=fget32(SUEF);
		CommandStatus.ByteWithinSector=fget32(SUEF);

		CommandStatus.CurrentTrackPtr=GetTrackPtr(CommandStatus.TrackAddr);
		if (CommandStatus.CurrentTrackPtr!=NULL)
			CommandStatus.CurrentSectorPtr = GetSectorPtr(CommandStatus.CurrentTrackPtr,
			                                              CommandStatus.CurrentSector,
			                                              false);
		else
			CommandStatus.CurrentSectorPtr=NULL;
	}
}

/*--------------------------------------------------------------------------*/

void disc8271_dumpstate()
{
	WriteLog("8271:\n");
	WriteLog("  ResultReg=%02X\n", ResultReg);
	WriteLog("  StatusReg=%02X\n", StatusReg);
	WriteLog("  DataReg=%02X\n", DataReg);
	WriteLog("  Internal_Scan_SectorNum=%d\n", Internal_Scan_SectorNum);
	WriteLog("  Internal_Scan_Count=%u\n", Internal_Scan_Count);
	WriteLog("  Internal_ModeReg=%02X\n", Internal_ModeReg);
	WriteLog("  Internal_CurrentTrack=%d, %d\n", Internal_CurrentTrack[0],
	                                             Internal_CurrentTrack[1]);
	WriteLog("  Internal_DriveControlOutputPort=%02X\n", Internal_DriveControlOutputPort);
	WriteLog("  Internal_DriveControlInputPort=%02X\n", Internal_DriveControlInputPort);
	WriteLog("  Internal_BadTracks=(%d, %d) (%d, %d)\n", Internal_BadTracks[0][0],
	                                                     Internal_BadTracks[0][1],
	                                                     Internal_BadTracks[1][0],
	                                                     Internal_BadTracks[1][1]);
	WriteLog("  Disc8271Trigger=%d\n", Disc8271Trigger);
	WriteLog("  ThisCommand=%d\n", ThisCommand);
	WriteLog("  NParamsInThisCommand=%d\n", NParamsInThisCommand);
	WriteLog("  PresentParam=%d\n", PresentParam);
	WriteLog("  Selects=%d, %d\n", Selects[0], Selects[1]);
	WriteLog("  NextInterruptIsErr=%02X\n", NextInterruptIsErr);
}

/*--------------------------------------------------------------------------*/
void Get8271DiscInfo(int DriveNum, char *pFileName, int *Heads)
{
	strcpy(pFileName, FileNames[DriveNum]);
	*Heads = NumHeads[DriveNum];
}
