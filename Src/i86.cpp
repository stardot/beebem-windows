/****************************************************************************
*             real mode i286 emulator v1.4 by Fabrice Frances               *
*               (initial work based on David Hedley's pcemu)                *
****************************************************************************/
/* 26.March 2000 PeT changed set_irq_line */


#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "tube.h"
#include "beebmem.h"

#define LSB_FIRST

#include "osd_cpu.h"
#include "i86.h"

/*************************************
 *
 *  Interrupt line constants
 *
 *************************************/

enum
{
	/* line states */
	CLEAR_LINE,				/* clear (a fired, held or pulsed) line */
	ASSERT_LINE1,				/* assert an interrupt immediately */
	ASSERT_LINE4,				/* assert an interrupt immediately */
	INPUT_LINE_IRQ4,
	INPUT_LINE_NMI
};

/* All pre-i286 CPUs have a 1MB address space */
#define AMASK 0xfffff

/* I86 registers */
typedef union
{									   /* eight general registers */
	UINT16 w[8];					   /* viewed as 16 bits registers */
	UINT8 b[16];					   /* or as 8 bit registers */
}
i86basicregs;

typedef struct
{
	i86basicregs regs;
	UINT32 pc;
	UINT32 prevpc;
	UINT32 base[4];
	UINT16 sregs[4];
	UINT16 flags;
	int (*irq_callback) (int irqline);
	int AuxVal, OverVal, SignVal, ZeroVal, CarryVal, DirVal;		/* 0 or non-0 valued flags */
	UINT8 ParityVal;
	UINT8 TF, IF;				   /* 0 or 1 valued flags */
	UINT8 MF;						   /* V30 mode flag */
	UINT8 int_vector;
	INT8 nmi_state;
	INT8 irq_state;
	INT8 test_state;	/* PJB 03/05 */
	int extra_cycles;       /* extra cycles for interrupts */
}
i86_Regs;

int i86_ICount;

static i86_Regs I;
static UINT32 prefix_base;	        /* base address of the latest prefix segment */
static UINT8 seg_prefix;		    /* prefix segment indicator */

static UINT8 parity_table[256];

#include "i86time.c"

static struct i86_timing cycles;

extern int i386_dasm_one(char *buffer, UINT32 eip, int addr_size, int op_size);

extern unsigned char TubeintStatus;
extern unsigned char TubeNMIStatus;

void io_write_byte_8(offs_t address, UINT8 data);
UINT8 io_read_byte_8(offs_t address);

void DoDMA(void);
void MemoryDump(int addr, int count);

int dis_count = 0;
int trace_186 = 0;

UINT8 *                     opcode_base;
offs_t						opcode_mask = 0xfffff;			/* mask to apply to the opcode address */

/* ----- opcode and opcode argument reading ----- */
INLINE UINT8  cpu_readop(offs_t A)			{ return (opcode_base[(A) & opcode_mask]); }
INLINE UINT8  cpu_readop_arg(offs_t A)			{ return (opcode_base[(A) & opcode_mask]); }

#define change_pc(pc)																	\


// address_space active_address_space[ADDRESS_SPACES];/* address space data */


INLINE void program_write_byte_8(offs_t address, UINT8 data)
{
    if (address < 0xf0000)
    {
		opcode_base[address] = data;
    }
}

INLINE UINT8 program_read_byte_8(offs_t address)
{
    return opcode_base[address];
}

#include "ea.h"

#undef PREFIX
#undef PREFIX86

#define PREFIX(name) i186##name
#define PREFIX86(name) i186##name
#define PREFIX186(name) i186##name

#include "instr86.h"
#include "instr86.c"

void i86_init(void)
{
	unsigned int i, j, c;
	BREGS reg_name[8] = {AL, CL, DL, BL, AH, CH, DH, BH};
	for (i = 0; i < 256; i++)
	{
		for (j = i, c = 0; j > 0; j >>= 1)
			if (j & 1)
				c++;

		parity_table[i] = !(c & 1);
	}

	for (i = 0; i < 256; i++)
	{
		Mod_RM.reg.b[i] = reg_name[(i & 0x38) >> 3];
		Mod_RM.reg.w[i] = (WREGS) ((i & 0x38) >> 3);
	}

	for (i = 0xc0; i < 0x100; i++)
	{
		Mod_RM.RM.w[i] = (WREGS) (i & 7);
		Mod_RM.RM.b[i] = (BREGS) reg_name[i & 7];
	}

}

