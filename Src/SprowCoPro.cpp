/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  David Sharp
ARM7TDMI Co-Processor Emulator
Copyright (C) 2010 Kieran Mockford

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

#include <map>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "tube.h"

#include "beebmem.h"
#include "armulator.h"
#include "SprowCoPro.h"

std::map<int, int> registers;
DWORD ticks = 0;

static void PutRegister(ARMul_State * state, ARMword registerNumber, ARMword data);
static ARMword PeekRegister(ARMul_State * state, ARMword registerNumber);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSprowCoPro::CSprowCoPro()
{
}

CSprowCoPro::InitResult CSprowCoPro::Init(const char* ROMPath)
{
    FILE *testFile = fopen(ROMPath, "rb");

    if (testFile != nullptr)
    {
        fseek(testFile, 0, SEEK_END);
        long length = ftell(testFile);
        fseek(testFile, 0, SEEK_SET);
        fread(m_ROMMemory, length, 1, testFile);
        fclose(testFile);
    }
    else
    {
        return InitResult::FileNotFound;
    }

    ARMul_EmulateInit();
    m_State = ARMul_NewState();
    m_State->ROMDataPtr = m_ROMMemory;

    ARMul_MemoryInit(m_State, 0x4000000); // 64mb
    ticks = GetTickCount();
    m_CycleCount = 0;

    return InitResult::Success;
}

void CSprowCoPro::Reset()
{
    // ARMul_EmulateInit();
    // m_State = ARMul_NewState();
    m_State->ROMDataPtr = m_ROMMemory;
    ticks = GetTickCount();
    m_State->pc = 0x000;
    m_State->Reg[15] = 0x000;
    ARMul_WriteWord(m_State, RMPCON, 0);
    ARMul_WriteWord(m_State, ROMSEL, 1);
    m_CycleCount = 0;
}

// Execute a number of 64MHz cycles

void CSprowCoPro::Execute(int Cycles)
{
    // check for tube interrupts
    if (TubeNMIStatus && !(m_State->IFFlags & 0x1))
    {
        TubeNMIStatus = 0;
        m_State->NfiqSig = LOW;
        m_State->Exception = TRUE;
    }
    else if (((TubeintStatus & (1 << R4)) || (TubeintStatus & (1 << R1))) && !(m_State->IFFlags & 0x2))
    {
        PutRegister(m_State, IRN, INT_EX3);
        m_State->NirqSig = LOW;
        m_State->Exception = TRUE;
    }

    m_CycleCount += Cycles;

    while (m_CycleCount > 0)
    {
        ARMul_DoInstr(m_State);

        m_State->Exception = FALSE;
        m_State->NirqSig = HIGH;
        m_State->NfiqSig = HIGH;

        // TODO: This is not accurate, it assumes instructions take 3 cycles each,
        // on average. More info at:
        // https://developer.arm.com/documentation/ddi0210/c/Instruction-Cycle-Timings

        m_CycleCount -= 3;
    }
}

CSprowCoPro::~CSprowCoPro()
{
    if (m_State != nullptr)
    {
        ARMul_MemoryExit(m_State);
        m_State = nullptr;
    }
}

#ifdef VALIDATE			/* for running the validate suite */
#define TUBE 48 * 1024 * 1024	/* write a char on the screen */
#define ABORTS 1
#endif

/* #define ABORTS */

#ifdef ABORTS			/* the memory system will abort */
/* For the old test suite Abort between 32 Kbytes and 32 Mbytes
For the new test suite Abort between 8 Mbytes and 26 Mbytes */
/* #define LOWABORT 32 * 1024
#define HIGHABORT 32 * 1024 * 1024 */
#define LOWABORT 8 * 1024 * 1024
#define HIGHABORT 26 * 1024 * 1024
#endif

#define OFFSETBITS 0xffff
#define INSN_SIZE 4

#define XOS_Bit                        0x020000
#define OS_ReadMonotonicTime           0x000042

