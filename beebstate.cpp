/****************************************************************************/
/*                               Beebem                                     */
/*                               ------                                     */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/****************************************************************************/
/* Beeb state save and restore funcitonality - Mike Wyatt 7/6/97 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "6502core.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "beebsound.h"
#include "beebmem.h"
#include "atodconv.h"
#include "beebstate.h"
#include "main.h"

#ifdef WIN32
#include <windows.h>
#endif

/*--------------------------------------------------------------------------*/
void BeebSaveState(char *FileName)
{
	FILE *StateFile;
	BeebState StateData;
	size_t StateSize;

	/* Get all the state data */
	memset(&StateData, 0, BEEB_STATE_SIZE);
	strcpy(StateData.Tag, BEEB_STATE_FILE_TAG);
	Save6502State(StateData.CPUState);
	SaveSysVIAState(StateData.SysVIAState);
	SaveUserVIAState(StateData.UserVIAState);
	SaveVideoState(StateData.VideoState);
	SaveMemState(StateData.RomState,StateData.MemState,StateData.SWRamState);

	if (StateData.RomState[0] == 0)
		StateSize = BEEB_STATE_SIZE_NO_SWRAM;
	else
		StateSize = BEEB_STATE_SIZE;

	/* Write the data to the file */
	StateFile = fopen(FileName,"wb");
	if (StateFile != NULL)
	{
		if (fwrite(&StateData,1,StateSize,StateFile) != StateSize)
		{
#ifdef WIN32
			char errstr[200];
			sprintf(errstr, "Failed to write to BeebState file:\n  %s", FileName);
			MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
#else
			fprintf(stderr,"Failed to write to BeebState file: %s\n",FileName);
#endif
		}
		fclose(StateFile);
	}
	else
	{
#ifdef WIN32
	char errstr[200];
	sprintf(errstr, "Cannot open BeebState file:\n  %s", FileName);
	MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
#else
	fprintf(stderr,"Cannot open BeebState file: %s\n",FileName);
#endif
	}
}

/*--------------------------------------------------------------------------*/
void BeebRestoreState(char *FileName)
{
	FILE *StateFile;
	BeebState StateData;
	size_t StateSize;

	memset(&StateData, 0, BEEB_STATE_SIZE);

	/* Read the data from the file */
	StateFile = fopen(FileName,"rb");
	if (StateFile != NULL)
	{
		StateSize = fread(&StateData,1,BEEB_STATE_SIZE,StateFile);
		if ((StateSize == BEEB_STATE_SIZE && StateData.RomState[0] != 0) ||
			(StateSize == BEEB_STATE_SIZE_NO_SWRAM && StateData.RomState[0] == 0))
		{
			if (strcmp(StateData.Tag, BEEB_STATE_FILE_TAG) == 0)
			{
				/* Restore all the state data */
				Restore6502State(StateData.CPUState);
				RestoreSysVIAState(StateData.SysVIAState);
				RestoreUserVIAState(StateData.UserVIAState);
				RestoreVideoState(StateData.VideoState);
				RestoreMemState(StateData.RomState,StateData.MemState,StateData.SWRamState);

				/* Now reset parts of the emulator that are not restored */
				AtoDInit();
				if (SoundEnabled)
				{
					SoundReset();
					SoundInit();
				}
			}
			else
			{
#ifdef WIN32
				char errstr[200];
				sprintf(errstr, "Not a BeebState file:\n  %s", FileName);
				MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
#else
				fprintf(stderr,"Not a BeebState file: %s\n",FileName);
#endif
			}
		}
		else
		{
#ifdef WIN32
			char errstr[200];
			sprintf(errstr, "BeebState file is wrong size:\n  %s", FileName);
			MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
#else
			fprintf(stderr,"BeebState file is wrong size: %s\n",FileName);
#endif
		}
		fclose(StateFile);
	}
	else
	{
#ifdef WIN32
	char errstr[200];
	sprintf(errstr, "Cannot open BeebState file:\n  %s", FileName);
	MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
#else
	fprintf(stderr,"Cannot open BeebState file: %s\n",FileName);
#endif
	}
}