void i86_reset(void)
{
	memset(&I, 0, sizeof (I));

	I.sregs[CS] = 0xf000;
	I.base[CS] = SegBase(CS);
	I.pc = 0xffff0 & AMASK;
	ExpandFlags(I.flags);

	change_pc(I.pc);

	cycles = i186_cycles;
}

void i86_exit(void)
{
	/* nothing to do ? */
}

/* ASG 971222 -- added these interface functions */

static void set_irq_line(int irqline, int state)
{
	if (irqline == INPUT_LINE_NMI)
	{
		if (I.nmi_state == state)
			return;

		/* on a rising edge, signal the NMI */
		if (state != CLEAR_LINE)
		{
			I.nmi_state = state;
			PREFIX(_interrupt)(I86_NMI_INT_VECTOR);
		}
	}
	else
	{
        if (I.irq_state != state)
        {
    		/* if the IF is set, signal an interrupt */
	    	if ( (state != CLEAR_LINE) && I.IF)
            {
	            I.irq_state = state;
                PREFIX(_interrupt)(12);
            }
            else
            {
                I.irq_state = CLEAR_LINE;
            }
        }
	}
}

/* PJB 03/05 */
static void set_test_line(int state)
{
        I.test_state = !state;
}

void Dis186(void)

{
char buffer[256];
char buff[256];
int le, i;

	le = i386_dasm_one(buffer, I.pc, 0, 0) & 0xff;
            
    sprintf(buff, "[%04x:%04x] AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x DS=%04x ES=%04x SS:SP = %04x:%04x ", 
        I.sregs[CS], (I.pc - I.base[CS]) & 0xffff, 
        I.regs.w[AX], I.regs.w[BX], I.regs.w[CX], I.regs.w[DX], I.regs.w[SI], I.regs.w[DI], I.sregs[DS], I.sregs[ES],
		I.sregs[SS], I.regs.w[SP]);
	
	for (i = 0; i < le; ++i) 
	{
		sprintf(buff + strlen(buff), "%02x ", ReadByte(I.pc + i));
	}
        
	for (i = le; i < 10; ++i) 
	{
		sprintf(buff + strlen(buff), "   ");
	}

	sprintf(buff + strlen(buff), "%s", buffer); 
    WriteLog(buff);

}

int i186_execute(int num_cycles)
{
static int lastInt = 0;

	/* adjust for any interrupts that came in */
	i86_ICount = num_cycles;
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	/* run until we're out */
	while (i86_ICount > 0)
	{

		seg_prefix = FALSE;
		I.prevpc = I.pc;

        TABLE186;

        if (TubeintStatus & (1<<R1))
            set_irq_line(INPUT_LINE_IRQ4, ASSERT_LINE1);

        if (TubeintStatus & (1<<R4))
            set_irq_line(INPUT_LINE_IRQ4, ASSERT_LINE4);

        if (TubeintStatus == 0)
            set_irq_line(INPUT_LINE_IRQ4, CLEAR_LINE);
        
        lastInt = TubeintStatus;

// DRQ and DMA transfer
        
        if (TubeNMIStatus) DoDMA();

	}

	/* adjust for any interrupts that came in */
	i86_ICount -= I.extra_cycles;
	I.extra_cycles = 0;

	return num_cycles - i86_ICount;
}

offs_t i186_dasm(char *buffer, offs_t pc, UINT8 *oprom, UINT8 *opram, int bytes)
{
	sprintf(buffer, "$%02X", cpu_readop(pc));
	return 1;
}

extern unsigned char ReadTubeFromParasiteSide(unsigned char IOAddr);
extern void WriteTubeFromParasiteSide(unsigned char IOAddr,unsigned char IOData);


UINT8 io_read_byte_8(offs_t address)
{
int i;
UINT8 data;

    if ( (address >= 0x80) && (address <= 0x8f) )
    {
        i = (address - 0x80) / 2;
        data = ReadTubeFromParasiteSide(i);
    }
    else
	{
        data = 0xff;
	}

    return data;
}

int dma_src_segment = 0x00;
int dma_src_offset = 0x00;
offs_t dma_src = 0x00;
int dma_dst_segment = 0x00;
int dma_dst_offset = 0x00;
offs_t dma_dst = 0x00;
int dma_count = 0x00;
int dma_control = 0x00;

