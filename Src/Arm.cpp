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
// Arm.cpp: implementation of the CArm class.
// Part of Tarmac
// By David Sharp
// http://www.davidsharp.com
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Arm.h"
#include "ArmDisassembler.h"	// gives access to disassembler
#include "BeebMem.h"
#include "Log.h"
#include "Tube.h"
#include "UefState.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CArm::CArm()
{
}

CArm::InitResult CArm::init(const char *ROMPath)
{
	// set up pointers to each bank of registers
	curR[USR_MODE] = usrR;
	curR[SVC_MODE] = svcR;
	curR[IRQ_MODE] = irqR;
	curR[FIQ_MODE] = fiqR;

	// construct look up table of real values for encoded immediate
	// values in data processing instructions
	for(int rotate=0; rotate<16; rotate++)
	{
		for(int immediate=0; immediate<256; immediate++)
		{
			// rotate immediate value right by twice the rotate value
			int index = (rotate<<8) | immediate;

			immediateValue[index] = rorOperator(immediate, rotate<<1);

			// should carry be set
			if( getBit(immediateValue[index], 31) )
			{
				immediateCarry[index] = 1;
			}
			else
			{
				immediateCarry[index] = 0;
			}
		}
	}

	processorMode = SVC_MODE;

	// reset processor state to initial values
	reset();

	// reset instruction execution counter
	executionCount = 0;

	// ??? profiling usage of different modes
	previousProcessorMode = SVC_MODE;
	modeCounter = 0;

	// ??? profiling usage of different exceptions
	if(dynamicProfilingExceptionFreq)
	{
		// resetCounter = 0;
		undefCounter = 0;
		swiCounter = 0;
		prefAbortCounter = 0;
		dataAbortCounter = 0;
		addrExcepCounter = 0;
		irqCounter = 0;
		fiqCounter = 0;
		exceptionLastExecutionCount = 0;
	}

	if(dynamicProfilingRegisterUse)
	{
		for(int regNumber=0; regNumber<16; regNumber++)
		{
			registerGotCounter[regNumber] = 0;
			registerSetCounter[regNumber] = 0;
		}
	}

	if(dynamicProfilingConditionalExecution)
	{
		for(int condition=0; condition<16; condition++)
		{
			conditionallyExecuted[condition] =0;
			conditionallyNotExecuted[condition] = 0;
		}
	}

	if(dynamicProfilingCoprocessorUse)
	{
		lastCopro = 0;
	}

	modeTotal[USR_MODE] = 0;
	modeTotal[FIQ_MODE] = 0;
	modeTotal[IRQ_MODE] = 0;
	modeTotal[SVC_MODE] = 0;

	/////////////////////////////
	// set up test environment
	/////////////////////////////

	processorMode = USR_MODE;
	processorMode = SVC_MODE;
	r[15] = 0;
	prefetchInvalid = true;
	conditionFlags = 0;
	trace = 0;

	WriteLog("init_arm()\n");

	// load file into test memory
	FILE *ROMFile = fopen(ROMPath, "rb");

	if (ROMFile != nullptr)
	{
		fread(romMemory, 0x4000, 1, ROMFile);
		fclose(ROMFile);
	}
	else
	{
		return InitResult::FileNotFound;
	}

	memset(ramMemory, 0, 0x400000);
	memcpy(ramMemory, romMemory, 0x4000);

	/* uint32 memoryValue = 0;
	for(int x=0; x<4*11; x+=4)
	{
		(void)readWord(x, memoryValue);
		TRACE("%x = %x\n", x, memoryValue);
	} */

	return InitResult::Success;
}

void CArm::exec(int count)
{
	uint32 ci;
	char disassembly[256];
	char addressS[64];

	while (count > 0)
	{
		if (trace)
		{
			uint32 val;
			readWord(0xc50c, val);

			if (prefetchInvalid)
			{
				(void) readWord(pc, ci);

				Arm_disassemble(pc, ci, disassembly);

				sprintf(addressS, "0x%08x : %02x %02x %02x %02x ", pc,
						ci & 0xff, (ci >> 8) & 0xff, (ci >> 16) & 0xff, (ci >> 24) & 0xff );

			}
			else
			{
				(void) readWord(pc - 4, ci);

				Arm_disassemble(pc - 4, ci, disassembly);

				sprintf(addressS, "0x%08x : %02x %02x %02x %02x ", pc - 4,
						ci & 0xff, (ci >> 8) & 0xff, (ci >> 16) & 0xff, (ci >> 24) & 0xff );
			}

			WriteLog(" r0 = %08x r1 = %08x r2 = %08x r3 = %08x r4 = %08x r5 = %08x r6 = %08x r7 = %08x r8 = %08x : ",
					 getRegister(0), getRegister(1), getRegister(2), getRegister(3), getRegister(4),
					 getRegister(5), getRegister(6), getRegister(7), getRegister(8));

			WriteLog("%s : %08x : %s\n", addressS, val, disassembly);

			trace--;
		}

		// if ( ((pc & 0xffff) >= 0x1c48) && ((pc & 0xffff) <= 0x1c60) )
		// {
		//	trace = 100;
		// }

		run();
		count--;
	}
}

CArm::~CArm()
{
	// ??? output mode counter info
	//CString modeInfo;
	//modeInfo.Format("usr=%d fiq=%d irq=%d svc=%d \n", modeTotal[USR_MODE], modeTotal[FIQ_MODE], modeTotal[IRQ_MODE], modeTotal[SVC_MODE] );
	//reportFile.WriteString(modeInfo);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequencyReport();

	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsageReport();

	if(dynamicProfilingConditionalExecution)
		dynamicProfilingConditionalExeReport();
}

