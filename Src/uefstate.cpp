// UEF Game state code for BeebEm V1.38
// Note - This version will be the last of the V1.3 series.
// After that, BeebEm the V1.4 Series will commence.

// (C) June 2001 Richard Gellman

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
		fputc(9,UEFState); fputc(0,UEFState); // Version
		SaveEmuUEF(UEFState);
		Save6502UEF(UEFState);
		SaveMemUEF(UEFState);
		SaveVideoUEF(UEFState);
		SaveVIAUEF(UEFState);
		SaveSoundUEF(UEFState);
		if (MachineType!=3 && NativeFDC)
			Save8271UEF(UEFState);
		else
			Save1770UEF(UEFState);
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
			if (Block==0x046A) LoadEmuUEF(UEFState,Version);
			if (Block==0x0460) Load6502UEF(UEFState);
			if (Block==0x0461) LoadRomRegsUEF(UEFState);
			if (Block==0x0462) LoadMainMemUEF(UEFState);
			if (Block==0x0463) LoadShadMemUEF(UEFState);
			if (Block==0x0464) LoadPrivMemUEF(UEFState);
			if (Block==0x0465) LoadFileMemUEF(UEFState);
			if (Block==0x0466) LoadSWRMMemUEF(UEFState);
			if (Block==0x0467) LoadViaUEF(UEFState);
			if (Block==0x0468) LoadVideoUEF(UEFState);
			if (Block==0x046B) LoadSoundUEF(UEFState);
			if (Block==0x046D) LoadIntegraBHiddenMemUEF(UEFState);
			if (Block==0x046E) Load8271UEF(UEFState);
			if (Block==0x046F) Load1770UEF(UEFState);
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


