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

#ifndef MASTER512_COPRO_HEADER
#define MASTER512_COPRO_HEADER

// Master 512 80186 Coprocessor

#include <stdint.h>
#include <stdio.h>

// constants for expression endianness
enum endianness_t
{
	ENDIANNESS_LITTLE,
	ENDIANNESS_BIG
};

// declare native endianness to be one or the other
const endianness_t ENDIANNESS_NATIVE = ENDIANNESS_LITTLE;

// endian-based value: first value is if 'endian' is little-endian, second is if 'endian' is big-endian
#define ENDIAN_VALUE_LE_BE(endian,leval,beval)  (((endian) == ENDIANNESS_LITTLE) ? (leval) : (beval))

// endian-based value: first value is if native endianness is little-endian, second is if native is big-endian
#define NATIVE_ENDIAN_VALUE_LE_BE(leval,beval)  ENDIAN_VALUE_LE_BE(ENDIANNESS_NATIVE, leval, beval)

class Master512CoPro
{
	public:
		Master512CoPro();
		~Master512CoPro();
		Master512CoPro(const Master512CoPro&) = delete;
		Master512CoPro& operator=(const Master512CoPro&) = delete;

	public:
		void Reset();
		void Execute(int Cycles);

		void SaveState(FILE *SUEF);
		void LoadState(FILE *SUEF);

	private:
		void LoadBIOS();

		void CLK(uint8_t op);
		void CLKM(uint8_t op_reg, uint8_t op_mem);

		bool common_op(uint8_t op);

		uint8_t fetch()
		{
			uint8_t data = m_Memory[update_pc()];
			m_ip++;
			return data;
		}

		uint8_t fetch_op() { return fetch(); }

		uint16_t fetch_word()
		{
			uint16_t data = fetch();
			data |= fetch() << 8;
			return data;
		}

		uint8_t repx_op();

		uint32_t update_pc() { return m_pc = (m_sregs[CS] << 4) + m_ip; }

		uint32_t get_ea(int size, int op);

		uint32_t calc_addr(int seg, uint16_t offset, int size, int op, bool override = true);

		void PutMemB(int seg, uint16_t offset, uint8_t data);
		void PutMemW(int seg, uint16_t offset, uint16_t data);
		uint8_t GetMemB(int seg, uint16_t offset);
		uint16_t GetMemW(int seg, uint16_t offset);

		void interrupt(int int_num, int trap = 1);

		// Accessing memory and io
		uint8_t read_byte(uint32_t addr);
		uint16_t read_word(uint32_t addr);
		void write_byte(uint32_t addr, uint8_t data);
		void write_word(uint32_t addr, uint16_t data);
		uint8_t read_port_byte(uint16_t port);
		uint16_t read_port_word(uint16_t port);
		void write_port_byte(uint16_t port, uint8_t data);
		void write_port_byte_al(uint16_t port);
		void write_port_word(uint16_t port, uint16_t data);

		void program_write_byte_8(uint32_t address, uint8_t data);
		uint8_t program_read_byte_8(uint32_t address);

		uint8_t io_read_byte_8(uint16_t address);
		void io_write_byte_8(uint16_t address, uint8_t data);

		void PutbackRMByte(uint8_t data);
		void PutbackRMWord(uint16_t data);
		void PutImmRMWord();
		void PutRMWord(uint16_t val);
		void PutRMByte(uint8_t val);
		void PutImmRMByte();

		void DEF_br8();
		void DEF_wr16();
		void DEF_r8b();
		void DEF_r16w();
		void DEF_ald8();
		void DEF_axd16();

		uint8_t RegByte();
		uint16_t RegWord();
		void RegByte(uint8_t data);
		void RegWord(uint16_t data);
		uint16_t GetRMWord();
		uint16_t GetnextRMWord();
		uint8_t GetRMByte();

		void PUSH(uint16_t data);
		uint16_t POP();

		void JMP(bool cond);

		void ADJ4(int8_t param1, int8_t param2);
		void ADJB(int8_t param1, int8_t param2);

		// Setting flags

		void set_CFB(uint32_t x);
		void set_CFW(uint32_t x);
		void set_AF(uint32_t x, uint32_t y, uint32_t z);
		void set_SF(uint32_t x);
		void set_ZF(uint32_t x);
		void set_PF(uint32_t x);
		void set_SZPF_Byte(uint32_t x);
		void set_SZPF_Word(uint32_t x);
		void set_OFW_Add(uint32_t x, uint32_t y, uint32_t z);
		void set_OFB_Add(uint32_t x, uint32_t y, uint32_t z);
		void set_OFW_Sub(uint32_t x, uint32_t y, uint32_t z);
		void set_OFB_Sub(uint32_t x, uint32_t y, uint32_t z);

		uint16_t CompressFlags() const;
		void ExpandFlags(uint16_t f);

