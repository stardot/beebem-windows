/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2005  Jon Welch

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

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "debug.h"
#include "z80mem.h"
#include "z80.h"
#include "beebmem.h"

#include "tube.h"

extern int trace;

int Enable_Z80 = 0;
int trace_z80 = 0;
int debug_z80 = 0;
int TorchTube = 0;
int PreZPC = 0; // Previous Z80 PC

unsigned char z80_rom[65536L];
unsigned char z80_ram[65536L];

#define TRUE 1
#define FALSE 0

int inROM = 1;

unsigned char ReadZ80Mem(int pc)

{
unsigned char t;

	if (AcornZ80)
	{
		if (pc >= 0x8000) inROM = 0;
	}
	
    t = (inROM) ? z80_rom[pc & 0xffff] : z80_ram[pc & 0xffff];

//    if (trace_z80)
//        WriteLog("Read %02x from %04x in %s\n", t, pc, (inROM) ? "ROM" : "RAM");

    return t;

}

void WriteZ80Mem(int pc, unsigned char data)

{
//    if (inROM)
//    {
//        z80_rom[pc & 0xffff] = data;
//    }
//    else
//    {
        z80_ram[pc & 0xffff] = data;
//    }

//    if (trace_z80)
//        WriteLog("Writing %02x to %04x\n", data, pc);

}

/* 
Register dump is following format:
AF=0000 MZ5H3VNC BC=0000 DE=0000 HL=0000 IX=0000 I=00 PC=0000:00,00,00,00
AF'0000 MZ5H3VNC BC'0000 DE'0000 HL'0000 IY=0000 R=00 SP=0000:00,00,00,00
 */

void Disp_RegSet1(char *str)

{
	sprintf(str, "AF=%04X ",af[0]);
	sprintf(str + strlen(str), (af[0] & 128) ? "M" : "P");
	sprintf(str + strlen(str), (af[0] & 64) ? "Z" : ".");
	sprintf(str + strlen(str), (af[0] & 32) ? "5" : ".");
	sprintf(str + strlen(str), (af[0] & 16) ? "H" : ".");
	sprintf(str + strlen(str), (af[0] & 8) ? "3" : ".");
	sprintf(str + strlen(str), (af[0] & 4) ? "V" : ".");
	sprintf(str + strlen(str), (af[0] & 2) ? "N" : ".");
	sprintf(str + strlen(str), (af[0] & 1) ? "C" : ".");
	sprintf(str + strlen(str), " BC=%04X DE=%04X HL=%04X", regs[0].bc, regs[0].de, regs[0].hl);
	sprintf(str + strlen(str), " IX=%04X I=%02X PC=%04X",ix, ir & 255, pc);
	sprintf(str + strlen(str), ":%02X,%02X,%02X,%02X",ReadZ80Mem(pc),ReadZ80Mem(pc+1),ReadZ80Mem(pc+2),ReadZ80Mem(pc+3));
}

void Disp_RegSet2(char *str)

{
	sprintf(str, "AF'%04X ",af[1]);
	sprintf(str + strlen(str), (af[1] & 128) ? "M" : "P");
	sprintf(str + strlen(str), (af[1] & 64) ? "Z" : ".");
	sprintf(str + strlen(str), (af[1] & 32) ? "5" : ".");
	sprintf(str + strlen(str), (af[1] & 16) ? "H" : ".");
	sprintf(str + strlen(str), (af[1] & 8) ? "3" : ".");
	sprintf(str + strlen(str), (af[1] & 4) ? "V" : ".");
	sprintf(str + strlen(str), (af[1] & 2) ? "N" : ".");
	sprintf(str + strlen(str), (af[1] & 1) ? "C" : ".");
	sprintf(str + strlen(str), " BC'%04X DE'%04X HL'%04X",regs[1].bc,regs[1].de,regs[1].hl);
	sprintf(str + strlen(str), " IY=%04X R=%02x SP=%04X",iy, regs_sel, sp);
	sprintf(str + strlen(str), ":%02X,%02X,%02X,%02X\n",ReadZ80Mem(sp),ReadZ80Mem(sp+1),ReadZ80Mem(sp+2),ReadZ80Mem(sp+3));
}


