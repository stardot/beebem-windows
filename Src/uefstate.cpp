/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2009  Mike Wyatt

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

// UEF Game state code.

#include <stdio.h>
#include "6502core.h"
#include "beebmem.h"
#include "video.h"
#include "via.h"
#include "beebwin.h"
#include "main.h"
#include "beebsound.h"
#include "disc8271.h"
#include "disc1770.h"
#include "tube.h"
#include "serial.h"
#include "atodconv.h"

FILE *UEFState;

void fput32(unsigned int word32,FILE *fileptr) {
	fputc(word32&255,fileptr);
	fputc((word32>>8)&255,fileptr);
	fputc((word32>>16)&255,fileptr);
	fputc((word32>>24)&255,fileptr);
}

void fput16(unsigned int word16,FILE *fileptr) {
	fputc(word16&255,fileptr);
	fputc((word16>>8)&255,fileptr);
}

unsigned int fget32(FILE *fileptr) {
	unsigned int tmpvar;
	tmpvar =fgetc(fileptr);
	tmpvar|=fgetc(fileptr)<<8;
	tmpvar|=fgetc(fileptr)<<16;
	tmpvar|=fgetc(fileptr)<<24;
	return(tmpvar);
}

unsigned int fget16(FILE *fileptr) {
	unsigned int tmpvar;
	tmpvar =fgetc(fileptr);
	tmpvar|=fgetc(fileptr)<<8;
	return(tmpvar);
}

void SaveUEFState(char *StateName) {
	UEFState=fopen(StateName,"wb");
	if (UEFState != NULL)
	{
		fprintf(UEFState,"UEF File!");
		fputc(0,UEFState); // UEF Header
		fputc(12,UEFState); fputc(0,UEFState); // Version
		mainWin->SaveEmuUEF(UEFState);
		Save6502UEF(UEFState);
		SaveMemUEF(UEFState);
		SaveVideoUEF(UEFState);
		SaveVIAUEF(UEFState);
		SaveSoundUEF(UEFState);
		if (MachineType!=3 && NativeFDC)
			Save8271UEF(UEFState);
		else
			Save1770UEF(UEFState);
		if (EnableTube) {
			SaveTubeUEF(UEFState);
			Save65C02UEF(UEFState);
			Save65C02MemUEF(UEFState);
		}
		SaveSerialUEF(UEFState);
		SaveAtoDUEF(UEFState);
		fclose(UEFState);
	}
	else
	{
		char errstr[256];
		sprintf(errstr, "Failed to write state file: %s", StateName);
		MessageBox(GETHWND,errstr,"BeebEm",MB_ICONERROR|MB_OK);
	}
}

void LoadUEFState(char *StateName) {
	char errmsg[256];
	char UEFId[10];
	int CompletionBits=0; // These bits should be filled in
	long RPos=0,FLength,CPos;
	unsigned int Block,Length;
	int Version;
	strcpy(UEFId,"BlankFile");
	UEFState=fopen(StateName,"rb");
	if (UEFState != NULL)
	{
		fseek(UEFState,NULL,SEEK_END);
		FLength=ftell(UEFState);
		fseek(UEFState,0,SEEK_SET);  // Get File length for eof comparison.
		fread(UEFId,10,1,UEFState);
		if (strcmp(UEFId,"UEF File!")!=0) {
			MessageBox(GETHWND,"The file selected is not a UEF File.","BeebEm",MB_ICONERROR|MB_OK);
			fclose(UEFState);
			return;
		}
		Version=fget16(UEFState);
		sprintf(errmsg,"UEF Version %x",Version);
		//MessageBox(GETHWND,errmsg,"BeebEm",MB_OK);
		RPos=ftell(UEFState);

		while (ftell(UEFState)<FLength) {
			Block=fget16(UEFState);
			Length=fget32(UEFState);
			CPos=ftell(UEFState);
			sprintf(errmsg,"Block %04X - Length %d (%04X)",Block,Length,Length);
			//MessageBox(GETHWND,errmsg,"BeebEm",MB_ICONERROR|MB_OK);
			if (Block==0x046A) mainWin->LoadEmuUEF(UEFState,Version);
			if (Block==0x0460) Load6502UEF(UEFState);
			if (Block==0x0461) LoadRomRegsUEF(UEFState);
			if (Block==0x0462) LoadMainMemUEF(UEFState);
			if (Block==0x0463) LoadShadMemUEF(UEFState);
			if (Block==0x0464) LoadPrivMemUEF(UEFState);
			if (Block==0x0465) LoadFileMemUEF(UEFState);
			if (Block==0x0466) LoadSWRamMemUEF(UEFState);
			if (Block==0x0467) LoadViaUEF(UEFState);
			if (Block==0x0468) LoadVideoUEF(UEFState);
			if (Block==0x046B) LoadSoundUEF(UEFState);
			if (Block==0x046D) LoadIntegraBHiddenMemUEF(UEFState);
			if (Block==0x046E) Load8271UEF(UEFState);
			if (Block==0x046F) Load1770UEF(UEFState,Version);
			if (Block==0x0470) LoadTubeUEF(UEFState);
			if (Block==0x0471) Load65C02UEF(UEFState);
			if (Block==0x0472) Load65C02MemUEF(UEFState);
			if (Block==0x0473) LoadSerialUEF(UEFState);
			if (Block==0x0474) LoadAtoDUEF(UEFState);
			if (Block==0x0475) LoadSWRomMemUEF(UEFState);
			fseek(UEFState,CPos+Length,SEEK_SET); // Skip unrecognised blocks (and over any gaps)
		}

		fclose(UEFState);

		mainWin->SetRomMenu();
		mainWin->SetDiscWriteProtects();
	}
	else
	{
		char errstr[256];
		sprintf(errstr, "Cannot open state file: %s", StateName);
		MessageBox(GETHWND,errstr,"BeebEm",MB_ICONERROR|MB_OK);
	}
}


