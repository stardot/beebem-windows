/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
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
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* 6502 core - 6502 emulator core - David Alan Gilbert 16/10/94 */

/* Mike Wyatt 7/6/97 - Added undocumented instructions */

#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>

#include "6502core.h"
#include "beebmem.h"
#include "beebsound.h"
#include "disc8271.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "atodconv.h"
#include "main.h"

#ifdef WIN32
#include <windows.h>
#define INLINE inline
#else
#define INLINE
#endif

int IgnoreIllegalInstructions = 1;

extern int DumpAfterEach;

CycleCountT TotalCycles=0;

static int ProgramCounter;
static int Accumulator,XReg,YReg;
static unsigned char StackReg,PSR;

unsigned char intStatus=0; /* bit set (nums in IRQ_Nums) if interrupt being caused */
unsigned char NMIStatus=0; /* bit set (nums in NMI_Nums) if NMI being caused */
unsigned int NMILock=0; /* Well I think NMI's are maskable - to stop repeated NMI's - the lock is released when an RTI is done */

typedef int int16;
/* Stats */
int Stats[256];

enum PSRFlags {
  FlagC=1,
  FlagZ=2,
  FlagI=4,
  FlagD=8,
  FlagB=16,
  FlagV=64,
  FlagN=128
};

/* Note how GETCFLAG is special since being bit 0 we don't need to test it to get a clean 0/1 */
#define GETCFLAG ((PSR & FlagC))
#define GETZFLAG ((PSR & FlagZ)>0)
#define GETIFLAG ((PSR & FlagI)>0)
#define GETDFLAG ((PSR & FlagD)>0)
#define GETBFLAG ((PSR & FlagB)>0)
#define GETVFLAG ((PSR & FlagV)>0)
#define GETNFLAG ((PSR & FlagN)>0)

/* Types for internal function arrays */
typedef void (*InstrHandlerFuncType)(int16 Operand);
typedef int16 (*AddrModeHandlerFuncType)(int WantsAddr);

static int CyclesTable[]={
  7,6,0,0,0,3,5,5,3,2,2,0,0,4,6,0, /* 0 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 1 */
  6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0, /* 2 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 3 */
  6,6,0,0,0,3,5,0,3,2,2,2,3,4,6,0, /* 4 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 5 */
  6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0, /* 6 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 7 */
  2,6,0,0,3,3,3,3,2,0,2,0,4,4,4,0, /* 8 */
  2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0, /* 9 */
  2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0, /* a */
  2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0, /* b */
  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* c */
  2,5,0,0,0,4,6,0,2,4,0,0,4,4,7,0, /* d */
  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* e */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0  /* f */
}; /* CyclesTable */

/* The number of cycles to be used by the current instruction - exported to
   allow fernangling by memory subsystem */
unsigned int Cycles;

/* A macro to speed up writes - uses a local variable called 'tmpaddr' */
#define FASTWRITE(addr,val) tmpaddr=addr; if (tmpaddr<0x8000) BEEBWRITEMEM_DIRECT(tmpaddr,val) else BeebWriteMem(tmpaddr,val);

/* Get a two byte address from the program counter, and then post inc the program counter */
#define GETTWOBYTEFROMPC(var) \
  var=WholeRam[ProgramCounter]; \
  var|=(WholeRam[ProgramCounter+1]<<8); \
  ProgramCounter+=2;

/*----------------------------------------------------------------------------*/
INLINE int SignExtendByte(signed char in) {
  /*if (in & 0x80) return(in | 0xffffff00); else return(in); */
  /* I think this should sign extend by virtue of the casts - gcc does anyway - the code
  above will definitly do the trick */
  return((int)in);
} /* SignExtendByte */

/*----------------------------------------------------------------------------*/
/* Set the Z flag if 'in' is 0, and N if bit 7 is set - leave all other bits  */
/* untouched.                                                                 */
INLINE static void SetPSRZN(const unsigned char in) {
  PSR&=~(FlagZ | FlagN);
  PSR|=((in==0)<<1) | (in & 128);
}; /* SetPSRZN */

