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
	fprintf(UEFState,"UEF File!");
	fputc(0,UEFState); // UEF Header
	fputc(8,UEFState); fputc(0,UEFState); // Version
	SaveEmuUEF(UEFState);
	Save6502UEF(UEFState);
	SaveMemUEF(UEFState);
	SaveVideoUEF(UEFState);
	SaveVIAUEF(UEFState);
	SaveSoundUEF(UEFState);
	fclose(UEFState);
}

void LoadUEFState(char *StateName) {
	char errmsg[256];
	char UEFId[10];
	int CompletionBits=0; // These bits should be filled in
	bool BeenReset=FALSE,Rescan=TRUE;;
	long RPos=0,FLength,CPos;
	unsigned int Block,Length;
	int hv,lv;
	bool Recognised;
	strcpy(UEFId,"BlankFile");
	UEFState=fopen(StateName,"rb");
	fseek(UEFState,NULL,SEEK_END);
	FLength=ftell(UEFState);
	fseek(UEFState,0,SEEK_SET);  // Get File length for eof comparison.
	fread(UEFId,10,1,UEFState);
	if (strcmp(UEFId,"UEF File!")!=0) {
		MessageBox(GETHWND,"The file selected is not a UEF File.","BeebEm",MB_ICONERROR|MB_OK);
		return;
	}
	hv=fgetc(UEFState);
	lv=fgetc(UEFState); // Version
	sprintf(errmsg,"UEF Version %d.%d",hv,lv);
	//MessageBox(GETHWND,errmsg,"BeebEm",MB_OK);
	RPos=ftell(UEFState);
	Rescan=TRUE;
	//while (Rescan) {
		while (ftell(UEFState)<FLength) {
			Block=fget16(UEFState);
			Length=fget32(UEFState);
			CPos=ftell(UEFState);
			Recognised=FALSE;
			sprintf(errmsg,"Block %04X - Length %d (%04X)",Block,Length,Length);
			//MessageBox(GETHWND,errmsg,"BeebEm",MB_ICONERROR|MB_OK);
			if (Block==0x046A) {
				//BeenReset=TRUE;
				LoadEmuUEF(UEFState);
				Recognised=TRUE;
				//fseek(UEFState,RPos,SEEK_SET);
			}
			if (BeenReset) Rescan=FALSE; // Don't rescan
			if (Block==0x0460) { Load6502UEF(UEFState); Recognised=TRUE; }
			if (Block==0x0461) { LoadRomRegsUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0462) { LoadMainMemUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0463) { LoadShadMemUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0464) { LoadPrivMemUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0465) { LoadFileMemUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0466) { LoadSWRMMemUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0467) { LoadViaUEF(UEFState); Recognised=TRUE; }
			if (Block==0x0468) { LoadVideoUEF(UEFState); Recognised=TRUE; }
			if (Block==0x046B) { LoadSoundUEF(UEFState); Recognised=TRUE; }
			fseek(UEFState,CPos+Length,SEEK_SET); // Skip unrecognised blocks (and over any gaps)
		}
		// if (!BeenReset) mainWin->ResetBeebSystem(0,0,1);
	//} 
	fclose(UEFState);
}


