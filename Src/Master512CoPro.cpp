/*
BeebEm - BBC Micro and Master 128 Emulator

Copyright (C) 2020  Chris Needham
Copyright (C) 2020  Carl and MAME contributors

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/****************************************************************************

    NEC V20/V30/V33 emulator modified back to a 8086/80186 emulator

    (Re)Written June-September 2000 by Bryan McPhail (mish@tendril.co.uk) based
    on code by Oliver Bergmann (Raul_Bloodworth@hotmail.com) who based code
    on the i286 emulator by Fabrice Frances which had initial work based on
    David Hedley's pcemu(!).

****************************************************************************/

// Master 512 80186 Coprocessor

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "Master512CoPro.h"
#include "Main.h"
#include "Log.h"
#include "Tube.h"
#include "UEFState.h"

Master512CoPro master512CoPro;

extern unsigned char TubeintStatus;
extern unsigned char TubeNMIStatus;

// All pre-i286 CPUs have a 1MB address space
const uint32_t AMASK = 0xfffff;

#define CF      (m_CarryVal!=0)
#define SF      (m_SignVal<0)
#define ZF      (m_ZeroVal==0)
#define PF      m_parity_table[(uint8_t)m_ParityVal]
#define AF      (m_AuxVal!=0)
#define OF      (m_OverVal!=0)

// The interrupt number of a pending external interrupt pending NMI is 2.
// For INTR interrupts, the level is caught on the bus during an INTA cycle

const uint32_t INT_IRQ = 0x01;
const uint32_t NMI_IRQ = 0x02;

enum
{
	// line states
	CLEAR_LINE,   // clear (a fired, held or pulsed) line
	ASSERT_LINE1, // assert an interrupt immediately
	ASSERT_LINE4, // assert an interrupt immediately
	INPUT_LINE_IRQ4,
	INPUT_LINE_TEST,
	INPUT_LINE_NMI
};

enum
{
	EXCEPTION, IRET,                                // EXCEPTION, iret
	INT3, INT_IMM, INTO_NT, INTO_T,                 // intS
	OVERRIDE,                                       // SEGMENT OVERRIDES
	FLAG_OPS, LAHF, SAHF,                           // FLAG OPERATIONS
	AAA, AAS, AAM, AAD,                             // ARITHMETIC ADJUSTS
	DAA, DAS,                                       // DECIMAL ADJUSTS
	CBW, CWD,                                       // SIGN EXTENSION
	HLT, LOAD_PTR, LEA, NOP, WAIT, XLAT,            // MISC

	JMP_SHORT, JMP_NEAR, JMP_FAR,                   // DIRECT jmpS
	JMP_R16, JMP_M16, JMP_M32,                      // INDIRECT jmpS
	CALL_NEAR, CALL_FAR,                            // DIRECT callS
	CALL_R16, CALL_M16, CALL_M32,                   // INDIRECT callS
	RET_NEAR, RET_FAR, RET_NEAR_IMM, RET_FAR_IMM,   // RETURNS
	JCC_NT, JCC_T, JCXZ_NT, JCXZ_T,                 // CONDITIONAL jmpS
	LOOP_NT, LOOP_T, LOOPE_NT, LOOPE_T,             // LOOPS

	IN_IMM8, IN_IMM16, IN_DX8, IN_DX16,             // PORT READS
	OUT_IMM8, OUT_IMM16, OUT_DX8, OUT_DX16,         // PORT WRITES

	MOV_RR8, MOV_RM8, MOV_MR8,                      // MOVE, 8-BIT
	MOV_RI8, MOV_MI8,                               // MOVE, 8-BIT IMMEDIATE
	MOV_RR16, MOV_RM16, MOV_MR16,                   // MOVE, 16-BIT
	MOV_RI16, MOV_MI16,                             // MOVE, 16-BIT IMMEDIATE
	MOV_AM8, MOV_AM16, MOV_MA8, MOV_MA16,           // MOVE, al/ax MEMORY
	MOV_SR, MOV_SM, MOV_RS, MOV_MS,                 // MOVE, SEGMENT REGISTERS
	XCHG_RR8, XCHG_RM8,                             // EXCHANGE, 8-BIT
	XCHG_RR16, XCHG_RM16, XCHG_AR16,                // EXCHANGE, 16-BIT

	PUSH_R16, PUSH_M16, PUSH_SEG, PUSHF,            // PUSHES
	POP_R16, POP_M16, POP_SEG, POPF,                // POPS

	ALU_RR8, ALU_RM8, ALU_MR8,                      // alu OPS, 8-BIT
	ALU_RI8, ALU_MI8, ALU_MI8_RO,                   // alu OPS, 8-BIT IMMEDIATE
	ALU_RR16, ALU_RM16, ALU_MR16,                   // alu OPS, 16-BIT
	ALU_RI16, ALU_MI16, ALU_MI16_RO,                // alu OPS, 16-BIT IMMEDIATE
	ALU_R16I8, ALU_M16I8, ALU_M16I8_RO,             // alu OPS, 16-BIT W/8-BIT IMMEDIATE
	MUL_R8, MUL_R16, MUL_M8, MUL_M16,               // mul
	IMUL_R8, IMUL_R16, IMUL_M8, IMUL_M16,           // imul
	DIV_R8, DIV_R16, DIV_M8, DIV_M16,               // div
	IDIV_R8, IDIV_R16, IDIV_M8, IDIV_M16,           // idiv
	INCDEC_R8, INCDEC_R16, INCDEC_M8, INCDEC_M16,   // inc/dec
	NEGNOT_R8, NEGNOT_R16, NEGNOT_M8, NEGNOT_M16,   // neg/not

	ROT_REG_1, ROT_REG_BASE, ROT_REG_BIT,           // REG SHIFT/ROTATE
	ROT_M8_1, ROT_M8_BASE, ROT_M8_BIT,              // M8 SHIFT/ROTATE
	ROT_M16_1, ROT_M16_BASE, ROT_M16_BIT,           // M16 SHIFT/ROTATE

	CMPS8, REP_CMPS8_BASE, REP_CMPS8_COUNT,         // cmps 8-BIT
	CMPS16, REP_CMPS16_BASE, REP_CMPS16_COUNT,      // cmps 16-BIT
	SCAS8, REP_SCAS8_BASE, REP_SCAS8_COUNT,         // scas 8-BIT
	SCAS16, REP_SCAS16_BASE, REP_SCAS16_COUNT,      // scas 16-BIT
	LODS8, REP_LODS8_BASE, REP_LODS8_COUNT,         // lods 8-BIT
	LODS16, REP_LODS16_BASE, REP_LODS16_COUNT,      // lods 16-BIT
	STOS8, REP_STOS8_BASE, REP_STOS8_COUNT,         // stos 8-BIT
	STOS16, REP_STOS16_BASE, REP_STOS16_COUNT,      // stos 16-BIT
	MOVS8, REP_MOVS8_BASE, REP_MOVS8_COUNT,         // movs 8-BIT
	MOVS16, REP_MOVS16_BASE, REP_MOVS16_COUNT,      // movs 16-BIT

	INS8, REP_INS8_BASE, REP_INS8_COUNT,            // (80186) ins 8-BIT
	INS16, REP_INS16_BASE, REP_INS16_COUNT,         // (80186) ins 16-BIT
	OUTS8, REP_OUTS8_BASE, REP_OUTS8_COUNT,         // (80186) outs 8-BIT
	OUTS16, REP_OUTS16_BASE, REP_OUTS16_COUNT,      // (80186) outs 16-BIT
	PUSH_IMM, PUSHA, POPA,                          // (80186) push IMMEDIATE, pusha/popa
	IMUL_RRI8, IMUL_RMI8,                           // (80186) imul IMMEDIATE 8-BIT
	IMUL_RRI16, IMUL_RMI16,                         // (80186) imul IMMEDIATE 16-BIT
	ENTER0, ENTER1, ENTER_BASE, ENTER_COUNT, LEAVE, // (80186) enter/leave
	BOUND                                           // (80186) bound
};

static const uint8_t Timing[] =
{
	51,32,              // exception, IRET
	2, 0, 4, 2,         // INTs
	2,                  // segment overrides
	2, 4, 4,            // flag operations
	4, 4,83,60,         // arithmetic adjusts
	4, 4,               // decimal adjusts
	2, 5,               // sign extension
	2,24, 2, 2, 3,11,   // misc

	15,15,15,           // direct JMPs
	11,18,24,           // indirect JMPs
	19,28,              // direct CALLs
	16,21,37,           // indirect CALLs
	20,32,24,31,        // returns
	4,16, 6,18,         // conditional JMPs
	5,17, 6,18,         // loops

	10,14, 8,12,        // port reads
	10,14, 8,12,        // port writes

	2, 8, 9,            // move, 8-bit
	4,10,               // move, 8-bit immediate
	2, 8, 9,            // move, 16-bit
	4,10,               // move, 16-bit immediate
	10,10,10,10,        // move, AL/AX memory
	2, 8, 2, 9,         // move, segment registers
	4,17,               // exchange, 8-bit
	4,17, 3,            // exchange, 16-bit

	15,24,14,14,        // pushes
	12,25,12,12,        // pops

	3, 9,16,            // ALU ops, 8-bit
	4,17,10,            // ALU ops, 8-bit immediate
	3, 9,16,            // ALU ops, 16-bit
	4,17,10,            // ALU ops, 16-bit immediate
	4,17,10,            // ALU ops, 16-bit w/8-bit immediate
	70,118,76,128,      // MUL
	80,128,86,138,      // IMUL
	80,144,86,154,      // DIV
	101,165,107,175,    // IDIV
	3, 2,15,15,         // INC/DEC
	3, 3,16,16,         // NEG/NOT

	2, 8, 4,            // reg shift/rotate
	15,20, 4,           // m8 shift/rotate
	15,20, 4,           // m16 shift/rotate

	22, 9,21,           // CMPS 8-bit
	22, 9,21,           // CMPS 16-bit
	15, 9,14,           // SCAS 8-bit
	15, 9,14,           // SCAS 16-bit
	12, 9,11,           // LODS 8-bit
	12, 9,11,           // LODS 16-bit
	11, 9,10,           // STOS 8-bit
	11, 9,10,           // STOS 16-bit
	18, 9,17,           // MOVS 8-bit
	18, 9,17,           // MOVS 16-bit
};

// DMA control register

const uint16_t DEST_MIO                = 0x8000;
const uint16_t DEST_DECREMENT          = 0x4000;
const uint16_t DEST_INCREMENT          = 0x2000;
const uint16_t DEST_NO_CHANGE          = DEST_DECREMENT | DEST_INCREMENT;
const uint16_t DEST_INCDEC_MASK        = DEST_DECREMENT | DEST_INCREMENT;
const uint16_t SRC_MIO                 = 0x1000;
const uint16_t SRC_DECREMENT           = 0x0800;
const uint16_t SRC_INCREMENT           = 0x0400;
const uint16_t SRC_NO_CHANGE           = SRC_DECREMENT | SRC_INCREMENT;
const uint16_t SRC_INCDEC_MASK         = SRC_DECREMENT | SRC_INCREMENT;
const uint16_t TERMINATE_ON_ZERO       = 0x0200;
const uint16_t INTERRUPT_ON_ZERO       = 0x0100;
const uint16_t SYNC_MASK               = 0x00C0;
const uint16_t SYNC_SOURCE             = 0x0040;
const uint16_t SYNC_DEST               = 0x0080;
const uint16_t CHANNEL_PRIORITY        = 0x0020;
const uint16_t TIMER_DRQ               = 0x0010;
const uint16_t CHG_NOCHG               = 0x0004;

const uint16_t ST_STOP = 0x0002;

void Master512CoPro::execute_set_input(int inptnum, int state)
{
	if (inptnum == INPUT_LINE_NMI)
	{
		if (m_nmi_state == state)
		{
			return;
		}

		m_nmi_state = state;

		if (state != CLEAR_LINE)
		{
			m_pending_irq |= NMI_IRQ;
		}
	}
	else if (inptnum == INPUT_LINE_TEST)
	{
		m_test_state = state;
	}
	else
	{
		m_irq_state = state;

		if (state == CLEAR_LINE)
		{
			m_pending_irq &= ~INT_IRQ;
		}
		else
		{
			m_pending_irq |= INT_IRQ;
		}
	}
}

void Master512CoPro::CLK(uint8_t op)
{
	m_icount -= Timing[op];
}

void Master512CoPro::CLKM(uint8_t op_reg, uint8_t op_mem)
{
	m_icount -= (m_modrm >= 0xc0) ? Timing[op_reg] : Timing[op_mem];
}