/*----------------------------------------------------------------------------*/
/* Note: n is 128 for true - not 1                                            */
INLINE static void SetPSR(int mask,int c,int z,int i,int d,int b, int v, int n) {
  PSR&=~mask;
  PSR|=c | (z<<1) | (i<<2) | (d<<3) | (b<<4) | (v<<6) | n;
} /* SetPSR */

/*----------------------------------------------------------------------------*/
/* NOTE!!!!! n is 128 or 0 - not 1 or 0                                       */
INLINE static void SetPSRCZN(int c,int z, int n) {
  PSR&=~(FlagC | FlagZ | FlagN);
  PSR|=c | (z<<1) | n;
} /* SetPSRCZN */

/*----------------------------------------------------------------------------*/
void DumpRegs(void) {
  static char FlagNames[]="CZIDB-VNczidb-vn";
  int FlagNum;

  fprintf(stderr,"  PC=0x%x A=0x%x X=0x%x Y=0x%x S=0x%x PSR=0x%x=",
    ProgramCounter,Accumulator,XReg,YReg,StackReg,PSR);
  for(FlagNum=0;FlagNum<8;FlagNum++)
    fputc(FlagNames[FlagNum+8*((PSR & (1<<FlagNum))==0)],stderr);
  fputc('\n',stderr);
} /* DumpRegs */

/*----------------------------------------------------------------------------*/
INLINE static void Push(unsigned char ToPush) {
  BEEBWRITEMEM_DIRECT(0x100+StackReg,ToPush);
  StackReg--;
} /* Push */

/*----------------------------------------------------------------------------*/
INLINE static unsigned char Pop(void) {
  StackReg++;
  return(WholeRam[0x100+StackReg]);
} /* Pop */

/*----------------------------------------------------------------------------*/
INLINE static void PushWord(int16 topush) {
  Push((topush>>8) & 255);
  Push(topush & 255);
} /* PushWord */

/*----------------------------------------------------------------------------*/
INLINE static int16 PopWord() {
  int16 RetValue;

  RetValue=Pop();
  RetValue|=(Pop()<<8);
  return(RetValue);
} /* PopWord */

/*-------------------------------------------------------------------------*/
/* Relative addressing mode handler                                        */
INLINE static int16 RelAddrModeHandler_Data(void) {
  int EffectiveAddress;

  /* For branches - is this correct - i.e. is the program counter incremented
     at the correct time? */
  EffectiveAddress=SignExtendByte((signed char)WholeRam[ProgramCounter++]);
  EffectiveAddress+=ProgramCounter;

  return(EffectiveAddress);
} /* RelAddrModeHandler */

/*----------------------------------------------------------------------------*/
INLINE static void ADCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  int TmpResultV,TmpResultC;
  if (!GETDFLAG) {
    TmpResultC=Accumulator+operand+GETCFLAG;
    TmpResultV=(signed char)Accumulator+(signed char)operand+GETCFLAG;
    Accumulator=TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, (TmpResultC & 256)>0,Accumulator==0,0,0,0,((Accumulator & 128)>0) ^ (TmpResultV<0),(Accumulator & 128));
  } else {
    int ZFlag=0,NFlag=0,CFlag=0,VFlag=0;
    int TmpResult,TmpCarry=0;
    int ln,hn;

    /* Z flag determined from 2's compl result, not BCD result! */
    TmpResult=Accumulator+operand+GETCFLAG;
    ZFlag=((TmpResult & 0xff)==0);

    ln=(Accumulator & 0xf)+(operand & 0xf)+GETCFLAG;
    if (ln>9) {
      ln += 6;
      ln &= 0xf;
      TmpCarry=0x10;
    }
    hn=(Accumulator & 0xf0)+(operand & 0xf0)+TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    NFlag=hn & 128;
    VFlag=((hn & 128)==0) ^ ((Accumulator & 128)==0);
    if (hn>0x90) {
      hn += 0x60;
      hn &= 0xf0;
      CFlag=1;
    }
    Accumulator=hn|ln;
    SetPSR(FlagC | FlagZ | FlagV | FlagN,CFlag,ZFlag,0,0,0,VFlag,NFlag);
  }
} /* ADCInstrHandler */

/*----------------------------------------------------------------------------*/
INLINE static void ANDInstrHandler(int16 operand) {
  Accumulator=Accumulator & operand;
  PSR&=~(FlagZ | FlagN);
  PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
} /* ANDInstrHandler */

