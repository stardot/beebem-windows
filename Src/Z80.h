/* Header file for the instruction set simulator.
   Copyright (C) 1995  Frank D. Cringle.
   Modifications for MMU and CP/M 3.1 Copyright (C) 2000/2003 by Andreas Gerlich

This file is part of yaze-ag - yet another Z80 emulator by ag.

Yaze-ag is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#ifndef Z80_HEADER
#define Z80_HEADER

/* SEE limits and BYTE-, WORD- and FASTREG - defintions im MEM_MMU.h */

typedef unsigned short WORD;

/* two sets of accumulator / flags */
extern WORD af[2];
extern int af_sel;

/* two sets of 16-bit registers */
extern struct ddregs {
	WORD bc;
	WORD de;
	WORD hl;
} regs[2];

extern int regs_sel;

extern WORD ir;
extern WORD ix;
extern WORD iy;
extern WORD sp;
extern WORD pc;
extern WORD IFF1;
extern WORD IFF2;

/* see definitions for memory in mem_mmu.h */

extern FASTWORK simz80(FASTREG PC);

#define FLAG_C	1
#define FLAG_N	2
#define FLAG_P	4
#define FLAG_H	16
#define FLAG_Z	64
#define FLAG_S	128

#define SETFLAG(f,c)	AF = (c) ? AF | FLAG_ ## f : AF & ~FLAG_ ## f
#define TSTFLAG(f)	((AF & FLAG_ ## f) != 0)

#define ldig(x)		((x) & 0xf)
#define hdig(x)		(((x)>>4)&0xf)
#define lreg(x)		((x)&0xff)
#define hreg(x)		(((x)>>8)&0xff)

#define Setlreg(x, v)	x = (((x)&0xff00) | ((v)&0xff))
#define Sethreg(x, v)	x = (((x)&0xff) | (((v)&0xff) << 8))

/* SEE functions for manipulating of memory in mem_mmu.h 
      line RAM, GetBYTE, GetWORD, PutBYTE, PutWORD, .... 
*/

extern unsigned char Z80ReadIO(unsigned int port);
extern void Z80WriteIO(unsigned int port, unsigned char value);

void Z80Execute();
void Z80Init();
void Debug_Z80();
int Z80_Disassemble(int adr, char *s);
void PrintHex(int PC);
unsigned char ReadZ80Mem(int pc);
void WriteZ80Mem(int pc, unsigned char data);
void Disp_RegSet1(char *str);
void Disp_RegSet2(char *str);

void z80_NMI_Interrupt(void);
void z80_IRQ_Interrupt(void);
void set_Z80_irq_line(bool state);
void set_Z80_nmi_line(bool state);

void SaveZ80UEF(FILE *SUEF);
void LoadZ80UEF(FILE *SUEF);

#endif
