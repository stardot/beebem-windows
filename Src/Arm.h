/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  David Sharp

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

//////////////////////////////////////////////////////////////////////
// Arm.h: declarations for the CArm class.
// Part of Tarmac
// By David Sharp
// http://www.davidsharp.com
//////////////////////////////////////////////////////////////////////

#include "TarmacGlobals.h"

// define constants

// PSR flags
#define N_FLAG		(1<<31)	// negative
#define Z_FLAG		(1<<30)	// zero
#define C_FLAG		(1<<29)	// carry
#define V_FLAG		(1<<28)	// overflow
#define I_FLAG		(1<<27)	// interrupt disable
#define F_FLAG		(1<<26)	// fast interrupt disable
#define M1_FLAG		(1<<1)	// processor mode (bit 1)
#define M2_FLAG		(1<<0)	// processor mode (bit 2)

// masks for PSR
#define PSR_MASK	(N_FLAG | Z_FLAG | C_FLAG | V_FLAG | I_FLAG | F_FLAG | M1_FLAG | M2_FLAG)
#define PC_MASK		(~PSR_MASK)

// processor modes for bits 0-1 of PSR
#define USR_MODE	0		// user
#define FIQ_MODE	1		// fast interrupt request
#define IRQ_MODE	2		// interrupt request
#define SVC_MODE	3		// supervisor

// hardware vector addresses
#define RESET_VECTOR					0x0000000
#define UNDEFINED_INSTRUCTION_VECTOR	0x0000004
#define SOFTWARE_INTERRUPT_VECTOR		0x0000008
#define PREFETCH_ABORT_VECTOR			0x000000c
#define DATA_ABORT_VECTOR				0x0000010
#define ADDRESS_EXCEPTION_VECTOR		0x0000014
#define INTERRUPT_REQUEST_VECTOR		0x0000018
#define FAST_INTERRUPT_REQUEST_VECTOR	0x000001c

// ARM shift types operand fields
#define LSL			0
#define LSR			1
#define ASR			2
#define ROR			3

typedef unsigned int Reg;
typedef unsigned int Word;
typedef unsigned char Byte;
#define MODEFLAGS	(M1_FLAG | M2_FLAG)
#define INTRFLAGS	(I_FLAG | F_FLAG)
#define PSRMASK		(N_FLAG|Z_FLAG|C_FLAG|V_FLAG | MODEFLAGS | INTRFLAGS)
#define PCMASK		(~PSRMASK)

// signals are a block of flags for RedSquirrel to allow IOC to adjust/examine interrupts
// "Each device can call IOC.irqa_interrupt(mask), IOC.irqb_interrupt(mask) or
// IOC.firq_interrupt(mask) to tell IOC which interrupt has occurred. IOC then calls
// ARM.signal_fast_interrupt(state) or ARM.signal_interrupt(state) as appropriate which
// sets or clears a bit in signals word (iff that interrupt is enabled). If any signal is
// set the arm will handle the interrupt at the end of the current instruction." - GB

union Signals
{
	uint32 all;
	struct
	{
		uint8 step_once;			// UI wants to stop
		uint8 interrupt;			// interrupt may have occurred
		uint8 fast_interrupt;		// fast interrupt may have occurred
		uint8 unused;			
	};
};

// dynamic profiling options
const bool dynamicProfilingExceptionFreq = FALSE;
const bool dynamicProfilingRegisterUse = FALSE;
const bool dynamicProfilingConditionalExecution = FALSE;
const bool dynamicProfilingCoprocessorUse = TRUE;

extern int Enable_Arm;
extern int ArmTube;

class CArm 
{