int SWI_vector_installed = FALSE;
int tenval = 1;
#define PRIMEPIPE     4
#define RESUME        8
#define FLUSHPIPE state->NextInstr |= PRIMEPIPE
unsigned
ARMul_OSHandleSWI (ARMul_State * state, ARMword number)
{
    // OS_ReadMonotonicTime
    // Normally this is handled by the CoPro by one of the system
    // timers. However we'll handle it directly here for accuracy
    if (number == OS_ReadMonotonicTime || number == XOS_Bit + OS_ReadMonotonicTime)
    {
        DWORD currentTicks = GetTickCount();
        state->Reg[0] = (currentTicks - ticks) / 10;
        return TRUE;
    }

    if (SWI_vector_installed)
    {
        ARMword cpsr = ARMul_GetCPSR (state);
        ARMword i_size = INSN_SIZE;

        ARMul_SetSPSR (state, SVC32MODE, cpsr);

        cpsr &= ~0xbf;
        cpsr |= SVC32MODE | 0x80;
        ARMul_SetCPSR (state, cpsr);

        state->RegBank[SVCBANK][14] = state->Reg[14] = state->Reg[15] - i_size;
        state->NextInstr            = RESUME;
        state->Reg[15]              = state->pc = ARMSWIV;
        FLUSHPIPE;
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

static ARMword PeekRegister(ARMul_State * /* state */, ARMword registerNumber)
{
    std::map<int,int>::iterator it = registers.find(registerNumber);

    if (it == registers.end())
    {
        return 0;
    }
    else
    {
        return it->second;
    }
}

static ARMword GetRegister(ARMul_State * state, ARMword registerNumber)
{
    int data = PeekRegister(state, registerNumber);

    switch(registerNumber)
    {
    case IRN:
        {
            registers[registerNumber] = 0;
            registers[CIL] = 0;
            break;
        }
    }

    return data;
}

/***************************************************************************\
*        Get a Word from Virtual Memory, maybe allocating the page          *
\***************************************************************************/

static ARMword
GetWord (ARMul_State * state, ARMword address, int /* check */)
{
    unsigned char *offset_address;
    unsigned char *base_address;

    // All fetches are word-aligned, caller rearranges bytes as needed
    address &= ~(ARMword)3;

    // Hardware Registers..
    if (address >= 0x78000000 && address < 0xc0000000)
    {
        return GetRegister(state, address);
    }

    if (address >= 0xF0000000 && address <= 0xF0000010)
    {
        return 0xFF;
    }

    // If the ROM has been selected to appear at 0x00000000 then
    // we need to ensure that accesses go to the ROM
    if (state->romSelectRegister & 1 && ((state->remapControlRegister & 8) == 0))
    {
        base_address = state->ROMDataPtr;
    }
    else
    {
        base_address = state->MemDataPtr;
    }

    if (address <= state->MemSize) // RAM
    {
        offset_address = (base_address + address);
    }
    else if (address >= 0xC8000000 && address <= 0xC8080000) // Always ROM
    {
        address = address - 0xC8000000;
        offset_address = (state->ROMDataPtr + address);
    }
    else if (address >= 0xC0000000 && address < 0xD0000000)
    {
        unsigned int wrapaddress = address - 0xC0000000;
        offset_address = state->MemDataPtr + (wrapaddress % (unsigned int)state->MemSize);
    }
    else
    {
        // Where are we??
        return 0;
    }

    ARMword *actual_address = (ARMword*)offset_address;
    return actual_address[0];
}

static void PutRegister(ARMul_State * state, ARMword registerNumber, ARMword data)
{
    registers[registerNumber] = data;

    switch (registerNumber)
    {
    case FIQEN:
        {
            if (data == 0)
            {
                state->NfiqSig = HIGH;
            }
            else
            {
                state->NfiqSig = LOW;
            }
            break;
        }
    case ILC:
        {
            if ((data & 0xF0000000) > 0)
            {
                state->NirqSig = HIGH; // enabled
            }
            else if ((data & 0xF0000000) == 0)
            {
                state->NirqSig = LOW; // disabled
            }
            break;
    case CILCL:
        {
            if (data == 0)
            {
                PutRegister(state, CIL, 0);
            }
            break;
        }
        }
    }
}

/***************************************************************************\
*        Put a Word into Virtual Memory, maybe allocating the page          *
\***************************************************************************/

static void
PutWord (ARMul_State * state, ARMword address, ARMword data, int /* check */)
{
    /*
    0xB7000004 Block clock control register BCKCTL R/W 16 0x0000
    0xB8000004 Clock stop register CLKSTP R/W 32 0x00000000
    0xB8000008 Clock select register CGBCNT0 R/W 32 0x00000000
    0xB800000C Clock wait register CKWT R/W 32 0x000000FF
    */

    // All stores are word-aligned and unrotated
    address &= ~(ARMword)3;

    if (address >= 0xF0000000 && address <= 0xF0000010)
    {
        return;
    }

    if (address == RMPCON) // Remap control register
    {
        state->remapControlRegister = data;
    }
    else if (address == ROMSEL) //ROM select register
    {
        state->romSelectRegister = data;
    }
    else if (address >= 0x78000000 && address < 0xc0000000)
    {
        PutRegister(state, address, data);
    }
    else
    {
        unsigned char *base_address;
        unsigned char *offset_address;
        ARMword *actual_address;

             // If the ROM has been selected to appear at 0x00000000 then
        // we need to ensure that accesses go to the ROM
        if (state->romSelectRegister & 1 && ((state->remapControlRegister & 8) == 0))
        {
            base_address = state->ROMDataPtr;
        }
        else
        {
            base_address = state->MemDataPtr;
        }

        if (address <= state->MemSize) // RAM
        {
            if (state->romSelectRegister & 1 && ((state->remapControlRegister & 8) == 0)) // we're trying to write to ROM - NOPE!
            {
                return;
            }
            else
            {
                offset_address = (base_address + address);

                if (address == 0x8)
                {
                    SWI_vector_installed = TRUE;
                }

                /* if (address <= 0x1c)
                {
                    int a= 0;
                    // WHAT!?
                } */
            }
        }
        else if (address >= 0xC0000000 && address < 0xC8000000)
        {
            unsigned int wrapaddress = address - 0xC0000000;
            offset_address = state->MemDataPtr + (wrapaddress % (unsigned int)state->MemSize);
        }
        else
        {
            // int a = 0; // where are we??
            return;
        }

        actual_address = (ARMword*)offset_address;
        actual_address[0] = data;
    }
}

/***************************************************************************\
*                      Initialise the memory interface                      *
\***************************************************************************/

unsigned
ARMul_MemoryInit (ARMul_State * state, unsigned long initmemsize)
{
    if (initmemsize)
        state->MemSize = initmemsize;

    unsigned char *memory = (unsigned char *)malloc(initmemsize);

    if (memory == nullptr)
        return FALSE;

    state->MemDataPtr = memory;

    return TRUE;
}

/***************************************************************************\
*                         Remove the memory interface                       *
\***************************************************************************/

void
ARMul_MemoryExit (ARMul_State * state)
{
    unsigned char* memory = state->MemDataPtr;

    free(memory);
}

/***************************************************************************\
*                   ReLoad Instruction                                     *
\***************************************************************************/

ARMword
ARMul_ReLoadInstr (ARMul_State * state, ARMword address, ARMword isize)
{
#ifdef ABORTS
    if (address >= LOWABORT && address < HIGHABORT)
    {
        ARMul_PREFETCHABORT (address);
        return ARMul_ABORTWORD;
    }
    else
    {
        ARMul_CLEARABORT;
    }
#endif

    if ((isize == 2) && (address & 0x2))
    {
        /* We return the next two halfwords: */
        ARMword lo = GetWord (state, address, FALSE);
        ARMword hi = GetWord (state, address + 4, FALSE);

        if (state->bigendSig == HIGH)
            return (lo << 16) | (hi >> 16);
        else
            return ((hi & 0xFFFF) << 16) | (lo >> 16);
    }

    return GetWord (state, address, TRUE);
}

/***************************************************************************\
*                   Load Instruction, Sequential Cycle                      *
\***************************************************************************/

ARMword ARMul_LoadInstrS (ARMul_State * state, ARMword address, ARMword isize)
{
    state->NumScycles++;

#ifdef HOURGLASS
    if ((state->NumScycles & HOURGLASS_RATE) == 0)
    {
        HOURGLASS;
    }
#endif

    return ARMul_ReLoadInstr (state, address, isize);
}

/***************************************************************************\
*                 Load Instruction, Non Sequential Cycle                    *
\***************************************************************************/

ARMword ARMul_LoadInstrN (ARMul_State * state, ARMword address, ARMword isize)
{
    state->NumNcycles++;

    return ARMul_ReLoadInstr (state, address, isize);
}

/***************************************************************************\
*                      Read Word (but don't tell anyone!)                   *
\***************************************************************************/

ARMword ARMul_ReadWord (ARMul_State * state, ARMword address)
{
#ifdef ABORTS
    if (address >= LOWABORT && address < HIGHABORT)
    {
        ARMul_DATAABORT (address);
        return ARMul_ABORTWORD;
    }
    else
    {
        ARMul_CLEARABORT;
    }
#endif

    return GetWord (state, address, TRUE);
}

/***************************************************************************\
*                        Load Word, Sequential Cycle                        *
\***************************************************************************/

ARMword ARMul_LoadWordS (ARMul_State * state, ARMword address)
{
    state->NumScycles++;

    return ARMul_ReadWord (state, address);
}

/***************************************************************************\
*                      Load Word, Non Sequential Cycle                      *
\***************************************************************************/

ARMword ARMul_LoadWordN (ARMul_State * state, ARMword address)
{
    state->NumNcycles++;

    return ARMul_ReadWord (state, address);
}

/***************************************************************************\
*                     Load Halfword, (Non Sequential Cycle)                 *
\***************************************************************************/

ARMword ARMul_LoadHalfWord (ARMul_State * state, ARMword address)
{
    state->NumNcycles++;

    ARMword temp = ARMul_ReadWord (state, address);
    ARMword offset = (((ARMword) state->bigendSig * 2) ^ (address & 2)) << 3;	/* bit offset into the word */

    return (temp >> offset) & 0xffff;
    //ARMword temp = ARMul_ReadWord (state, address);
    //return temp & 0xFFFF;
}

/***************************************************************************\
*                      Read Byte (but don't tell anyone!)                   *
\***************************************************************************/

ARMword ARMul_ReadByte (ARMul_State * state, ARMword address)
{
    if (address >= 0xF0000000 && address <= 0xF0000010)
    {
        if (address == 0xF0000010)
        {
            return tenval;
        }
        else
        {
            unsigned char addr = (address & 0x0f) / 2;
            unsigned char data = ReadTubeFromParasiteSide(addr);
            return data;
        }
    }

    ARMword temp = ARMul_ReadWord (state, address);
    ARMword offset = (((ARMword) state->bigendSig * 3) ^ (address & 3)) << 3;	/* bit offset into the word */

    return (temp >> offset & 0xffL);
    //return temp & 0xffL;
}

/***************************************************************************\
*                     Load Byte, (Non Sequential Cycle)                     *
\***************************************************************************/

ARMword ARMul_LoadByte (ARMul_State * state, ARMword address)
{
    state->NumNcycles++;

    return ARMul_ReadByte (state, address);
}

/***************************************************************************\
*                     Write Word (but don't tell anyone!)                   *
\***************************************************************************/

void
ARMul_WriteWord (ARMul_State * state, ARMword address, ARMword data)
{
#ifdef ABORTS
    if (address >= LOWABORT && address < HIGHABORT)
    {
        ARMul_DATAABORT (address);
        return;
    }
    else
    {
        ARMul_CLEARABORT;
    }
#endif

    PutWord (state, address, data, TRUE);
}

/***************************************************************************\
*                       Store Word, Sequential Cycle                        *
\***************************************************************************/

void
ARMul_StoreWordS (ARMul_State * state, ARMword address, ARMword data)
{
    state->NumScycles++;

    ARMul_WriteWord (state, address, data);
}

/***************************************************************************\
*                       Store Word, Non Sequential Cycle                        *
\***************************************************************************/

void
ARMul_StoreWordN (ARMul_State * state, ARMword address, ARMword data)
{
    state->NumNcycles++;

    ARMul_WriteWord (state, address, data);
}

/***************************************************************************\
*                    Store HalfWord, (Non Sequential Cycle)                 *
\***************************************************************************/

void
ARMul_StoreHalfWord (ARMul_State * state, ARMword address, ARMword data)
{
    ARMword temp;

    state->NumNcycles++;

#ifdef VALIDATE
    if (address == TUBE)
    {
        if (data == 4)
            state->Emulate = FALSE;
        else
            (void) putc ((char) data, stderr);	/* Write Char */
        return;
    }
#endif

    temp = ARMul_ReadWord (state, address);
    ARMword offset = (((ARMword) state->bigendSig * 2) ^ (address & 2)) << 3;	/* bit offset into the word */
    PutWord (state, address,
        (temp & ~(0xffffL << offset)) | ((data & 0xffffL) << offset),
        TRUE);
    //temp = ARMul_ReadWord (state, address);
    //temp = temp & 0xFFFF0000;
    //temp = temp | (data & 0xFFFF);
    //PutWord (state, address, temp, TRUE);
}

/***************************************************************************\
*                     Write Byte (but don't tell anyone!)                   *
\***************************************************************************/

void
ARMul_WriteByte (ARMul_State * state, ARMword address, ARMword data)
{
    if (address >= 0xF0000000 && address <= 0xF0000010)
    {
        if (address == 0xF0000010)
        {
            tenval = data;
            return;
        }
        else
        {
            unsigned char addr = (address & 0x0f) / 2;
            WriteTubeFromParasiteSide(addr, (unsigned char)data);
            return;
        }
    }

    ARMword temp = ARMul_ReadWord (state, address);
    ARMword offset = (((ARMword) state->bigendSig * 3) ^ (address & 3)) << 3;	/* bit offset into the word */

    PutWord (state, address,
        (temp & ~(0xffL << offset)) | ((data & 0xffL) << offset),
        TRUE);
    //temp = temp & 0xFFFFFF00;
    //temp = temp | (data & 0xFF);
    //PutWord (state, address, temp, TRUE);
}

/***************************************************************************\
*                    Store Byte, (Non Sequential Cycle)                     *
\***************************************************************************/

void
ARMul_StoreByte (ARMul_State * state, ARMword address, ARMword data)
{
    state->NumNcycles++;

#ifdef VALIDATE
    if (address == TUBE)
    {
        if (data == 4)
            state->Emulate = FALSE;
        else
            (void) putc ((char) data, stderr);	/* Write Char */
        return;
    }
#endif

    ARMul_WriteByte (state, address, data);
}

/***************************************************************************\
*                   Swap Word, (Two Non Sequential Cycles)                  *
\***************************************************************************/

ARMword ARMul_SwapWord (ARMul_State * state, ARMword address, ARMword data)
{
    state->NumNcycles++;

    ARMword temp = ARMul_ReadWord (state, address);

    state->NumNcycles++;

    PutWord (state, address, data, TRUE);

    return temp;
}

/***************************************************************************\
*                   Swap Byte, (Two Non Sequential Cycles)                  *
\***************************************************************************/

ARMword ARMul_SwapByte (ARMul_State * state, ARMword address, ARMword data)
{
    ARMword temp = ARMul_LoadByte(state, address);
    ARMul_StoreByte (state, address, data);

    return temp;
}

/***************************************************************************\
*                             Count I Cycles                                *
\***************************************************************************/

void
ARMul_Icycles (ARMul_State * state, unsigned number, ARMword /* address */)
{
    state->NumIcycles += number;
    ARMul_CLEARABORT;
}

/***************************************************************************\
*                             Count C Cycles                                *
\***************************************************************************/

void
ARMul_Ccycles (ARMul_State * state, unsigned number, ARMword /* address */)
{
    state->NumCcycles += number;
    ARMul_CLEARABORT;
}


/* Read a byte.  Do not check for alignment or access errors.  */

ARMword
ARMul_SafeReadByte (ARMul_State * state, ARMword address)
{
    ARMword temp = GetWord (state, address, FALSE);
    ARMword offset = (((ARMword) state->bigendSig * 3) ^ (address & 3)) << 3;

    return (temp >> offset & 0xffL);
}

void
ARMul_SafeWriteByte (ARMul_State * state, ARMword address, ARMword data)
{
    ARMword temp = GetWord (state, address, FALSE);
    ARMword offset = (((ARMword) state->bigendSig * 3) ^ (address & 3)) << 3;

    PutWord (state, address,
        (temp & ~(0xffL << offset)) | ((data & 0xffL) << offset),
        FALSE);
}
