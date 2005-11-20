#include <stdio.h>
#include <string.h>

#include "beebmem.h"
#include "mem_mmu.h"
#include "simz80.h"

#include "tube.h"

extern int trace;

int Enable_Z80 = 0;
int trace_z80 = 0;
int debug_z80 = 0;
int TorchTube = 0;

unsigned char z80_rom[65536L];
unsigned char z80_ram[65536L];

extern FILE *tlog;

#define TRUE 1
#define FALSE 0

int inROM = 1;

unsigned char ReadZ80Mem(int pc)

{
    return (inROM) ? z80_rom[pc & 0xffff] : z80_ram[pc & 0xffff];
}

void WriteZ80Mem(int pc, unsigned char data)

{
    z80_ram[pc & 0xffff] = data;
}

void disp_regs()
{

char buff[64];

    Z80_Disassemble(pc, buff);

    fprintf(tlog, "AF=%04X ",af[0]);	/* Main registers		*/
    if(af[0] & 128) fputc('M', tlog); else fputc('P', tlog);
    if(af[0] & 64)  fputc('Z', tlog); else fputc('.', tlog);
    if(af[0] & 32)  fputc('5', tlog); else fputc('.', tlog);
    if(af[0] & 16)  fputc('H', tlog); else fputc('.', tlog);
    if(af[0] & 8)   fputc('3', tlog); else fputc('.', tlog);
    if(af[0] & 4)   fputc('V', tlog); else fputc('.', tlog);
    if(af[0] & 2)   fputc('N', tlog); else fputc('.', tlog);
    if(af[0] & 1)   fputc('C', tlog); else fputc('.', tlog);
    fprintf(tlog, " BC=%04X DE=%04X HL=%04X", regs[0].bc, regs[0].de, regs[0].hl);
    fprintf(tlog, " IX=%04X I=%02X PC=%04X",ix, ir, pc);
    fprintf(tlog, ":%02X,%02X,%02X,%02X",ReadZ80Mem(pc),ReadZ80Mem(pc+1),ReadZ80Mem(pc+2),ReadZ80Mem(pc+3));
    fprintf(tlog, " %s\n", buff);

    fprintf(tlog, "AF'%04X ",af[1]);	/* Alternate registers		*/
    if(af[1] & 128) fputc('M', tlog); else fputc('P', tlog);
    if(af[1] & 64)  fputc('Z', tlog); else fputc('.', tlog);
    if(af[1] & 32)  fputc('5', tlog); else fputc('.', tlog);
    if(af[1] & 16)  fputc('H', tlog); else fputc('.', tlog);
    if(af[1] & 8)   fputc('3', tlog); else fputc('.', tlog);
    if(af[1] & 4)   fputc('V', tlog); else fputc('.', tlog);
    if(af[1] & 2)   fputc('N', tlog); else fputc('.', tlog);
    if(af[1] & 1)   fputc('C', tlog); else fputc('.', tlog);
    fprintf(tlog, " BC'%04X DE'%04X HL'%04X",regs[1].bc,regs[1].de,regs[1].hl);
    fprintf(tlog, " IY=%04X R=%02x SP=%04X",iy, regs_sel, sp);
    fprintf(tlog, ":%02X,%02X,%02X,%02X\n",ReadZ80Mem(sp),ReadZ80Mem(sp+1),ReadZ80Mem(sp+2),ReadZ80Mem(sp+3));
}

int in(unsigned int addr)

{
int value = 0xff;
int tmp;
int c;

    addr &= 255;

    if ( (addr == 0x05) || (addr == 0x01) )
    {
        value = ReadTorchTubeFromParasiteSide(1);        // Data Port

//        c = value & 127;
//        if ( (c < 32) || (c == 127) ) c = '.';
//        fprintf(tlog, "Read  Value = 0x%02x (%c)\n", value, c);

//        if (value == 0x80) trace_z80 = 1;
    
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
    
    return value;
}

void out(unsigned int addr, unsigned char value)

{
int c;

    addr &= 255;
    
    if ( (addr == 0x00) || (addr == 0x04) )
    {
        WriteTorchTubeFromParasiteSide(1, value);

//        c = value & 127;
//        if ( (c < 32) || (c == 127) ) c = '.';
//        fprintf(tlog, "Write Value = 0x%02x (%c)\n", value, c);

    }
    
}


void z80_execute()
{

    if (Enable_Z80)
    {

//        if (PC == 0x74C4) trace_z80 = 1;

        if (trace_z80)
        {
//            if (pc <= 0xf800)       // Don't trace BIOS cos toooo much data
                disp_regs();
        }

        pc = simz80(pc);
    }
}

void init_z80()

{
char path[256];
FILE *f;

//    fprintf(tlog, "init_z80()\n");

    strcpy(path,RomPath);
    strcat(path,"/beebfile/ccpn102.rom");

    f = fopen(path, "rb");

    fread(z80_rom, 8192, 1, f);
    fclose(f);

    inROM = 1;

    sp = 0x00;
    pc = 0x00;
    
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
        fprintf(tlog, "%04x : %s\n", t, buff);
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

    fprintf(tlog, "%s\n", buff);
		
}