	void outputSomeNonsenseCPP();

// functions
public:
	void dynamicProfilingCoprocessorUsage(uint32 currentInstruction);
	void dynamicProfilingExceptionFrequencyReport();
	void dynamicProfilingExceptionFrequency(char *exceptionName, uint32& counter);
	int GetMode();
	Reg GetUserRegister(unsigned int reg);
	void SetMode(int mode);
	Reg GetUserReg(int reg);
	Reg GetFirqReg(int reg);
	Reg GetIrqReg(int reg);
	Reg GetSvcReg(int reg);
	Reg GetReg(int reg);
	void set_stepping(bool state);
	Word current_pc();
	void stop_at(Word address);
	void ResetState();
	void signal_fast_interrupt(bool state);
	void signal_interrupt(bool state);
	
	uint32 pc;
	inline void performBranch();	

	// logic
	inline uint32 eorOperator(uint32 operand1, uint32 operand2);
	inline uint32 eorOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 andOperator(uint32 operand1, uint32 operand2);
	inline uint32 andOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 orrOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 orrOperator(uint32 operand1, uint32 operand2);

	// arithmetic
	inline void performMla();
	inline void performMlaS();
	inline void performMul();
	inline void performMulS();
	inline uint32 adcOperator(uint32 operand1, uint32 operand2);
	inline uint32 adcOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 addOperator(uint32 operand1, uint32 operand2);
	inline uint32 addOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 subOperator(uint32 operand1, uint32 operand2);
	inline uint32 subOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 sbcOperatorS(uint32 operand1, uint32 operand2);
	inline uint32 sbcOperator(uint32 operand1, uint32 operand2);

	// memory
	inline bool performDataTransferLoadByte(uint32 address, uint8& location);
	inline bool performDataTransferStoreByte(uint32 address, uint8 value);
	inline bool performDataTransferLoadWord(uint32 address, uint32& destination);
	inline bool performDataTransferStoreWord(uint32 address, uint32 data);
	inline bool performBlockDataTransferLoadS(uint rn, uint32 initialAddress, uint32 finalAddress);
	inline bool performBlockDataTransferLoad(uint rn, uint32 initialAddress, uint32 finalAddress);
	inline bool performBlockDataTransferStoreS(uint rn, uint32 initialAddress, uint32 finalAddress);
	inline bool performBlockDataTransferStore(uint rn, uint32 initialAddress, uint32 finalAddress);
	inline void performSingleDataSwapByte();
	inline void performSingleDataSwapWord();

	inline bool readWord(uint32 address, uint32& destination);
	inline bool readByte(uint32 address, uint8& destination);
	inline bool writeByte(uint32 address, uint8 value);
	inline bool writeWord(uint32 address, uint32 data);
	inline bool isValidAddress(uint32 address);

	// condition flags
	inline void clearTrans();
	inline void setTrans();
	inline void setProcessorMode(uint newMode);
	inline void setProcessorStatusFlags(uint32 mask, uint32 value);
	inline uint getConditionFlag(uint32 flagValue);
	inline void clearConditionFlags(uint32 flagValue);
	inline void setConditionFlags(uint32 flagValue);
	inline void updateNZFlags(uint32 value);
	inline void updateAddFlags(uint32 operand1, uint32 operand2, uint32 result);
	inline void updateSubFlags(uint32 operand1, uint32 operand2, uint32 result);
	inline bool executeConditionally(uint32 instruction);

	// generic instruction templates
	inline void setDestinationS(uint32 value);
	inline void setDestination(uint32 value);
	inline uint32 getDataTransferValueRegister();
	inline uint32 getDataTransferValueImmediate();
	// for data processing instructions operands
	inline uint32 getDataProcessingImmediateOperand1();		// read op1 when op2 is immediate
	inline uint32 getDataProcessingImmediateOperand2();		// read op2 when op2 is immediate
	inline uint32 getDataProcessingImmediateOperand2S();	// read op2 when op2 is immediate and S is set
	inline uint32 getDataProcessingRegisterOperand1();		// read op1 when op2 is register
	inline uint32 getDataProcessingRegisterOperand2();		// read op2 when op2 is register
	inline uint32 getDataProcessingRegisterOperand2S();		// read op2 when op2 is register and S is set