bool Master512CoPro::common_op(uint8_t op)
{
	switch (op)
	{
		case 0x00: // i_add_br8
			DEF_br8();
			set_CFB(ADDB());
			PutbackRMByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_MR8);
			break;

		case 0x01: // i_add_wr16
			DEF_wr16();
			set_CFW(ADDX());
			PutbackRMWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_MR16);
			break;

		case 0x02: // i_add_r8b
			DEF_r8b();
			set_CFB(ADDB());
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x03: // i_add_r16w
			DEF_r16w();
			set_CFW(ADDX());
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x04: // i_add_ald8
			DEF_ald8();
			set_CFB(ADDB());
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x05: // i_add_axd16
			DEF_axd16();
			set_CFW(ADDX());
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x06: // i_push_es
			PUSH(m_sregs[ES]);
			CLK(PUSH_SEG);
			break;

		case 0x07: // i_pop_es
			m_sregs[ES] = POP();
			CLK(POP_SEG);
			break;

		case 0x08: // i_or_br8
			DEF_br8();
			ORB();
			PutbackRMByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_MR8);
			break;

		case 0x09: // i_or_wr16
			DEF_wr16();
			ORW();
			PutbackRMWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_MR16);
			break;

		case 0x0a: // i_or_r8b
			DEF_r8b();
			ORB();
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x0b: // i_or_r16w
			DEF_r16w();
			ORW();
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x0c: // i_or_ald8
			DEF_ald8();
			ORB();
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x0d: // i_or_axd16
			DEF_axd16();
			ORW();
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x0e: // i_push_cs
			PUSH(m_sregs[CS]);
			CLK(PUSH_SEG);
			break;

		case 0x10: // i_adc_br8
		{
			DEF_br8();
			m_src += CF ? 1 : 0;
			uint32_t tmpcf = ADDB();
			PutbackRMByte((uint8_t)m_dst);
			set_CFB(tmpcf);
			CLKM(ALU_RR8,ALU_MR8);
			break;
		}

		case 0x11: // i_adc_wr16
		{
			DEF_wr16();
			m_src += CF ? 1 : 0;
			uint32_t tmpcf = ADDX();
			PutbackRMWord((uint16_t)m_dst);
			set_CFW(tmpcf);
			CLKM(ALU_RR16,ALU_MR16);
			break;
		}

		case 0x12: // i_adc_r8b
			DEF_r8b();
			m_src += CF ? 1 : 0;
			set_CFB(ADDB());
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x13: // i_adc_r16w
			DEF_r16w();
			m_src += CF ? 1 : 0;
			set_CFW(ADDX());
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x14: // i_adc_ald8
			DEF_ald8();
			m_src += CF ? 1 : 0;
			set_CFB(ADDB());
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x15: // i_adc_axd16
			DEF_axd16();
			m_src += CF ? 1 : 0;
			set_CFW(ADDX());
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x16: // i_push_ss
			PUSH(m_sregs[SS]);
			CLK(PUSH_SEG);
			break;

		case 0x17: // i_pop_ss
			m_sregs[SS] = POP();
			CLK(POP_SEG);
			m_no_interrupt = 1;
			break;

		case 0x18: // i_sbb_br8
		{
			DEF_br8();
			m_src += CF ? 1 : 0;
			uint32_t tmpcf = SUBB();
			PutbackRMByte((uint8_t)m_dst);
			set_CFB(tmpcf);
			CLKM(ALU_RR8,ALU_MR8);
			break;
		}

		case 0x19: // i_sbb_wr16
		{
			DEF_wr16();
			m_src += CF ? 1 : 0;
			uint32_t tmpcf = SUBX();
			PutbackRMWord((uint16_t)m_dst);
			set_CFW(tmpcf);
			CLKM(ALU_RR16,ALU_MR16);
			break;
		}

		case 0x1a: // i_sbb_r8b
			DEF_r8b();
			m_src += CF ? 1 : 0;
			set_CFB(SUBB());
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x1b: // i_sbb_r16w
			DEF_r16w();
			m_src += CF ? 1 : 0;
			set_CFW(SUBX());
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x1c: // i_sbb_ald8
			DEF_ald8();
			m_src += CF ? 1 : 0;
			set_CFB(SUBB());
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x1d: // i_sbb_axd16
			DEF_axd16();
			m_src += CF ? 1 : 0;
			set_CFW(SUBX());
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x1e: // i_push_ds
			PUSH(m_sregs[DS]);
			CLK(PUSH_SEG);
			break;

		case 0x1f: // i_pop_ds
			m_sregs[DS] = POP();
			CLK(POP_SEG);
			break;

		case 0x20: // i_and_br8
			DEF_br8();
			ANDB();
			PutbackRMByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_MR8);
			break;

		case 0x21: // i_and_wr16
			DEF_wr16();
			ANDX();
			PutbackRMWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_MR16);
			break;

		case 0x22: // i_and_r8b
			DEF_r8b();
			ANDB();
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x23: // i_and_r16w
			DEF_r16w();
			ANDX();
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x24: // i_and_ald8
			DEF_ald8();
			ANDB();
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x25: // i_and_axd16
			DEF_axd16();
			ANDX();
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x26: // i_es
			m_seg_prefix_next = true;
			m_prefix_seg = ES;
			CLK(OVERRIDE);
			break;

		case 0x27: // i_daa
			ADJ4(6,0x60);
			CLK(DAA);
			break;

		case 0x28: // i_sub_br8
			DEF_br8();
			set_CFB(SUBB());
			PutbackRMByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_MR8);
			break;

		case 0x29: // i_sub_wr16
			DEF_wr16();
			set_CFW(SUBX());
			PutbackRMWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_MR16);
			break;

		case 0x2a: // i_sub_r8b
			DEF_r8b();
			set_CFB(SUBB());
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x2b: // i_sub_r16w
			DEF_r16w();
			set_CFW(SUBX());
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x2c: // i_sub_ald8
			DEF_ald8();
			set_CFB(SUBB());
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x2d: // i_sub_axd16
			DEF_axd16();
			set_CFW(SUBX());
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x2e: // i_cs
			m_seg_prefix_next = true;
			m_prefix_seg = CS;
			CLK(OVERRIDE);
			break;

		case 0x2f: // i_das
			ADJ4(-6,-0x60);
			CLK(DAS);
			break;

		case 0x30: // i_xor_br8
			DEF_br8();
			XORB();
			PutbackRMByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_MR8);
			break;

		case 0x31: // i_xor_wr16
			DEF_wr16();
			XORW();
			PutbackRMWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x32: // i_xor_r8b
			DEF_r8b();
			XORB();
			RegByte((uint8_t)m_dst);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x33: // i_xor_r16w
			DEF_r16w();
			XORW();
			RegWord((uint16_t)m_dst);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x34: // i_xor_ald8
			DEF_ald8();
			XORB();
			m_regs.b[AL] = (uint8_t)m_dst;
			CLK(ALU_RI8);
			break;

		case 0x35: // i_xor_axd16
			DEF_axd16();
			XORW();
			m_regs.w[AX] = (uint16_t)m_dst;
			CLK(ALU_RI16);
			break;

		case 0x36: // i_ss
			m_seg_prefix_next = true;
			m_prefix_seg = SS;
			CLK(OVERRIDE);
			break;

		case 0x37: // i_aaa
			ADJB(6, (m_regs.b[AL] > 0xf9) ? 2 : 1);
			CLK(AAA);
			break;

		case 0x38: // i_cmp_br8
			DEF_br8();
			set_CFB(SUBB());
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x39: // i_cmp_wr16
			DEF_wr16();
			set_CFW(SUBX());
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x3a: // i_cmp_r8b
			DEF_r8b();
			set_CFB(SUBB());
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x3b: // i_cmp_r16w
			DEF_r16w();
			set_CFW(SUBX());
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x3c: // i_cmp_ald8
			DEF_ald8();
			set_CFB(SUBB());
			CLK(ALU_RI8);
			break;

		case 0x3d: // i_cmp_axd16
			DEF_axd16();
			set_CFW(SUBX());
			CLK(ALU_RI16);
			break;

		case 0x3e: // i_ds
			m_seg_prefix_next = true;
			m_prefix_seg = DS;
			CLK(OVERRIDE);
			break;

		case 0x3f: // i_aas
			ADJB(-6, (m_regs.b[AL] < 6) ? -2 : -1);
			CLK(AAS);
			break;

		case 0x40: // i_inc_ax
			IncWordReg(AX);
			CLK(INCDEC_R16);
			break;

		case 0x41: // i_inc_cx
			IncWordReg(CX);
			CLK(INCDEC_R16);
			break;

		case 0x42: // i_inc_dx
			IncWordReg(DX);
			CLK(INCDEC_R16);
			break;

		case 0x43: // i_inc_bx
			IncWordReg(BX);
			CLK(INCDEC_R16);
			break;

		case 0x44: // i_inc_sp
			IncWordReg(SP);
			CLK(INCDEC_R16);
			break;

		case 0x45: // i_inc_bp
			IncWordReg(BP);
			CLK(INCDEC_R16);
			break;

		case 0x46: // i_inc_si
			IncWordReg(SI);
			CLK(INCDEC_R16);
			break;

		case 0x47: // i_inc_di
			IncWordReg(DI);
			CLK(INCDEC_R16);
			break;

		case 0x48: // i_dec_ax
			DecWordReg(AX);
			CLK(INCDEC_R16);
			break;

		case 0x49: // i_dec_cx
			DecWordReg(CX);
			CLK(INCDEC_R16);
			break;

		case 0x4a: // i_dec_dx
			DecWordReg(DX);
			CLK(INCDEC_R16);
			break;

		case 0x4b: // i_dec_bx
			DecWordReg(BX);
			CLK(INCDEC_R16);
			break;

		case 0x4c: // i_dec_sp
			DecWordReg(SP);
			CLK(INCDEC_R16);
			break;

		case 0x4d: // i_dec_bp
			DecWordReg(BP);
			CLK(INCDEC_R16);
			break;

		case 0x4e: // i_dec_si
			DecWordReg(SI);
			CLK(INCDEC_R16);
			break;

		case 0x4f: // i_dec_di
			DecWordReg(DI);
			CLK(INCDEC_R16);
			break;

		case 0x50: // i_push_ax
			PUSH(m_regs.w[AX]);
			CLK(PUSH_R16);
			break;

		case 0x51: // i_push_cx
			PUSH(m_regs.w[CX]);
			CLK(PUSH_R16);
			break;

		case 0x52: // i_push_dx
			PUSH(m_regs.w[DX]);
			CLK(PUSH_R16);
			break;

		case 0x53: // i_push_bx
			PUSH(m_regs.w[BX]);
			CLK(PUSH_R16);
			break;

		case 0x54: // i_push_sp
			PUSH(m_regs.w[SP]-2);
			CLK(PUSH_R16);
			break;

		case 0x55: // i_push_bp
			PUSH(m_regs.w[BP]);
			CLK(PUSH_R16);
			break;

		case 0x56: // i_push_si
			PUSH(m_regs.w[SI]);
			CLK(PUSH_R16);
			break;

		case 0x57: // i_push_di
			PUSH(m_regs.w[DI]);
			CLK(PUSH_R16);
			break;

		case 0x58: // i_pop_ax
			m_regs.w[AX] = POP();
			CLK(POP_R16);
			break;

		case 0x59: // i_pop_cx
			m_regs.w[CX] = POP();
			CLK(POP_R16);
			break;

		case 0x5a: // i_pop_dx
			m_regs.w[DX] = POP();
			CLK(POP_R16);
			break;

		case 0x5b: // i_pop_bx
			m_regs.w[BX] = POP();
			CLK(POP_R16);
			break;

		case 0x5c: // i_pop_sp
			m_regs.w[SP] = POP();
			CLK(POP_R16);
			break;

		case 0x5d: // i_pop_bp
			m_regs.w[BP] = POP();
			CLK(POP_R16);
			break;

		case 0x5e: // i_pop_si
			m_regs.w[SI] = POP();
			CLK(POP_R16);
			break;

		case 0x5f: // i_pop_di
			m_regs.w[DI] = POP();
			CLK(POP_R16);
			break;

		// 8086 'invalid opcodes', as documented at http://www.os2museum.com/wp/?p=2147 and tested on real hardware
		// - 0x60 - 0x6f are aliases to 0x70 - 0x7f.
		// - 0xc0, 0xc1, 0xc8, 0xc9 are also aliases where the CPU ignores BIT 1 (*).
		// - 0xf1 is an alias to 0xf0.
		//
		//      Instructions are used in the boot sector for some versions of
		//      MS-DOS  (e.g. the DEC Rainbow-100 version of DOS 2.x)

		case 0x60:
		case 0x70: // i_jo
			JMP( OF);
			break;

		case 0x61:
		case 0x71: // i_jno
			JMP(!OF);
			break;

		case 0x62:
		case 0x72: // i_jc
			JMP( CF);
			break;

		case 0x63:
		case 0x73: // i_jnc
			JMP(!CF);
			break;

		case 0x64:
		case 0x74: // i_jz
			JMP( ZF);
			break;

		case 0x65:
		case 0x75: // i_jnz
			JMP(!ZF);
			break;

		case 0x66:
		case 0x76: // i_jce
			JMP(CF || ZF);
			break;

		case 0x67:
		case 0x77: // i_jnce
			JMP(!(CF || ZF));
			break;

		case 0x68:
		case 0x78: // i_js
			JMP(SF);
			break;

		case 0x69:
		case 0x79: // i_jns
			JMP(!SF);
			break;

		case 0x6a:
		case 0x7a: // i_jp
			JMP(PF != 0);
			break;

		case 0x6b:
		case 0x7b: // i_jnp
			JMP(PF == 0);
			break;

		case 0x6c:
		case 0x7c: // i_jl
			JMP((SF!=OF)&&(!ZF));
			break;

		case 0x6d:
		case 0x7d: // i_jnl
			JMP(SF==OF);
			break;

		case 0x6e:
		case 0x7e: // i_jle
			JMP((ZF)||(SF!=OF));
			break;

		case 0x6f:
		case 0x7f: // i_jnle
			JMP((SF==OF)&&(!ZF));
			break;

		case 0x80: // i_80pre
		{
			uint32_t tmpcf;
			m_modrm = fetch();
			m_dst = GetRMByte();
			m_src = fetch();
			if (m_modrm >=0xc0 )             { CLK(ALU_RI8); }
			else if ((m_modrm & 0x38)==0x38) { CLK(ALU_MI8_RO); }
			else                             { CLK(ALU_MI8); }
			switch (m_modrm & 0x38)
			{
			case 0x00:                      set_CFB(ADDB()); PutbackRMByte((uint8_t)m_dst); break;
			case 0x08:                      ORB();           PutbackRMByte((uint8_t)m_dst); break;
			case 0x10: m_src += CF ? 1 : 0; tmpcf = ADDB();  PutbackRMByte((uint8_t)m_dst); set_CFB(tmpcf); break;
			case 0x18: m_src += CF ? 1 : 0; tmpcf = SUBB();  PutbackRMByte((uint8_t)m_dst); set_CFB(tmpcf); break;
			case 0x20:                      ANDB();          PutbackRMByte((uint8_t)m_dst); break;
			case 0x28:                      set_CFB(SUBB()); PutbackRMByte((uint8_t)m_dst); break;
			case 0x30:                      XORB();          PutbackRMByte((uint8_t)m_dst); break;
			case 0x38:                      set_CFB(SUBB()); break; // CMP
			}
			break;
		}

		case 0x81: // i_81pre
		{
			uint32_t tmpcf;
			m_modrm = fetch();
			m_dst = GetRMWord();
			m_src = fetch_word();
			if (m_modrm >=0xc0 )             { CLK(ALU_RI16); }
			else if ((m_modrm & 0x38)==0x38) { CLK(ALU_MI16_RO); }
			else                             { CLK(ALU_MI16); }
			switch (m_modrm & 0x38)
			{
			case 0x00:                      set_CFW(ADDX()); PutbackRMWord((uint16_t)m_dst);   break;
			case 0x08:                      ORW();           PutbackRMWord((uint16_t)m_dst);   break;
			case 0x10: m_src += CF ? 1 : 0; tmpcf = ADDX();  PutbackRMWord((uint16_t)m_dst); set_CFW(tmpcf); break;
			case 0x18: m_src += CF ? 1 : 0; tmpcf = SUBX();  PutbackRMWord((uint16_t)m_dst); set_CFW(tmpcf); break;
			case 0x20:                      ANDX();          PutbackRMWord((uint16_t)m_dst);   break;
			case 0x28:                      set_CFW(SUBX()); PutbackRMWord((uint16_t)m_dst);   break;
			case 0x30:                      XORW();          PutbackRMWord((uint16_t)m_dst);   break;
			case 0x38:                      set_CFW(SUBX()); break;  // CMP
			}
			break;
		}

		case 0x82: // i_82pre
		{
			uint32_t tmpcf;
			m_modrm = fetch();
			m_dst = GetRMByte();
			m_src = (int8_t)fetch();
			if (m_modrm >= 0xc0)             { CLK(ALU_RI8); }
			else if ((m_modrm & 0x38)==0x38) { CLK(ALU_MI8_RO); }
			else                             { CLK(ALU_MI8); }
			switch (m_modrm & 0x38)
			{
			case 0x00:                      set_CFB(ADDB()); PutbackRMByte((uint8_t)m_dst);   break;
			case 0x08:                      ORB();           PutbackRMByte((uint8_t)m_dst);   break;
			case 0x10: m_src += CF ? 1 : 0; tmpcf = ADDB();  PutbackRMByte((uint8_t)m_dst); set_CFB(tmpcf); break;
			case 0x18: m_src += CF ? 1 : 0; tmpcf = SUBB();  PutbackRMByte((uint8_t)m_dst); set_CFB(tmpcf); break;
			case 0x20:                      ANDB();          PutbackRMByte((uint8_t)m_dst);   break;
			case 0x28:                      set_CFB(SUBB()); PutbackRMByte((uint8_t)m_dst);   break;
			case 0x30:                      XORB();          PutbackRMByte((uint8_t)m_dst);   break;
			case 0x38:                      set_CFB(SUBB()); break; // CMP
			}
			break;
		}

		case 0x83: // i_83pre
		{
			uint32_t tmpcf;
			m_modrm = fetch();
			m_dst = GetRMWord();
			m_src = (uint16_t)((int16_t)((int8_t)fetch()));
			if (m_modrm >= 0xc0)             { CLK(ALU_R16I8); }
			else if ((m_modrm & 0x38)==0x38) { CLK(ALU_M16I8_RO); }
			else                             { CLK(ALU_M16I8); }
			switch (m_modrm & 0x38)
			{
			case 0x00:                      set_CFW(ADDX()); PutbackRMWord((uint16_t)m_dst); break;
			case 0x08:                      ORW();  PutbackRMWord((uint16_t)m_dst); break;
			case 0x10: m_src += CF ? 1 : 0; tmpcf = ADDX(); PutbackRMWord((uint16_t)m_dst); set_CFW(tmpcf); break;
			case 0x18: m_src += CF ? 1 : 0; tmpcf = SUBX(); PutbackRMWord((uint16_t)m_dst); set_CFW(tmpcf); break;
			case 0x20:                      ANDX(); PutbackRMWord((uint16_t)m_dst); break;
			case 0x28:                      set_CFW(SUBX()); PutbackRMWord((uint16_t)m_dst); break;
			case 0x30:                      XORW(); PutbackRMWord((uint16_t)m_dst); break;
			case 0x38:                      set_CFW(SUBX());                       break; /* CMP */
			}
			break;
		}

		case 0x84: // i_test_br8
			DEF_br8();
			ANDB();
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x85: // i_test_wr16
			DEF_wr16();
			ANDX();
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x86: // i_xchg_br8
			DEF_br8();
			RegByte((uint8_t)m_dst);
			PutbackRMByte((uint8_t)m_src);
			CLKM(XCHG_RR8,XCHG_RM8);
			break;

		case 0x87: // i_xchg_wr16
			DEF_wr16();
			RegWord((uint16_t)m_dst);
			PutbackRMWord((uint16_t)m_src);
			CLKM(XCHG_RR16,XCHG_RM16);
			break;

		case 0x88: // i_mov_br8
			m_modrm = fetch();
			m_src = RegByte();
			PutRMByte((uint8_t)m_src);
			CLKM(ALU_RR8,ALU_MR8);
			break;

		case 0x89: // i_mov_wr16
			m_modrm = fetch();
			m_src = RegWord();
			PutRMWord((uint16_t)m_src);
			CLKM(ALU_RR16,ALU_MR16);
			break;

		case 0x8a: // i_mov_r8b
			m_modrm = fetch();
			m_src = GetRMByte();
			RegByte((uint8_t)m_src);
			CLKM(ALU_RR8,ALU_RM8);
			break;

		case 0x8b: // i_mov_r16w
			m_modrm = fetch();
			m_src = GetRMWord();
			RegWord((uint16_t)m_src);
			CLKM(ALU_RR16,ALU_RM16);
			break;

		case 0x8c: // i_mov_wsreg
			m_modrm = fetch();
			PutRMWord(m_sregs[(m_modrm & 0x18) >> 3]); // confirmed on hw: modrm bit 5 ignored
			CLKM(MOV_RS,MOV_MS);
			break;

		case 0x8d: // i_lea
			m_modrm = fetch();
			get_ea(0, I8086_NONE);
			RegWord(m_eo);
			CLK(LEA);
			break;

		case 0x8e: // i_mov_sregw
			m_modrm = fetch();
			m_src = GetRMWord();
			m_sregs[(m_modrm & 0x18) >> 3] = (uint16_t)m_src; // confirmed on hw: modrm bit 5 ignored
			CLKM(MOV_SR,MOV_SM);
			m_no_interrupt = 1; // Disable IRQ after load segment register.
			break;

		case 0x8f: // i_popw
			m_modrm = fetch();
			PutRMWord( POP() );
			CLKM(POP_R16,POP_M16);
			break;

		case 0x90: // i_nop
			CLK(NOP);
			break;

		case 0x91: // i_xchg_axcx
			XchgAXReg(CX);
			CLK(XCHG_AR16);
			break;

		case 0x92: // i_xchg_axdx
			XchgAXReg(DX);
			CLK(XCHG_AR16);
			break;

		case 0x93: // i_xchg_axbx
			XchgAXReg(BX);
			CLK(XCHG_AR16);
			break;

		case 0x94: // i_xchg_axsp
			XchgAXReg(SP);
			CLK(XCHG_AR16);
			break;

		case 0x95: // i_xchg_axbp
			XchgAXReg(BP);
			CLK(XCHG_AR16);
			break;

		case 0x96: // i_xchg_axsi
			XchgAXReg(SI);
			CLK(XCHG_AR16);
			break;

		case 0x97: // i_xchg_axdi
			XchgAXReg(DI);
			CLK(XCHG_AR16);
			break;

		case 0x98: // i_cbw
			m_regs.b[AH] = (m_regs.b[AL] & 0x80) ? 0xff : 0;
			CLK(CBW);
			break;

		case 0x99: // i_cwd
			m_regs.w[DX] = (m_regs.b[AH] & 0x80) ? 0xffff : 0;
			CLK(CWD);
			break;

		case 0x9a: // i_call_far
			{
				uint16_t tmp = fetch_word();
				uint16_t tmp2 = fetch_word();
				PUSH(m_sregs[CS]);
				PUSH(m_ip);
				m_ip = tmp;
				m_sregs[CS] = tmp2;
				CLK(CALL_FAR);
			}
			break;

		case 0x9b: // i_wait
			// Wait for assertion of /TEST
			if (m_test_state == 0)
			{
				m_icount = 0;
				m_ip--;
			}
			else
				CLK(WAIT);
			break;

		case 0x9c: // i_pushf
			PUSH(CompressFlags());
			CLK(PUSHF);
			break;

		case 0x9d: // i_popf
			i_popf();
			break;

		case 0x9e: // i_sahf
			{
				uint16_t tmp = (CompressFlags() & 0xff00) | (m_regs.b[AH] & 0xd5);
				ExpandFlags(tmp);
				CLK(SAHF);
			}
			break;

		case 0x9f: // i_lahf
			m_regs.b[AH] = (uint8_t)CompressFlags();
			CLK(LAHF);
			break;

		case 0xa0: // i_mov_aldisp
			{
				uint16_t addr = fetch_word();
				m_regs.b[AL] = GetMemB(DS, addr);
				CLK(MOV_AM8);
			}
			break;

		case 0xa1: // i_mov_axdisp
			{
				uint16_t addr = fetch_word();
				m_regs.w[AX] = GetMemW(DS, addr);
				CLK(MOV_AM16);
			}
			break;

		case 0xa2: // i_mov_dispal
			{
				uint16_t addr = fetch_word();
				PutMemB(DS, addr, m_regs.b[AL]);
				CLK(MOV_MA8);
			}
			break;

		case 0xa3: // i_mov_dispax
			{
				uint16_t addr = fetch_word();
				PutMemW(DS, addr, m_regs.w[AX]);
				CLK(MOV_MA16);
			}
			break;

		case 0xa4: // i_movsb
			i_movsb();
			break;

		case 0xa5: // i_movsw
			i_movsw();
			break;

		case 0xa6: // i_cmpsb
			i_cmpsb();
			break;

		case 0xa7: // i_cmpsw
			i_cmpsw();
			break;

		case 0xa8: // i_test_ald8
			DEF_ald8();
			ANDB();
			CLK(ALU_RI8);
			break;

		case 0xa9: // i_test_axd16
			DEF_axd16();
			ANDX();
			CLK(ALU_RI16);
			break;

		case 0xaa: // i_stosb
			i_stosb();
			break;

		case 0xab: // i_stosw
			i_stosw();
			break;

		case 0xac: // i_lodsb
			i_lodsb();
			break;

		case 0xad: // i_lodsw
			i_lodsw();
			break;

		case 0xae: // i_scasb
			i_scasb();
			break;

		case 0xaf: // i_scasw
			i_scasw();
			break;

		case 0xb0: // i_mov_ald8
			m_regs.b[AL] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb1: // i_mov_cld8
			m_regs.b[CL] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb2: // i_mov_dld8
			m_regs.b[DL] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb3: // i_mov_bld8
			m_regs.b[BL] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb4: // i_mov_ahd8
			m_regs.b[AH] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb5: // i_mov_chd8
			m_regs.b[CH] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb6: // i_mov_dhd8
			m_regs.b[DH] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb7: // i_mov_bhd8
			m_regs.b[BH] = fetch();
			CLK(MOV_RI8);
			break;

		case 0xb8: // i_mov_axd16
			m_regs.b[AL] = fetch();
			m_regs.b[AH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xb9: // i_mov_cxd16
			m_regs.b[CL] = fetch();
			m_regs.b[CH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xba: // i_mov_dxd16
			m_regs.b[DL] = fetch();
			m_regs.b[DH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xbb: // i_mov_bxd16
			m_regs.b[BL] = fetch();
			m_regs.b[BH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xbc: // i_mov_spd16
			m_regs.b[SPL] = fetch();
			m_regs.b[SPH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xbd: // i_mov_bpd16
			m_regs.b[BPL] = fetch();
			m_regs.b[BPH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xbe: // i_mov_sid16
			m_regs.b[SIL] = fetch();
			m_regs.b[SIH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xbf: // i_mov_did16
			m_regs.b[DIL] = fetch();
			m_regs.b[DIH] = fetch();
			CLK(MOV_RI16);
			break;

		case 0xc0: // 0xc0 is 0xc2 - see (*)
		case 0xc2: // i_ret_d16
			{
				uint16_t count = fetch_word();
				m_ip = POP();
				m_regs.w[SP] += count;
				CLK(RET_NEAR_IMM);
			}
			break;

		case 0xc1: // 0xc1 is 0xc3 - see (*)
		case 0xc3: // i_ret
			m_ip = POP();
			CLK(RET_NEAR);
			break;

		case 0xc4: // i_les_dw
			m_modrm = fetch();
			RegWord( GetRMWord() );
			m_sregs[ES] = GetnextRMWord();
			CLK(LOAD_PTR);
			break;

		case 0xc5: // i_lds_dw
			m_modrm = fetch();
			RegWord( GetRMWord() );
			m_sregs[DS] = GetnextRMWord();
			CLK(LOAD_PTR);
			break;

		case 0xc6: // i_mov_bd8
			m_modrm = fetch();
			PutImmRMByte();
			CLKM(MOV_RI8,MOV_MI8);
			break;

		case 0xc7: // i_mov_wd16
			m_modrm = fetch();
			PutImmRMWord();
			CLKM(MOV_RI16,MOV_MI16);
			break;

		case 0xc8: // 0xc8 = 0xca - see (*)
		case 0xca: // i_retf_d16
			{
				uint16_t count = fetch_word();
				m_ip = POP();
				m_sregs[CS] = POP();
				m_regs.w[SP] += count;
				CLK(RET_FAR_IMM);
			}
			break;

		case 0xc9: // 0xc9 = 0xcb  - see (*)
		case 0xcb: // i_retf
			m_ip = POP();
			m_sregs[CS] = POP();
			CLK(RET_FAR);
			break;

		case 0xcc: // i_int3
			interrupt(3, 0);
			CLK(INT3);
			break;

		case 0xcd: // i_int
			interrupt(fetch(), 0);
			CLK(INT_IMM);
			break;

		case 0xce: // i_into
			if (OF)
			{
				interrupt(4, 0);
				CLK(INTO_T);
			}
			else
				CLK(INTO_NT);
			break;

		case 0xcf: // i_iret
			m_ip = POP();
			m_sregs[CS] = POP();
			i_popf();
			CLK(IRET);
			break;

		case 0xd0: // i_rotshft_b
			m_modrm = fetch();
			m_src = GetRMByte();
			m_dst = m_src;
			CLKM(ROT_REG_1,ROT_M8_1);
			switch (m_modrm & 0x38)
			{
			case 0x00: ROL_BYTE();  PutbackRMByte((uint8_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x80; break;
			case 0x08: ROR_BYTE();  PutbackRMByte((uint8_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x80; break;
			case 0x10: ROLC_BYTE(); PutbackRMByte((uint8_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x80; break;
			case 0x18: RORC_BYTE(); PutbackRMByte((uint8_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x80; break;
			case 0x30:
			case 0x20: SHL_BYTE(1); m_OverVal = (m_src ^ m_dst) & 0x80; break;
			case 0x28: SHR_BYTE(1); m_OverVal = (m_src ^ m_dst) & 0x80; break;
			case 0x38: SHRA_BYTE(1); m_OverVal = 0; break;
			}
			break;

		case 0xd1: // i_rotshft_w
			m_modrm = fetch();
			m_src = GetRMWord();
			m_dst = m_src;
			CLKM(ROT_REG_1,ROT_M8_1);
			switch (m_modrm & 0x38)
			{
			case 0x00: ROL_WORD();  PutbackRMWord((uint16_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x8000; break;
			case 0x08: ROR_WORD();  PutbackRMWord((uint16_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x8000; break;
			case 0x10: ROLC_WORD(); PutbackRMWord((uint16_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x8000; break;
			case 0x18: RORC_WORD(); PutbackRMWord((uint16_t)m_dst); m_OverVal = (m_src ^ m_dst) & 0x8000; break;
			case 0x30:
			case 0x20: SHL_WORD(1); m_OverVal = (m_src ^ m_dst) & 0x8000;  break;
			case 0x28: SHR_WORD(1); m_OverVal = (m_src ^ m_dst) & 0x8000;  break;
			case 0x38: SHRA_WORD(1); m_OverVal = 0; break;
			}
			break;

		case 0xd4: // i_aam
		{
			uint8_t base = fetch();
			if(!base)
			{
				interrupt(0);
				break;
			}
			m_regs.b[AH] = m_regs.b[AL] / base;
			m_regs.b[AL] %= base;
			set_SZPF_Word(m_regs.w[AX]);
			CLK(AAM);
			break;
		}

		case 0xd5: // i_aad
		{
			uint8_t base = fetch();
			m_regs.b[AL] = m_regs.b[AH] * base + m_regs.b[AL];
			m_regs.b[AH] = 0;
			set_SZPF_Byte(m_regs.b[AL]);
			CLK(AAD);
			break;
		}

		case 0xd6: // i_salc
			m_regs.b[AL] = (CF ? 0xff : 0);
			CLK(ALU_RR8);  // is sbb al,al
			break;

		case 0xd7: // i_trans
			m_regs.b[AL] = GetMemB( DS, m_regs.w[BX] + m_regs.b[AL] );
			CLK(XLAT);
			break;

		case 0xe0: // i_loopne
			{
				int8_t disp = (int8_t)fetch();

				m_regs.w[CX]--;

				if (!ZF && m_regs.w[CX])
				{
					m_ip = m_ip + disp;
					CLK(LOOP_T);
				}
				else
					CLK(LOOP_NT);
			}
			break;

		case 0xe1: // i_loope
			{
				int8_t disp = (int8_t)fetch();

				m_regs.w[CX]--;

				if (ZF && m_regs.w[CX])
				{
					m_ip = m_ip + disp;
					CLK(LOOPE_T);
				}
				else
					CLK(LOOPE_NT);
			}
			break;

		case 0xe2: // i_loop
			{
				int8_t disp = (int8_t)fetch();

				m_regs.w[CX]--;

				if (m_regs.w[CX])
				{
					m_ip = m_ip + disp;
					CLK(LOOP_T);
				}
				else
					CLK(LOOP_NT);
			}
			break;

		case 0xe3: // i_jcxz
			{
				int8_t disp = (int8_t)fetch();

				if (m_regs.w[CX] == 0)
				{
					m_ip = m_ip + disp;
					CLK(JCXZ_T);
				}
				else
					CLK(JCXZ_NT);
			}
			break;

		case 0xe4: // i_inal
			// if (m_lock) m_lock_handler(1);
			m_regs.b[AL] = read_port_byte( fetch() );
			// if (m_lock) { m_lock_handler(0); m_lock = false; }
			CLK(IN_IMM8);
			break;

		case 0xe5: // i_inax
			{
				uint8_t port = fetch();
				m_regs.w[AX] = read_port_word(port);
				CLK(IN_IMM16);
			}
			break;

		case 0xe6: // i_outal
			{
				uint8_t port = fetch();
				write_port_byte(port, m_regs.b[AL]);
				CLK(OUT_IMM8);
			}
			break;

		case 0xe7: // i_outax
			{
				uint8_t port = fetch();
				write_port_word(port, m_regs.w[AX]);
				CLK(OUT_IMM16);
			}
			break;

		case 0xe8: // i_call_d16
			{
				int16_t tmp = (int16_t)fetch_word();

				PUSH(m_ip);
				m_ip = m_ip + tmp;
				CLK(CALL_NEAR);
			}
			break;

		case 0xe9: // i_jmp_d16
			{
				int16_t offset = (int16_t)fetch_word();
				m_ip += offset;
				CLK(JMP_NEAR);
			}
			break;

		case 0xea: // i_jmp_far
			{
				uint16_t tmp = fetch_word();
				uint16_t tmp1 = fetch_word();

				m_sregs[CS] = tmp1;
				m_ip = tmp;
				CLK(JMP_FAR);
			}
			break;

		case 0xeb: // i_jmp_d8
			{
				int tmp = (int)((int8_t)fetch());

				CLK(JMP_SHORT);
				if (tmp==-2 && m_no_interrupt==0 && (m_pending_irq==0) && m_icount>0)
				{
					m_icount%=12; // cycle skip
				}
				m_ip = (uint16_t)(m_ip+tmp);
			}
			break;

		case 0xec: // i_inaldx
			m_regs.b[AL] = read_port_byte(m_regs.w[DX]);
			CLK(IN_DX8);
			break;

		case 0xed: // i_inaxdx
			m_regs.w[AX] = read_port_word(m_regs.w[DX]);
			CLK(IN_DX16);
			break;

		case 0xee: // i_outdxal
			write_port_byte(m_regs.w[DX], m_regs.b[AL]);
			CLK(OUT_DX8);
			break;

		case 0xef: // i_outdxax
			write_port_word(m_regs.w[DX], m_regs.w[AX]);
			CLK(OUT_DX16);
			break;

		case 0xf0: // i_lock
		case 0xf1: // 0xf1 is 0xf0; verified on real CPU
			// WriteLog("%06x: Warning - BUSLOCK\n", m_pc); // Why warn for using lock instruction?
			// m_lock = true;
			m_no_interrupt = 1;
			CLK(NOP);
			break;

		case 0xf2: // i_repne
			{
				bool invalid = false;
				uint8_t next = repx_op();
				uint16_t c = m_regs.w[CX];

				switch (next)
				{
				case 0xa4:  CLK(OVERRIDE); if (c) do { i_movsb(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xa5:  CLK(OVERRIDE); if (c) do { i_movsw(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xa6:  CLK(OVERRIDE); if (c) do { i_cmpsb(); c--; } while (c>0 && !ZF && m_icount>0);   m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xa7:  CLK(OVERRIDE); if (c) do { i_cmpsw(); c--; } while (c>0 && !ZF && m_icount>0);   m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xaa:  CLK(OVERRIDE); if (c) do { i_stosb(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xab:  CLK(OVERRIDE); if (c) do { i_stosw(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xac:  CLK(OVERRIDE); if (c) do { i_lodsb(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xad:  CLK(OVERRIDE); if (c) do { i_lodsw(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xae:  CLK(OVERRIDE); if (c) do { i_scasb(); c--; } while (c>0 && !ZF && m_icount>0);   m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xaf:  CLK(OVERRIDE); if (c) do { i_scasw(); c--; } while (c>0 && !ZF && m_icount>0);   m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				default:
					WriteLog("%06x: REPNE invalid\n", m_pc);
					// Decrement IP so the normal instruction will be executed next
					m_ip--;
					invalid = true;
					break;
				}
				if(c && !invalid)
				{
					if(!(ZF && ((next & 6) == 6)))
						m_ip = m_prev_ip;
				}
			}
			break;

		case 0xf3: // i_repe
			{
				bool invalid = false;
				uint8_t next = repx_op();
				uint16_t c = m_regs.w[CX];

				switch (next)
				{
				case 0xa4:  CLK(OVERRIDE); if (c) do { i_movsb(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xa5:  CLK(OVERRIDE); if (c) do { i_movsw(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xa6:  CLK(OVERRIDE); if (c) do { i_cmpsb(); c--; } while (c>0 && ZF && m_icount>0);    m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xa7:  CLK(OVERRIDE); if (c) do { i_cmpsw(); c--; } while (c>0 && ZF && m_icount>0);    m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xaa:  CLK(OVERRIDE); if (c) do { i_stosb(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xab:  CLK(OVERRIDE); if (c) do { i_stosw(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xac:  CLK(OVERRIDE); if (c) do { i_lodsb(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xad:  CLK(OVERRIDE); if (c) do { i_lodsw(); c--; } while (c>0 && m_icount>0);          m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xae:  CLK(OVERRIDE); if (c) do { i_scasb(); c--; } while (c>0 && ZF && m_icount>0);    m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				case 0xaf:  CLK(OVERRIDE); if (c) do { i_scasw(); c--; } while (c>0 && ZF && m_icount>0);    m_regs.w[CX]=c; m_seg_prefix = false; m_seg_prefix_next = false; break;
				default:
					WriteLog("%06x: REPE invalid\n", m_pc);
					// Decrement IP so the normal instruction will be executed next
					m_ip--;
					invalid = true;
					break;
				}
				if(c && !invalid)
				{
					if(!(!ZF && ((next & 6) == 6)))
						m_ip = m_prev_ip;
				}
			}
			break;

		case 0xf4: // i_hlt
			WriteLog("%06x: HALT\n", m_pc);
			m_icount = 0;
			m_halt = true;
			break;

		case 0xf5: // i_cmc
			m_CarryVal = !m_CarryVal;
			CLK(FLAG_OPS);
			break;

		case 0xf6: // i_f6pre
			{
				uint32_t uresult, uresult2;
				int32_t result, result2;

				m_modrm = fetch();
				uint32_t tmp = GetRMByte();

				switch (m_modrm & 0x38)
				{
				case 0x00:  // TEST
				case 0x08:  // TEST (alias)
					tmp &= fetch();
					m_CarryVal = m_OverVal = 0;
					set_SZPF_Byte(tmp);
					CLKM(ALU_RI8,ALU_MI8_RO);
					break;

				case 0x10:  // NOT
					PutbackRMByte((uint8_t)~tmp);
					CLKM(NEGNOT_R8,NEGNOT_M8);
					break;

				case 0x18:  // NEG
					m_CarryVal = (tmp!=0) ? 1 : 0;
					tmp = (~tmp) + 1;
					set_SZPF_Byte(tmp);
					PutbackRMByte((uint8_t)tmp);
					CLKM(NEGNOT_R8,NEGNOT_M8);
					break;

				case 0x20:  // MUL
					uresult = m_regs.b[AL] * tmp;
					m_regs.w[AX] = (uint16_t)uresult;
					m_CarryVal = m_OverVal = (m_regs.b[AH] != 0) ? 1 : 0;
					set_ZF(m_regs.w[AX]);
					CLKM(MUL_R8,MUL_M8);
					break;

				case 0x28:  // IMUL
					result = (int16_t)((int8_t)m_regs.b[AL])*(int16_t)((int8_t)tmp);
					m_regs.w[AX] = (uint16_t)result;
					m_CarryVal = m_OverVal = (m_regs.b[AH] != 0) ? 1 : 0;
					set_ZF(m_regs.w[AX]);
					CLKM(IMUL_R8,IMUL_M8);
					break;

				case 0x30:  // DIV
					if (tmp)
					{
						uresult = m_regs.w[AX];
						uresult2 = uresult % tmp;
						if ((uresult /= tmp) > 0xff)
						{
							interrupt(0);
						}
						else
						{
							m_regs.b[AL] = (uint8_t)uresult;
							m_regs.b[AH] = (uint8_t)uresult2;
						}
					}
					else
					{
						interrupt(0);
					}
					CLKM(DIV_R8,DIV_M8);
					break;

				case 0x38:  // IDIV
					if (tmp)
					{
						result = (int16_t)m_regs.w[AX];
						result2 = result % (int16_t)((int8_t)tmp);
						if ((result /= (int16_t)((int8_t)tmp)) > 0xff)
						{
							interrupt(0);
						}
						else
						{
							m_regs.b[AL] = (uint8_t)result;
							m_regs.b[AH] = (uint8_t)result2;
						}
					}
					else
					{
						interrupt(0);
					}
					CLKM(IDIV_R8,IDIV_M8);
					break;
				}
			}
			break;

		case 0xf7: // i_f7pre
			{
				uint32_t tmp, tmp2;
				uint32_t uresult, uresult2;
				int32_t result, result2;

				m_modrm = fetch();
				tmp = GetRMWord();

				switch (m_modrm & 0x38)
				{
				case 0x00:  // TEST
				case 0x08:  // TEST (alias)
					tmp2 = fetch_word();
					tmp &= tmp2;
					m_CarryVal = m_OverVal = 0;
					set_SZPF_Word(tmp);
					CLKM(ALU_RI16,ALU_MI16_RO);
					break;

				case 0x10:  // NOT
					PutbackRMWord((uint16_t)~tmp);
					CLKM(NEGNOT_R16,NEGNOT_M16);
					break;

				case 0x18:  // NEG
					m_CarryVal = (tmp!=0) ? 1 : 0;
					tmp = (~tmp) + 1;
					set_SZPF_Word(tmp);
					PutbackRMWord((uint16_t)tmp);
					CLKM(NEGNOT_R16,NEGNOT_M16);
					break;

				case 0x20:  // MUL
					uresult = m_regs.w[AX]*tmp;
					m_regs.w[AX] = uresult & 0xffff;
					m_regs.w[DX] = ((uint32_t)uresult)>>16;
					m_CarryVal = m_OverVal = (m_regs.w[DX] != 0) ? 1 : 0;
					set_ZF(m_regs.w[AX] | m_regs.w[DX]);
					CLKM(MUL_R16,MUL_M16);
					break;

				case 0x28:  // IMUL
					result = (int32_t)((int16_t)m_regs.w[AX]) * (int32_t)((int16_t)tmp);
					m_regs.w[AX] = result & 0xffff;
					m_regs.w[DX] = result >> 16;
					m_CarryVal = m_OverVal = (m_regs.w[DX] != 0) ? 1 : 0;
					set_ZF(m_regs.w[AX] | m_regs.w[DX]);
					CLKM(IMUL_R16,IMUL_M16);
					break;

				case 0x30:  // DIV
					if (tmp)
					{
						uresult = (((uint32_t)m_regs.w[DX]) << 16) | m_regs.w[AX];
						uresult2 = uresult % tmp;
						if ((uresult /= tmp) > 0xffff)
						{
							interrupt(0);
						}
						else
						{
							m_regs.w[AX] = (uint16_t)uresult;
							m_regs.w[DX] = (uint16_t)uresult2;
						}
					}
					else
					{
						interrupt(0);
					}
					CLKM(DIV_R16,DIV_M16);
					break;

				case 0x38:  // IDIV
					if (tmp)
					{
						result = ((uint32_t)m_regs.w[DX] << 16) + m_regs.w[AX];
						result2 = result % (int32_t)((int16_t)tmp);
						if ((result /= (int32_t)((int16_t)tmp)) > 0xffff)
						{
							interrupt(0);
						}
						else
						{
							m_regs.w[AX] = (uint16_t)result;
							m_regs.w[DX] = (uint16_t)result2;
						}
					}
					else
					{
						interrupt(0);
					}
					CLKM(IDIV_R16,IDIV_M16);
					break;
				}
			}
			break;

		case 0xf8: // i_clc
			m_CarryVal = 0;
			CLK(FLAG_OPS);
			break;

		case 0xf9: // i_stc
			m_CarryVal = 1;
			CLK(FLAG_OPS);
			break;

		case 0xfa: // i_cli
			m_IF = 0;
			CLK(FLAG_OPS);
			break;

		case 0xfb: // i_sti
			m_IF = 1;
			m_no_interrupt = 1;
			CLK(FLAG_OPS);
			break;

		case 0xfc: // i_cld
			m_DF = 0;
			CLK(FLAG_OPS);
			break;

		case 0xfd: // i_std
			m_DF = 1;
			CLK(FLAG_OPS);
			break;

		case 0xfe: // i_fepre
			{
				uint32_t tmp, tmp1;
				m_modrm = fetch();
				tmp = GetRMByte();

				switch (m_modrm & 0x38)
				{
				case 0x00:  // INC
					tmp1 = tmp + 1;
					m_OverVal = (tmp == 0x7f);
					set_AF(tmp1, tmp, 1);
					set_SZPF_Byte(tmp1);
					PutbackRMByte((uint8_t)tmp1);
					CLKM(INCDEC_R8, INCDEC_M8);
					break;

				case 0x08:  // DEC
					tmp1 = tmp-1;
					m_OverVal = (tmp == 0x80);
					set_AF(tmp1, tmp, 1);
					set_SZPF_Byte(tmp1);
					PutbackRMByte((uint8_t)tmp1);
					CLKM(INCDEC_R8, INCDEC_M8);
					break;

				default:
					WriteLog("%06x: FE Pre with unimplemented mod\n", m_pc);
					break;
				}
			}
			break;

		case 0xff: // i_ffpre
			{
				uint32_t tmp, tmp1;
				m_modrm = fetch();
				tmp = GetRMWord();

				switch (m_modrm & 0x38)
				{
				case 0x00:  // INC
					tmp1 = tmp+1;
					m_OverVal = (tmp == 0x7fff);
					set_AF(tmp1, tmp, 1);
					set_SZPF_Word(tmp1);
					PutbackRMWord((uint16_t)tmp1);
					CLKM(INCDEC_R16, INCDEC_M16);
					break;

				case 0x08:  /* DEC */
					tmp1 = tmp-1;
					m_OverVal = (tmp == 0x8000);
					set_AF(tmp1, tmp, 1);
					set_SZPF_Word(tmp1);
					PutbackRMWord((uint16_t)tmp1);
					CLKM(INCDEC_R16, INCDEC_M16);
					break;

				case 0x10:  /* CALL */
					PUSH(m_ip);
					m_ip = (uint16_t)tmp;
					CLKM(CALL_R16, CALL_M16);
					break;

				case 0x18:  /* CALL FAR */
					tmp1 = m_sregs[CS];
					m_sregs[CS] = GetnextRMWord();
					PUSH((uint16_t)tmp1);
					PUSH(m_ip);
					m_ip = (uint16_t)tmp;
					CLK(CALL_M32);
					break;

				case 0x20:  /* JMP */
					m_ip = (uint16_t)tmp;
					CLKM(JMP_R16,JMP_M16);
					break;

				case 0x28:  /* JMP FAR */
					m_ip = (uint16_t)tmp;
					m_sregs[CS] = GetnextRMWord();
					CLK(JMP_M32);
					break;

				case 0x30:
					PUSH((uint16_t)tmp);
					CLKM(PUSH_R16, PUSH_M16);
					break;

				default:
					m_icount -= 10;
					WriteLog("%06x: FF Pre with unimplemented mod\n", m_pc);
					break;
				}
			}
			break;

		default:
			return false;
	}

	return true;
}

Master512CoPro::Master512CoPro()
{
	m_Memory = new unsigned char[0x100000];

	// Set up parity lookup table.
	for (uint16_t i = 0;i < 256; i++)
	{
		uint16_t c = 0;

		for (uint16_t j = i; j > 0; j >>= 1)
		{
			if (j & 1) c++;
		}

		m_parity_table[i] = !(c & 1);
	}

	static const BREGS reg_name[8] = { AL, CL, DL, BL, AH, CH, DH, BH };

	for (uint16_t i = 0; i < 256; i++)
	{
		m_Mod_RM.reg.b[i] = reg_name[(i & 0x38) >> 3];
		m_Mod_RM.reg.w[i] = (WREGS)((i & 0x38) >> 3);
	}

	for (uint16_t i = 0xc0; i < 0x100; i++)
	{
		m_Mod_RM.RM.w[i] = (WREGS)(i & 7);
		m_Mod_RM.RM.b[i] = (BREGS)reg_name[i & 7];
	}

	memset(&m_regs, 0, sizeof(m_regs));
	memset(m_sregs, 0, sizeof(m_sregs));
}

Master512CoPro::~Master512CoPro()
{
	delete [] m_Memory;
}

void Master512CoPro::Reset()
{
	LoadBIOS();
/*
	I.sregs[CS] = 0xf000;
	I.base[CS] = SegBase(CS); // #define SegBase(Seg) 			(I.sregs[Seg] << 4)
	I.pc = 0xffff0 & AMASK;
	ExpandFlags(I.flags);

	change_pc(I.pc);

	cycles = i186_cycles;
*/

	// 8086 state
	m_ZeroVal = 1;
	m_ParityVal = 1;
	m_regs.w[AX] = 0;
	m_regs.w[CX] = 0;
	m_regs.w[DX] = 0;
	m_regs.w[BX] = 0;
	m_regs.w[SP] = 0;
	m_regs.w[BP] = 0;
	m_regs.w[SI] = 0;
	m_regs.w[DI] = 0;
	m_sregs[ES] = 0;
	m_sregs[CS] = 0xf000;
	m_sregs[SS] = 0;
	m_sregs[DS] = 0;
	m_ip = 0xfff0;
	m_prev_ip = 0;
	m_SignVal = 0;
	m_AuxVal = 0;
	m_OverVal = 0;
	m_CarryVal = 0;
	m_TF = 0;
	m_IF = 0;
	m_DF = 0;
	m_IOPL = 3; // 8086 IOPL always 3
	m_NT = 1; // 8086 NT always 1
	m_MF = 1; // 8086 MF always 1, 80286 always 0
	// m_int_vector = 0;
	m_pending_irq = 0;
	m_nmi_state = 0;
	m_irq_state = 0;
	m_no_interrupt = 0;
	m_fire_trap = 0;
	m_prefix_seg = 0;
	m_seg_prefix = false;
	m_seg_prefix_next = false;
	m_ea = 0;
	m_eo = 0;
	m_modrm = 0;
	m_dst = 0;
	m_src = 0;
	m_halt = false;
	m_easeg = DS;

	// 80186 state

	// reset the interrupt state
	/*
	m_intr.priority_mask    = 0x0007;
	m_intr.timer[0]         = 0x000f;
	m_intr.timer[1]         = 0x000f;
	m_intr.timer[2]         = 0x000f;
	m_intr.dma[0]           = 0x000f;
	m_intr.dma[1]           = 0x000f;
	m_intr.ext[0]           = 0x000f;
	m_intr.ext[1]           = 0x000f;
	m_intr.ext[2]           = 0x000f;
	m_intr.ext[3]           = 0x000f;
	m_intr.in_service       = 0x0000;

	m_intr.pending          = 0x0000;
	m_intr.ack_mask         = 0x0000;
	m_intr.request          = 0x0000;
	m_intr.status           = 0x0000;
	m_intr.poll_status      = 0x0000;
	m_intr.ext_state        = 0x00;
	*/
	// m_reloc = 0x20ff;
	// m_mem.upper = 0xfffb;

	for (auto & elem : m_dma)
	{
		elem.drq_state = false;
		elem.control = 0;
	}

	/* for (auto & elem : m_timer)
	{
		elem.control = 0;
		elem.maxA = 0;
		elem.maxB = 0;
		elem.active_count = false;
		elem.count = 0;
	} */

	R1Status = 0;
	ResetTube();
}

void Master512CoPro::Execute(int Cycles)
{
	// adjust for any interrupts that came in
	m_icount = Cycles;
	// m_icount -= I.extra_cycles;
	// I.extra_cycles = 0;

	while (m_icount > 0)
	{

#if 0

		if ((m_dma[0].drq_state && (m_dma[0].control & ST_STOP)) || (m_dma[1].drq_state && (m_dma[1].control & ST_STOP)))
		{
			int channel = m_last_dma ? 0 : 1;
			m_last_dma = !m_last_dma;
			if (!(m_dma[1].drq_state && (m_dma[1].control & ST_STOP)))
				channel = 0;
			else if (!(m_dma[0].drq_state && (m_dma[0].control & ST_STOP)))
				channel = 1;
			else if ((m_dma[0].control & CHANNEL_PRIORITY) && !(m_dma[1].control & CHANNEL_PRIORITY))
				channel = 0;
			else if ((m_dma[1].control & CHANNEL_PRIORITY) && !(m_dma[0].control & CHANNEL_PRIORITY))
				channel = 1;
			m_icount--;
			// drq_callback(channel);
			continue;
		}

#endif

		if (m_seg_prefix_next)
		{
			m_seg_prefix = true;
			m_seg_prefix_next = false;
		}
		else
		{
			m_prev_ip = m_ip;
			m_seg_prefix = false;

			// Dispatch IRQ
			if (m_pending_irq && m_no_interrupt == 0)
			{
				if (m_pending_irq & NMI_IRQ)
				{
					interrupt(2);
					m_pending_irq &= ~NMI_IRQ;
					m_halt = false;
				}
				else if (m_IF)
				{
					interrupt(12);
					m_halt = false;
				}
			}

			if (m_halt)
			{
				m_icount = 0;
				return;
			}

			// No interrupt allowed between last instruction and this one
			if (m_no_interrupt)
			{
				m_no_interrupt--;
			}

			// Trap should allow one instruction to be executed
			if (m_fire_trap)
			{
				if (m_fire_trap >= 2)
				{
					interrupt(1);
					m_fire_trap = 0;
				}
				else
				{
					m_fire_trap++;
				}
			}
		}

		uint8_t op = fetch_op();

		switch (op)
		{
			case 0x60: // i_pusha
				{
					uint32_t tmp = m_regs.w[SP];

					PUSH(m_regs.w[AX]);
					PUSH(m_regs.w[CX]);
					PUSH(m_regs.w[DX]);
					PUSH(m_regs.w[BX]);
					PUSH((uint16_t)tmp);
					PUSH(m_regs.w[BP]);
					PUSH(m_regs.w[SI]);
					PUSH(m_regs.w[DI]);
					CLK(PUSHA);
				}
				break;

			case 0x61: // i_popa
				m_regs.w[DI] = POP();
				m_regs.w[SI] = POP();
				m_regs.w[BP] = POP();
				POP();
				m_regs.w[BX] = POP();
				m_regs.w[DX] = POP();
				m_regs.w[CX] = POP();
				m_regs.w[AX] = POP();
				CLK(POPA);
				break;

			case 0x62: // i_bound
				{
					m_modrm = fetch();
					uint32_t low = GetRMWord();
					uint32_t high = GetnextRMWord();
					uint32_t tmp = RegWord();
					if (tmp<low || tmp>high)
						interrupt(5);
					CLK(BOUND);
					WriteLog("%06x: bound %04x high %04x low %04x tmp\n", m_pc, high, low, tmp);
				}
				break;

			case 0x68: // i_push_d16
				PUSH(fetch_word());
				CLK(PUSH_IMM);
				break;

			case 0x69: // i_imul_d16
				{
					DEF_r16w();
					uint32_t tmp = fetch_word();
					m_dst = (int32_t)((int16_t)m_src)*(int32_t)((int16_t)tmp);
					m_CarryVal = m_OverVal = (((int32_t)m_dst) >> 15 != 0) && (((int32_t)m_dst) >> 15 != -1);
					RegWord((uint16_t)m_dst);
					CLKM(IMUL_RRI16, IMUL_RMI16);
				}
				break;

			case 0x6a: // i_push_d8
				PUSH((uint16_t)((int16_t)((int8_t)fetch())));
				CLK(PUSH_IMM);
				break;

			case 0x6b: // i_imul_d8
				{
					DEF_r16w();
					uint32_t src2 = (uint16_t)((int16_t)((int8_t)fetch()));
					m_dst = (int32_t)((int16_t)m_src)*(int32_t)((int16_t)src2);
					m_CarryVal = m_OverVal = (((int32_t)m_dst) >> 15 != 0) && (((int32_t)m_dst) >> 15 != -1);
					RegWord((uint16_t)m_dst);
					CLKM(IMUL_RRI8, IMUL_RMI8);
				}
				break;

			case 0x6c: // i_insb
				i_insb();
				break;

			case 0x6d: // i_insw
				i_insw();
				break;

			case 0x6e: // i_outsb
				i_outsb();
				break;

			case 0x6f: // i_outsw
				i_outsw();
				break;

			case 0x8e: // i_mov_sregw
				m_modrm = fetch();
				m_src = GetRMWord();
				CLKM(MOV_SR,MOV_SM);
				switch (m_modrm & 0x38)
				{
				case 0x00: // mov es,ew
					m_sregs[ES] = (uint16_t)m_src;
					break;
				case 0x10: // mov ss,ew
					m_sregs[SS] = (uint16_t)m_src;
					m_no_interrupt = 1;
					break;
				case 0x18: // mov ds,ew
					m_sregs[DS] = (uint16_t)m_src;
					break;
				default:
					WriteLog("%06x: Mov Sreg - Invalid register\n", m_pc);
					m_ip = m_prev_ip;
					interrupt(6);
					break;
				}
				break;

			case 0xc0: // i_rotshft_bd8
				{
					m_modrm = fetch();
					m_src = GetRMByte();
					m_dst = m_src;
					uint8_t c = fetch() & 0x1f;
					CLKM(ROT_REG_BASE,ROT_M8_BASE);
					m_icount -= Timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
						case 0x00: do { ROL_BYTE();  c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x08: do { ROR_BYTE();  c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x10: do { ROLC_BYTE(); c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x18: do { RORC_BYTE(); c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x30:
						case 0x20: SHL_BYTE(c); break;
						case 0x28: SHR_BYTE(c); break;
						case 0x38: SHRA_BYTE(c); break;
						}
					}
				}
				break;

			case 0xc1: // i_rotshft_wd8
				{
					m_modrm = fetch();
					m_src = GetRMWord();
					m_dst = m_src;
					uint8_t c = fetch() & 0x1f;
					CLKM(ROT_REG_BASE,ROT_M16_BASE);
					m_icount -= Timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
						case 0x00: do { ROL_WORD();  c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
						case 0x08: do { ROR_WORD();  c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
						case 0x10: do { ROLC_WORD(); c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
						case 0x18: do { RORC_WORD(); c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
						case 0x30:
						case 0x20: SHL_WORD(c); break;
						case 0x28: SHR_WORD(c); break;
						case 0x38: SHRA_WORD(c); break;
						}
					}
				}
				break;

			case 0xc8: // i_enter
				{
					uint16_t nb = fetch();
					nb |= fetch() << 8;
					uint8_t level = fetch();
					CLK((uint8_t)(!level ? ENTER0 : (level == 1) ? ENTER1 : ENTER_BASE));
					if (level > 1)
						m_icount -= level * Timing[ENTER_COUNT];
					PUSH(m_regs.w[BP]);
					m_regs.w[BP] = m_regs.w[SP];
					m_regs.w[SP] -= nb;
					for (uint8_t i = 1; i < level; i++)
					{
						PUSH(GetMemW(SS, m_regs.w[BP] - i * 2));
					}
					if (level)
					{
						PUSH(m_regs.w[BP]);
					}
				}
				break;

			case 0xc9: // i_leave
				m_regs.w[SP] = m_regs.w[BP];
				m_regs.w[BP] = POP();
				CLK(LEAVE);
				break;

			case 0xd2: // i_rotshft_bcl
				{
					m_modrm = fetch();
					m_src = GetRMByte();
					m_dst = m_src;
					uint8_t c = m_regs.b[CL] & 0x1f;
					CLKM(ROT_REG_BASE,ROT_M16_BASE);
					m_icount -= Timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
						case 0x00: do { ROL_BYTE();  c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x08: do { ROR_BYTE();  c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x10: do { ROLC_BYTE(); c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x18: do { RORC_BYTE(); c--; } while (c>0); PutbackRMByte((uint8_t)m_dst); break;
						case 0x30:
						case 0x20: SHL_BYTE(c); break;
						case 0x28: SHR_BYTE(c); break;
						case 0x38: SHRA_BYTE(c); break;
						}
					}
				}
				break;

			case 0xd3: // i_rotshft_wcl
				{
					m_modrm = fetch();
					m_src = GetRMWord();
					m_dst = m_src;
					uint8_t c = m_regs.b[CL] & 0x1f;
					CLKM(ROT_REG_BASE,ROT_M16_BASE);
					m_icount -= Timing[ROT_REG_BIT] * c;
					if (c)
					{
						switch (m_modrm & 0x38)
						{
							case 0x00: do { ROL_WORD();  c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
							case 0x08: do { ROR_WORD();  c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
							case 0x10: do { ROLC_WORD(); c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
							case 0x18: do { RORC_WORD(); c--; } while (c>0); PutbackRMWord((uint16_t)m_dst); break;
							case 0x30:
							case 0x20: SHL_WORD(c); break;
							case 0x28: SHR_WORD(c); break;
							case 0x38: SHRA_WORD(c); break;
						}
					}
				}
				break;

			case 0xd8: // i_esc
			case 0xd9:
			case 0xda:
			case 0xdb:
			case 0xdc:
			case 0xdd:
			case 0xde:
			case 0xdf:
				/* if (m_reloc & 0x8000)
				{
					m_ip = m_prev_ip;
					interrupt(7);
					break;
				} */
				m_modrm = fetch();
				GetRMByte();
				CLK(NOP);
				// The 80187 has the FSTSW AX instruction
				// if ((m_modrm == 0xe0) && (op == 0xdf))
				//	m_regs.w[AX] = 0xffff;  // FPU not present
				break;

			case 0xe6: // i_outal
				write_port_byte_al(fetch());
				CLK(OUT_IMM8);
				break;

			case 0xee: // i_outdxal
				write_port_byte_al(m_regs.w[DX]);
				CLK(OUT_DX8);
				break;

			case 0xf2: // i_repne
			case 0xf3:
				{
					bool pass = false;
					uint8_t next = repx_op();
					uint16_t c = m_regs.w[CX];

					switch (next)
					{
					case 0x6c:  CLK(OVERRIDE); if (c) do { i_insb();  c--; } while (c>0 && m_icount>0); m_regs.w[CX] = c; m_seg_prefix = false; m_seg_prefix_next = false; break;
					case 0x6d:  CLK(OVERRIDE); if (c) do { i_insw();  c--; } while (c>0 && m_icount>0); m_regs.w[CX] = c; m_seg_prefix = false; m_seg_prefix_next = false; break;
					case 0x6e:  CLK(OVERRIDE); if (c) do { i_outsb(); c--; } while (c>0 && m_icount>0); m_regs.w[CX] = c; m_seg_prefix = false; m_seg_prefix_next = false; break;
					case 0x6f:  CLK(OVERRIDE); if (c) do { i_outsw(); c--; } while (c>0 && m_icount>0); m_regs.w[CX] = c; m_seg_prefix = false; m_seg_prefix_next = false; break;
					default:
						// Decrement IP and pass on
						m_ip -= 1 + (m_seg_prefix_next ? 1 : 0);
						pass = true;
						break;
					}
					if (!pass)
					{
						if (c)
							m_ip = m_prev_ip;
						break;
					}
				}
				// through to default
			default:
				if (!common_op(op))
				{
					m_icount -= 10; // UD fault timing?
					WriteLog("%06x: Invalid Opcode %02x\n", m_pc, op);
					m_ip = m_prev_ip;
					interrupt(6); // 80186 has #UD
					break;
				}
		}

		if (TubeintStatus & (1 << R1))
			execute_set_input(INPUT_LINE_IRQ4, ASSERT_LINE1);

		if (TubeintStatus & (1 << R4))
			execute_set_input(INPUT_LINE_IRQ4, ASSERT_LINE4);

		if (TubeintStatus == 0)
			execute_set_input(INPUT_LINE_IRQ4, CLEAR_LINE);

		// lastInt = TubeintStatus;

		if (TubeNMIStatus) DoDMA();
	}

	// adjust for any interrupts that came in
	// i86_ICount -= I.extra_cycles;
	// I.extra_cycles = 0;

	// return num_cycles - i86_ICount;
}

inline void Master512CoPro::PUSH(uint16_t data)
{
	write_word(calc_addr(SS, m_regs.w[SP] - 2, 2, I8086_WRITE, false), data);
	m_regs.w[SP] -= 2;
}

inline uint16_t Master512CoPro::POP()
{
	uint16_t data = read_word(calc_addr(SS, m_regs.w[SP], 2, I8086_READ, false));

	m_regs.w[SP] += 2;
	return data;
}

inline void Master512CoPro::JMP(bool cond)
{
	int8_t rel = (int8_t)fetch();

	if (cond)
	{
		m_ip += rel;
		CLK(JCC_T);
	}
	else
	{
		CLK(JCC_NT);
	}
}

void Master512CoPro::ADJ4(int8_t param1, int8_t param2)
{
	if (AF || ((m_regs.b[AL] & 0xf) > 9))
	{
		uint16_t tmp = m_regs.b[AL] + param1;
		m_regs.b[AL] = (uint8_t)tmp;
		m_AuxVal = 1;
		m_CarryVal |= tmp & 0x100;
	}

	if (CF || (m_regs.b[AL]>0x9f))
	{
		m_regs.b[AL] += param2;
		m_CarryVal = 1;
	}

	set_SZPF_Byte(m_regs.b[AL]);
}

inline void Master512CoPro::ADJB(int8_t param1, int8_t param2)
{
	if (AF || ((m_regs.b[AL] & 0xf) > 9))
	{
		m_regs.b[AL] += param1;
		m_regs.b[AH] += param2;
		m_AuxVal = 1;
		m_CarryVal = 1;
	}
	else
	{
		m_AuxVal = 0;
		m_CarryVal = 0;
	}

	m_regs.b[AL] &= 0x0F;
}

inline void Master512CoPro::set_CFB(uint32_t x)
{
	m_CarryVal = x & 0x100;
}

inline void Master512CoPro::set_CFW(uint32_t x)
{
	m_CarryVal = x & 0x10000;
}

inline void Master512CoPro::set_AF(uint32_t x, uint32_t y, uint32_t z)
{
	m_AuxVal = (x ^ (y ^ z)) & 0x10;
}

inline void Master512CoPro::set_SF(uint32_t x)
{
	m_SignVal = x;
}

inline void Master512CoPro::set_ZF(uint32_t x)
{
	m_ZeroVal = x;
}

inline void Master512CoPro::set_PF(uint32_t x)
{
	m_ParityVal = x;
}

inline void Master512CoPro::set_SZPF_Byte(uint32_t x)
{
	m_SignVal = m_ZeroVal = m_ParityVal = (int8_t)x;
}

inline void Master512CoPro::set_SZPF_Word(uint32_t x)
{
	m_SignVal = m_ZeroVal = m_ParityVal = (int16_t)x;
}

inline void Master512CoPro::set_OFW_Add(uint32_t x, uint32_t y, uint32_t z)
{
	m_OverVal = (x ^ y) & (x ^ z) & 0x8000;
}

inline void Master512CoPro::set_OFB_Add(uint32_t x, uint32_t y, uint32_t z)
{
	m_OverVal = (x ^ y) & (x ^ z) & 0x80;
}

inline void Master512CoPro::set_OFW_Sub(uint32_t x, uint32_t y, uint32_t z)
{
	m_OverVal = (z ^ y) & (z ^ x) & 0x8000;
}

inline void Master512CoPro::set_OFB_Sub(uint32_t x, uint32_t y, uint32_t z)
{
	m_OverVal = (z ^ y) & (z ^ x) & 0x80;
}

uint16_t Master512CoPro::CompressFlags() const
{
	return (CF ? 1 : 0)
	       | (1 << 1)
	       | (PF ? 4 : 0)
	       | (AF ? 0x10 : 0)
	       | (ZF ? 0x40 : 0)
	       | (SF ? 0x80 : 0)
	       | (m_TF << 8)
	       | (m_IF << 9)
	       | (m_DF << 10)
	       | (OF << 11)
	       | (m_IOPL << 12)
	       | (m_NT << 14)
	       | (m_MF << 15);
}

void Master512CoPro::ExpandFlags(uint16_t f)
{
	m_CarryVal = (f) & 1;
	m_ParityVal = !((f) & 4);
	m_AuxVal = (f) & 16;
	m_ZeroVal = !((f) & 64);
	m_SignVal = (f) & 128 ? -1 : 0;
	m_TF = ((f) & 256) == 256;
	m_IF = ((f) & 512) == 512;
	m_DF = ((f) & 1024) == 1024;
	m_OverVal = (f) & 2048;
	m_IOPL = (f >> 12) & 3;
	m_NT = ((f) & 0x4000) == 0x4000;
	m_MF = ((f) & 0x8000) == 0x8000;
}

inline void Master512CoPro::i_insb()
{
	uint32_t ea = calc_addr(ES, m_regs.w[DI], 1, I8086_WRITE);
	write_byte(ea, read_port_byte(m_regs.w[DX]));
	m_regs.w[DI] += -2 * m_DF + 1;
	CLK(IN_IMM8);
}

inline void Master512CoPro::i_insw()
{
	uint32_t ea = calc_addr(ES, m_regs.w[DI], 2, I8086_WRITE);
	write_word(ea, read_port_word(m_regs.w[DX]));
	m_regs.w[DI] += -4 * m_DF + 2;
	CLK(IN_IMM16);
}

inline void Master512CoPro::i_outsb()
{
	write_port_byte(m_regs.w[DX], GetMemB(DS, m_regs.w[SI]));
	m_regs.w[SI] += -2 * m_DF + 1;
	CLK(OUT_IMM8);
}

inline void Master512CoPro::i_outsw()
{
	write_port_word(m_regs.w[DX], GetMemW(DS, m_regs.w[SI]));
	m_regs.w[SI] += -4 * m_DF + 2;
	CLK(OUT_IMM16);
}

inline void Master512CoPro::i_movsb()
{
	uint8_t tmp = GetMemB(DS, m_regs.w[SI]);
	PutMemB(ES, m_regs.w[DI], tmp);
	m_regs.w[DI] += -2 * m_DF + 1;
	m_regs.w[SI] += -2 * m_DF + 1;
	CLK(MOVS8);
}

inline void Master512CoPro::i_movsw()
{
	uint16_t tmp = GetMemW(DS, m_regs.w[SI]);
	PutMemW(ES, m_regs.w[DI], tmp);
	m_regs.w[DI] += -4 * m_DF + 2;
	m_regs.w[SI] += -4 * m_DF + 2;
	CLK(MOVS16);
}

inline void Master512CoPro::i_cmpsb()
{
	m_src = GetMemB(ES, m_regs.w[DI]);
	m_dst = GetMemB(DS, m_regs.w[SI]);
	set_CFB(SUBB());
	m_regs.w[DI] += -2 * m_DF + 1;
	m_regs.w[SI] += -2 * m_DF + 1;
	CLK(CMPS8);
}

inline void Master512CoPro::i_cmpsw()
{
	m_src = GetMemW(ES, m_regs.w[DI]);
	m_dst = GetMemW(DS, m_regs.w[SI]);
	set_CFW(SUBX());
	m_regs.w[DI] += -4 * m_DF + 2;
	m_regs.w[SI] += -4 * m_DF + 2;
	CLK(CMPS16);
}

inline void Master512CoPro::i_stosb()
{
	PutMemB(ES, m_regs.w[DI], m_regs.b[AL]);
	m_regs.w[DI] += -2 * m_DF + 1;
	CLK(STOS8);
}

inline void Master512CoPro::i_stosw()
{
	PutMemW(ES, m_regs.w[DI], m_regs.w[AX]);
	m_regs.w[DI] += -4 * m_DF + 2;
	CLK(STOS16);
}

inline void Master512CoPro::i_lodsb()
{
	m_regs.b[AL] = GetMemB(DS, m_regs.w[SI]);
	m_regs.w[SI] += -2 * m_DF + 1;
	CLK(LODS8);
}

inline void Master512CoPro::i_lodsw()
{
	m_regs.w[AX] = GetMemW(DS, m_regs.w[SI]);
	m_regs.w[SI] += -4 * m_DF + 2;
	CLK(LODS16);
}

inline void Master512CoPro::i_scasb()
{
	m_src = GetMemB(ES, m_regs.w[DI]);
	m_dst = m_regs.b[AL];
	set_CFB(SUBB());
	m_regs.w[DI] += -2 * m_DF + 1;
	CLK(SCAS8);
}

inline void Master512CoPro::i_scasw()
{
	m_src = GetMemW(ES, m_regs.w[DI]);
	m_dst = m_regs.w[AX];
	set_CFW(SUBX());
	m_regs.w[DI] += -4 * m_DF + 2;
	CLK(SCAS16);
}

inline void Master512CoPro::i_popf()
{
	uint16_t tmp = POP();

	ExpandFlags(tmp | 0xf000);
	CLK(POPF);
	if (m_TF)
	{
		m_fire_trap = 1;
	}
}

inline void Master512CoPro::IncWordReg(uint8_t reg)
{
	uint32_t tmp = m_regs.w[reg];
	uint32_t tmp1 = tmp + 1;

	m_OverVal = (tmp == 0x7fff);
	set_AF(tmp1, tmp, 1);
	set_SZPF_Word(tmp1);
	m_regs.w[reg] = (uint16_t)tmp1;
}

inline void Master512CoPro::DecWordReg(uint8_t reg)
{
	uint32_t tmp = m_regs.w[reg];
	uint32_t tmp1 = tmp - 1;

	m_OverVal = (tmp == 0x8000);
	set_AF(tmp1, tmp, 1);
	set_SZPF_Word(tmp1);
	m_regs.w[reg] = (uint16_t)tmp1;
}

inline void Master512CoPro::ROL_BYTE()
{
	m_CarryVal = m_dst & 0x80;
	m_dst = (m_dst << 1) | (CF ? 1 : 0);
}

inline void Master512CoPro::ROR_BYTE()
{
	m_CarryVal = m_dst & 0x1;
	m_dst = (m_dst >> 1) | (CF ? 0x80 : 0x00);
}

inline void Master512CoPro::ROL_WORD()
{
	m_CarryVal = m_dst & 0x8000;
	m_dst = (m_dst << 1) | (CF ? 1 : 0);
}

inline void Master512CoPro::ROR_WORD()
{
	m_CarryVal = m_dst & 0x1;
	m_dst = (m_dst >> 1) + (CF ? 0x8000 : 0x0000);
}

inline void Master512CoPro::ROLC_WORD()
{
	m_dst = (m_dst << 1) | (CF ? 1 : 0);
	set_CFW(m_dst);
}

inline void Master512CoPro::ROLC_BYTE()
{
	m_dst = (m_dst << 1) | (CF ? 1 : 0);
	set_CFB(m_dst);
}

inline void Master512CoPro::RORC_BYTE()
{
	m_dst |= (CF ? 0x100 : 0x00);
	m_CarryVal = m_dst & 0x01;
	m_dst >>= 1;
}

inline void Master512CoPro::RORC_WORD()
{
	m_dst |= (CF ? 0x10000 : 0);
	m_CarryVal = m_dst & 0x01;
	m_dst >>= 1;
}

inline void Master512CoPro::SHL_BYTE(uint8_t c)
{
	while (c--)
		m_dst <<= 1;

	set_CFB(m_dst);
	set_SZPF_Byte(m_dst);
	PutbackRMByte((uint8_t)m_dst);
}

inline void Master512CoPro::SHL_WORD(uint8_t c)
{
	while (c--)
		m_dst <<= 1;

	set_CFW(m_dst);
	set_SZPF_Word(m_dst);
	PutbackRMWord((uint16_t)m_dst);
}

inline void Master512CoPro::SHR_BYTE(uint8_t c)
{
	while (c--)
	{
		m_CarryVal = m_dst & 0x01;
		m_dst >>= 1;
	}

	set_SZPF_Byte(m_dst);
	PutbackRMByte((uint8_t)m_dst);
}

inline void Master512CoPro::SHR_WORD(uint8_t c)
{
	while (c--)
	{
		m_CarryVal = m_dst & 0x01;
		m_dst >>= 1;
	}

	set_SZPF_Word(m_dst);
	PutbackRMWord((uint16_t)m_dst);
}

inline void Master512CoPro::SHRA_BYTE(uint8_t c)
{
	while (c--)
	{
		m_CarryVal = m_dst & 0x01;
		m_dst = ((int8_t)m_dst) >> 1;
	}

	set_SZPF_Byte(m_dst);
	PutbackRMByte((uint8_t)m_dst);
}

inline void Master512CoPro::SHRA_WORD(uint8_t c)
{
	while (c--)
	{
		m_CarryVal = m_dst & 0x01;
		m_dst = ((int16_t)m_dst) >> 1;
	}

	set_SZPF_Word(m_dst);
	PutbackRMWord((uint16_t)m_dst);
}

inline void Master512CoPro::XchgAXReg(uint8_t reg)
{
	uint16_t tmp = m_regs.w[reg];

	m_regs.w[reg] = m_regs.w[AX];
	m_regs.w[AX] = tmp;
}

inline uint32_t Master512CoPro::ADDB()
{
	uint32_t res = m_dst + m_src;

	set_OFB_Add(res, m_src, m_dst);
	set_AF(res, m_src, m_dst);
	set_SZPF_Byte(res);
	m_dst = res & 0xff;
	return res;
}

inline uint32_t Master512CoPro::ADDX()
{
	uint32_t res = m_dst + m_src;

	set_OFW_Add(res, m_src, m_dst);
	set_AF(res, m_src, m_dst);
	set_SZPF_Word(res);
	m_dst = res & 0xffff;
	return res;
}

inline uint32_t Master512CoPro::SUBB()
{
	uint32_t res = m_dst - m_src;

	set_OFB_Sub(res, m_src, m_dst);
	set_AF(res, m_src, m_dst);
	set_SZPF_Byte(res);
	m_dst = res & 0xff;
	return res;
}

inline uint32_t Master512CoPro::SUBX()
{
	uint32_t res = m_dst - m_src;

	set_OFW_Sub(res, m_src, m_dst);
	set_AF(res, m_src, m_dst);
	set_SZPF_Word(res);
	m_dst = res & 0xffff;
	return res;
}

inline void Master512CoPro::ORB()
{
	m_dst |= m_src;
	m_CarryVal = m_OverVal = m_AuxVal = 0;
	set_SZPF_Byte(m_dst);
}

inline void Master512CoPro::ORW()
{
	m_dst |= m_src;
	m_CarryVal = m_OverVal = m_AuxVal = 0;
	set_SZPF_Word(m_dst);
}

inline void Master512CoPro::ANDB()
{
	m_dst &= m_src;
	m_CarryVal = m_OverVal = m_AuxVal = 0;
	set_SZPF_Byte(m_dst);
}

inline void Master512CoPro::ANDX()
{
	m_dst &= m_src;
	m_CarryVal = m_OverVal = m_AuxVal = 0;
	set_SZPF_Word(m_dst);
}

inline void Master512CoPro::XORB()
{
	m_dst ^= m_src;
	m_CarryVal = m_OverVal = m_AuxVal = 0;
	set_SZPF_Byte(m_dst);
}

inline void Master512CoPro::XORW()
{
	m_dst ^= m_src;
	m_CarryVal = m_OverVal = m_AuxVal = 0;
	set_SZPF_Word(m_dst);
}

uint8_t Master512CoPro::repx_op()
{
	uint8_t next = fetch_op();
	bool seg_prefix = false;
	int seg = 0;

	switch (next)
	{
	case 0x26:
		seg_prefix = true;
		seg = ES;
		break;
	case 0x2e:
		seg_prefix = true;
		seg = CS;
		break;
	case 0x36:
		seg_prefix = true;
		seg = SS;
		break;
	case 0x3e:
		seg_prefix = true;
		seg = DS;
		break;
	}

	if (seg_prefix)
	{
		m_seg_prefix = true;
		m_seg_prefix_next = true;
		m_prefix_seg = seg;
		next = fetch_op();
		CLK(OVERRIDE);
	}

	return next;
}

uint32_t Master512CoPro::get_ea(int size, int op)
{
	uint16_t e16;

	switch (m_modrm & 0xc7)
	{
	case 0x00:
		m_icount -= 7;
		m_eo = m_regs.w[BX] + m_regs.w[SI];
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x01:
		m_icount -= 8;
		m_eo = m_regs.w[BX] + m_regs.w[DI];
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x02:
		m_icount -= 8;
		m_eo = m_regs.w[BP] + m_regs.w[SI];
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x03:
		m_icount -= 7;
		m_eo = m_regs.w[BP] + m_regs.w[DI];
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x04:
		m_icount -= 5;
		m_eo = m_regs.w[SI];
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x05:
		m_icount -= 5;
		m_eo = m_regs.w[DI];
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x06:
		m_icount -= 6;
		m_eo = fetch_word();
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x07:
		m_icount -= 5;
		m_eo = m_regs.w[BX];
		m_ea = calc_addr(DS, m_eo, size, op);
		break;

	case 0x40:
		m_icount -= 11;
		m_eo = m_regs.w[BX] + m_regs.w[SI] + (int8_t)fetch();
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x41:
		m_icount -= 12;
		m_eo = m_regs.w[BX] + m_regs.w[DI] + (int8_t)fetch();
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x42:
		m_icount -= 12;
		m_eo = m_regs.w[BP] + m_regs.w[SI] + (int8_t)fetch();
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x43:
		m_icount -= 11;
		m_eo = m_regs.w[BP] + m_regs.w[DI] + (int8_t)fetch();
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x44:
		m_icount -= 9;
		m_eo = m_regs.w[SI] + (int8_t)fetch();
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x45:
		m_icount -= 9;
		m_eo = m_regs.w[DI] + (int8_t)fetch();
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x46:
		m_icount -= 9;
		m_eo = m_regs.w[BP] + (int8_t)fetch();
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x47:
		m_icount -= 9;
		m_eo = m_regs.w[BX] + (int8_t)fetch();
		m_ea = calc_addr(DS, m_eo, size, op);
		break;

	case 0x80:
		m_icount -= 11;
		e16 = fetch_word();
		m_eo = m_regs.w[BX] + m_regs.w[SI] + (int16_t)e16;
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x81:
		m_icount -= 12;
		e16 = fetch_word();
		m_eo = m_regs.w[BX] + m_regs.w[DI] + (int16_t)e16;
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x82:
		m_icount -= 11;
		e16 = fetch_word();
		m_eo = m_regs.w[BP] + m_regs.w[SI] + (int16_t)e16;
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x83:
		m_icount -= 11;
		e16 = fetch_word();
		m_eo = m_regs.w[BP] + m_regs.w[DI] + (int16_t)e16;
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x84:
		m_icount -= 9;
		e16 = fetch_word();
		m_eo = m_regs.w[SI] + (int16_t)e16;
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x85:
		m_icount -= 9;
		e16 = fetch_word();
		m_eo = m_regs.w[DI] + (int16_t)e16;
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	case 0x86:
		m_icount -= 9;
		e16 = fetch_word();
		m_eo = m_regs.w[BP] + (int16_t)e16;
		m_ea = calc_addr(SS, m_eo, size, op);
		break;
	case 0x87:
		m_icount -= 9;
		e16 = fetch_word();
		m_eo = m_regs.w[BX] + (int16_t)e16;
		m_ea = calc_addr(DS, m_eo, size, op);
		break;
	}
	return m_ea;
}

void Master512CoPro::interrupt(int int_num, int /* trap */)
{
	uint16_t dest_off = read_word(int_num * 4);
	uint16_t dest_seg = read_word(int_num * 4 + 2);

	PUSH(CompressFlags());
	m_TF = m_IF = 0;

	m_easeg = CS;

	PUSH(m_sregs[CS]);
	PUSH(m_ip);
	m_prev_ip = m_ip = dest_off;
	m_sregs[CS] = dest_seg;
}

inline uint32_t Master512CoPro::calc_addr(int seg, uint16_t offset, int /* size */, int /* op */, bool override)
{
	if (m_seg_prefix && (seg == DS || seg == SS) && override)
	{
		m_easeg = m_seg_prefix;
		return ((m_sregs[m_prefix_seg] << 4) + offset);
	}
	else
	{
		m_easeg = seg;
		return ((m_sregs[seg] << 4) + offset);
	}
}

inline void Master512CoPro::PutMemB(int seg, uint16_t offset, uint8_t data)
{
	write_byte(calc_addr(seg, offset, 1, I8086_WRITE), data);
}

inline void Master512CoPro::PutMemW(int seg, uint16_t offset, uint16_t data)
{
	write_word(calc_addr(seg, offset, 2, I8086_WRITE), data);
}

inline uint8_t Master512CoPro::GetMemB(int seg, uint16_t offset)
{
	return read_byte(calc_addr(seg, offset, 1, I8086_READ));
}

inline uint16_t Master512CoPro::GetMemW(int seg, uint16_t offset)
{
	return read_word(calc_addr(seg, offset, 2, I8086_READ));
}

inline void Master512CoPro::PutbackRMByte(uint8_t data)
{
	if (m_modrm >= 0xc0)
	{
		m_regs.b[m_Mod_RM.RM.b[m_modrm]] = data;
	}
	else
	{
		write_byte(m_ea, data);
	}
}

inline void Master512CoPro::PutbackRMWord(uint16_t data)
{
	if (m_modrm >= 0xc0)
	{
		m_regs.w[m_Mod_RM.RM.w[m_modrm]] = data;
	}
	else
	{
		write_word(m_ea, data);
	}
}

inline void Master512CoPro::PutImmRMWord()
{
	if (m_modrm >= 0xc0)
	{
		m_regs.w[m_Mod_RM.RM.w[m_modrm]] = fetch_word();
	}
	else
	{
		uint32_t addr = get_ea(2, I8086_WRITE);
		write_word(addr, fetch_word());
	}
}

inline void Master512CoPro::PutRMWord(uint16_t val)
{
	if (m_modrm >= 0xc0)
	{
		m_regs.w[m_Mod_RM.RM.w[m_modrm]] = val;
	}
	else
	{
		write_word(get_ea(2, I8086_WRITE), val);
	}
}

inline void Master512CoPro::PutRMByte(uint8_t val)
{
	if (m_modrm >= 0xc0)
	{
		m_regs.b[m_Mod_RM.RM.b[m_modrm]] = val;
	}
	else
	{
		write_byte(get_ea(1, I8086_WRITE), val);
	}
}

inline void Master512CoPro::PutImmRMByte()
{
	if (m_modrm >= 0xc0)
	{
		m_regs.b[m_Mod_RM.RM.b[m_modrm]] = fetch();
	}
	else
	{
		uint32_t addr = get_ea(1, I8086_WRITE);
		write_byte(addr, fetch());
	}
}

inline void Master512CoPro::DEF_br8()
{
	m_modrm = fetch();
	m_src = RegByte();
	m_dst = GetRMByte();
}

inline void Master512CoPro::DEF_wr16()
{
	m_modrm = fetch();
	m_src = RegWord();
	m_dst = GetRMWord();
}

inline void Master512CoPro::DEF_r8b()
{
	m_modrm = fetch();
	m_dst = RegByte();
	m_src = GetRMByte();
}

inline void Master512CoPro::DEF_r16w()
{
	m_modrm = fetch();
	m_dst = RegWord();
	m_src = GetRMWord();
}

inline void Master512CoPro::DEF_ald8()
{
	m_src = fetch();
	m_dst = m_regs.b[AL];
}

inline void Master512CoPro::DEF_axd16()
{
	m_src = fetch_word();
	m_dst = m_regs.w[AX];
}

inline uint8_t Master512CoPro::RegByte()
{
	return m_regs.b[m_Mod_RM.reg.b[m_modrm]];
}

inline uint16_t Master512CoPro::RegWord()
{
	return m_regs.w[m_Mod_RM.reg.w[m_modrm]];
}

inline void Master512CoPro::RegByte(uint8_t data)
{
	m_regs.b[m_Mod_RM.reg.b[m_modrm]] = data;
}

inline void Master512CoPro::RegWord(uint16_t data)
{
	m_regs.w[m_Mod_RM.reg.w[m_modrm]] = data;
}

inline uint16_t Master512CoPro::GetRMWord()
{
	if (m_modrm >= 0xc0)
	{
		return m_regs.w[m_Mod_RM.RM.w[m_modrm]];
	}
	else
	{
		return read_word(get_ea(2, I8086_READ));
	}
}

inline uint16_t Master512CoPro::GetnextRMWord()
{
	return read_word((m_ea & ~0xffff) | ((m_ea + 2) & 0xffff));
}

inline uint8_t Master512CoPro::GetRMByte()
{
	if (m_modrm >= 0xc0)
	{
		return m_regs.b[m_Mod_RM.RM.b[m_modrm]];
	}
	else
	{
		return read_byte(get_ea(1, I8086_READ));
	}
}

uint8_t Master512CoPro::read_byte(uint32_t addr)
{
	return program_read_byte_8(addr);
}

uint16_t Master512CoPro::read_word(uint32_t addr)
{
	return program_read_byte_8(addr) | (program_read_byte_8(addr + 1) << 8);
}

void Master512CoPro::write_byte(uint32_t addr, uint8_t data)
{
	program_write_byte_8(addr, data);
}

void Master512CoPro::write_word(uint32_t addr, uint16_t data)
{
	program_write_byte_8(addr, data & 0xff);
	program_write_byte_8((addr + 1), (data >> 8) & 0xff);
}

uint8_t Master512CoPro::read_port_byte(uint16_t port)
{
	return io_read_byte_8(port);
}

uint16_t Master512CoPro::read_port_word(uint16_t port)
{
	return io_read_byte_8(port) | (io_read_byte_8(port + 1) << 8);
}

void Master512CoPro::write_port_byte(uint16_t port, uint8_t data)
{
	io_write_byte_8(port, data);
}

void Master512CoPro::write_port_byte_al(uint16_t port)
{
	io_write_byte_8(port, m_regs.b[AL]);
}

void Master512CoPro::write_port_word(uint16_t port, uint16_t data)
{
	io_write_byte_8(port, data & 0xff);
	io_write_byte_8(port + 1, data >> 8);
}

void Master512CoPro::program_write_byte_8(uint32_t address, uint8_t data)
{
	if (address < 0xf0000)
	{
		m_Memory[address] = data;
	}
}

uint8_t Master512CoPro::program_read_byte_8(uint32_t address)
{
	return m_Memory[address];
}


uint8_t Master512CoPro::io_read_byte_8(uint16_t address)
{
	uint8_t data;

	if (address >= 0x80 && address <= 0x8f)
	{
		int i = (address - 0x80) / 2;
		data = ReadTubeFromParasiteSide(i);
	}
	else
	{
		data = 0xff;
	}

	return data;
}

void Master512CoPro::io_write_byte_8(uint16_t address, uint8_t data)
{
	if (address >= 0xffc0 && address <= 0xffcf)
	{
		switch (address - 0xffc0)
		{
			case 0x00:
				m_dma[0].source = (m_dma[0].source & 0xfff00) | data;
				break;
			case 0x01:
				m_dma[0].source = (m_dma[0].source & 0xf00ff) | (data << 8);
				break;
			case 0x02:
				m_dma[0].source = (m_dma[0].source & 0x0ffff) | ((data & 0x0f) << 16);
				break;
			case 0x03:
				break;
			case 0x04:
				m_dma[0].dest = (m_dma[0].dest & 0xfff00) | data;
				break;
			case 0x05:
				m_dma[0].dest = (m_dma[0].dest & 0xf00ff) | (data << 8);
				break;
			case 0x06:
				m_dma[0].dest = (m_dma[0].dest & 0x0ffff) | ((data & 0x0f) << 16);
				break;
			case 0x07:
				break;
			case 0x08:
				m_dma[0].count = (m_dma[0].count & 0xff00) | data;
				break;
			case 0x09:
				m_dma[0].count = (m_dma[0].count & 0xff) | (data << 8);
				break;
			case 0x0a:
				m_dma[0].control = (m_dma[0].control & 0xff00) | data;
				break;
			case 0x0b:
				m_dma[0].control = (m_dma[0].control & 0xff) | (data << 8);
				break;
			case 0x0c:
				break;
			case 0x0d:
				break;
			case 0x0e:
				break;
			case 0x0f:
				break;
		}
		return;
	}

	if (address >= 0x80 && address <= 0x8f)
	{
		int i = (address - 0x80) / 2;

		WriteTubeFromParasiteSide(i, data);
	}
}

void Master512CoPro::LoadBIOS()
{
	memset(m_Memory, 0, 0x100000);

	char path[256];
	strcpy(path, mainWin->GetUserDataPath());
	strcat(path, "BeebFile/BIOS.rom");

	FILE* f = fopen(path, "rb");

	if (f != nullptr)
	{
		fread(m_Memory + 0xf0000, 0x4000, 1, f);
		memcpy(m_Memory + 0xf4000, m_Memory + 0xf0000, 0x4000);
		memcpy(m_Memory + 0xf8000, m_Memory + 0xf0000, 0x4000);
		memcpy(m_Memory + 0xfc000, m_Memory + 0xf0000, 0x4000);
		fclose(f);
	}
	else
	{
		mainWin->Report(MessageType::Error, "Could not open BIOS image file:\n %s", path);
	}
}

void Master512CoPro::DoDMA()
{
	if (m_dma[0].source < 0x100)
	{
		// I/O to MEM
		uint8_t data = io_read_byte_8((uint16_t)m_dma[0].source);
		program_write_byte_8(m_dma[0].dest, data);
		m_dma[0].dest++;
	}
	else
	{
		// MEM to I/O
		uint8_t data = program_read_byte_8(m_dma[0].source);
		io_write_byte_8((uint16_t)m_dma[0].dest, data);
		m_dma[0].source++;
	}
}

void Master512CoPro::SaveState(FILE *SUEF)
{
	UEFWriteBuf(m_Memory, 0x100000, SUEF);

	for (int i = 0; i < 8; i++)
	{
		UEFWrite16(m_regs.w[i], SUEF);
	}

	for (int i = 0; i < 4; i++)
	{
		UEFWrite16(m_sregs[i], SUEF);
	}

	UEFWrite8(m_TF, SUEF);
	UEFWrite8(m_IF, SUEF);
	UEFWrite8(m_DF, SUEF);
	UEFWrite8(m_IOPL, SUEF);
	UEFWrite8(m_NT, SUEF);
	UEFWrite8(m_MF, SUEF);

	UEFWrite16(m_ip, SUEF);
	UEFWrite16(m_prev_ip, SUEF);

	UEFWrite32(m_SignVal, SUEF);
	UEFWrite32(m_AuxVal, SUEF);
	UEFWrite32(m_OverVal, SUEF);
	UEFWrite32(m_ZeroVal, SUEF);
	UEFWrite32(m_CarryVal, SUEF);
	UEFWrite32(m_ParityVal, SUEF);

	UEFWrite32(m_pending_irq, SUEF);
	UEFWrite32(m_nmi_state, SUEF);
	UEFWrite32(m_irq_state, SUEF);
	UEFWrite8(m_no_interrupt, SUEF);
	UEFWrite8(m_fire_trap, SUEF);
	UEFWrite8(m_test_state, SUEF);

	UEFWrite32(m_icount, SUEF);

	UEFWrite32(m_prefix_seg, SUEF);
	UEFWrite8(m_seg_prefix, SUEF);
	UEFWrite8(m_seg_prefix_next, SUEF);

	UEFWrite32(m_ea, SUEF);
	UEFWrite16(m_eo, SUEF);
	UEFWrite32(m_easeg, SUEF);

	UEFWrite8(m_modrm, SUEF);
	UEFWrite32(m_dst, SUEF);
	UEFWrite32(m_src, SUEF);
	UEFWrite32(m_pc, SUEF);

	UEFWrite8(m_halt, SUEF);

	for (int i = 0; i < 2; i++)
	{
		UEFWrite8(m_dma[i].drq_state, SUEF);
		UEFWrite32(m_dma[i].source, SUEF);
		UEFWrite32(m_dma[i].dest, SUEF);
		UEFWrite16(m_dma[i].count, SUEF);
		UEFWrite16(m_dma[i].control, SUEF);
	}

	UEFWrite8(m_last_dma, SUEF);
}

void Master512CoPro::LoadState(FILE *SUEF)
{
	UEFReadBuf(m_Memory, 0x100000, SUEF);

	for (int i = 0; i < 8; i++)
	{
		m_regs.w[i] = UEFRead16(SUEF);
	}

	for (int i = 0; i < 4; i++)
	{
		m_sregs[i] = UEFRead16(SUEF);
	}

	m_TF = UEFRead8(SUEF);
	m_IF = UEFRead8(SUEF);
	m_DF = UEFRead8(SUEF);
	m_IOPL = UEFRead8(SUEF);
	m_NT = UEFRead8(SUEF);
	m_MF = UEFRead8(SUEF);

	m_ip = UEFRead16(SUEF);
	m_prev_ip = UEFRead16(SUEF);

	m_SignVal = UEFRead32(SUEF);
	m_AuxVal = UEFRead32(SUEF);
	m_OverVal = UEFRead32(SUEF);
	m_ZeroVal = UEFRead32(SUEF);
	m_CarryVal = UEFRead32(SUEF);
	m_ParityVal = UEFRead32(SUEF);

	m_pending_irq = UEFRead32(SUEF);
	m_nmi_state = UEFRead32(SUEF);
	m_irq_state = UEFRead32(SUEF);
	m_no_interrupt = UEFRead8(SUEF);
	m_fire_trap = UEFRead8(SUEF);
	m_test_state = UEFRead8(SUEF);

	m_icount = UEFRead32(SUEF);

	m_prefix_seg = UEFRead32(SUEF);
	m_seg_prefix = UEFReadBool(SUEF);
	m_seg_prefix_next = UEFReadBool(SUEF);

	m_ea = UEFRead32(SUEF);
	m_eo = UEFRead16(SUEF);
	m_easeg = UEFRead32(SUEF);

	m_modrm = UEFRead8(SUEF);
	m_dst = UEFRead32(SUEF);
	m_src = UEFRead32(SUEF);
	m_pc = UEFRead32(SUEF);

	m_halt = UEFReadBool(SUEF);

	for (int i = 0; i < 2; i++)
	{
		m_dma[i].drq_state = UEFReadBool(SUEF);
		m_dma[i].source = UEFRead32(SUEF);
		m_dma[i].dest = UEFRead32(SUEF);
		m_dma[i].count = UEFRead16(SUEF);
		m_dma[i].control = UEFRead16(SUEF);
	}

	m_last_dma = UEFReadBool(SUEF);
}