void CArm::run()
{
	// note, if the while(true) loop is placed inside run() as it may need to be for speed
	// then returns after exceptions need to be changed to continues!

	// ??? profile usage of processor modes
	//modeTotal[processorMode]++;
	//modeCounter++;
	//if(previousProcessorMode != processorMode)
	//{
	//	CString modeInfo;
	//	modeInfo.Format("mode=%d count=%d\n",previousProcessorMode, modeCounter-1);
	//	reportFile.WriteString(modeInfo);
	//
	//	modeCounter = 0;
	//	previousProcessorMode = processorMode;
	//}

	// has prefetch been invalidated by previously executed instruction
	if(prefetchInvalid)
	{
		// get the program counter value from r15
		pc = getRegister(15);

		// increment r15 by 8 to account for pipelining effect
		setRegisterWithPrefetch(15, getRegister(15) + 8 );
		prefetchInvalid = false;

		// refill the pipeline by fetching into the prefetch instruction
		if( !readWord(pc, prefetchInstruction) )
		{
			exceptionDataAbort();
			return;
		}
		pc += 4;
	}

	// prefetched instruction becomes the current instruction
	currentInstruction = prefetchInstruction;
	// prefetch next instruction
	if( !readWord(pc, prefetchInstruction) )
	{
		exceptionPrefetchAbort();
		return;
	}

	// increment total instruction executed counter
	executionCount++;

	if(dynamicProfilingConditionalExecution)
		dynamicProfilingConditionalExe(currentInstruction);

	// instruction condition codes match PSR so that instruction should be executed
	if( executeConditionally(currentInstruction) )
	{
		// decode instruction type from bits 20-27
		switch( getField(currentInstruction, 20, 27) )
		{
			// data processing instructions have operand 2 as register

			// and rd, rn, rm
			// mul rd, rm, rs
			case 0x00:
			{
				if( isExtendedInstruction(currentInstruction) )
				{
					// mul rd, rm, rs
					performMul();
				}
				else
				{
					// and rd, rn, rm
					setDestination( andOperator(getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2()) );
				}
				break;
			}
			// andS rd, rn, rm
			// mulS rd, rm, rs
			case 0x01:
			{
				if( isExtendedInstruction(currentInstruction) )
				{
					// mulS rd, rm, rs
					performMulS();
				}
				else
				{
					// andS rd, rn, rm
					setDestinationS( andOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2S()) );
				}
				break;
			}
			// eor rd, rn, rm
			// mla rd, rm, rs
			case 0x02:
			{
				if( isExtendedInstruction(currentInstruction) )
				{
					// mla rd, rm, rs
					performMla();
				}
				else
				{
					// eor rd, rn, rm
					setDestination( eorOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				}
				break;
			}
			// eorS rd, rn, rm
			// mlaS rd, rm, rs
			case 0x03:
			{
				if( isExtendedInstruction(currentInstruction) )
				{
					// mlaS rd, rm, rs
					performMlaS();
				}
				else
				{
					// eorS rd, rn, rm
					setDestinationS( eorOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2S() ) );
				}
				break;
			}
			// sub rd, rn, rm
			case 0x04:
			{
				setDestination( subOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// subS rd, rn, rm
			case 0x05:
			{
				setDestinationS( subOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// rsb rd, rn, rm
			case 0x06:
			{
				// note reversal of operand 1 and 2
				setDestination( subOperator( getDataProcessingRegisterOperand2(), getDataProcessingRegisterOperand1() ) );
				break;
			}
			// rsbS rd, rn, rm
			case 0x07:
			{
				setDestinationS( subOperatorS( getDataProcessingRegisterOperand2(), getDataProcessingRegisterOperand1() ) );
				break;
			}
			// add rd, rn, rm
			case 0x08:
			{
				setDestination( addOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// addS rd, rn, rm
			case 0x09:
			{
				setDestinationS( addOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// adc rd, rn, rm
			case 0x0A:
			{
				setDestination( adcOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// adcS rd, rn, rm
			case 0x0B:
			{
				setDestinationS( adcOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// sbc rd, rn, rm
			case 0x0C:
			{
				setDestination( sbcOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// sbcS rd, rn, rm
			case 0x0D:
			{
				setDestinationS( sbcOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// rsc rd, rn, rm
			case 0x0E:
			{
				setDestination( sbcOperator( getDataProcessingRegisterOperand2(), getDataProcessingRegisterOperand1() ) );
				break;
			}
			// rscS rd, rn, rm
			case 0x0F:
			{
				setDestinationS( sbcOperatorS( getDataProcessingRegisterOperand2(), getDataProcessingRegisterOperand1() ) );
				break;
			}
			// tst rd, rn, rm
			// swp rd, rn, rm
			case 0x10:
			{
				if( isExtendedInstruction(currentInstruction) )
				{
					// swp rd, rn, rm
					performSingleDataSwapWord();
				}
				else
				{
					// tst rd, rn, rm
					// S flag (bit 20) not set so NOP
					// ARM6 = MRS
				}
				break;
			}
			// tstP (PSR), rn, rm
			// tst rn, rm
			case 0x11:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// tstP (PSR), rn, rm
					// updates entire PSR (if in user mode then just condition flags)
					setProcessorStatusRegister( andOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				}
				else
				{
					// tst rn, rm
					// updates only condition flags
					(void)andOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2S() );
				}
				break;
			}
			// teq rd, rn, rm
			case 0x12:
			{
				// S flag (bit 20) not set so NOP
				// ARM6 - MSR
				break;
			}
			// teqP (PSR), rn, rm
			// teq rn, rm
			case 0x13:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// teqP (PSR), rn, rm
					setProcessorStatusRegister( eorOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				}
				else
				{
					// teq rn, rm
					(void)eorOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2S() );
				}
				break;
			}
			// swp rd, rn, rm
			// cmp rd, rn, rm - NOP
			case 0x14:
			{
				if( isExtendedInstruction(currentInstruction) )
				{
					// swp rd, rn, rm
					performSingleDataSwapByte();
				}
				else
				{
					// cmp rd, rn, rm
					// S flag (bit 20) not set so NOP
					// ARM6 - MRS
				}
				break;
			}
			// cmpP (PSR), rn, rm
			// cmp rn, rm
			case 0x15:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// cmpP (PSR), rn, rm
					setProcessorStatusRegister( subOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				}
				else
				{
					// cmp rn, rm
					(void)subOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() );
				}
				break;

			}
			// cmn rd, rn, rm - NOP
			case 0x16:
			{
				// S flag (bit 20) not set so NOP
				// ARM6 - MSR
				break;
			}
			// cmnP (PSR), rn, rm
			// cmn rn, rm
			case 0x17:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// cmnP (PSR), rn, rm
					setProcessorStatusRegister( addOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				}
				else
				{
					// cmn rn, rm
					(void)addOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() );
				}
				break;
			}
			// orr rd, rn, rm
			case 0x18:
			{
				setDestination( orrOperator( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2() ) );
				break;
			}
			// orrS rd, rn, rm
			case 0x19:
			{
				setDestinationS( orrOperatorS( getDataProcessingRegisterOperand1(), getDataProcessingRegisterOperand2S() ) );
				break;
			}
			// mov rd, rn, rm		(rn is ignored)
			case 0x1A:
			{
				setDestination( getDataProcessingRegisterOperand2() );

				// ??? remove keepRunning - for simple code only
				// checks for mov pc,r14
				//if(currentInstruction == 0xE1A0F00E)
				//	keepRunning = false;
				break;
			}
			// movS rd, rn, rm
			case 0x1B:
			{
				uint32 value = getDataProcessingRegisterOperand2S();
				updateNZFlags( value );
				setDestinationS( value );
				break;
			}
			// bic rd, rn, rm
			case 0x1C:
			{
				// and with inverted operand2
				setDestination( andOperator( getDataProcessingRegisterOperand1(), ~getDataProcessingRegisterOperand2() ) );
				break;
			}
			// bicS rd, rn, rm
			case 0x1D:
			{
				setDestinationS( andOperatorS( getDataProcessingRegisterOperand1(), ~getDataProcessingRegisterOperand2S() ) );
				break;
			}
			// mvn rd, rn, rm
			case 0x1E:
			{
				setDestination( ~getDataProcessingRegisterOperand2() );
				break;
			}
			// mvnS rd, rn, rm
			case 0x1F:
			{
				uint32 value = ~getDataProcessingRegisterOperand2S();
				updateNZFlags( value );
				setDestinationS( value );
				break;
			}

			// data processing instructions have operand 2 as an immediate value

			// and rd, rn, imm
			case 0x20:
			{
				setDestination( andOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// andS rd, rn, imm
			case 0x21:
			{
				setDestinationS( andOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2S() ) );
				break;
			}
			// eor rd, rn, imm
			case 0x22:
			{
				setDestination( eorOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// eorS rd, rn, imm
			case 0x23:
			{
				setDestinationS( eorOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2S() ) );
				break;
			}
			// sub rd, rn, imm
			case 0x24:
			{
				setDestination( subOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// subS rd, rn, imm
			case 0x25:
			{
				setDestinationS( subOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// rsb rd, rn, imm
			case 0x26:
			{
				setDestination( subOperator( getDataProcessingImmediateOperand2(), getDataProcessingImmediateOperand1() ) );
				break;
			}
			// rsbS rd, rn, imm
			case 0x27:
			{
				setDestinationS( subOperatorS( getDataProcessingImmediateOperand2(), getDataProcessingImmediateOperand1() ) );
				break;
			}
			// add rd, rn, imm
			case 0x28:
			{
				setDestination( addOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// addS rd, rn, imm
			case 0x29:
			{
				setDestinationS( addOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// adc rd, rn, imm
			case 0x2A:
			{
				setDestination( adcOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// adcS rd, rn, imm
			case 0x2B:
			{
				setDestinationS( adcOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// sbc rd, rn, imm
			case 0x2C:
			{
				setDestination( sbcOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// sbcS rd, rn, imm
			case 0x2D:
			{
				setDestinationS( sbcOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// rsc rd, rn, imm
			case 0x2E:
			{
				setDestination( sbcOperator( getDataProcessingImmediateOperand2(), getDataProcessingImmediateOperand1() ) );
				break;
			}
			// rscS rd, rn, imm
			case 0x2F:
			{
				setDestinationS( sbcOperatorS( getDataProcessingImmediateOperand2(), getDataProcessingImmediateOperand1() ) );
				break;
			}
			// tst rd, rn, imm
			case 0x30:
			{
				// S flag (bit 20) not set so NOP
				break;
			}
			// tstP (PSR), rn, imm
			// tst rn, imm
			case 0x31:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// tstP (PSR), rn, imm
					setProcessorStatusRegister( andOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				}
				else
				{
					// tst rn, imm
					(void)andOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2S() );
				}
			}
			// teq rd, rn, imm
			case 0x32:
			{
				// S flag (bit 20) not set so NOP
				break;
			}
			// teqP (PSR), rn, imm
			// teq rn, imm
			case 0x33:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// tstP (PSR), rn, imm
					setProcessorStatusRegister( eorOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				}
				else
				{
					// tst rn, imm
					(void)eorOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2S() );
				}

				break;
			}
			// cmp rd, rn, imm
			case 0x34:
			{
				// S flag (bit 20) not set so NOP
				break;
			}
			// cmpP (PSR), rn, imm
			// cmp rn, imm
			case 0x35:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// cmpP (PSR), rn, imm
					setProcessorStatusRegister( subOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				}
				else
				{
					// cmp rn, imm
					(void)subOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() );
				}
				break;
			}
			// cmn rd, rn, imm
			case 0x36:
			{
				// S flag (bit 20) not set so NOP
				break;
			}
			// cmnP (PSR), rn, imm
			// cmn rn, imm
			case 0x37:
			{
				uint rd = getField(currentInstruction, 12, 15);
				if( rd == 15 )
				{
					// cmnP (PSR), rn, imm
					setProcessorStatusRegister( addOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				}
				else
				{
					// cmn rn, imm
					(void)addOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() );
				}
				break;
			}
			// orr rd, rn, imm
			case 0x38:
			{
				setDestination( orrOperator( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2() ) );
				break;
			}
			// orrS rd, rn, imm
			case 0x39:
			{
				setDestinationS( orrOperatorS( getDataProcessingImmediateOperand1(), getDataProcessingImmediateOperand2S() ) );
				break;
			}
			// mov rd, rn, imm		(rn is ignored)
			case 0x3A:
			{
				// literally just fetch operand and set the destination to it
				setDestination( getDataProcessingImmediateOperand2() );
				break;
			}
			// movS rd, rn, imm
			case 0x3B:
			{
				uint32 value = getDataProcessingImmediateOperand2S();
				updateNZFlags(value);
				setDestinationS( value );
				break;
			}
			// bic rd, rn, imm
			case 0x3C:
			{
				setDestination( andOperator( getDataProcessingImmediateOperand1(), ~getDataProcessingImmediateOperand2() ) );
				break;
			}
			// bicS rd, rn, imm
			case 0x3D:
			{
				setDestinationS( andOperatorS( getDataProcessingImmediateOperand1(), ~getDataProcessingImmediateOperand2S() ) );
				break;
			}
			// mvn rd, rn, imm
			case 0x3E:
			{
				setDestination( ~getDataProcessingImmediateOperand2() );
				break;
			}
			// mvnS rd, rn, imm
			case 0x3F:
			{
				uint32 value = ~getDataProcessingImmediateOperand2S();
				updateNZFlags(value);
				setDestinationS(value);
				break;
			}

			// single data transfer immediate

			// str rd, [rn], -imm
			case 0x40:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint32 storeValue = getRegisterWithPSRAndPipelining( getField(currentInstruction, 12, 15));

				// if str performed ok
				if( performDataTransferStoreWord(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );
				}
				break;
			}
			// ldr rd, [rn], -imm
			case 0x41:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address
				uint32 baseAddress = getRegister( rn );	// get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				uint32 location;
				// if not problems loading word from memory
				if( performDataTransferLoadWord(baseAddress, location) )
				{
					// update base address
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );
					// set register value
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strT rd, [rn], -imm
			case 0x42:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint32 storeValue = getRegisterWithPSRAndPipelining( getField(currentInstruction, 12, 15));

				clearTrans();

				// if str performed ok
				if( performDataTransferStoreWord(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );
				}

				if(processorMode != USR_MODE)
				{
					setTrans();
				}

				break;
			}
			// ldrT rd, [rn], -imm
			case 0x43:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address
				uint32 baseAddress = getRegister( rn );	// get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				clearTrans();

				uint32 location;
				// if not problems loading word from memory
				if( performDataTransferLoadWord(baseAddress, location) )
				{
					// update base address
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );

					if(processorMode != USR_MODE)
					{
						setTrans();
					}

					// set register value
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn], -imm
			case 0x44:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint8 storeValue = (uint8)getRegisterWithPSRAndPipelining(getField(currentInstruction, 12, 15));
				// storeValue get cast to uint8 when written to memory

				// if str performed ok
				if( performDataTransferStoreByte(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );
				}

				break;
			}
			// ldrB rd, [rn], -imm
			case 0x45:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address
				uint32 baseAddress = getRegister( rn );	// get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				uint8 location;
				// if not problems loading word from memory
				if( performDataTransferLoadByte(baseAddress, location) )
				{
					// update base address
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );
					// set register value
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strBT rd, [rn], -imm
			case 0x46:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint8 storeValue = (uint8)getRegisterWithPSRAndPipelining(getField(currentInstruction, 12, 15));

				clearTrans();

				// if str performed ok
				if( performDataTransferStoreByte(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );
				}

				if(processorMode != USR_MODE)
				{
					setTrans();
				}

				break;
			}
			// ldrBT rd, [rn], -imm
			case 0x47:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				clearTrans();

				uint8 location;
				// if str performed ok
				if( performDataTransferLoadByte(baseAddress, location) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress - getDataTransferValueImmediate() );

					if(processorMode != USR_MODE)
					{
						setTrans();
					}

					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// repeats opcodes 0x40 - 0x47 except that immediate offset is added rather
			// than subtracted from the base register

			// str rd, [rn], imm
			case 0x48:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint32 storeValue = getRegisterWithPSRAndPipelining( getField(currentInstruction, 12, 15));

				// if str performed ok
				if( performDataTransferStoreWord(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );
				}
				break;
			}
			// ldr rd, [rn], imm
			case 0x49:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address
				uint32 baseAddress = getRegister( rn );	// get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				uint32 location;
				// if not problems loading word from memory
				if( performDataTransferLoadWord(baseAddress, location) )
				{
					// update base address
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );
					// set register value
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strT rd, [rn], imm
			case 0x4A:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint32 storeValue = getRegisterWithPSRAndPipelining( getField(currentInstruction, 12, 15));

				clearTrans();

				// if str performed ok
				if( performDataTransferStoreWord(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );
				}

				if(processorMode != USR_MODE)
				{
					setTrans();
				}
				break;
			}
			// ldrT rd, [rn], imm
			case 0x4B:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address
				uint32 baseAddress = getRegister( rn );	// get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				clearTrans();

				uint32 location;
				// if not problems loading word from memory
				if( performDataTransferLoadWord(baseAddress, location) )
				{
					// update base address
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );

					if(processorMode != USR_MODE)
					{
						setTrans();
					}

					// set register value
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn], imm
			case 0x4C:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint8 storeValue = (uint8)getRegisterWithPSRAndPipelining(getField(currentInstruction, 12, 15));
				// storeValue get cast to uint8 when written to memory

				// if str performed ok
				if( performDataTransferStoreByte(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );
				}

				break;
			}
			// ldrB rd, [rn], imm
			case 0x4D:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address
				uint32 baseAddress = getRegister( rn );	// get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				uint8 location;
				// if not problems loading word from memory
				if( performDataTransferLoadByte(baseAddress, location) )
				{
					// update base address
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );
					// set register value
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strBT rd, [rn], imm
			case 0x4E:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get value to store from rd
				uint8 storeValue = (uint8)getRegisterWithPSRAndPipelining(getField(currentInstruction, 12, 15));

				clearTrans();

				// if str performed ok
				if( performDataTransferStoreByte(baseAddress, storeValue) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );
				}

				if(processorMode != USR_MODE)
				{
					setTrans();
				}

				break;
			}
			// ldrBT rd, [rn], imm
			case 0x4F:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint32 baseAddress = getRegister( rn ); // get base address from rn
				// get register to load into
				uint rd = getField(currentInstruction, 12, 15);

				clearTrans();

				uint8 location;
				// if str performed ok
				if( performDataTransferLoadByte(baseAddress, location) )
				{
					// then update base register
					setRegisterWithPrefetch(rn, baseAddress + getDataTransferValueImmediate() );

					if(processorMode != USR_MODE)
					{
						setTrans();
					}

					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// str rd, [rn, -imm]
			case 0x50:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreWord(getRegister(rn) - getDataTransferValueImmediate(), (uint8)getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldr rd, [rn, -imm]
			case 0x51:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 location;
				if( performDataTransferLoadWord( getRegister(rn) - getDataTransferValueImmediate(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// str rd, [rn, -imm]!
			// ! means write back the index-adjusted address to the base register
			case 0x52:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) - getDataTransferValueImmediate();
				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldr rd, [rn, -imm]!
			case 0x53:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) - getDataTransferValueImmediate();
				uint32 location;
				if( performDataTransferLoadWord( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, -imm]
			case 0x54:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreByte(getRegister(rn) - getDataTransferValueImmediate(), (uint8)getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldrB rd, [rn, -imm]
			case 0x55:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint8 location;
				if( performDataTransferLoadByte( getRegister(rn) - getDataTransferValueImmediate(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, -imm]!
			case 0x56:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) - getDataTransferValueImmediate();
				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldrB rd, [rn, -imm]!
			case 0x57:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) - getDataTransferValueImmediate();
				uint8 location;
				if( performDataTransferLoadByte( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// repeat of 0x50 - 0x57 but adding rather than subtracting the offset

			// str rd, [rn, imm]
			case 0x58:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreWord(getRegister(rn) + getDataTransferValueImmediate(), getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldr rd, [rn, imm]
			case 0x59:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 location;
				if( performDataTransferLoadWord( getRegister(rn) + getDataTransferValueImmediate(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// str rd, [rn, imm]!
			case 0x5A:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) + getDataTransferValueImmediate();
				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldr rd, [rn, imm]!
			case 0x5B:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) + getDataTransferValueImmediate();
				uint32 location;
				if( performDataTransferLoadWord( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, imm]
			case 0x5C:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreByte(getRegister(rn) + getDataTransferValueImmediate(), (uint8)getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldrB rd, [rn, imm]
			case 0x5D:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint8 location;
				if( performDataTransferLoadByte( getRegister(rn) + getDataTransferValueImmediate(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, imm]!
			case 0x5E:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) + getDataTransferValueImmediate();
				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldrB rd, [rn, imm]!
			case 0x5F:
			{
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) + getDataTransferValueImmediate();
				uint8 location;
				if( performDataTransferLoadByte( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// single data transfer register
			// in every case, if bit 4 is set then throw undefined instruction exception

			// str rd, [rn], -reg
			// implicit write back
			case 0x60:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);
				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
				}
				break;
			}
			// ldr rd, [rn], -reg
			case 0x61:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint32 location;
				if( performDataTransferLoadWord( address, location) )
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strT rd, [rn],-reg
			case 0x62:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);

				clearTrans();

				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
				}
				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// ldrT rd, [rn], -reg
			case 0x63:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint32 location;

				clearTrans();

				if( performDataTransferLoadWord( address, location) )
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
					if( processorMode != USR_MODE )
					{
						setTrans();
					}
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn], -reg
			case 0x64:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);
				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
				}
				break;
			}
			// ldrB rd, [rn], -reg
			case 0x65:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint8 location;
				if( performDataTransferLoadByte( address, location) )
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strBT rd, [rn],-reg
			case 0x66:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);

				clearTrans();

				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
				}
				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// ldrBT rd, [rn], -reg
			case 0x67:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint8 location;

				clearTrans();

				if( performDataTransferLoadByte( address, location) )
				{
					setRegisterWithPrefetch(rn, address - getDataTransferValueRegister() );
					if( processorMode != USR_MODE )
					{
						setTrans();
					}
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// repeat of 0x60-0x67 but incrementing the offset

			// str rd, [rn], reg
			case 0x68:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);
				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
				}
				break;
			}
			// ldr rd, [rn], reg
			case 0x69:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint32 location;
				if( performDataTransferLoadWord( address, location) )
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strT rd, [rn], reg
			case 0x6A:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);

				clearTrans();

				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
				}
				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// ldrT rd, [rn], reg
			case 0x6B:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint32 location;

				clearTrans();

				if( performDataTransferLoadWord( address, location) )
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
					if( processorMode != USR_MODE )
					{
						setTrans();
					}
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn], reg
			case 0x6C:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);
				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
				}
				break;
			}
			// ldrB rd, [rn], reg
			case 0x6D:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint8 location;
				if( performDataTransferLoadByte( address, location) )
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strBT rd, [rn], reg
			case 0x6E:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn);

				clearTrans();

				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
				}
				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// ldrBT rd, [rn],reg
			case 0x6F:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn);
				uint8 location;

				clearTrans();

				if( performDataTransferLoadByte( address, location) )
				{
					setRegisterWithPrefetch(rn, address + getDataTransferValueRegister() );
					if( processorMode != USR_MODE )
					{
						setTrans();
					}
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// str rd, [rn, -reg]
			case 0x70:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreWord(getRegister(rn) - getDataTransferValueRegister(), getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldr rd, [rn, -reg]
			case 0x71:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 location;
				if( performDataTransferLoadWord( getRegister(rn) - getDataTransferValueRegister(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// str rd, [rn, -reg]!
			case 0x72:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) - getDataTransferValueRegister();
				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldr rd, [rn, -reg]!
			case 0x73:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) - getDataTransferValueRegister();
				uint32 location;
				if( performDataTransferLoadWord( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, -reg]
			case 0x74:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreByte(getRegister(rn) - getDataTransferValueRegister(), (uint8)getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldrB rd, [rn, -reg]
			case 0x75:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint8 location;
				if( performDataTransferLoadByte( getRegister(rn) - getDataTransferValueRegister(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, -reg]!
			case 0x76:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) - getDataTransferValueRegister();
				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldrB rd, [rn, -reg]!
			case 0x77:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) - getDataTransferValueRegister();
				uint8 location;
				if( performDataTransferLoadByte( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// repeat of 0x70 - 0x77 but with index added

			// str rd, [rn, reg]
			case 0x78:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreWord(getRegister(rn) + getDataTransferValueRegister(), getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldr rd, [rn, reg]
			case 0x79:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 location;
				if( performDataTransferLoadWord( getRegister(rn) + getDataTransferValueRegister(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// str rd, [rn, reg]!
			case 0x7A:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) + getDataTransferValueRegister();
				if( performDataTransferStoreWord( address, getRegisterWithPSRAndPipelining(rd) ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldr rd, [rn, reg]!
			case 0x7B:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) + getDataTransferValueRegister();
				uint32 location;
				if( performDataTransferLoadWord( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, reg]
			case 0x7C:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				performDataTransferStoreByte(getRegister(rn) + getDataTransferValueRegister(), (uint8)getRegisterWithPSRAndPipelining(rd));
				break;
			}
			// ldrB rd, [rn, reg]
			case 0x7D:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint8 location;
				if( performDataTransferLoadByte( getRegister(rn) + getDataTransferValueRegister(), location) )
				{
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}
			// strB rd, [rn, reg]!
			case 0x7E:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to store
				uint32 address = getRegister(rn) + getDataTransferValueRegister();
				if (performDataTransferStoreByte(address, (uint8)getRegisterWithPSRAndPipelining(rd)))
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// ldrB rd, [rn, reg]!
			case 0x7F:
			{
				if( getBit(currentInstruction, 4) )
				{
					exceptionUndefinedInstruction();
					return;
				}
				uint rn = getField(currentInstruction, 16, 19); // base address register
				uint rd = getField(currentInstruction, 12, 15); // get register to load to
				uint32 address = getRegister(rn) + getDataTransferValueRegister();
				uint8 location;
				if( performDataTransferLoadByte( address, location ) )
				{
					// writeback
					setRegisterWithPrefetch(rn, address);
					setRegisterWithPrefetch(rd, location);
				}
				break;
			}

			// block data transfer

			// notes:
			// number of bits set in register list is * 4 for the number of bytes
			// per register that is to be stored to get the final address
			// syntax:
			// rn is the base address
			// DA - decrement after, DB - decrement before
			// IA - increment after, IB - increment before
			// ! means write back value to rn after data transfer
			// ^ S flag is set meaning repercussions for PSR if r15 involved, see manuals
			// stmDA rn, {list}
			case 0x80:
			{
				uint32 rn = getField(currentInstruction, 16, 19);
				uint32 baseAddress = getRegisterWithPSR(rn);
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2);
				performBlockDataTransferStore(rn, baseAfterTransfer+4, baseAddress);
				break;
			}
			// ldmDA rn, {list}
			case 0x81:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoad(rn, baseAfterTransfer+4, baseAddress);
				break;
			}
			// stmDA rn!, {list}
			case 0x82:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStore(rn, baseAfterTransfer+4, baseAfterTransfer);
				break;
			}
			// ldmDA rn!, {list}
			case 0x83:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoad(rn, baseAfterTransfer+4, baseAfterTransfer);
				break;
			}
			// stmDA rn, {list}^
			case 0x84:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStoreS(rn, baseAfterTransfer+4, baseAddress);
				break;
			}
			// ldmDA rn, {list}^
			case 0x85:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoadS(rn, baseAfterTransfer+4, baseAddress);
				break;
			}
			// stmDA rn!, {list}^
			case 0x86:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStoreS(rn, baseAfterTransfer+4, baseAfterTransfer);
				break;
			}
			// ldmDA rn!, {list}^
			case 0x87:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoadS(rn, baseAfterTransfer+4, baseAfterTransfer);
				break;
			}
			// stmIA rn, {list}
			case 0x88:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferStore(rn, baseAddress, baseAddress);
				break;
			}
			// ldmIA rn, {list}
			case 0x89:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferLoad(rn, baseAddress, baseAddress);
				break;
			}
			// stmIA rn!, {list}
			case 0x8A:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStore(rn, baseAddress, baseAfterTransfer);
				break;
			}
			// ldmIA rn!, {list}
			case 0x8B:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoad(rn, baseAddress, baseAfterTransfer);
				break;
			}
			// stmIA rn, {list}^
			case 0x8C:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferStoreS(rn, baseAddress, baseAddress);
				break;
			}
			// ldmIA rn, {list}^
			case 0x8D:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferLoadS(rn, baseAddress, baseAddress);
				break;
			}
			// stmIA rn!, {list}^
			case 0x8E:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStoreS(rn, baseAddress, baseAfterTransfer);
				break;
			}
			// ldmIA rn!, {list}^
			case 0x8F:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoadS(rn, baseAddress, baseAfterTransfer);
				break;
			}
			// stmDB rn, {list}
			case 0x90:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStore(rn, baseAfterTransfer, baseAddress);
				break;
			}
			// ldmDB rn, {list}
			case 0x91:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoad(rn, baseAfterTransfer, baseAddress);
				break;
			}
			// stmDB rn!, {list}
			case 0x92:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStore(rn, baseAfterTransfer, baseAfterTransfer);
				break;
			}
			// ldmDB rn!, {list}
			case 0x93:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoad(rn, baseAfterTransfer, baseAfterTransfer);
				break;
			}
			// stmDB rn, {list}^
			case 0x94:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStoreS(rn, baseAfterTransfer, baseAddress);
				break;
			}
			// ldmDB rn, {list}^
			case 0x95:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoadS(rn, baseAfterTransfer, baseAddress);
				break;
			}
			// stmDB rn!, {list}^
			case 0x96:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStoreS(rn, baseAfterTransfer, baseAfterTransfer);
				break;
			}
			// ldmDB rn!, {list}^
			case 0x97:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress - (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoadS(rn, baseAfterTransfer, baseAfterTransfer);
				break;
			}
			// stmIB rn, {list}
			case 0x98:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferStore(rn, baseAddress+4, baseAddress);
				break;
			}
			// ldmIB rn, {list}
			case 0x99:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferLoad(rn, baseAddress+4, baseAddress);
				break;
			}
			// stmIB rn!, {list}
			case 0x9A:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStore(rn, baseAddress+4, baseAfterTransfer);
				break;
			}
			// ldmIB rn!, {list}
			case 0x9B:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoad(rn, baseAddress+4, baseAfterTransfer);
				break;
			}
			// stmIB rn, {list}^
			case 0x9C:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferStoreS(rn, baseAddress+4, baseAddress);
				break;
			}
			// ldmIB rn, {list}^
			case 0x9D:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				performBlockDataTransferLoadS(rn, baseAddress+4, baseAddress);
				break;
			}
			// stmIB rn!, {list}^
			case 0x9E:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferStoreS(rn, baseAddress+4, baseAfterTransfer);
				break;
			}
			// ldmIB rn!, {list}^
			case 0x9F:
			{
				uint32 rn = getField(currentInstruction, 16, 19);			// register containing base address
				uint32 baseAddress = getRegisterWithPSR(rn);				// get base address
				uint32 registerList = getField(currentInstruction, 0,15);	// list of regs to transfer
				uint32 baseAfterTransfer = baseAddress + (countSetBits(registerList) << 2); // calculate final address
				performBlockDataTransferLoadS(rn, baseAddress+4, baseAfterTransfer);
				break;
			}

			// branch

			// B PC + offset
			case 0xA0:
			case 0xA1:
			case 0xA2:
			case 0xA3:
			case 0xA4:
			case 0xA5:
			case 0xA6:
			case 0xA7:
			// B PC- offset
			case 0xA8:
			case 0xA9:
			case 0xAA:
			case 0xAB:
			case 0xAC:
			case 0xAD:
			case 0xAE:
			case 0xAF:
			{
				performBranch();
				break;
			}
			// BL PC + offset
			case 0xB0:
			case 0xB1:
			case 0xB2:
			case 0xB3:
			case 0xB4:
			case 0xB5:
			case 0xB6:
			case 0xB7:
			// BL PC- offset
			case 0xB8:
			case 0xB9:
			case 0xBA:
			case 0xBB:
			case 0xBC:
			case 0xBD:
			case 0xBE:
			case 0xBF:
			{
				// store the current PC in link register (r14)
				// -4 accounts for the pipelining not adjusting this value
				setRegister(14, getRegisterWithPSR(15) - 4);
				// perform branch as usual
				performBranch();
				break;
			}

			// co-processor data transfer

			// STC Crd, [rn], -imm
			case 0xC0:
			case 0xC4:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				if( coprocessorDataTransferStore(address) )
				{
					setRegisterWithPrefetch(rn, address - coprocessorDataTransferOffset() );
				}
				break;
			}
			// LDC Crd, [rn], -imm
			case 0xC1:
			case 0xC5:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				if( coprocessorDataTransferLoad(address) )
				{
					setRegisterWithPrefetch(rn, address - coprocessorDataTransferOffset() );
				}
				break;
			}
			// STRC Crd, [rn],-imm
			case 0xC2:
			case 0xC6:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				clearTrans();

				if( coprocessorDataTransferStore(address) )
				{
					setRegisterWithPrefetch(rn, address - coprocessorDataTransferOffset() );
				}

				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// LDRC Crd, [rn], -imm
			case 0xC3:
			case 0xC7:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				clearTrans();

				if( coprocessorDataTransferLoad(address) )
				{
					setRegisterWithPrefetch(rn, address - coprocessorDataTransferOffset() );
				}

				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// STC Crd, [rn], imm
			case 0xC8:
			case 0xCC:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				if( coprocessorDataTransferStore(address) )
				{
					setRegisterWithPrefetch(rn, address + coprocessorDataTransferOffset() );
				}
				break;
			}
			// LDC Crd, [rn], imm
			case 0xC9:
			case 0xCD:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				if( coprocessorDataTransferLoad(address) )
				{
					setRegisterWithPrefetch(rn, address + coprocessorDataTransferOffset() );
				}
				break;
			}
			// STRC Crd, [rn],imm
			case 0xCA:
			case 0xCE:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				clearTrans();

				if( coprocessorDataTransferStore(address) )
				{
					setRegisterWithPrefetch(rn, address + coprocessorDataTransferOffset() );
				}

				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// LDRC Crd, [rn], imm
			case 0xCB:
			case 0xCF:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn);
				clearTrans();

				if( coprocessorDataTransferLoad(address) )
				{
					setRegisterWithPrefetch(rn, address + coprocessorDataTransferOffset() );
				}

				if( processorMode != USR_MODE )
				{
					setTrans();
				}
				break;
			}
			// STC Crd, [rn, -imm]
			case 0xD0:
			case 0xD4:
			{
				uint rn = getField(currentInstruction, 16, 19);
				coprocessorDataTransferStore( getRegister(rn) - coprocessorDataTransferOffset() );
				break;
			}
			// LDC Crd, [rn, -imm]
			case 0xD1:
			case 0xD5:
			{
				uint rn = getField(currentInstruction, 16, 19);
				coprocessorDataTransferLoad( getRegister(rn) - coprocessorDataTransferOffset() );
				break;
			}
			// STC Crd, [rn, -imm]!
			case 0xD2:
			case 0xD6:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn) - coprocessorDataTransferOffset();
				if( coprocessorDataTransferStore(address) )
				{
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// LDC Crd, [rn, -imm]!
			case 0xD3:
			case 0xD7:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn) - coprocessorDataTransferOffset();
				if( coprocessorDataTransferLoad(address) )
				{
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// STC Crd, [rn, imm]
			case 0xD8:
			case 0xDC:
			{
				uint rn = getField(currentInstruction, 16, 19);
				coprocessorDataTransferStore( getRegister(rn) + coprocessorDataTransferOffset() );
				break;
			}
			// LDC Crd, [rn, imm]
			case 0xD9:
			case 0xDD:
			{
				uint rn = getField(currentInstruction, 16, 19);
				coprocessorDataTransferLoad( getRegister(rn) + coprocessorDataTransferOffset() );
				break;
			}
			// STC Crd, [rn, imm]!
			case 0xDA:
			case 0xDE:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn) + coprocessorDataTransferOffset();
				if( coprocessorDataTransferStore(address) )
				{
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}
			// LDC Crd, [rn, imm]!
			case 0xDB:
			case 0xDF:
			{
				uint rn = getField(currentInstruction, 16, 19);
				uint32 address = getRegister(rn) + coprocessorDataTransferOffset();
				if( coprocessorDataTransferLoad(address) )
				{
					setRegisterWithPrefetch(rn, address);
				}
				break;
			}

			// MRC
			// CDO
			case 0xE0:
			case 0xE2:
			case 0xE4:
			case 0xE6:
			case 0xE8:
			case 0xEA:
			case 0xEC:
			case 0xEE:
			{
				if ( getBit(currentInstruction, 4) )
				{
					// MRC
					coprocessorRegisterTransferWrite();
				}
				else
				{
					// CDO
					coprocessorDataOperation();
				}
				break;
			}
			// MCR
			// CDO
			case 0xE1:
			case 0xE3:
			case 0xE5:
			case 0xE7:
			case 0xE9:
			case 0xEB:
			case 0xED:
			case 0xEF:
			{
				if ( getBit(currentInstruction, 4) )
				{
					// MCR
					coprocessorRegisterTransferRead();
				}
				else
				{
					// CDO
					coprocessorDataOperation();
				}
				break;
				break;
			}

			// software interrupt
			case 0xF0:
			case 0xF1:
			case 0xF2:
			case 0xF3:
			case 0xF4:
			case 0xF5:
			case 0xF6:
			case 0xF7:
			case 0xF8:
			case 0xF9:
			case 0xFA:
			case 0xFB:
			case 0xFC:
			case 0xFD:
			case 0xFE:
			case 0xFF:
			{
				/*
				// ??? remove me
				// simplistic SWI faking
				switch( getField(currentInstruction, 0, 23) )
				{
					// OS_NewLine
					case 0x00003:
					{
						TRACE("\n");
						break;
					}
					// OS_WriteI+"*"
					case 0x0012A:
					{
						TRACE("*");
						break;
					}
				}
				*/

				// throw software interrupt exception
				exceptionSoftwareInterrupt();
				break;
			}

			default :	// ??? DISPLAY ERROR MESSAGE - I'VE MISSED OUT AN INSTRUCTION CASE
						WriteLog("ERROR UNKNOWN OPCODE %02x\n", getField(currentInstruction, 20, 27) );
						break;
		} // end instruction decoding switch
	} // end conditional execution

	// handle PC incrementation (happens whether instruction conditionally executed or not)
	// if prefetch invalidated
	if(prefetchInvalid)
	{
		pc = getRegister(15);
		// adjust PC but don't change prefetch
		setRegister( 15, (getRegister(15) + 8) & PC_MASK );

		prefetchInvalid = false;

		if( !readWord(pc, prefetchInstruction) )
		{
			exceptionPrefetchAbort();
			return;
		}
		pc += 4;
	}
	else
	{
		// else if prefetch ok
		pc += 4;
		setRegister( 15, getRegister(15) + 4 );
	}

	// check for interrupts

	if(TubeNMIStatus)
	{
		if (processorMode != FIQ_MODE)
		{
//			WriteLog("Entering FIQ Mode\n");
			exceptionFastInterruptRequest();
		}
	}
	else if(TubeintStatus & (1<<R1))
	{
		if (processorMode != IRQ_MODE)
		{
//			WriteLog("Entering IRQ Mode\n");
			exceptionInterruptRequest();
		}
	}
	else if(TubeintStatus & (1<<R4))
	{
		if (processorMode != IRQ_MODE)
		{
//			WriteLog("Entering IRQ Mode\n");
			exceptionInterruptRequest();
		}
	}
}