	// register access
	inline uint32 getRegister(uint regNumber);
	inline uint32 getRegisterWithPSR(uint regNumber);
	inline uint32 getRegisterWithPipelining(uint regNumber);
	inline uint32 getRegisterWithPSRAndPipelining(uint regNumber);
	inline void setRegister(uint regNumber, uint32 value);
	inline void setRegisterWithPrefetch(uint regNumber, uint32 value);
	inline uint32 getProcessorStatusRegister();
	inline void setProcessorStatusRegister(uint32 value);
	inline uint32 getProgramCounter();

	// barrel shifter
	inline uint32 rrxOperator(uint32 value);
		
	// coprocessor
	inline bool coprocessorDataOperation();
	inline bool coprocessorRegisterTransferRead();
	inline bool coprocessorRegisterTransferWrite();
	inline uint32 coprocessorDataTransferOffset();
	inline bool coprocessorDataTransferLoad(uint32 address);
	inline bool coprocessorDataTransferStore(uint32 address);

	// exceptions
	inline void exceptionFastInterruptRequest();
	inline void exceptionInterruptRequest();
	inline void exceptionPrefetchAbort();
	inline void exceptionSoftwareInterrupt();
	inline void exceptionUndefinedInstruction();
	inline void exceptionReset();
	inline void exceptionAddress();
	inline void exceptionDataAbort();

	// control
	void exec(int count);
	void run(void);
	void reset();
	
	// utility
	static inline bool getNegative(uint32 value);
	static inline bool getPositive(uint32 value);
	static inline bool isExtendedInstruction(uint32 instruction);

	// construct / destruct
	CArm();
	virtual ~CArm();
	
	// variables
	Signals signals;
	bool pending_interrupt;
	bool pending_fast_interrupt;
	int instr_count;
	Word stop_address;
	
private:
	uint32 lastCopro;				// num of instructions executed since last copro instruction profiled
	void dynamicProfilingConditionalExe(uint32 currentInstruction);
	void dynamicProfilingConditionalExeReport();
	void dynamicProfilingRegisterUsageReport();
	void dynamicProfilingRegisterUsage(uint regNumber, bool get);
	
	
	
	// variables
	uint processorMode;				// in bits 0-1
	uint interruptDisableFlag;		// in bit 26
	uint fastInterruptDisableFlag;	// in bit 27
	uint conditionFlags;			// store NZCV flags in bits 0-3
	uint trace;
	
	bool	prefetchInvalid;
	uint32	prefetchInstruction;
	uint32	currentInstruction;
	uint32	r[16];					// current bank of registers
	uint32	usrR[16];				// user mode registers
	uint32	svcR[16];				// supervisor mode registers
	uint32	irqR[16];				// interrupt mode registers
	uint32	fiqR[16];				// fast interrupt mode registers
	uint32	*curR[4];				// pointer to current mode's registers

	// look up tables
	uint32 immediateCarry[4096];
	uint32 immediateValue[4096];
	
	// TEST ENVIRONMENT VARIABLES
	uint8 romMemory[0x4000];		// 16 KBytes of rom memory
	uint8 ramMemory[0x400000];		// 4 MBytes of ram memory
	bool keepRunning;				// keep calling run()
//	CArmDecoder decoder;			// create instance of disassembler
	uint32 executionCount;
	uint32 executionCountThresh;	// executionCount value to start dumping diss at
	uint32 previousProcessorMode;
	uint32 modeCounter;
	uint32 modeTotal[4];
	uint32 resetCounter;
	uint32 undefCounter;
	uint32 swiCounter;
	uint32 prefAbortCounter;
	uint32 dataAbortCounter;
	uint32 addrExcepCounter;
	uint32 irqCounter;
	uint32 fiqCounter;
	uint32 exceptionLastExecutionCount;
	uint32 registerSetCounter[16];
	uint32 registerGotCounter[16];
	uint32 conditionallyExecuted[16];
	uint32 conditionallyNotExecuted[16];
	int iocounter;
	// END OF TEST ENVIRONMENT VARIABLES

};