		void i_insb();
		void i_insw();
		void i_outsb();
		void i_outsw();
		void i_movsb();
		void i_movsw();
		void i_cmpsb();
		void i_cmpsw();
		void i_stosb();
		void i_stosw();
		void i_lodsb();
		void i_lodsw();
		void i_scasb();
		void i_scasw();
		void i_popf();

		void IncWordReg(uint8_t reg);
		void DecWordReg(uint8_t reg);
		void ROL_BYTE();
		void ROR_BYTE();
		void ROL_WORD();
		void ROR_WORD();
		void ROLC_WORD();
		void ROLC_BYTE();
		void RORC_BYTE();
		void RORC_WORD();
		void SHL_BYTE(uint8_t c);
		void SHL_WORD(uint8_t c);
		void SHR_BYTE(uint8_t c);
		void SHR_WORD(uint8_t c);
		void SHRA_BYTE(uint8_t c);
		void SHRA_WORD(uint8_t c);
		void XchgAXReg(uint8_t reg);
		uint32_t ADDB();
		uint32_t ADDX();
		uint32_t SUBB();
		uint32_t SUBX();
		void ORB();
		void ORW();
		void ANDB();
		void ANDX();
		void XORB();
		void XORW();

		void execute_set_input(int inptnum, int state);
		void DoDMA();

	private:
		unsigned char* m_Memory;

		// Eight general registers, viewed as 16-bit or as 8-bit registers
		union
		{
			uint16_t w[8];
			uint8_t  b[16];
		} m_regs;

		enum BREGS {
			AL = NATIVE_ENDIAN_VALUE_LE_BE(0x0, 0x1),
			AH = NATIVE_ENDIAN_VALUE_LE_BE(0x1, 0x0),
			CL = NATIVE_ENDIAN_VALUE_LE_BE(0x2, 0x3),
			CH = NATIVE_ENDIAN_VALUE_LE_BE(0x3, 0x2),
			DL = NATIVE_ENDIAN_VALUE_LE_BE(0x4, 0x5),
			DH = NATIVE_ENDIAN_VALUE_LE_BE(0x5, 0x4),
			BL = NATIVE_ENDIAN_VALUE_LE_BE(0x6, 0x7),
			BH = NATIVE_ENDIAN_VALUE_LE_BE(0x7, 0x6),
			SPL = NATIVE_ENDIAN_VALUE_LE_BE(0x8, 0x9),
			SPH = NATIVE_ENDIAN_VALUE_LE_BE(0x9, 0x8),
			BPL = NATIVE_ENDIAN_VALUE_LE_BE(0xa, 0xb),
			BPH = NATIVE_ENDIAN_VALUE_LE_BE(0xb, 0xa),
			SIL = NATIVE_ENDIAN_VALUE_LE_BE(0xc, 0xd),
			SIH = NATIVE_ENDIAN_VALUE_LE_BE(0xd, 0xc),
			DIL = NATIVE_ENDIAN_VALUE_LE_BE(0xe, 0xf),
			DIH = NATIVE_ENDIAN_VALUE_LE_BE(0xf, 0xe)
		};

		enum SREGS { ES = 0, CS, SS, DS };
		enum WREGS { AX = 0, CX, DX, BX, SP, BP, SI, DI };

		enum {
			I8086_READ,
			I8086_WRITE,
			I8086_FETCH,
			I8086_NONE
		};

		uint16_t  m_sregs[4];

		uint8_t m_TF, m_IF, m_DF; // 0 or 1 valued flags
		uint8_t m_IOPL, m_NT, m_MF;

		uint16_t m_ip;
		uint16_t m_prev_ip;

		int32_t m_SignVal;
		uint32_t m_AuxVal; // 0 or non-0 valued flags
		uint32_t m_OverVal;
		uint32_t m_ZeroVal;
		uint32_t m_CarryVal;
		uint32_t m_ParityVal;

		uint32_t m_pending_irq;
		int m_nmi_state;
		int m_irq_state;
		uint8_t m_no_interrupt;
		uint8_t m_fire_trap;
		int m_test_state;

		int m_icount;

		uint32_t m_prefix_seg; // the latest prefix segment
		bool m_seg_prefix; // prefix segment indicator
		bool m_seg_prefix_next; // prefix segment for next instruction

		uint32_t m_ea;
		uint16_t m_eo; // Effective offset of the address (before segment is added)
		int m_easeg;

		// Used during execution of instructions
		uint8_t m_modrm;
		uint32_t m_dst;
		uint32_t m_src;
		uint32_t m_pc;

		// Lookup tables
		uint8_t m_parity_table[256];

		struct {
			struct {
				int w[256];
				int b[256];
			} reg;
			struct {
				int w[256];
				int b[256];
			} RM;
		} m_Mod_RM;

		bool m_halt;

		struct dma_state
		{
			bool drq_state;
			uint32_t source;
			uint32_t dest;
			uint16_t count;
			uint16_t control;
		};

		dma_state m_dma[2];
		bool m_last_dma;
};

extern Master512CoPro master512CoPro;

#endif
