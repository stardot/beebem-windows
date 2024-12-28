/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Mike Wyatt

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

#ifndef UEF_HEADER
#define UEF_HEADER

#include <string>
#include <vector>

#include "TapeMap.h"

#include "zlib/zlib.h"

// Some defines related to the status byte - these may change!
constexpr int UEF_TYPE_MASK = 0x30000;
constexpr int UEF_BYTE_MASK = 0x000ff;

// Some macros for reading parts of the status byte
constexpr int UEF_CARRIER_TONE = 0x00000;
constexpr int UEF_DATA         = 0x10000;
constexpr int UEF_GAP          = 0x20000;
constexpr int UEF_EOF          = 0x30000;

constexpr int           UEFRES_TYPE(int x) { return x & UEF_TYPE_MASK; }
constexpr unsigned char UEFRES_BYTE(int x) { return x & UEF_BYTE_MASK; }

// Some possible return states
enum class UEFResult
{
	Success,
	NotUEF,
	NotTape,
	NoFile,
	ReadFailed,
	WriteFailed,
	EndOfFile
};

struct UEFChunkInfo
{
	int type;
	int len;
	std::vector<unsigned char> data;
	int l1;
	int l2;
	int unlock_offset;
	int crc;
	int start_time;
	int end_time;
};

class UEFTapeImage
{
	public:
		UEFTapeImage();
		~UEFTapeImage();
		UEFTapeImage(const UEFTapeImage&) = delete;
		UEFTapeImage& operator=(const UEFTapeImage&) = delete;

	public:
		void New();
		UEFResult Open(const char *FileName);
		UEFResult Save(const char* FileName);
		void Close();
		void Reset();

		// Setup
		void SetClock(int Speed);
		void SetUnlock(bool Unlock);
		void SetChunkClock();

		// Poll mode
		int GetData(int Time);

		void CreateTapeMap(std::vector<TapeMapEntry>& TapeMap);

		// Writing
		UEFResult PutData(int Data, int Time);
		bool IsModified() const;

	private:
		UEFResult LoadData(const char *FileName);
		const UEFChunkInfo* FindChunk(int Time);
		UEFResult WriteChunk();

	private:
		std::string m_FileName;
		std::vector<UEFChunkInfo> m_Chunks;
		int m_ClockSpeed;
		const UEFChunkInfo *m_LastChunk;
		bool m_Unlock;
		int m_LastPutData;
		bool m_Modified;
		UEFChunkInfo m_Chunk;
};

#endif
