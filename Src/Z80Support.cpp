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

#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "Z80mem.h"
#include "Z80.h"
#include "BeebMem.h"
#include "Debug.h"
#include "FileUtils.h"
#include "Log.h"
#include "Main.h"
#include "Tube.h"
#include "UefState.h"

bool trace_z80 = false;
int PreZPC = 0; // Previous Z80 PC

unsigned char z80_rom[65536];
unsigned char z80_ram[65536];

bool inROM = true;

unsigned char ReadZ80Mem(int addr)
{
	if (TubeType == TubeDevice::AcornZ80)
	{
		if (addr >= 0x8000) inROM = false;
	}

	unsigned char t = (inROM) ? z80_rom[addr & 0x1fff] : z80_ram[addr & 0xffff];

	// if (trace_z80)
	//	WriteLog("Read %02x from %04x in %s\n", t, addr, (inROM) ? "ROM" : "RAM");

	return t;
}

void WriteZ80Mem(int addr, unsigned char data)
{
	// if (inROM)
	// {
	//	z80_rom[pc & 0xffff] = data;
	// }
	// else
	// {
		z80_ram[addr & 0xffff] = data;
	// }

	// if (trace_z80)
	//	WriteLog("Writing %02x to %04x\n", data, addr);
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

unsigned char Z80ReadIO(unsigned int addr)
{
	unsigned char value = 0xff;

	addr &= 0xff;

	if (TubeType == TubeDevice::AcornZ80)
	{
		value = ReadTubeFromParasiteSide(addr);
	}
	else
	{
		if (addr == 0x05 || addr == 0x01)
		{
			value = ReadTorchTubeFromParasiteSide(1); // Data Port
		}

		if (addr == 0x06 || addr == 0x02)
		{
			value = ReadTorchTubeFromParasiteSide(0); // Status Port
			unsigned char tmp = 0x00;
			if (value & 128) tmp |= 2;      // Tube data available
			if (value & 64) tmp |= 128;     // Tube not full
			value = tmp;
		}

		inROM = (addr & 4) == 0;
	}

	return value;
}

void Z80WriteIO(unsigned int addr, unsigned char value)
{
	addr &= 255;

	if (TubeType == TubeDevice::AcornZ80)
	{
		WriteTubeFromParasiteSide(addr, value);
	}
	else
	{
		if (addr == 0x00 || addr == 0x04)
		{
			WriteTorchTubeFromParasiteSide(1, value);
		}

		inROM = (addr & 4) == 0;
	}
}

void Z80Execute()
{
	if (trace_z80)
	{
		if (pc <= 0xf800) // Don't trace BIOS cos toooo much data
			disp_regs();
	}

	// Output debug info
	if (DebugEnabled)
		DebugDisassembler(pc, PreZPC, 0, 0, 0, 0, 0, false);

	PreZPC = pc;
	pc = (WORD)simz80(pc);

	if (TubeType == TubeDevice::AcornZ80)
	{
		if (TubeintStatus & (1 << R1))
			set_Z80_irq_line(true);

		if (TubeintStatus & (1 << R4))
			set_Z80_irq_line(true);

		if (TubeintStatus == 0)
			set_Z80_irq_line(false);

		if (TubeNMIStatus)
			set_Z80_nmi_line(true);
		else
			set_Z80_nmi_line(false);
	}
}

void Z80Init()
{
	char PathName[MAX_PATH];

	WriteLog("Z80Init()\n");

	if (TubeType == TubeDevice::AcornZ80)
	{
		strcpy(PathName, RomPath);
		AppendPath(PathName, "BeebFile");
		AppendPath(PathName, "Z80.rom");

		FILE *f = fopen(PathName, "rb");

		if (f != nullptr)
		{
			size_t BytesRead = fread(z80_rom, 1, 4096, f);

			fclose(f);

			if (BytesRead != 4096)
			{
				mainWin->Report(MessageType::Error,
				                "Failed to read ROM file:\n  %s",
				                PathName);
			}
		}
		else
		{
			mainWin->Report(MessageType::Error,
			                "Cannot open ROM:\n %s", PathName);
		}
	}
	else // Tube::TorchZ80
	{
		strcpy(PathName, RomPath);
		AppendPath(PathName, "BeebFile");
		AppendPath(PathName, "CCPN102.rom");

		FILE *f = fopen(PathName, "rb");

		if (f != nullptr)
		{
			size_t BytesRead = fread(z80_rom, 1, 8192, f);

			if (BytesRead <= 2048)
			{
				memcpy(z80_rom + 2048, z80_rom, 2048);
				memcpy(z80_rom + 4096, z80_rom, 2048);
				memcpy(z80_rom + 6144, z80_rom, 2048);
			}
			else if (BytesRead <= 4096)
			{
				memcpy(z80_rom + 4096, z80_rom, 4096);
			}

			fclose(f);
		}
		else
		{
			mainWin->Report(MessageType::Error,
			                "Cannot open ROM:\n %s", PathName);
		}
	}

	inROM = true;

	sp = 0x00;
	pc = 0x00;

	// Clear all registers, PC and SP have already been set
	af[0]=0; regs[0].bc=0; regs[0].de=0; regs[0].hl=0;
	af[1]=0; regs[1].bc=0; regs[1].de=0; regs[1].hl=0;
	ix=0; iy=0; ir=0; regs_sel = 0;
}

void Debug_Z80()
{
	char buff[256];

	int t = 0x4400;

	for (int a = 0; a < 512; ++a)
	{
		int s = Z80_Disassemble(t, buff);
		WriteLog("%04x : %s\n", t, buff);
		t += s;
	}

	trace_z80 = true;

	for (int a = 0; a < 32; ++a)
	{
		PrintHex(0x4400 + a * 16);
	}
}

void PrintHex(int addr)
{
	char buff[80];

	const int num = 16;

	char *p = buff;
	sprintf(p, "%04X : ", addr);
	p += 7;

	for (int i = 0; i < 16; ++i)
	{
		sprintf(p, "%02X ", ReadZ80Mem(addr + i));
		p += 3;
	}

	strcpy(p, " ");
	p += 1;

	for (int i = 0; i < num; ++i)
	{
		int a = ReadZ80Mem(addr + i) & 127;
		if (a < 32 || a == 127) a = '.';
		if (a == '%') a = '.';
		*p++ = (char)a;
		*p = 0;
	}

	WriteLog("%s\n", buff);
}

void SaveZ80UEF(FILE *SUEF)
{
	UEFWriteBuf(z80_ram, sizeof(z80_ram), SUEF);

	// Z80 registers
	UEFWrite16(af[0], SUEF);
	UEFWrite16(af[1], SUEF);
	UEFWrite8(af_sel, SUEF);

	for (int i = 0; i < 2; i++)
	{
		UEFWrite16(regs[i].bc, SUEF);
		UEFWrite16(regs[i].de, SUEF);
		UEFWrite16(regs[i].hl, SUEF);
	}

	UEFWrite8(regs_sel, SUEF);

	UEFWrite16(ir, SUEF);
	UEFWrite16(ix, SUEF);
	UEFWrite16(iy, SUEF);
	UEFWrite16(sp, SUEF);
	UEFWrite16(pc, SUEF);
	UEFWrite16(IFF1, SUEF);
	UEFWrite16(IFF2, SUEF);

	UEFWrite16(pc, SUEF);
	UEFWrite16(PreZPC, SUEF);
	UEFWrite8(inROM, SUEF);
}

void LoadZ80UEF(FILE *SUEF)
{
	UEFReadBuf(z80_ram, sizeof(z80_ram), SUEF);

	// Z80 registers
	af[0] = UEFRead16(SUEF);
	af[1] = UEFRead16(SUEF);
	af_sel = UEFRead8(SUEF);

	for (int i = 0; i < 2; i++)
	{
		regs[i].bc = UEFRead16(SUEF);
		regs[i].de = UEFRead16(SUEF);
		regs[i].hl = UEFRead16(SUEF);
	}

	regs_sel = UEFRead8(SUEF);

	ir = UEFRead16(SUEF);
	ix = UEFRead16(SUEF);
	iy = UEFRead16(SUEF);
	sp = UEFRead16(SUEF);
	pc = UEFRead16(SUEF);
	IFF1 = UEFRead16(SUEF);
	IFF2 = UEFRead16(SUEF);

	pc = UEFRead16(SUEF);
	PreZPC = UEFRead16(SUEF);
	inROM = UEFReadBool(SUEF);
}