//////////////////////////////////////////////////////////////////////
// instruction templates
//////////////////////////////////////////////////////////////////////

// tests for bit patter 1001 in bits 4-7 to determine the difference
// between a data processing instruction and a MUL or SWP instruction
inline bool CArm::isExtendedInstruction(uint32 instruction)
{
	return ((instruction & 0x90) == 0x90);
}

// returns true if we should execute the instruction, false otherwise
inline bool CArm::executeConditionally(uint32 instruction)
{
	// implements a look up table taking the arguments of
	// the current PSR and the instruction's condition field and
	// returning true if the instruction should be executed, false otherwise

	/*
	// Graeme Barnes code from RS.
	static const unsigned short test[16] = {
		0xf0f0,		// EQ,  1111000011110000 x1xx
		0x0f0f,		// NE,	0000111100001111 x0xx
		0xcccc,		// CS,  1100110011001100 xx1x
		0x3333,		// CC,	0011001100110011 xx0x
		0xff00,		// MI,	1111111100000000 1xxx
		0x00ff,		// PL,	0000000011111111 0xxx
		0xaaaa,		// VS,  1010101010101010 xxx1
		0x5555,		// VC,  0101010101010101 xxx0
		0x0c0c,		// HI,  0000110000001100 x01x
		0xf3f3,		// LS,  1111001111110011 xx0x or x1xx
		0xaa55,		// GE,	1010101001010101 1xx1 or 0xx0
		0x55aa,		// LT,	0101010110101010 1xx0 or 0xx1
		0x0a05,		// GT,	0000101000000101 10x1 or 00x0
		0xf5fa,		// LE,  1111010111111010 1xx0 or 0xx1 or x1xx
		0xffff,		// AL,	1111111111111111 1111
		0x0000,		// NV,	0000000000000000 0000
	};

	return getBit(test[instruction>>28], conditionFlags);
	*/

	// ??? my version, untested
	static const bool testCondition[256] = {
		0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,	// eq
		1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,	// ne
		0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,	// cs
		1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,	// cc
		0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,	// mi
		1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,	// pl
		0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,	// vs
		1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,	// vc
		0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,	// hi
		1,1,0,0,1,1,1,1,1,1,0,0,1,1,1,1,	// ls
		1,0,1,0,1,0,1,0,0,1,0,1,0,1,0,1,	// ge
		0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0,	// lt
		1,0,1,0,0,0,0,0,0,1,0,1,0,0,0,0,	// gt
		0,1,0,1,1,1,1,1,1,0,1,0,1,1,1,1,	// le
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// al
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0		// nv
	};

	// index into table is 8 bits;
	// 0-3: processor condition flags NZCV
	// 4-8: instruction condition code
	return testCondition[ ((instruction>>24) & 0xf0) | conditionFlags];
}