INLINE static void ASLInstrHandler(int16 address) {
  unsigned char oldVal,newVal;
  oldVal=BEEBREADMEM_FAST(address);
  newVal=(((unsigned int)oldVal)<<1);
  BEEBWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler */

INLINE static void ASLInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;
  /* Accumulator */
  oldVal=Accumulator;
  Accumulator=newVal=(((unsigned int)Accumulator)<<1);
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler_Acc */

INLINE static void BCCInstrHandler(void) {
  if (!GETCFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BCCInstrHandler */

INLINE static void BCSInstrHandler(void) {
  if (GETCFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BCSInstrHandler */

INLINE static void BEQInstrHandler(void) {
  if (GETZFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BEQInstrHandler */

INLINE static void BITInstrHandler(int16 operand) {
  PSR&=~(FlagZ | FlagN | FlagV);
  /* z if result 0, and NV to top bits of operand */
  PSR|=(((Accumulator & operand)==0)<<1) | (operand & 192);
} /* BITInstrHandler */

INLINE static void BMIInstrHandler(void) {
  if (GETNFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BMIInstrHandler */

INLINE static void BNEInstrHandler(void) {
  if (!GETZFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BNEInstrHandler */

INLINE static void BPLInstrHandler(void) {
  if (!GETNFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
}; /* BPLInstrHandler */

INLINE static void BRKInstrHandler(void) {
  PushWord(ProgramCounter+1);
  SetPSR(FlagB,0,0,0,0,1,0,0); /* Set B before pushing */
  Push(PSR);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Set I after pushing - see Birnbaum */
  ProgramCounter=BeebReadMem(0xfffe) | (BeebReadMem(0xffff)<<8);
} /* BRKInstrHandler */

INLINE static void BVCInstrHandler(void) {
  if (!GETVFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BVCInstrHandler */

INLINE static void BVSInstrHandler(void) {
  if (GETVFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Cycles++;
  } else ProgramCounter++;
} /* BVSInstrHandler */

INLINE static void CMPInstrHandler(int16 operand) {
  /* NOTE! Should we consult D flag ? */
  unsigned char result=Accumulator-operand;
  SetPSRCZN(Accumulator>=operand,Accumulator==operand,result & 128);
} /* CMPInstrHandler */

INLINE static void CPXInstrHandler(int16 operand) {
  unsigned char result=(XReg-operand);
  SetPSRCZN(XReg>=operand,XReg==operand,result & 128);
} /* CPXInstrHandler */

INLINE static void CPYInstrHandler(int16 operand) {
  unsigned char result=(YReg-operand);
  SetPSRCZN(YReg>=operand,YReg==operand,result & 128);
} /* CPYInstrHandler */

INLINE static void DECInstrHandler(int16 address) {
  unsigned char val;

  val=BEEBREADMEM_FAST(address);

  val=(val-1);

  BEEBWRITEMEM_FAST(address,val);
  SetPSRZN(val);
} /* DECInstrHandler */

INLINE static void DEXInstrHandler(void) {
  XReg=(XReg-1) & 255;
  SetPSRZN(XReg);
} /* DEXInstrHandler */

INLINE static void EORInstrHandler(int16 operand) {
  Accumulator^=operand;
  SetPSRZN(Accumulator);
} /* EORInstrHandler */

INLINE static void INCInstrHandler(int16 address) {
  unsigned char val;

  val=BEEBREADMEM_FAST(address);

  val=(val+1) & 255;

  BEEBWRITEMEM_FAST(address,val);
  SetPSRZN(val);
} /* INCInstrHandler */

INLINE static void INXInstrHandler(void) {
  XReg+=1;
  XReg&=255;
  SetPSRZN(XReg);
} /* INXInstrHandler */

INLINE static void JSRInstrHandler(int16 address) {
  PushWord(ProgramCounter-1);
  ProgramCounter=address;
} /* JSRInstrHandler */

INLINE static void LDAInstrHandler(int16 operand) {
  Accumulator=operand;
  SetPSRZN(Accumulator);
} /* LDAInstrHandler */

INLINE static void LDXInstrHandler(int16 operand) {
  XReg=operand;
  SetPSRZN(XReg);
} /* LDXInstrHandler */

INLINE static void LDYInstrHandler(int16 operand) {
  YReg=operand;
  SetPSRZN(YReg);
} /* LDYInstrHandler */

INLINE static void LSRInstrHandler(int16 address) {
  unsigned char oldVal,newVal;
  oldVal=BEEBREADMEM_FAST(address);
  newVal=(((unsigned int)oldVal)>>1);
  BEEBWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 1)>0, newVal==0,0);
} /* LSRInstrHandler */

INLINE static void LSRInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;
  /* Accumulator */
  oldVal=Accumulator;
  Accumulator=newVal=(((unsigned int)Accumulator)>>1) & 255;
  SetPSRCZN((oldVal & 1)>0, newVal==0,0);
} /* LSRInstrHandler_Acc */

INLINE static void ORAInstrHandler(int16 operand) {
  Accumulator=Accumulator | operand;
  SetPSRZN(Accumulator);
} /* ORAInstrHandler */

INLINE static void ROLInstrHandler(int16 address) {
  unsigned char oldVal,newVal;

  oldVal=BEEBREADMEM_FAST(address);
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  BEEBWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 128)>0,newVal==0,newVal & 128);
} /* ROLInstrHandler */

INLINE static void ROLInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;

  oldVal=Accumulator;
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  Accumulator=newVal;
  SetPSRCZN((oldVal & 128)>0,newVal==0,newVal & 128);
} /* ROLInstrHandler_Acc */

INLINE static void RORInstrHandler(int16 address) {
  unsigned char oldVal,newVal;

  oldVal=BEEBREADMEM_FAST(address);
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  BEEBWRITEMEM_FAST(address,newVal);
  SetPSRCZN(oldVal & 1,newVal==0,newVal & 128);
} /* RORInstrHandler */

INLINE static void RORInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;

  oldVal=Accumulator;
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  Accumulator=newVal;
  SetPSRCZN(oldVal & 1,newVal==0,newVal & 128);
} /* RORInstrHandler_Acc */

INLINE static void SBCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  int TmpResultV,TmpResultC;
  if (!GETDFLAG) {
    TmpResultV=(signed char)Accumulator-(signed char)operand-(1-GETCFLAG);
    TmpResultC=Accumulator-operand-(1-GETCFLAG);
    Accumulator=TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, TmpResultC>=0,Accumulator==0,0,0,0,
      ((Accumulator & 128)>0) ^ ((TmpResultV & 256)!=0),(Accumulator & 128));
  } else {
    int ZFlag=0,NFlag=0,CFlag=1,VFlag=0;
    int TmpResult,TmpCarry=0;
    int ln,hn;

    /* Z flag determined from 2's compl result, not BCD result! */
    TmpResult=Accumulator-operand-(1-GETCFLAG);
    ZFlag=((TmpResult & 0xff)==0);

    ln=(Accumulator & 0xf)-(operand & 0xf)-(1-GETCFLAG);
    if (ln<0) {
      ln-=6;
      ln&=0xf;
      TmpCarry=0x10;
    }
    hn=(Accumulator & 0xf0)-(operand & 0xf0)-TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    NFlag=hn & 128;
    VFlag=((hn & 128)==0) ^ ((Accumulator & 128)==0);
    if (hn<0) {
      hn-=0x60;
      hn&=0xf0;
      CFlag=0;
    }
    Accumulator=hn|ln;
    SetPSR(FlagC | FlagZ | FlagV | FlagN,CFlag,ZFlag,0,0,0,VFlag,NFlag);
  }
} /* SBCInstrHandler */

INLINE static void STXInstrHandler(int16 address) {
  BEEBWRITEMEM_FAST(address,XReg);
} /* STXInstrHandler */

INLINE static void STYInstrHandler(int16 address) {
  BEEBWRITEMEM_FAST(address,YReg);
} /* STYInstrHandler */

INLINE static void BadInstrHandler(int opcode) {
	if (!IgnoreIllegalInstructions)
	{
#ifdef WIN32
		char errstr[250];
		sprintf(errstr,"Unsupported 6502 instruction 0x%02X at 0x%04X\n"
			"  OK - instruction will be skipped\n"
			"  Cancel - dump memory and exit",opcode,ProgramCounter-1);
		if (MessageBox(GETHWND,errstr,"BBC Emulator",MB_OKCANCEL|MB_ICONERROR) == IDCANCEL)
		{
			beebmem_dumpstate();
			exit(0);
		}
#else
		fprintf(stderr,"Bad instruction handler called:\n");
		DumpRegs();
		fprintf(stderr,"Dumping main memory\n");
		beebmem_dumpstate();
		abort();
#endif
	}

	/* Do not know what the instruction does but can guess if it is 1,2 or 3 bytes */
	switch (opcode & 0xf)
	{
	/* One byte instructions */
	case 0xa:
		break;

	/* Two byte instructions */
	case 0x0:
	case 0x2:  /* Inst 0xf2 causes the 6502 to hang! Try it on your BBC Micro */
	case 0x3:
	case 0x4:
	case 0x7:
	case 0x9:
	case 0xb:
		ProgramCounter++;
		break;

	/* Three byte instructions */
	case 0xc:
	case 0xe:
	case 0xf:
		ProgramCounter+=2;
		break;
	}
} /* BadInstrHandler */

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Data(void) {
  int FullAddress;

  /* Get the address from after the instruction */
  
  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(BEEBREADMEM_FAST(FullAddress));
} /* AbsAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Address(void) {
  int FullAddress;

  /* Get the address from after the instruction */
  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(FullAddress);
} /* AbsAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page addressing mode handler                                       */
INLINE static int16 ZeroPgAddrModeHandler_Address(void) {
  return(WholeRam[ProgramCounter++]);
} /* ZeroPgAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Data(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(WholeRam[ProgramCounter++]+XReg) & 255;

  EffectiveAddress=WholeRam[ZeroPageAddress] | (WholeRam[ZeroPageAddress+1]<<8);
  return(BEEBREADMEM_FAST(EffectiveAddress));
} /* IndXAddrModeHandler_Data */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Address(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(WholeRam[ProgramCounter++]+XReg) & 255;

  EffectiveAddress=WholeRam[ZeroPageAddress] | (WholeRam[ZeroPageAddress+1]<<8);
  return(EffectiveAddress);
} /* IndXAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=WholeRam[ProgramCounter++];
  EffectiveAddress=WholeRam[ZPAddr]+YReg;
  EffectiveAddress+=(WholeRam[ZPAddr+1]<<8);

  return(BEEBREADMEM_FAST(EffectiveAddress));
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=WholeRam[ProgramCounter++];
  EffectiveAddress=WholeRam[ZPAddr]+YReg;
  EffectiveAddress+=(WholeRam[ZPAddr+1]<<8);

  return(EffectiveAddress);
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(WholeRam[ProgramCounter++]+XReg) & 255;
  return(WholeRam[EffectiveAddress]);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(WholeRam[ProgramCounter++]+XReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with X offset addressing mode handler                          */
INLINE static int16 AbsXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress);
  EffectiveAddress+=XReg;
  EffectiveAddress&=0xffff;

  return(BEEBREADMEM_FAST(EffectiveAddress));
} /* AbsXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with X offset addressing mode handler                          */
INLINE static int16 AbsXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  EffectiveAddress+=XReg;
  EffectiveAddress&=0xffff;

  return(EffectiveAddress);
} /* AbsXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with Y offset addressing mode handler                          */
INLINE static int16 AbsYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  EffectiveAddress+=YReg;

  return(BEEBREADMEM_FAST(EffectiveAddress));
} /* AbsYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with Y offset addressing mode handler                          */
INLINE static int16 AbsYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  EffectiveAddress+=YReg;

  return(EffectiveAddress);
} /* AbsYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indirect addressing mode handler                                        */
INLINE static int16 IndAddrModeHandler_Address(void) {
  /* For jump indirect only */
  int VectorLocation;
  int EffectiveAddress;

  GETTWOBYTEFROMPC(VectorLocation)

  EffectiveAddress=BEEBREADMEM_FAST(VectorLocation);
  EffectiveAddress|=BEEBREADMEM_FAST(VectorLocation+1) << 8;

  return(EffectiveAddress);
} /* IndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(WholeRam[ProgramCounter++]+YReg) & 255;
  return(WholeRam[EffectiveAddress]);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(WholeRam[ProgramCounter++]+YReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Initialise 6502core                                                     */