void disp_regs()
{

char buff[64];
char str[256];
	
Z80_Disassemble(pc, buff);

sprintf(str, "AF=%04X ",af[0]);
sprintf(str + strlen(str), (af[0] & 128) ? "M" : "P");
sprintf(str + strlen(str), (af[0] & 64) ? "Z" : ".");
sprintf(str + strlen(str), (af[0] & 32) ? "5" : ".");
sprintf(str + strlen(str), (af[0] & 16) ? "H" : ".");
sprintf(str + strlen(str), (af[0] & 8) ? "3" : ".");
sprintf(str + strlen(str), (af[0] & 4) ? "V" : ".");
sprintf(str + strlen(str), (af[0] & 2) ? "N" : ".");
sprintf(str + strlen(str), (af[0] & 1) ? "C" : ".");
sprintf(str + strlen(str), " BC=%04X DE=%04X HL=%04X", regs[0].bc, regs[0].de, regs[0].hl);
sprintf(str + strlen(str), " IX=%04X I=%02X PC=%04X",ix, ir & 255, pc);
sprintf(str + strlen(str), ":%02X,%02X,%02X,%02X",ReadZ80Mem(pc),ReadZ80Mem(pc+1),ReadZ80Mem(pc+2),ReadZ80Mem(pc+3));

WriteLog("%s %s\n", str, buff);

sprintf(str, "AF'%04X ",af[1]);
sprintf(str + strlen(str), (af[1] & 128) ? "M" : "P");
sprintf(str + strlen(str), (af[1] & 64) ? "Z" : ".");
sprintf(str + strlen(str), (af[1] & 32) ? "5" : ".");
sprintf(str + strlen(str), (af[1] & 16) ? "H" : ".");
sprintf(str + strlen(str), (af[1] & 8) ? "3" : ".");
sprintf(str + strlen(str), (af[1] & 4) ? "V" : ".");
sprintf(str + strlen(str), (af[1] & 2) ? "N" : ".");
sprintf(str + strlen(str), (af[1] & 1) ? "C" : ".");
sprintf(str + strlen(str), " BC'%04X DE'%04X HL'%04X",regs[1].bc,regs[1].de,regs[1].hl);
sprintf(str + strlen(str), " IY=%04X R=%02x SP=%04X",iy, regs_sel, sp);
sprintf(str + strlen(str), ":%02X,%02X,%02X,%02X\n",ReadZ80Mem(sp),ReadZ80Mem(sp+1),ReadZ80Mem(sp+2),ReadZ80Mem(sp+3));

WriteLog("%s\n", str);

}

int in(unsigned int addr)

{
int value = 0xff;
int tmp;
// int c;

    addr &= 255;

	if (AcornZ80)
	{
        value = ReadTubeFromParasiteSide(addr);
	}
	else
	{
		if ( (addr == 0x05) || (addr == 0x01) )
		{
			value = ReadTorchTubeFromParasiteSide(1);        // Data Port

		}

		if ( (addr == 0x06) || (addr == 0x02) )
		{
			value = ReadTorchTubeFromParasiteSide(0);        // Status Port
			tmp = 0x00;
			if (value & 128) tmp |= 2;      // Tube data available
			if (value & 64) tmp |= 128;     // Tube not full
			value = tmp;
		}

		if (addr == 0x02) inROM = 1;

		if (addr == 0x06) inROM = 0;
    }
	
    return value;
}

void out(unsigned int addr, unsigned char value)

{
    addr &= 255;
    
	if (AcornZ80)
	{
        WriteTubeFromParasiteSide(addr, value);
	}
	else
	{
		if ( (addr == 0x00) || (addr == 0x04) )
		{
			WriteTorchTubeFromParasiteSide(1, value);
		}
	}
}

void z80_execute()
{
    if (Enable_Z80)
    {

        if (trace_z80)
        {
            if (pc <= 0xf800)       // Don't trace BIOS cos toooo much data
                disp_regs();
        }

		// Output debug info
		if (DebugEnabled)
			DebugDisassembler(pc, PreZPC, 0, 0, 0, 0, 0, false);
		
		PreZPC = pc;
		pc = simz80(pc);

		if (AcornZ80)
		{
			if (TubeintStatus & (1<<R1))
				set_Z80_irq_line(1);
		
			if (TubeintStatus & (1<<R4))
				set_Z80_irq_line(1);
		
			if (TubeintStatus == 0)
				set_Z80_irq_line(0);
		
			if (TubeNMIStatus) 
				set_Z80_nmi_line(1);
			else
				set_Z80_nmi_line(0);
		}
	}
}

void init_z80()

{
char path[256];
FILE *f;

    WriteLog("init_z80()\n");

	if (AcornZ80)
	{
		strcpy(path, RomPath);
		strcat(path, "BeebFile/Z80.ROM");
		
		f = fopen(path, "rb");
		if (f != NULL)
		{
			fread(z80_rom, 4096, 1, f);
			fclose(f);
		}
	}
	else
	{
		strcpy(path, RomPath);
		strcat(path, "BeebFile/CCPN102.ROM");

		f = fopen(path, "rb");
		if (f != NULL)
		{
			fread(z80_rom, 8192, 1, f);
			fclose(f);
		}
	}
	
    inROM = 1;

    sp = 0x00;
    pc = 0x00;
    
/* Clear all registers, PC and SP have already been set			*/
    af[0]=0; regs[0].bc=0; regs[0].de=0; regs[0].hl=0;
    af[1]=0; regs[1].bc=0; regs[1].de=0; regs[1].hl=0;
    ix=0; iy=0; ir=0; regs_sel = 0;

}

void Debug_Z80()

{
char buff[256];
int s, t, a;


    t = 0x4400;
    for (a = 0; a < 512; ++a)
    {
        s = Z80_Disassemble(t, buff);
        WriteLog("%04x : %s\n", t, buff);
        t += s;
    }
            
    trace_z80 = 1;

    for (a = 0; a < 32; ++a)
    {
        PrintHex(0x4400 + a * 16);
    }

}

void PrintHex(int addr)

{
char buff[80];
int i, a;
int num;
char *p;

	num = 16;

	p = buff;
	sprintf(p, "%04X : ", addr);
	p += 7;

	for (i = 0; i < num; ++i)
	{
		sprintf(p, "%02X ", ReadZ80Mem(addr + i));
		p += 3;
	}

	strcpy(p, " ");
	p += 1;

	for (i = 0; i < num; ++i)
	{
		a = ReadZ80Mem(addr + i) & 127;
		if (a < 32 || a == 127) a = '.';
		if (a == '%') a = '.';
		*p++ = a;
        *p = 0;
    }

    WriteLog("%s\n", buff);
		
}