// puts the value in the correct register for the current (data processing) instruction
inline void CArm::setDestination(uint32 value)
{
	uint rd = getField(currentInstruction, 12, 15);
	setRegisterWithPrefetch(rd, value);
}

// puts the value in the correct register for the current (data processing) instruction
// and update PSR
inline void CArm::setDestinationS(uint32 value)
{
	uint rd = getField(currentInstruction, 12, 15);

	setRegisterWithPrefetch(rd, value);

	// if destination register is 15 then need to change the PSR from it's value
	if(rd == 15)
		setProcessorStatusRegister(value);
}

inline void CArm::performMul()
{
	// note, mul's rd is in a different bit field to other instructions
	uint rd = getField(currentInstruction, 16, 19);

	// no affect if destination register is r15
	if( rd != 15 )
	{
		uint rm = getField(currentInstruction, 0, 3);
		if(rd == rm)
		{
			setRegister(rd, 0);
		}
		else
		{
			uint rs = getField(currentInstruction, 8, 11);
			uint32 result = getRegisterWithPSRAndPipelining(rm) * getRegister(rs);
			setRegister(rd, result);
		}
	}
	// if rd is 15 then nothing happens
}

inline void CArm::performMulS()
{
	// note, mul's rd is in a different bit field to other instructions
	uint rd = getField(currentInstruction, 16, 19);

	// no affect if destination register is r15
	if( rd != 15 )
	{
		uint rm = getField(currentInstruction, 0, 3);
		if(rd == rm)
		{
			setRegister(rd, 0);
			// clear N flag
			clearConditionFlags(N_FLAG);
			// ensure Z flag set
			setConditionFlags(Z_FLAG);
		}
		else
		{
			uint rs = getField(currentInstruction, 8, 11);
			uint32 result = getRegisterWithPSRAndPipelining(rm) * getRegister(rs);
			setRegister(rd, result);

			// update N and Z flags
			updateNZFlags(result);
		}
	}
	// if rd is 15 then nothing happens
}

inline void CArm::performMla()
{
	// note, mla's rd is in a different bit field to other instructions
	uint rd = getField(currentInstruction, 16, 19);

	// no affect if destination register is r15
	if( rd != 15 )
	{
		// get register number of first multiplicand
		uint rm = getField(currentInstruction, 0, 3);
		// specifying rd and rm as the same as unpredictable results
		if(rd == rm)
		{
			setRegister(rd, 0);
		}
		else
		{
			// get register number of accumulator value
			uint rn = getField(currentInstruction, 12, 15);
			// get register number of second multiplicand
			uint rs = getField(currentInstruction, 8, 11);

			uint32 result = ( getRegisterWithPSRAndPipelining(rm) * getRegister(rs) ) + getRegisterWithPSR(rn);

			setRegister(rd, result);
		}
	}
	// if rd is 15 then nothing happens
}

// very similar to plain MLA
inline void CArm::performMlaS()
{
	// note, mlaS's rd is in a different bit field to other instructions
	uint rd = getField(currentInstruction, 16, 19);

	// no affect if destination register is r15
	if( rd != 15 )
	{
		// get register number of first multiplicand
		uint rm = getField(currentInstruction, 0, 3);
		// specifying rd and rm as the same as unpredictable results
		if(rd == rm)
		{
			setRegister(rd, 0);

			// clear N flag
			clearConditionFlags(N_FLAG);
			// ensure Z flag set
			setConditionFlags(Z_FLAG);
		}
		else
		{
			// get register number of accumulator value
			uint rn = getField(currentInstruction, 12, 15);
			// get register number of second multiplicand
			uint rs = getField(currentInstruction, 8, 11);

			uint32 result = ( getRegisterWithPSRAndPipelining(rm) * getRegister(rs) ) + getRegisterWithPSR(rn);

			updateNZFlags(result);

			setRegister(rd, result);
		}
	}
	// if rd is 15 then nothing happens
}

inline void CArm::performBranch()
{
	// get branch offset
	uint32 offset = getField(currentInstruction, 0, 23);

	// adjust the PC
	// note, offset is stored >>2 since bits 0-1 are always clear (word-aligned memory)
	setRegisterWithPrefetch(15, getRegister(15) + (offset<<2) );

	prefetchInvalid = true;
}

