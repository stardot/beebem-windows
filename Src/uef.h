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
constexpr int UEF_MMASK    = (3 << 16);
constexpr int UEF_BYTEMASK = 0xff;

// Some macros for reading parts of the status byte
constexpr int UEF_CARRIER_TONE = (0 << 16);
constexpr int UEF_DATA         = (1 << 16);
constexpr int UEF_GAP          = (2 << 16);
constexpr int UEF_EOF          = (3 << 16);

constexpr int           UEFRES_TYPE(int x) { return x & UEF_MMASK;    }
constexpr unsigned char UEFRES_BYTE(int x) { return x & UEF_BYTEMASK; }

// Some possible return states
enum class UEFResult
{
	Success,
	NotUEF,
	NotTape,
	NoFile
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

class UEFFileWriter
{
	public:
		UEFFileWriter();
		~UEFFileWriter();

	public:
		UEFResult Open(const char *FileName);
		UEFResult PutData(int Data, int Time);
		void Close();

	private:
		UEFResult WriteChunk();

	private:
		std::string m_FileName;
		gzFile m_OutputFile;
		int m_LastPutData;
		UEFChunkInfo m_Chunk;
};

class UEFFileReader
{
	public:
		UEFFileReader();
		~UEFFileReader();

	public:
		UEFResult Open(const char *FileName);
		void Close();

		// Setup
		void SetClock(int Speed);
		void SetUnlock(bool Unlock);

		// Poll mode
		int GetData(int Time);

		void CreateTapeMap(std::vector<TapeMapEntry>& TapeMap);

	private:
		UEFResult LoadData(const char *FileName);
		UEFChunkInfo* FindChunk(int Time);

	private:
		std::string m_FileName;
		std::vector<UEFChunkInfo> m_Chunks;
		int m_ClockSpeed;
		UEFChunkInfo *m_LastChunk;
		bool m_Unlock;
};

#endif