void io_write_byte_8(offs_t address, UINT8 data)
{
int i;

    if ( (address >= 0xffc0) && (address <= 0xffcf) )
    {

        switch (address - 0xffc0)
        {
        case 0x00 :
            dma_src_offset = (dma_src_offset & 0xff00) | data;
            break;
        case 0x01 :
            dma_src_offset = (dma_src_offset & 0xff) | (data << 8);
	        dma_src = ((dma_src_segment & 0x0f) << 16) + dma_src_offset;
            break;
        case 0x02 :
            dma_src_segment = (dma_src_segment & 0xff00) | data;
            break;
        case 0x03 :
            dma_src_segment = (dma_src_segment & 0xff) | (data << 8);
	        dma_src = ((dma_src_segment & 0x0f) << 16) + dma_src_offset;
            break;
        case 0x04 :
            dma_dst_offset = (dma_dst_offset & 0xff00) | data;
            break;
        case 0x05 :
            dma_dst_offset = (dma_dst_offset & 0xff) | (data << 8);
	        dma_dst = ((dma_dst_segment & 0x0f) << 16) + dma_dst_offset;
            break;
        case 0x06 :
            dma_dst_segment = (dma_dst_segment & 0xff00) | data;
            break;
        case 0x07 :
            dma_dst_segment = (dma_dst_segment & 0xff) | (data << 8);
	        dma_dst = ((dma_dst_segment & 0x0f) << 16) + dma_dst_offset;
            break;
        case 0x08 :
            dma_count = (dma_count & 0xff00) | data;
            break;
        case 0x09 :
            dma_count = (dma_count & 0xff) | (data << 8);
            break;
        case 0x0a :
            dma_control = (dma_control & 0xff00) | data;
			break;
        case 0x0b :
            dma_control = (dma_control & 0xff) | (data << 8);
			break;
        case 0x0c :
            break;
        case 0x0d :
            break;
        case 0x0e :
            break;
        case 0x0f :
            break;
        }
        return;
    }

    if ( (address >= 0x80) && (address <= 0x8f) )
    {
        i = (address - 0x80) / 2;

        WriteTubeFromParasiteSide( i, data);

        return;

    }

}

void i86_main(void)

{
FILE *f;
char path[256];

    opcode_base = (unsigned char *) malloc(0x100000L);

    memset(opcode_base, 0, 0x100000L);    

	strcpy(path, mainWin->GetUserDataPath());
	strcat(path, "BeebFile/bios.rom");

    f = fopen(path, "rb");
    fseek(f, 0, SEEK_SET);
    fread(opcode_base + 0xf0000L, 16384, 1, f);
    fseek(f, 0, SEEK_SET);
    fread(opcode_base + 0xf4000L, 16384, 1, f);
    fseek(f, 0, SEEK_SET);
    fread(opcode_base + 0xf8000L, 16384, 1, f);
    fseek(f, 0, SEEK_SET);
    fread(opcode_base + 0xfc000L, 16384, 1, f);
    fclose(f);

    i86_init();
    i86_reset();

    R1Status=0;
    ResetTube();

}

void MemoryDump(int addr, int count)
{
	int a, b;
	int s, e;
	int v;
	char info[80];

	s = addr & 0xffff0;
	e = (addr + count - 1) | 0xf;
	if (e > 0xfffff)
		e = 0xfffff;
	for (a = s; a < e; a += 16)
	{
		sprintf(info, "%04X  ", a);

		for (b = 0; b < 16; ++b)
		{
			sprintf(info+strlen(info), "%02X ", program_read_byte_8(a + b));
		}

		for (b = 0; b < 16; ++b)
		{
			v = program_read_byte_8(a + b);
			if (v < 32 || v > 127)
				v = '.';
			sprintf(info+strlen(info), "%c", v);
		}

		WriteLog("%s", info);
	}

}

void DoDMA(void)

{
UINT8 data;

    if (dma_src < 0x100)		// I/O to MEM
    {
        data = io_read_byte_8(dma_src);
        program_write_byte_8(dma_dst, data);
		dma_dst++;

    }
    else						// MEM to I/O
    {
		data = program_read_byte_8(dma_src);
		io_write_byte_8(dma_dst, data);
		dma_src++;
    }
}