// single data swap for 32bit word
// used to implement an atomic (indivisible) swap function to allow implementation
// of semaphores.
inline void CArm::performSingleDataSwapWord()
{

	uint32 rn = getField(currentInstruction,16,19);	// address in memory to read value from
	// get address to read from and write to
	uint32 address = getRegisterWithPSR(rn);

	// address is valid 26 bit address
	if( isValidAddress(address) )
	{
		uint32 readValue;
		// read value to be read from address
		if( readWord(address, readValue) )
		{
			// get replacement value to be swapped into rn's address
			uint32 rm = getField(currentInstruction,0,3);
			uint32 writeValue = getRegisterWithPSRAndPipelining(rm);

			// write new value back into address
			if( writeWord(address, writeValue) )
			{
				// get register to store value to
				uint32 rd = getField(currentInstruction,12,15);
				// update destination register
				setRegisterWithPrefetch(rd, readValue);
			}
			// failed to write value from rm to address
			else
			{
				exceptionDataAbort();
				return;
			}
		}
		// failed to read value from address in rn
		else
		{
			exceptionDataAbort();
			return;
		}
	}
	// invalid address in rn
	else
	{
		exceptionAddress();
		return;
	}

}

// single data swap for byte
inline void CArm::performSingleDataSwapByte()
{
	uint32 rn = getField(currentInstruction,16,19);	// address in memory to read value from
	// get address to read from and write to
	uint32 address = getRegisterWithPSR(rn);

	// address is valid 26 bit address
	if( isValidAddress(address) )
	{
		uint8 readValue;
		// read value to be read from address
		if( readByte(address, readValue) )
		{
			// get replacement value to be swapped into rn's address
			uint32 rm = getField(currentInstruction,0,3);
			uint8 writeValue = (uint8)getRegisterWithPSRAndPipelining(rm);

			// write new value back into address
			if( writeByte(address, writeValue) )
			{
				// get register to store value to
				uint32 rd = getField(currentInstruction, 12, 15);
				// update destination register
				setRegisterWithPrefetch(rd, readValue);
			}
			// failed to write value from rm to address
			else
			{
				exceptionDataAbort();
				return;
			}
		}
		// failed to read value from address in rn
		else
		{
			exceptionDataAbort();
			return;
		}
	}
	// invalid address in rn
	else
	{
		exceptionAddress();
		return;
	}
}

//////////////////////////////////////////////////////////////////////
// logic routines
//////////////////////////////////////////////////////////////////////

inline uint32 CArm::andOperator(uint32 operand1, uint32 operand2)
{
	return operand1 & operand2;
}

inline uint32 CArm::andOperatorS(uint32 operand1, uint32 operand2)
{
	uint32 result = operand1 & operand2;
	updateNZFlags(result);
	return result;
}

inline uint32 CArm::orrOperator(uint32 operand1, uint32 operand2)
{
	return operand1 | operand2;
}

inline uint32 CArm::orrOperatorS(uint32 operand1, uint32 operand2)
{
	uint32 result = operand1 | operand2;
	updateNZFlags(result);
	return result;
}

inline uint32 CArm::eorOperator(uint32 operand1, uint32 operand2)
{
	return operand1 ^ operand2;
}

inline uint32 CArm::eorOperatorS(uint32 operand1, uint32 operand2)
{
	uint32 result = operand1 ^ operand2;
	updateNZFlags(result);
	return result;
}

inline uint32 CArm::addOperator(uint32 operand1, uint32 operand2)
{
	return operand1 + operand2;
}

inline uint32 CArm::addOperatorS(uint32 operand1, uint32 operand2)
{
	uint32 result = operand1 + operand2;

	updateAddFlags(operand1, operand2, result);

	return result;
}

inline uint32 CArm::adcOperator(uint32 operand1, uint32 operand2)
{
	if( getConditionFlag( C_FLAG) )
	{
		return operand1 + operand2 + 1;
	}
	else
	{
		return operand1 + operand2;
	}
}

inline uint32 CArm::adcOperatorS(uint32 operand1, uint32 operand2)
{
	if( getConditionFlag( C_FLAG) )
	{
		uint32 result = operand1 + operand2 + 1;
		updateAddFlags(operand1, operand2, result);
		return result;
	}
	else
	{
		uint32 result = operand1 + operand2;
		updateAddFlags(operand1, operand2, result);
		return result;
	}
}

inline uint32 CArm::subOperator(uint32 operand1, uint32 operand2)
{
	return operand1 - operand2;
}

inline uint32 CArm::subOperatorS(uint32 operand1, uint32 operand2)
{
	uint32 result = operand1 - operand2;
	updateSubFlags(operand1, operand2, result);
	return result;
}

inline uint32 CArm::sbcOperator(uint32 operand1, uint32 operand2)
{
	// if carry flag set
	if( getConditionFlag( C_FLAG ) )
	{
		return operand1 - operand2;
	}
	else
	{
		// if carry flag not set i.e. borrow has occurred so subtract 1
		return operand1 - operand2 - 1;
	}
}

inline uint32 CArm::sbcOperatorS(uint32 operand1, uint32 operand2)
{
	uint32 result;

	// if carry flag set
	if( getConditionFlag( C_FLAG ) )
	{
		result = operand1 - operand2;
	}
	else
	{
		// if carry flag not set i.e. borrow has occurred so subtract 1
		result = operand1 - operand2 - 1;
	}

	// if operand1 >= operand2 and the top bit of either operand was set
	updateSubFlags(operand1, operand2, result);

	return result;
}

//////////////////////////////////////////////////////////////////////
// barrel shifter routines
//////////////////////////////////////////////////////////////////////

// note, other barrel shifter routines have been moved to the
// TarmacGlobals.h file since they are required in other parts
// of the program and are very generic anyway.

inline uint32 CArm::rrxOperator(uint32 value)
{
	// if the carry flag is set then set the top bit of the value after shifting
	if( getConditionFlag(C_FLAG) )
	{
		return (value>>1) | 0x80000000;
	}
	else
	{
		return value >> 1;
	}
}

//////////////////////////////////////////////////////////////////////
// sub-instruction templates i.e. make templates out of these
//////////////////////////////////////////////////////////////////////

// get operand 1 value for data processing instructions when operand 2 is immediate
inline uint32 CArm::getDataProcessingImmediateOperand1()
{
	return getRegister( getField(currentInstruction, 16, 19) );
}

// get operand 2 value for data processing instructions when operand 2 is immediate
inline uint32 CArm::getDataProcessingImmediateOperand2()
{
	return immediateValue[ getField(currentInstruction, 0, 11) ];
}

// get operand 2 value for data processing instructions when operand 2 is immediate
// and the S flag is set
inline uint32 CArm::getDataProcessingImmediateOperand2S()
{
	uint32 immediate = getField(currentInstruction, 0, 11);
	uint32 shiftValue = getField(currentInstruction, 8, 11);

	// if the barrel shifter is used to encode this immediate
	if(shiftValue)
	{
		// adjust the carry flag appropriately
		if( immediateCarry[immediate] )
			setConditionFlags(C_FLAG);
		else
			clearConditionFlags(C_FLAG);
	}

	return immediateValue[immediate];
}

// get operand 1 value for data processing instructions when operand 2 is register
// pc is 12 above current instruction, not the usual 8
inline uint32 CArm::getDataProcessingRegisterOperand1()
{
	uint regNumber = getField(currentInstruction, 16, 19);

	// if operand1 is r15 and the shift-amount for op2 is register-specified (bit 4)
	// then take account of pipelining fetching this operand late

	if( (regNumber == 15) && getBit(currentInstruction, 4) )
		return getRegisterWithPipelining(15);
	else
		return getRegister(regNumber);
}

// get operand 2 value for data processing instructions when operand 2 is register
inline uint32 CArm::getDataProcessingRegisterOperand2()
{
	// get register number to be shifted
	uint32 rm = getField(currentInstruction, 0, 3);

	// if there's no shift specified (as commonly happens) then don't go through all the steps
	// just return the specified register value
	if( !getField(currentInstruction,4,11) )
		return getRegisterWithPSR(rm);

	// if register-specified shift amount
	if( getBit(currentInstruction, 4) )
	{
		// get register number containing shift amount
		uint rs = getField(currentInstruction, 8, 11);
		// get shift amount (only lowest byte is taken as shift amount)
		uint8 shiftAmount = getRegister(rs) & 0xff;
		// get register value to be shifted
		uint32 regValue = getRegisterWithPSRAndPipelining(rm);

		if(shiftAmount == 0)
		{
			// nothing happens
			return regValue;
		}
		else if(shiftAmount < 32)
		{
			// switch on shift type
			switch( getField(currentInstruction, 5, 6) )
			{
				case LSL : return lslOperator(regValue, shiftAmount); break;
				case LSR : return lsrOperator(regValue, shiftAmount); break;
				case ASR : return asrOperator(regValue, shiftAmount); break;
				case ROR : return rorOperator(regValue, shiftAmount); break;
			}
		}
		else
		{
			// shiftAmount >= 32
			// switch on shift type
			switch( getField(currentInstruction, 5, 6) )
			{
				case LSL :
				{
					return 0; break;
				}
				case LSR :
				{
					return 0; break;
				}
				case ASR :
				{
					return asrOperator(regValue, 31); break;
				}
				case ROR :
				{
					// limit shift amount to 31, no need to be higher
					return rorOperator(regValue, shiftAmount & 0x1f); break;
				}
			}
		}
	}
	else
	{
		// else immediate-specified shift amount
		// get immediate value shift amount
		uint32 shiftAmount = getField(currentInstruction, 7, 11);
		// get register value to be shifted
		uint32 regValue = getRegisterWithPSR(rm);

		if(shiftAmount == 0)
		{
			// switch on shift type
			switch( getField(currentInstruction, 5, 6) )
			{
				case LSL : return regValue; break;
				// note, from ARM3 datasheet, LSR #0 actually encodes LSR #32 !
				case LSR : return 0; break;
				// note, from ARM3 datasheet, ASR #0 actually encodes AS #32 !
				case ASR : return asrOperator(regValue, 31); break;
				case ROR : return rrxOperator(regValue); break;
			}
		}
		else
		{
			// else shiftAmount != 0
			// switch on shift type
			switch( getField(currentInstruction, 5, 6) )
			{
				case LSL : return lslOperator(regValue, shiftAmount); break;
				case LSR : return lsrOperator(regValue, shiftAmount); break;
				case ASR : return asrOperator(regValue, shiftAmount); break;
				case ROR : return rorOperator(regValue, shiftAmount); break;
			}
		}
	}

	// this can never happen since every case already will have returned
	return 0;
}

// get operand 2 value for data processing instructions when operand 2 is register
// and the S flag is set
// affects the Carry flag appropriately
inline uint32 CArm::getDataProcessingRegisterOperand2S()
{
	// get register number to be shifted
	uint32 rm = getField(currentInstruction, 0, 3);

	// if there's no shift specified (as commonly happens) then don't go through all the steps
	// just return the specified register value
	if( !getField(currentInstruction, 4, 11) )
		return getRegisterWithPSR(rm);

	uint32 carry = 0;
	uint32 result = 0;

	// if register-specified shift amount
	if( getBit(currentInstruction, 4) )
	{
		// get register number containing shift amount
		uint rs = getField(currentInstruction, 8, 11);
		// get shift amount
		uint8 shiftAmount = getRegister(rs) & 0xff;

		// get register value to be shifted pipelining effect
		uint32 regValue = getRegisterWithPSRAndPipelining(rm);

		if( shiftAmount == 0 )
		{
			// has no effect
			result = regValue;
		}
		else
		{
			// shift > 32
			if(shiftAmount > 32)
			{
				switch( getField(currentInstruction, 5, 6) )
				{
					case LSL : carry = 0; result = 0; break;
					case LSR : carry = 0; result = 0; break;
					case ASR : carry = getBit(regValue, 31); result = asrOperator(regValue, 31); break;
					case ROR : carry = getBit(regValue, (shiftAmount-1) & 0x1f); result = rorOperator(regValue, shiftAmount & 0x1f); break;
				}
			}
			else if(shiftAmount == 32)
			{
				// else if shiftAmount in range 1 to 32
				switch( getField(currentInstruction, 5, 6) )
				{
					case LSL : carry = getBit(regValue, 0); result = 0; break;
					case LSR : carry = getBit(regValue, 31); result = 0; break;
					case ASR : carry = getBit(regValue, 31); result = asrOperator(regValue, 31); break;
					case ROR : carry = getBit(regValue, 31); result = regValue; break;
				}
			}
			else
			{
				// shift < 32
				switch( getField(currentInstruction, 5, 6) )
				{
					case LSL :	carry = getBit(regValue, 32 - shiftAmount);
								result = lslOperator(regValue, shiftAmount); break;
					case LSR :	carry = getBit(regValue, shiftAmount - 1);
								result = lsrOperator(regValue, shiftAmount); break;
					case ASR :	carry = getBit(regValue, shiftAmount - 1);
								result = asrOperator(regValue, shiftAmount); break;
					case ROR :	carry = getBit(regValue, shiftAmount - 1);
								result = rorOperator(regValue, shiftAmount); break;
				}
			}
		} // end else shiftAmount != 0
	}
	else
	{
		// else immediate-specified shift amount
		// get amount to shift register by
		uint32 shiftAmount = getField(currentInstruction, 7, 11);

		// get register to be shifted
		uint32 regValue = getRegisterWithPSR(rm);

		if(shiftAmount == 0)
		{
			// switch on shift type
			switch( getField(currentInstruction, 5, 6) )
			{
				case LSL:
				{
					// not reached since is same as no shift which is already
					// accounted for in first check
					break;
				}
				case LSR:
				{
					carry = getBit(regValue, 31);
					result = 0;
					break;
				}
				case ASR:
				{
					carry = getBit(regValue, 31);
					result = asrOperator(regValue, 31);
					break;
				}
				// note, ROR with shiftAmount == 0 is RRX
				case ROR:
				{
					carry = getBit(regValue, 0);
					result = rrxOperator(regValue);
					break;
				}
			}
		}
		else
		{
			// else if shiftAmount != 0
			// switch on shift type
			switch( getField(currentInstruction, 5, 6) )
			{
				case LSL :
				{
					carry = getBit(regValue, 32 - shiftAmount);
					result = lslOperator(regValue, shiftAmount);
					break;
				}
				case LSR :
				{
					carry = getBit(regValue, shiftAmount - 1);
					result = lsrOperator(regValue, shiftAmount);
					break;
				}
				case ASR :
				{
					carry = getBit(regValue, shiftAmount - 1);
					result = asrOperator(regValue, shiftAmount);
					break;
				}
				case ROR :
				{
					carry = getBit(regValue, shiftAmount - 1);
					result = rorOperator(regValue, shiftAmount);
					break;
				}
			}
		}
	}

	// set carry flag appropriately
	if(carry)
		setConditionFlags(C_FLAG);
	else
		clearConditionFlags(C_FLAG);

	return result;
}