void Init6502core(void) {
  ProgramCounter=BeebReadMem(0xfffc) | (BeebReadMem(0xfffd)<<8);
  Accumulator=XReg=YReg=0; /* For consistancy of execution */
  StackReg=0xff; /* Initial value ? */
  PSR=FlagI; /* Interrupts off for starters */

  intStatus=0;
  NMIStatus=0;
  NMILock=0;
} /* Init6502core */

#include "via.h"

/*-------------------------------------------------------------------------*/
void DoInterrupt(void) {
  PushWord(ProgramCounter);
  Push(PSR & ~FlagB);
  ProgramCounter=BeebReadMem(0xfffe) | (BeebReadMem(0xffff)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0);
} /* DoInterrupt */

/*-------------------------------------------------------------------------*/
void DoNMI(void) {
  /*cerr << "Doing NMI\n"; */
  NMILock=1;
  PushWord(ProgramCounter);
  Push(PSR);
  ProgramCounter=BeebReadMem(0xfffa) | (BeebReadMem(0xfffb)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Normal interrupts should be disabled during NMI ? */
} /* DoNMI */

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec6502Instruction(void) {
  static int CurrentInstruction;
  static int tmpaddr;
  static int OldNMIStatus;

  int loop;
  for(loop=0;loop<512;loop++) {

  /* Read an instruction and post inc program couter */
  CurrentInstruction=WholeRam[ProgramCounter++];

  /*cout << "Fetch at " << hex << (ProgramCounter-1) << " giving 0x" << CurrentInstruction << dec << "\n"; */
  Cycles=CyclesTable[CurrentInstruction]; 

  /*Stats[CurrentInstruction]++; */
  switch (CurrentInstruction) {
    case 0x00:
      BRKInstrHandler();
      break;
    case 0x01:
      ORAInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x05:
      ORAInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0x06:
      ASLInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x08:
      Push(PSR); /* PHP */
      break;
    case 0x09:
      ORAInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0x0a:
      ASLInstrHandler_Acc();
      break;
    case 0x0d:
      ORAInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x0e:
      ASLInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x10:
      BPLInstrHandler();
      break;
    case 0x30:
      BMIInstrHandler();
      break;
    case 0x50:
      BVCInstrHandler();
      break;
    case 0x70:
      BVSInstrHandler();
      break;
    case 0x90:
      BCCInstrHandler();
      break;
    case 0xb0:
      BCSInstrHandler();
      break;
    case 0xd0:
      BNEInstrHandler();
      break;
    case 0xf0:
      BEQInstrHandler();
      break;
    case 0x11:
      ORAInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x15:
      ORAInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x16:
      ASLInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x18:
      PSR&=255-FlagC; /* CLC */
      break;
    case 0x19:
      ORAInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x1d:
      ORAInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x1e:
      ASLInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x20:
      JSRInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x21:
      ANDInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x24:
      BITInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0x25:
      ANDInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0x26:
      ROLInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x28:
      PSR=Pop(); /* PLP */
      break;
    case 0x29:
      ANDInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0x2a:
      ROLInstrHandler_Acc();
      break;
    case 0x2c:
      BITInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x2d:
      ANDInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x2e:
      ROLInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x31:
      ANDInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x35:
      ANDInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x36:
      ROLInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x38:
      PSR|=FlagC; /* SEC */
      break;
    case 0x39:
      ANDInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x3d:
      ANDInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x3e:
      ROLInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x40:
      PSR=Pop(); /* RTI */
      ProgramCounter=PopWord();
      NMILock=0;
      break;
    case 0x41:
      EORInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x45:
      EORInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0x46:
      LSRInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x48:
      Push(Accumulator); /* PHA */
      break;
    case 0x49:
      EORInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0x4a:
      LSRInstrHandler_Acc();
      break;
    case 0x4c:
      ProgramCounter=AbsAddrModeHandler_Address(); /* JMP */
      break;
    case 0x4d:
      EORInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x4e:
      LSRInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x51:
      EORInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x55:
      EORInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x56:
      LSRInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x58:
      PSR&=255-FlagI; /* CLI */
      break;
    case 0x59:
      EORInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x5d:
      EORInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x5e:
      LSRInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x60:
      ProgramCounter=PopWord()+1; /* RTS */
      break;
    case 0x61:
      ADCInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x65:
      ADCInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0x66:
      RORInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x68:
      Accumulator=Pop(); /* PLA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x69:
      ADCInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0x6a:
      RORInstrHandler_Acc();
      break;
    case 0x6c:
      ProgramCounter=IndAddrModeHandler_Address(); /* JMP */
      break;
    case 0x6d:
      ADCInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x6e:
      RORInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x71:
      ADCInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x75:
      ADCInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x76:
      RORInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x78:
      PSR|=FlagI; /* SEI */
      break;
    case 0x79:
      ADCInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x7d:
      ADCInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x7e:
      RORInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x81:
      FASTWRITE(IndXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x84:
      BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),YReg);
      break;
    case 0x85:
      BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x86:
      BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),XReg);
      break;
    case 0x88:
      YReg=(YReg-1) & 255; /* DEY */
      PSR&=~(FlagZ | FlagN);
      PSR|=((YReg==0)<<1) | (YReg & 128);
      break;
    case 0x8a:
      Accumulator=XReg; /* TXA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x8c:
      STYInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x8d:
      FASTWRITE(AbsAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x8e:
      STXInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x91:
      FASTWRITE(IndYAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x94:
      STYInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x95:
      FASTWRITE(ZeroPgXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x96:
      STXInstrHandler(ZeroPgYAddrModeHandler_Address());
      break;
    case 0x98:
      Accumulator=YReg; /* TYA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x99:
      FASTWRITE(AbsYAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x9a:
      StackReg=XReg; /* TXS */
      break;
    case 0x9d:
      FASTWRITE(AbsXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0xa0:
      LDYInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xa1:
      LDAInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xa2:
      LDXInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xa4:
      LDYInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xa5:
      LDAInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xa6:
      LDXInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xa8:
      YReg=Accumulator; /* TAY */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0xa9:
      LDAInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xaa:
      XReg=Accumulator; /* TXA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0xac:
      LDYInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xad:
      LDAInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xae:
      LDXInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xb1:
      LDAInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xb4:
      LDYInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xb5:
      LDAInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xb6:
      LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
      break;
    case 0xb8:
      PSR&=255-FlagV; /* CLV */
      break;
    case 0xb9:
      LDAInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xba:
      XReg=StackReg; /* TSX */
      PSR&=~(FlagZ | FlagN);
      PSR|=((XReg==0)<<1) | (XReg & 128);
      break;
    case 0xbc:
      LDYInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xbd:
      LDAInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xbe:
      LDXInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xc0:
      CPYInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xc1:
      CMPInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xc4:
      CPYInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xc5:
      CMPInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xc6:
      DECInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0xc8:
      YReg+=1; /* INY */
      YReg&=255;
      PSR&=~(FlagZ | FlagN);
      PSR|=((YReg==0)<<1) | (YReg & 128);
      break;
    case 0xc9:
      CMPInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xca:
      DEXInstrHandler();
      break;
    case 0xcc:
      CPYInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xcd:
      CMPInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xce:
      DECInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0xd1:
      CMPInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xd5:
      CMPInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xd6:
      DECInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0xd8:
      PSR&=255-FlagD; /* CLD */
      break;
    case 0xd9:
      CMPInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xdd:
      CMPInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xde:
      DECInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0xe0:
      CPXInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xe1:
      SBCInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xe4:
      CPXInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xe5:
      SBCInstrHandler(WholeRam[WholeRam[ProgramCounter++]]/*zp */);
      break;
    case 0xe6:
      INCInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0xe8:
      INXInstrHandler();
      break;
    case 0xe9:
      SBCInstrHandler(WholeRam[ProgramCounter++]); /* immediate */
      break;
    case 0xea:
      /* NOP */
      break;
    case 0xec:
      CPXInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xed:
      SBCInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xee:
      INCInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0xf1:
      SBCInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xf5:
      SBCInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xf6:
      INCInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0xf8:
      PSR|=FlagD; /* SED */
      break;
    case 0xf9:
      SBCInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xfd:
      SBCInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xfe:
      INCInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x07: /* Undocumented Instruction: ASL zp and ORA zp */
      {
        int16 zpaddr = ZeroPgAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(WholeRam[zpaddr]);
      }
      break;
    case 0x4b: /* Undocumented Instruction: AND imm and LSR A */
      ANDInstrHandler(WholeRam[ProgramCounter++]);
      LSRInstrHandler_Acc();
      break;
    case 0x87: /* Undocumented Instruction: SAX zp (i.e. (zp) = A & X) */
      /* This one does not seem to change the processor flags */
      WholeRam[ZeroPgAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0xa3: /* Undocumented Instruction: LAX (zp,X) */
      LDAInstrHandler(IndXAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xa7: /* Undocumented Instruction: LAX zp */
      LDAInstrHandler(WholeRam[WholeRam[ProgramCounter++]]);
      XReg = Accumulator;
      break;
    case 0xaf: /* Undocumented Instruction: LAX abs */
      LDAInstrHandler(AbsAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xb3: /* Undocumented Instruction: LAX (zp),Y */
      LDAInstrHandler(IndYAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xb7: /* Undocumented Instruction: LAX zp,Y */
      LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
      Accumulator = XReg;
      break;
    case 0xbf: /* Undocumented Instruction: LAX abs,Y */
      LDAInstrHandler(AbsYAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    default:
      BadInstrHandler(CurrentInstruction);
      break;
	break;
  }; /* OpCode switch */

  OldNMIStatus=NMIStatus;
  /* NOTE: Check IRQ status before polling hardware - this is essential for
     Rocket Raid to work since it polls the IFR in the sys via for start of 
     frame - but with interrupts enabled.  If you do the interrupt check later
     then the interrupt handler will always be entered and rocket raid will
     never see it */
  if ((intStatus) && (!GETIFLAG)) DoInterrupt();
  
  TotalCycles+=Cycles;
  if (TotalCycles > CycleCountWrap)
  {
    TotalCycles -= CycleCountWrap;
    AdjustTrigger(AtoDTrigger);
    AdjustTrigger(SoundTrigger);
    AdjustTrigger(Disc8271Trigger);
    AdjustTrigger(AMXTrigger);
    AdjustTrigger(PrinterTrigger);
    AdjustTrigger(VideoTriggerCount);
  }

  VideoPoll(Cycles);
  SysVIA_poll(Cycles);
  UserVIA_poll(Cycles);
  Disc8271_poll(Cycles);
  AtoD_poll(Cycles);
#ifdef SOUNDSUPPORT
  Sound_Trigger(Cycles);
#endif

  if ((NMIStatus) && (!OldNMIStatus)) DoNMI();
  };
} /* Exec6502Instruction */

/*-------------------------------------------------------------------------*/
void Save6502State(unsigned char *CPUState) {
	CPUState[0] = ProgramCounter & 255;
	CPUState[1] = (ProgramCounter >> 8) & 255;
	CPUState[2] = Accumulator;
	CPUState[3] = XReg;
	CPUState[4] = YReg;
	CPUState[5] = StackReg;
	CPUState[6] = PSR;
}

/*-------------------------------------------------------------------------*/
void Restore6502State(unsigned char *CPUState) {
	ProgramCounter = CPUState[0] + (CPUState[1] << 8);
	Accumulator = CPUState[2];
	XReg = CPUState[3];
	YReg = CPUState[4];
	StackReg = CPUState[5];
	PSR = CPUState[6];

	/* Reset the other globals as well */
	TotalCycles = 0;
	intStatus = 0;
	NMIStatus = 0;
	NMILock = 0;
}

/*-------------------------------------------------------------------------*/
/* Dump state                                                              */
void core_dumpstate(void) {
  cerr << "core:\n";
  DumpRegs();
}; /* core_dumpstate */