inline uint32 CArm::getDataTransferValueImmediate()
{
	return getField(currentInstruction, 0, 11);
}

inline uint32 CArm::getDataTransferValueRegister()
{
	uint rm = getField(currentInstruction, 0, 3);
	// if r15 then unpredictable results
	uint32 regValue = getRegisterWithPSR(rm);

	uint shiftAmount = getField(currentInstruction, 7, 11);

	// if the shiftAmount is zero i.e. no shift (includes test for RRX)
	if( shiftAmount == 0 )
	{
		switch( getField(currentInstruction, 5,6) )
		{
			case LSL : break;
			case LSR : regValue = 0; break;
			case ASR : regValue = asrOperator(regValue, 31); break;
			case ROR : regValue = rrxOperator(regValue); break;
		}
	}
	else
	{
		switch( getField(currentInstruction, 5,6) )
		{
			case LSL : regValue = lslOperator(regValue, shiftAmount); break;
			case LSR : regValue = lsrOperator(regValue, shiftAmount); break;
			case ASR : regValue = asrOperator(regValue, shiftAmount); break;
			case ROR : regValue = rorOperator(regValue, shiftAmount); break;
		}
	}
	return regValue;
}


//////////////////////////////////////////////////////////////////////
// memory access
////////////////////////////////////////////////// ////////////////////

inline bool CArm::performDataTransferStoreWord(uint32 address, uint32 data)
{
	// check address is 26 bit
	if( isValidAddress(address) )
	{
		if( !writeWord(address, data) )
		{
			// if can't write word throw data abort
			exceptionDataAbort();
			return false;
		}
		else
		{
			// else word written ok
			return true;
		}
	}
	else
	{
		// if invalid address then throw address exception
		exceptionAddress();
		return false;
	}
}

inline bool CArm::performDataTransferLoadWord(uint32 address, uint32 &destination)
{
	// if address is valid in a 26bit address space
	if( isValidAddress(address) )
	{
		// if unable to read the word from memory
		if( !readWord(address, destination) )
		{
			exceptionDataAbort();
			return false;
		}
		else
		{
			// if word read is not word aligned, the addressed byte is
			// rotated so that it is the LSB. Taken from RS, confirmed
			// in ARM610 datasheet and tested on Risc PC.
			if( address & 3 )
			{
				WriteLog("LoadWord from non word aligned address %08x, pc = %08x\n", address, pc);
				destination = rorOperator(destination, (address & 3)<<3 );
			}

			// loaded ok
			return true;
		}
	}
	else
	{
		exceptionAddress();
		return false;
	}
}

inline bool CArm::performDataTransferStoreByte(uint32 address, uint8 value)
{
	// if address is valid in 26bit address space
	if( isValidAddress(address) )
	{
		if( !writeByte(address, value) )
		{
			// ???
			exceptionDataAbort();
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		exceptionAddress();
		return false;
	}
}

inline bool CArm::performDataTransferLoadByte(uint32 address, uint8 &location)
{
	// if address is valid in 26bit address space
	if( isValidAddress(address) )
	{
		if( !readByte(address, location) )
		{
			exceptionDataAbort();
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		exceptionAddress();
		return false;
	}
}

// return TRUE if address is a maximum of 26 bits
inline bool CArm::isValidAddress(uint32 address)
{
	return ((address & 0xFC000000) == 0 );
}

//////////////////////////////////////////////////////////////////////
// low level interface to rest of memory system
////////////////////////////////////////////////// ////////////////////

inline bool CArm::readWord(uint32 address, uint32& destination)
{
	// read word at address into destination, return TRUE if ok
	uint32 value = 0;

	if (address < 0x1000000)
	{
		value |= ramMemory[(address & 0x3fffff)];
		value |= ramMemory[(address & 0x3fffff) +1]<<8;
		value |= ramMemory[(address & 0x3fffff) +2]<<16;
		value |= ramMemory[(address & 0x3fffff) +3]<<24;
		destination = value;
		return true;
	}

	if ((address & ~0x1f) == 0x1000000)
	{
//		WriteLog("Read word from tube %08x, reg %d\n", address, (address & 0x1c) >> 2);
		destination = 0xff;
		return true;
	}

	if ( (address >= 0x3000000) && (address < 0x3004000) )
	{
		value |= romMemory[(address & 0x3fff)];
		value |= romMemory[(address & 0x3fff) +1]<<8;
		value |= romMemory[(address & 0x3fff) +2]<<16;
		value |= romMemory[(address & 0x3fff) +3]<<24;
		destination = value;
		return true;
	}

	WriteLog("Bad ARM read word from %08x\n", address);
	destination = 0xff;
	return false;
}

inline bool CArm::writeWord(uint32 address, uint32 data)
{
	if (address < 0x1000000)
	{
		ramMemory[(address & 0x3fffff)] = data & 0xff;
		ramMemory[(address & 0x3fffff)+1] = (data >> 8) & 0xff;
		ramMemory[(address & 0x3fffff)+2] = (data >> 16) & 0xff;
		ramMemory[(address & 0x3fffff)+3] = (data >> 24) & 0xff;
		return true;
	}

	if ((address & ~0x1f) == 0x1000000)
	{
//		WriteLog("Write word %08x to tube %08x, reg %d\n", data, address, (address & 0x1c) >> 2);
		return true;
	}

	WriteLog("Bad ARM write word %08x to %08x\n", data, address);
	return false;
}

inline bool CArm::readByte(uint32 address, uint8 &destination)
{
	// read word at address into destination, return TRUE if ok
	uint32 value = 0;

	if (address < 0x1000000)
	{
		value |= ramMemory[(address & 0x3fffff)];
		destination = static_cast<uint8>(value);
		return true;
	}

	if ((address & ~0x1f) == 0x1000000)
	{
		destination = ReadTubeFromParasiteSide((address & 0x1c) >> 2);
//		WriteLog("Read byte from tube %08x, reg %d returned %d\n", address, (address & 0x1c) >> 2, destination);
		return true;
	}

	if (address >= 0x3000000 && address < 0x3004000)
	{
		value |= romMemory[(address & 0x3fff)];
		destination = static_cast<uint8>(value);
		return true;
	}

	WriteLog("Bad ARM read byte from %08x\n", address);
	destination = 0xff;
	return false;
}

inline bool CArm::writeByte(uint32 address, uint8 value)
{
	if (address < 0x1000000)
	{

/*
		if ( ( (value & 0xff) == 0) && (address == 0xc501) )
		{

			uint32 val;
			readWord(0xc500, val);

			WriteLog("Write 0 to 0xc501 - %08x\n", val);
			trace = 10;
		}
*/

		ramMemory[(address & 0x3fffff)] = value & 0xff;

//		if ((value & 0xff) == 255)
//		{
//			WriteLog("Write byte %02x to %08x\n", value, address);
//			trace = 1;
//		}

		return true;
	}

	if ((address & ~0x1f) == 0x1000000)
	{
//		WriteLog("Write byte %02x (%c) to tube %08x, reg %d\n", value,
//			((value & 127) > 31) && ((value & 127) != 127) ? value & 127 : '.',
//			address, (address & 0x1c) >> 2);
        WriteTubeFromParasiteSide((address & 0x1c) >> 2, value);
		return TRUE;
	}

	WriteLog("Bad ARM write byte %02x to %08x\n", value, address);
	return false;
}

// set the TRANS line low so that MEMC does a logical-to-physical translation even when the
// processor is in a supervisor mode
inline void CArm::clearTrans()
{
}

// set the TRANS line high
inline void CArm::setTrans()
{
}

// note, structure for block data transfers largely borrowed from RedSquirrel

// these block data transfer templates take 3 arguments:
//		rn             - register containing base address (so writeback to it can occur)
//		initialAddress - address to start loading to/storing from
//		finalAddress   - last address to be loaded to/storing from
// returns true if successful, false otherwise

// perform STM no writeback
inline bool CArm::performBlockDataTransferStore(uint rn, uint32 initialAddress, uint32 finalAddress)
{
	if(isValidAddress(initialAddress))
	{
		// register list is in bits 0-15 of the current instruction

		// find and store the first register in the list (except r15)
		int index;
		for(index=0; index<15; index++)
		{
			// if found a set bit
			if( getBit(currentInstruction, index) )
			{
				// write word to memory
				if( !writeWord(initialAddress, getRegister(index) ) )
				{
					// ??? rn is highly unlikely to be r15 and the result is unpredictable if it is
					// in event of a data abort, update rn
					if(rn != 15)
						setRegister(rn, finalAddress);

					exceptionDataAbort();
					return false;
				}
				initialAddress += 4;
				break; // get out of loop
			}
		}

		// now first word has been stored, rn is updated
		// ??? again rn is highly unlikely to be r15 and result is unpredictable anyway
		if(rn != 15)
			setRegister(rn, finalAddress);

		// store all the remaining registers if specified
		index++;					// update index to next bit to check
		for( ; index<15; index++)	// intentionally uninitialised
		{
			if( getBit(currentInstruction, index) )
			{
				// write word to memory
				if( !writeWord(initialAddress, getRegister(index) ) )
				{
					exceptionDataAbort();
					return false;
				}
				initialAddress += 4;
			}
		}

		// handle r15 separately
		if( getBit(currentInstruction, 15) )
		{
			if( !writeWord(initialAddress, getRegisterWithPSRAndPipelining(15) ) )
			{
				exceptionDataAbort();
				return false;
			}
		}
		// successful
		return true;
	}
	else
	{
		// do not update base register if it's r15
		// ??? according to the ARM ARM and GB it's unpredictable
		// has no noticable effect on emulation quality
		//if (rn != 15)
		//	setRegister(rn, finalAddress);

		exceptionAddress();
		return false;
	}
}

// STM with S flag set (^)
inline bool CArm::performBlockDataTransferStoreS(uint rn, uint32 initialAddress, uint32 finalAddress)
{
	if(isValidAddress(initialAddress))
	{
		// since we're faking adjusting the processor mode in order to store user mode banked
		// registers, data aborts are thrown at the end of the method once the proper
		// registers have been restored, so just flag them for now.
		bool dataAbortOccurred = false;

		// register list is in bits 0-15 of the current instruction

		// note, from ARM ARM 3-116
		// the S bit indicates that when the processor is in a privileged mode, the user
		// mode banked registers are transferred and not the registers of the current mode.
		uint previousMode = USR_MODE;
		// if r15 is NOT to be stored and we're not in user mode then temporarily change to user mode
		if( !getBit(currentInstruction, 15) && (processorMode != USR_MODE) )
		{
			previousMode = processorMode;
			setProcessorMode(USR_MODE);
		}

		// find and store the first register in the list (except r15)
		int index;
		for(index=0; index<15; index++)
		{
			// if found a set bit
			if( getBit(currentInstruction, index) )
			{
				// write word to memory
				if( !writeWord(initialAddress, getRegister(index) ) )
				{
					dataAbortOccurred = TRUE;
				}
				initialAddress += 4;
				break; // get out of loop
			}
		}

		// now first word has been stored, if not r15 and writeback is selected then rn is updated
		if(rn != 15 && getBit(currentInstruction,21) )
			setRegister(rn, finalAddress);
		// ??? again rn is highly unlikely to be r15 and result is unpredictable anyway

		if(!dataAbortOccurred)
		{
			// update index to next bit to check
			index++;
			for( ; index<15; index++)	// intentionally uninitialised
			{
				if( getBit(currentInstruction, index) )
				{
					// write word to memory
					if( !writeWord(initialAddress, getRegister(index) ) )
					{
						dataAbortOccurred = TRUE;
						break; // get out of loop
					}
					initialAddress += 4;
				}
			}
		}

		// if no data abort yet then handle r15 separately
		if(!dataAbortOccurred && (getBit(currentInstruction, 15)) )
		{
			if( !writeWord(initialAddress, getRegisterWithPSRAndPipelining(15) ) )
			{
				dataAbortOccurred = true;
			}
		}

		// if mode temporarily changed to user mode then change it back again
		if(previousMode != USR_MODE)
			setProcessorMode(previousMode);

		if(dataAbortOccurred)
		{
			exceptionDataAbort();
			return false;
		}

		// otherwise successful
		return true;
	}
	else
	{
		// do not update base register if it's r15
		// ??? according to the ARM ARM and GB it's unpredictable
		// 000
		//if (rn != 15)
		//	setRegister(rn, finalAddress);

		exceptionAddress();
		return false;
	}
}

// LDM no S flag
inline bool CArm::performBlockDataTransferLoad(uint rn, uint32 initialAddress, uint32 finalAddress)
{
	if( isValidAddress(initialAddress) )
	{
		// handle writeback
		//if(rn != 15)
			setRegister(rn, finalAddress);

		// find and load all registers in the list (including r15)
		for(int index=0; index<=15; index++)
		{
			// if found a set bit
			if( getBit(currentInstruction, index) )
			{
				// read appropriate word from memory
				uint32 location;
				if( !readWord(initialAddress, location) )
				{
					// update base address
					if(rn != 15)
						setRegister(rn, finalAddress);

					exceptionDataAbort();

					return false;
				}
				// if load was ok then update value read to appropriate register
				setRegister(index, location);

				initialAddress += 4;
			}
		}

		// if r15 was loaded to then prefetch will have been corrupted
		if( getBit(currentInstruction, 15) )
		{
			setRegister(15, getRegister(15) & PC_MASK);
			prefetchInvalid = true;
		}
		// successful
		return true;
	}
	else
	{
		// do not update base register if it's r15
		// ??? according to the ARM ARM and GB it's unpredictable
		// 000
		//if (rn != 15)
		//	setRegister(rn, finalAddress);

		exceptionAddress();
		return false;
	}
}

// LDM with S flag
// ??? note, many fixes have been made to RedSquirrel in order to run RISC OS
// that vary from the data manuals which are frustratingly vague, these
// have been used in this method and are configurable on or off
inline bool CArm::performBlockDataTransferLoadS(uint rn, uint32 initialAddress, uint32 finalAddress)
{
	const bool fixRISCOS = true;

	if( isValidAddress(initialAddress) )
	{
		// since we're faking adjusting the processor mode in order to store user mode banked
		// registers, data aborts are thrown at the end of the method once the proper
		// registers have been restored, so just flag them for now.
		bool dataAbortOccurred = false;

		if(fixRISCOS)
		{
			// RS - FPEmulator expects writeback to SVC bank
			//if(rn != 15)
				setRegister(rn, finalAddress);
		}

		// note, from ARM ARM 3-116
		// the S bit indicates that when the processor is in a privileged mode, the user
		// mode banked registers are transferred and not the registers of the current mode.
		uint previousMode = USR_MODE;
		// if r15 is NOT to be loaded and we're not in user mode then temporarily change to user mode
		if( !getBit(currentInstruction, 15) && (processorMode != USR_MODE) )
		{
			previousMode = processorMode;
			setProcessorMode(USR_MODE);
		}

		if(fixRISCOS)
		{
			// RS - the writeback should be to the user bank
			if( (rn != 15) && getBit(currentInstruction, 21) )
				setRegister(rn, finalAddress);
		}

		// find and load all registers in the list (including r15)
		for(int index=0; index<=15; index++)
		{
			// if found a set bit
			if( getBit(currentInstruction, index) )
			{
				// read appropriate word from memory
				uint32 location;
				if( !readWord(initialAddress, location) )
				{
					dataAbortOccurred = true;

					if(fixRISCOS)
					{
						// update base address if writeback and not rn != r15
						if( (rn != 15) && getBit(currentInstruction, 21) )
							setRegister(rn, finalAddress);
					}

					break; // get out of loop

				}
				// if load was ok then update value read to appropriate register
				setRegister(index, location);
				initialAddress += 4;
			}
		}

		if( getBit(currentInstruction,15) && !dataAbortOccurred )
		{
			uint32 r15 = getRegister(15);
			setProcessorStatusRegister(r15);
			setRegisterWithPrefetch(15, r15);
		}

		// ???
		/*
		// handle r15 separately
		if(!dataAbortOccurred)
		{
			if(getBit(currentInstruction, 15) )
			{
				uint32 location;
				if( !readWord(initialAddress, location) )
				{
					dataAbortOccurred = TRUE;

					if(fixRISCOS)
					{
						// update base address if writeback and not rn != r15
						if( (rn != 15) && getBit(currentInstruction, 21) )
							setRegister(rn, finalAddress);
					}
				}
				else
				{
					// if load was ok then update value read to appropriate register
					// update PC (which automatically invalidates the prefetch)
					setRegisterWithPrefetch(index, location);
					// update PSR
					setProcessorStatusRegister( location );
					initialAddress += 4;
				}
			}
		}
		*/

		// if changed mode temporarily then change back to USER
		if(previousMode != USR_MODE)
		{
			// RS - this is not as documented but the FPEmulator code appears to rely on it
			setProcessorMode(previousMode);
		}

		if(dataAbortOccurred)
		{
			if(fixRISCOS)
			{
				// ??? removal doesn't seem to have any effect
				//if(rn != 15)
				//	setRegister(rn, finalAddress);
			}

			exceptionDataAbort();
			return false;
		}

		// otherwise successful
		return true;
	}
	else
	{
		// do not update base register if it's r15
		// 000
		//if(rn != 15)
		//	setRegister(rn, finalAddress);

		exceptionAddress();
		return false;
	}
}

//////////////////////////////////////////////////////////////////////
// utility methods
//////////////////////////////////////////////////////////////////////

// returns TRUE if the *unsigned* argument is positive (or 0) when signed
inline bool CArm::getPositive(uint32 value)
{
	return (((int32)value) >= 0);
}

// returns TRUE if the *unsigned* argument is negative when signed
inline bool CArm::getNegative(uint32 value)
{
	return (((int32)value) < 0);
}

// changes the processor mode
inline void CArm::setProcessorMode(uint newMode)
{
	// Each mode has some 'shadow' registers which are only visible in that mode.
	// When changing mode, the current mode's registers have to be stored to
	// the shadow registers and the new mode's registers copied into the current
	// registers to give that impression.

	// note, mode changes should happen seldom enough that it is faster to
	// physically copy the registers on mode changes than it is to use a pointer offset
	// to the appropriate bank of registers for every register access

	if( newMode != processorMode)
	{
		// get pointer to bank of registers for old mode
		uint32 *oldR = curR[processorMode];

		// if the current mode was FIQ (and the new mode isn't), r8-r12 must also be restored
		if( processorMode == FIQ_MODE )
		{
			for(int regNum=8; regNum<13; regNum++)
			{
				fiqR[regNum] = r[regNum];
				r[regNum] = usrR[regNum];
			}
		}

		// switch to handle specific mode's registers
		switch(newMode)
		{
			case USR_MODE:
			{
				// change r13
				oldR[13] = r[13];
				r[13] = usrR[13];
				// change r14
				oldR[14] = r[14];
				r[14] = usrR[14];
				break;
			}
			case FIQ_MODE:
			{
				// change r13
				oldR[13] = r[13];
				r[13] = fiqR[13];
				// change r14
				oldR[14] = r[14];
				r[14] = fiqR[14];

				// save out user r8-r12 and copy in fiq r8-r12
				// no need to check if fiq regs are already visible, as we already know
				// that the previous mode != FIQ_MODE
				for(int regNum=8; regNum<13; regNum++)
				{
					usrR[regNum] = r[regNum];
					r[regNum] = fiqR[regNum];
				}
				break;
			}
			case IRQ_MODE:
			{
				// change r13
				oldR[13] = r[13];
				r[13] = irqR[13];
				// change r14
				oldR[14] = r[14];
				r[14] = irqR[14];
				break;
			}
			case SVC_MODE:
			{
				// change r13
				oldR[13] = r[13];
				r[13] = svcR[13];
				// change r14
				oldR[14] = r[14];
				r[14] = svcR[14];
				break;
			}
		}
		// update current processor mode to new mode
		processorMode = newMode;
	}

	// adjust TRANS line
	if(processorMode != USR_MODE)
	{
		setTrans();
	}
	else
	{
		clearTrans();
	}
}

// returns the PC from r15
inline uint32 CArm::getProgramCounter()
{
	return r[15] & PC_MASK;
}

inline uint32 CArm::getProcessorStatusRegister()
{
	return (conditionFlags << 28) | interruptDisableFlag | fastInterruptDisableFlag | processorMode;
}

// takes an argument which could a register argument and changes the PSR to reflect
// the details in bits 26-31 and 0-1 of that argument
inline void CArm::setProcessorStatusRegister(uint32 value)
{
	// update NZCV
	conditionFlags = value >> 28;

	// processor mode and interrupt flags are not adjusted in user mode
	if( processorMode != USR_MODE )
	{
		// update mode and interrupt flags
		setProcessorStatusFlags( I_FLAG | F_FLAG | M1_FLAG | M2_FLAG, value);
	}
}

// clear and set specific flags in the PSR
inline void CArm::setProcessorStatusFlags(uint32 mask, uint32 value)
{
	// get mode and interrupt flags
	uint32 oldFlags = getProcessorStatusRegister() & ( I_FLAG | F_FLAG | M1_FLAG | M2_FLAG );
	// clear mask and set new bits
	uint32 newFlags = (oldFlags & ~mask) | (value & mask);

	// if flags have changed
	if(oldFlags != newFlags)
	{
		// update processor mode
		setProcessorMode(newFlags & (M1_FLAG | M2_FLAG) );
		// update (fast)interrupt disable flags
		interruptDisableFlag = newFlags & I_FLAG;
		fastInterruptDisableFlag = newFlags & F_FLAG;
	}
}

// set a register to a given value
// if it's r15 then don't update PSR but do invalidate the prefetch
inline void CArm::setRegisterWithPrefetch(uint regNumber, uint32 value)
{
	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsage(regNumber, false);

	// if r15 then only adjust PC (not PSR as well)
	if(regNumber == 15)
	{
		r[15] = value & PC_MASK;
		// prefetched instruction is invalidated
		prefetchInvalid = true;
	}
	else
	{
		r[regNumber] = value;
	}
}

// plain accessor method for registers, adjusts PSR if r15 etc.
inline void CArm::setRegister(uint regNumber, uint32 value)
{
	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsage(regNumber, false);

	r[regNumber] = value;
}

// get the plain value of a register (without psr if r15)
inline uint32 CArm::getRegister(uint regNumber)
{
	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsage(regNumber, true);

	return r[regNumber];
}

// get the value of a register (with psr if r15)
inline uint32 CArm::getRegisterWithPSR(uint regNumber)
{
	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsage(regNumber, true);

	if(regNumber == 15)
	{
		return ( r[15] | getProcessorStatusRegister() );
	}
	else
	{
		return r[regNumber];
	}
}

// get the value of a register (if r15 then +4 for pipelining effect)
inline uint32 CArm::getRegisterWithPipelining(uint regNumber)
{
	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsage(regNumber, true);

	if(regNumber == 15)
	{
		return r[15] + 4;
	}
	else
	{
		return r[regNumber];
	}
}

// get the value of a register (if r15 then with PSR and +4 for pipelining effect)
inline uint32 CArm::getRegisterWithPSRAndPipelining(uint regNumber)
{
	if(dynamicProfilingRegisterUse)
		dynamicProfilingRegisterUsage(regNumber, true);

	if(regNumber == 15)
	{
		return ( r[15] | getProcessorStatusRegister() ) + 4;
	}
	else
	{
		return r[regNumber];
	}
}

// set the appropriate flags
// should be called with for example C_FLAG which is set to 1<<29
// conditionFlags are however stored in bits 0-3
inline void CArm::setConditionFlags(uint32 flagValue)
{
	conditionFlags |= (flagValue >> 28);
}

// clears the appropriate flags
// should be called with for example C_FLAG which is set to 1<<29
inline void CArm::clearConditionFlags(uint32 flagValue)
{
	conditionFlags &= ~(flagValue >> 28);
}

// returns 0 if clear or non-zero if set
inline uint CArm::getConditionFlag(uint32 flagValue)
{
	return conditionFlags & (flagValue >> 28);
}

// updates the N and Z flags according to the value
inline void CArm::updateNZFlags(uint32 value)
{
	clearConditionFlags( N_FLAG | Z_FLAG );

	// check N and Z flags
	if(value == 0)
	{
		setConditionFlags(Z_FLAG);
	}
	else
	{
		if( getNegative(value) )
		{
			setConditionFlags(N_FLAG);
		}
	}
}

inline void CArm::updateSubFlags(uint32 operand1, uint32 operand2, uint32 result)
{
	// all flags need adjusting
	clearConditionFlags( N_FLAG | Z_FLAG | C_FLAG | V_FLAG );

	// carry flag - calculate if there was NOT a borrow from the subtraction
	// logic borrowed from ArcEm, incidentally the same as ARMphetamine and RedSquirrel
	if( ( getNegative(operand1)  && getPositive(operand2) ) ||
		( getNegative(operand1)  && getPositive(result)   ) ||
		( getPositive(operand2) && getPositive(result)    ) )
	{
		setConditionFlags(C_FLAG);
	}

	// if need to set V flag
	if( ( getNegative(operand1) && getPositive(operand2) && getPositive(result) ) ||
		( getPositive(operand1) && getNegative(operand2) && getNegative(result) ) )
	{
		setConditionFlags(V_FLAG);
	}

	// check N and Z flags
	if(result == 0)
	{
		setConditionFlags(Z_FLAG);
	}
	else
	{
		if( getNegative(result) )
		{
			setConditionFlags(N_FLAG);
		}
	}
}

inline void CArm::updateAddFlags(uint32 operand1, uint32 operand2, uint32 result)
{
	// all flags need adjusting
	clearConditionFlags( N_FLAG | Z_FLAG | C_FLAG | V_FLAG );

	// quick check to see if carry or overflow can possibly need updating
	if( (operand1 | operand2) >> 30)
	{
		// logic borrowed from ArcEm, incidentally the same as ARMphetamine and RedSquirrel

		// update carry
		if( ( getNegative(operand1) && getNegative(operand2) ) ||
			( getNegative(operand1) && getPositive(result) )   ||
			( getNegative(operand2) && getPositive(result) )   )
		{
			setConditionFlags( C_FLAG );
		}

		// update overflow
		if( ( getNegative(operand1) && getNegative(operand2) && getPositive(result) ) ||
			( getPositive(operand1) && getPositive(operand2) && getNegative(result) ) )
		{
			setConditionFlags( V_FLAG );
		}
	}

	// check N and Z flags
	if(result == 0)
	{
		setConditionFlags(Z_FLAG);
	}
	else
	{
		if( getNegative(result) )
		{
			setConditionFlags(N_FLAG);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// processor operations
//////////////////////////////////////////////////////////////////////

// hard reset processor
void CArm::reset()
{
	// invalidate prefetch
	prefetchInvalid = true;

	// clear all registers in all banks
	for(int rNum = 0; rNum<16; rNum++)
	{
		r[rNum] = 0;
		usrR[rNum] = 0;
		svcR[rNum] = 0;
		irqR[rNum] = 0;
		fiqR[rNum] = 0;
	}

	setProcessorMode(SVC_MODE);

	interruptDisableFlag = I_FLAG;
	fastInterruptDisableFlag = F_FLAG;

	setRegister(15, 0);
	setConditionFlags(0);
}

//////////////////////////////////////////////////////////////////////
// exceptions
//////////////////////////////////////////////////////////////////////

// general pattern for an exception is to
//
// 1) store the PC and PSR (PC sometimes adjusted -4 for pipelining)
// 2) switch to a different mode (often supervisor)
// 3) disable appropriate interrupts
// 4) set PC to point to the appropriate handling vector

inline void CArm::exceptionReset()
{
	// switch to supervisor mode and disable all interrupts
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG | F_FLAG, SVC_MODE | I_FLAG | F_FLAG );
	// jump to reset vector
	setRegisterWithPrefetch(15, RESET_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("Reset Exception", resetCounter);
}

// note, in each of these cases, calling setProcessorStatusFlags can cahnge the processor
// mode and cause r14 to be altered by swapping in a different modes shadow register
// it is therefore essential that the psr to be set is stored before the processor status flags are
// adjusted

inline void CArm::exceptionUndefinedInstruction()
{
	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG, SVC_MODE | I_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	// adjust PC -4
	setRegister(14, (getRegister(15) - 4) | psrCopy );

	// jump to address exception vector
	setRegisterWithPrefetch(15, UNDEFINED_INSTRUCTION_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("Undefined Instruction Exception", undefCounter);
}

inline void CArm::exceptionSoftwareInterrupt()
{
	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG, SVC_MODE | I_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	// adjust PC -4
	setRegister(14, (getRegister(15) - 4) | psrCopy );

	// jump to address exception vector
	setRegisterWithPrefetch(15, SOFTWARE_INTERRUPT_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("SWI Exception", swiCounter);
}

inline void CArm::exceptionPrefetchAbort()
{
	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG, SVC_MODE | I_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	// adjust PC -4
	setRegister(14, (getRegister(15) - 4) | psrCopy );

	// jump to vector
	setRegisterWithPrefetch(15, PREFETCH_ABORT_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("Prefetch Abort Exception", prefAbortCounter);
}

inline void CArm::exceptionDataAbort()
{
	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG, SVC_MODE | I_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	//  note from RS, PC does not need adjusting - 4
	setRegister(14, getRegister(15) | psrCopy );

	// jump to vector
	setRegisterWithPrefetch(15, DATA_ABORT_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("Data Abort Exception", dataAbortCounter);

}

inline void CArm::exceptionInterruptRequest()
{
	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG, IRQ_MODE | I_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	// adjust PC -4
	setRegister(14, (getRegister(15) - 4) | psrCopy );

	// jump to vector
	setRegisterWithPrefetch(15, INTERRUPT_REQUEST_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("IRQ Exception", irqCounter);
}

inline void CArm::exceptionFastInterruptRequest()
{
	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG | F_FLAG, FIQ_MODE | I_FLAG | F_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	// adjust PC -4
	setRegister(14, (getRegister(15) - 4) | psrCopy );

	// jump to vector
	setRegisterWithPrefetch(15, FAST_INTERRUPT_REQUEST_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("FIQ Exception", fiqCounter);
}

// only occurs on 26 bit architectures
inline void CArm::exceptionAddress()
{

	// make copy of PSR
	uint32 psrCopy = getProcessorStatusRegister();

	// switch to supervisor mode and disable interrupts (not fast interrupts though)
	setProcessorStatusFlags(M1_FLAG | M2_FLAG | I_FLAG, SVC_MODE | I_FLAG);

	// copy PC and PSR from r15 to r14 (link register)
	// adjust PC -4
	setRegister(14, (getRegister(15) - 4) | psrCopy );

	// jump to vector
	setRegisterWithPrefetch(15, ADDRESS_EXCEPTION_VECTOR);

	if(dynamicProfilingExceptionFreq)
		dynamicProfilingExceptionFrequency("Address Exception", addrExcepCounter);
}

//////////////////////////////////////////////////////////////////////
// co-processor operations
//////////////////////////////////////////////////////////////////////

// complete co-processor emulation is outside the scope of the project
// this interface is borrowed from red squirrel so that this class
// can make use of external coprocessor implementations designed for
// use with red squirrel

// copy data from ARM memory to coprocessor memory
inline bool CArm::coprocessorDataTransferStore(uint32 /* address */)
{
	exceptionAddress();
	return false;
}

// copy data from coprocessor memory to ARM memory
inline bool CArm::coprocessorDataTransferLoad(uint32 /* address */)
{
	exceptionAddress();
	return false;
}

// copy from ARM register to coprocessor register
inline bool CArm::coprocessorRegisterTransferWrite()
{
	// uint rd = getField(currentInstruction, 12, 15);

	// uint coprocessorNumber = getField(currentInstruction, 8, 11);

	exceptionUndefinedInstruction();
	return false;
}

// copy from coprocessor register to ARM register
inline bool CArm::coprocessorRegisterTransferRead()
{
	// uint rd = getField(currentInstruction, 12, 15);

	// uint coprocessorNumber = getField(currentInstruction, 8, 11);

	if(dynamicProfilingCoprocessorUse)
		dynamicProfilingCoprocessorUsage(currentInstruction);

	exceptionUndefinedInstruction();
	return false;
}

// perform a data operation (e.g. logic/arithmetic) on the coprocessor
inline bool CArm::coprocessorDataOperation()
{
	// uint coprocessorNumber = getField(currentInstruction, 8, 11);

	if(dynamicProfilingCoprocessorUse)
		dynamicProfilingCoprocessorUsage(currentInstruction);

	exceptionUndefinedInstruction();
	return false;
}

// get the coprocessor data transfer immediate offset from the current instruction
inline uint32 CArm::coprocessorDataTransferOffset()
{
	if(dynamicProfilingCoprocessorUse)
		dynamicProfilingCoprocessorUsage(currentInstruction);

	return (currentInstruction & 0xff) << 2;
}

//////////////////////////////////////////////////////////////////////
// Dynamic Profiling routines, for testing ARM feature usage
//////////////////////////////////////////////////////////////////////

void CArm::dynamicProfilingExceptionFrequency(const char *exceptionName, uint32 &counter)
{
	WriteLog("%s executionCount=%d\n", exceptionName, executionCount - exceptionLastExecutionCount);
	exceptionLastExecutionCount = executionCount;
	counter++;
}

void CArm::dynamicProfilingExceptionFrequencyReport()
{
	WriteLog(">>>>>>>>>> Report Totals\n");
	WriteLog("Reset = %d\n", resetCounter);
	WriteLog("Undefined Instruction = %d\n", undefCounter);
	WriteLog("SWI = %d\n", swiCounter);
	WriteLog("prefetch abort = %d\n", prefAbortCounter);
	WriteLog("data abort = %d\n", dataAbortCounter);
	WriteLog("address exception = %d\n", addrExcepCounter);
	WriteLog("IRQ exception = %d\n", irqCounter);
	WriteLog("FIQ exception = %d\n", fiqCounter);
}

void CArm::dynamicProfilingRegisterUsage(uint regNumber, bool get)
{
	if(get)
	{
		registerGotCounter[regNumber]++;
	}
	else
	{
		registerSetCounter[regNumber]++;
	}
}

void CArm::dynamicProfilingRegisterUsageReport()
{
	WriteLog("Register usage profiling");

	for(int regNumber=0; regNumber<16; regNumber++)
	{
		WriteLog("r%d got=%d set=%d\n", regNumber, registerGotCounter[regNumber], registerSetCounter[regNumber] );
	}
}

void CArm::dynamicProfilingConditionalExe(uint32 instruction)
{
	if( executeConditionally(instruction) )
	{
		// executed
		conditionallyExecuted[ getField(instruction, 28, 31) ]++;
	}
	else
	{
		// not executed
		conditionallyNotExecuted[ getField(instruction, 28, 31) ]++;
	}
}

void CArm::dynamicProfilingConditionalExeReport()
{
	WriteLog("Conditional Execution profiling");

	for(int condition=0; condition<16; condition++)
	{
		WriteLog("cond=%d exe=%d not=%d\n", condition, conditionallyExecuted[condition], conditionallyNotExecuted[condition] );
	}
}

void CArm::dynamicProfilingCoprocessorUsage(uint32 instruction)
{
	WriteLog("copro number=%d instructions=%d\n", getField(instruction, 8, 11), executionCount - lastCopro);
	lastCopro = executionCount;
}

void CArm::SaveState(FILE* SUEF)
{
	UEFWrite32(pc, SUEF);
	UEFWrite8(processorMode, SUEF);
	UEFWrite8(interruptDisableFlag, SUEF);
	UEFWrite8(fastInterruptDisableFlag, SUEF);
	UEFWrite8(conditionFlags, SUEF);
	UEFWrite8(prefetchInvalid, SUEF);
	UEFWrite32(prefetchInstruction, SUEF);
	UEFWrite32(currentInstruction, SUEF);

	for (int i = 0; i < 16; i++)
	{
		UEFWrite32(r[i], SUEF);
	}

	for (int i = 0; i < 16; i++)
	{
		UEFWrite32(usrR[i], SUEF);
	}

	for (int i = 0; i < 16; i++)
	{
		UEFWrite32(svcR[i], SUEF);
	}

	for (int i = 0; i < 16; i++)
	{
		UEFWrite32(irqR[i], SUEF);
	}

	UEFWriteBuf(ramMemory, sizeof(ramMemory), SUEF);
}

void CArm::LoadState(FILE* SUEF)
{
	pc = UEFRead32(SUEF);
	processorMode = UEFRead8(SUEF);
	interruptDisableFlag = UEFRead8(SUEF);
	fastInterruptDisableFlag = UEFRead8(SUEF);
	conditionFlags = UEFRead8(SUEF);
	prefetchInvalid = UEFReadBool(SUEF);
	prefetchInstruction = UEFRead32(SUEF);
	currentInstruction = UEFRead32(SUEF);

	for (int i = 0; i < 16; i++)
	{
		r[i] = UEFRead32(SUEF);
	}

	for (int i = 0; i < 16; i++)
	{
		usrR[i] = UEFRead32(SUEF);
	}

	for (int i = 0; i < 16; i++)
	{
		svcR[i] = UEFRead32(SUEF);
	}

	for (int i = 0; i < 16; i++)
	{
		irqR[i] = UEFRead32(SUEF);
	}

	UEFReadBuf(ramMemory, sizeof(ramMemory), SUEF);
}
