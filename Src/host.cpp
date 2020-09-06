/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch

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

// Host emulator trap opcode support for BeebEm
// 20-Aug-2016 JGH: Initial layout, OSGBPB 8,9
// 25-Sep-2016 JGH: pathtrans, OSFILE load,save
// 26-Sep-2016 JGH: OSFIND, OSBGET, OSBPUT, OSARGS, EOF, OSGBPB 1-4
// 28-Sep-2016 JGH: Translate Russell format BASIC files on loading
// 29-Sep-2016 JGH: Testing, debugging
// 30-Sep-2016 JGH: fileinfo, read .inf, read file header
// 05-Oct-2016 JGH: */name *command *RUN
// 22-Oct-2016 JGH: Filing system commands parsed, *DIR, *DELETE, *CDIR implemented
//             Commands must be space separated, eg '*DIR$' must be '*DIR $'
// 23-Oct-2016 JGH: Read/write/run 65c02 CoPro memory
// 07-Jan-2017 JGH: writing fileinfo done
// 12-Jan-2017 JGH: fixed bug in creating new .inf file
// 18-Jan-2017 JGH: OSGBPB returns Cy at EOF and truncates read, updates Addr/Offset
//             Read/write Z80 memory done, ARM memory not yet.
// To do: *COPY *MOUNT *RENAME *OPT BOOT
//        Memory read/write to ARM, 80x86 copro
// Bugs:  *MOVE selects memory mapping that causes emulator to crash
//        For the moment, this is checked for an returns an error to emulated machine
//
// There are two sets of 'emulator traps' to call the host system, the
// Acorn traps, opcodes &x3, and the Warm Silence traps, &03 and &23.
// While running 6502 code can work out which traps are available, it
// it not possible for the emulator to work out which should be provided.
// Consequently, it will have to be a run-time option.
//
// Acorn traps are executed inline, execution continues after the trap.
// Warm Silence traps execute an RTS as part of the call, execution continues
// at the previous JSR level.

#include <stdio.h>
#include <stdlib.h>
#include <shlobj.h>
#include "6502core.h"
#include "tube.h"
#include "main.h"
#include "host.h"
#include "beebwin.h"
#include "beebmem.h"
#include "z80mem.h"
// #include "arm.h"

extern unsigned char StackReg, PSR;
extern int Accumulator, XReg, YReg;
int XYReg, CommandLine;
DWORD EmulatorTrap = 0; // b0=Acorn, b1=WSS, b4=TransBasic
FILE *handles[16] = {
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};
#define VDFS_HANDMIN 0xF0
#define VDFS_HANDMAX 0xFF

#ifdef WIN32
#include <windows.h>
WIN32_FIND_DATA infobuf; // Block to read file info
SYSTEMTIME infotime;
HANDLE hFind; // Handle to search directory
#endif

// Do an RTS (buglet, can't work out how to call PopWord)
void rts() {
	StackReg++;
	ProgramCounter=WholeRam[0x100+StackReg];
	StackReg++;
	ProgramCounter=(ProgramCounter|((WholeRam[0x100+StackReg])<<8))+1;
}


// Read/write program memory, to/from files
// ========================================

// Generate an error - string must be < 253 characters
// ***NOTE*** Ensure you use the correct error numbers

int host_error(int num, const char* string) {
	WholeRam[0x100] = 0; // BRK
	WholeRam[0x101] = (char)num; // Error number
	strcpy((char *)(WholeRam+0x102), string); // Error string, &00
	ProgramCounter = 0x100; // Jump to execute BRK
	return -1;
}

// Copy byte to BBC memory, return updated address

int host_bytewr(int addr, char b) {
	if(WholeRam[0x27a]==0) addr|=0xff000000; // No Tube
	if((addr & 0xff000000) == 0xff000000) { // I/O memory
			WholeRam[(addr++) & 0xffff] = b; // Write byte to memory
			// Should be WritePaged() ?
	} else {
		if (TubeType == Tube::Acorn65C02) TubeRam[(addr++) & 0xffff] = b;
		if (TubeType == Tube::TorchZ80) z80_ram[(addr++) & 0xffff] = b;
		if (TubeType == Tube::AcornZ80) z80_ram[(addr++) & 0xffff] = b;
		// if (TubeType == Tube::AcornArm) ramMemory[addr++] = b;
		// if (TubeType == Tube::Master512CoPro)  Tube186[addr++] = b;
	}
	return addr;
}

// Load to BBC memory from open file, return length actually loaded
int host_memload(FILE *handle, int addr, int count) {
	int num, byte, io;

	if (WholeRam[0x27a]==0) addr|=0xFF000000; // No Tube
	if ((addr & 0xFF000000) == 0xFF000000) { // I/O memory
		io = addr & 0xFFFF0000;
		addr &= 0xFFFF;
		if (io == 0xFFFE0000) { // Load to current screen memory
			if (MachineType == Model::BPlus) { if (ShEn==1) io=0xFFFD0000; else io=0xFFFF0000; }
			if (MachineType == Model::Master128) { if (WholeRam[0xD0] & 16) io=0xFFFD0000; else io=0xFFFF0000; }
		}
		if (io == 0xFFFD0000 && MachineType != Model::IntegraB) { // Load to shadow memory
			if (count > 32768-addr) count=32768-addr;
			if (MachineType == Model::BPlus) fread((void *)(ShadowRam+addr-0x3000), 1, count, handle);
			if (MachineType == Model::Master128) fread((void *)(ShadowRAM+addr), 1, count, handle);
			return count;
		}
		if (count > 65536 - addr) count = 65536 - addr; // Prevent wrapround
		num=count;
		while (num--) {
			byte=fgetc(handle);
			WritePaged(addr, (unsigned char) byte);
			addr++;
		}
	} else {
		if (TubeType == Tube::Acorn65C02 || TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80) {
			addr&=0xFFFF;
			if (count > 0xFF00 - addr) count = 0xFF00 - addr; // Prevent wrapround
		}

		if (TubeType == Tube::Acorn65C02) fread(TubeRam + addr, 1, count, handle);
		if (TubeType == Tube::AcornZ80) fread(z80_ram + addr, 1, count, handle);
		if (TubeType == Tube::TorchZ80) fread(z80_ram + addr, 1, count, handle);
		// if (TubeType == Tube::AcornArm) fread(ramMemory + addr, 1, count, handle);
		// if (TubeType == Tube::Master510CoPro)  fread(Tube186 + addr, 1, count, handle);
	}
	return count;
}

// Save from BBC memory to open file, return length actually saved

int host_memsave(FILE *handle, int addr, int count) {
	if (WholeRam[0x27a] == 0) addr |= 0xff000000; // No Tube
	if ((addr & 0xff000000) == 0xff000000) { // I/O memory
		int io = addr & 0xFFFF0000;
		addr &= 0xFFFF;
		if (io == 0xFFFE0000) { // Save current screen memory
			if (MachineType == Model::BPlus) { if (ShEn == 1) io=0xFFFD0000; else io=0xFFFF0000; }
			if (MachineType == Model::Master128) { if (WholeRam[0xD0] & 16) io=0xFFFD0000; else io=0xFFFF0000; }
		}
		if (io == 0xFFFD0000 && MachineType != Model::IntegraB) { // Save shadow memory
			if (count > 32768-addr) count=32768-addr;
			if (MachineType == Model::BPlus) fwrite((void *)(ShadowRam+addr-0x3000), 1, count, handle);
			if (MachineType == Model::Master128) fwrite((void *)(ShadowRAM+addr), 1, count, handle);
			return count;
		}
		if (count > 0xFF00 - addr) count = 0xFF00 - addr; // Prevent wrapround
		int num = count;
		while (num--) {
			int byte = ReadPaged(addr); // Need to do this way to access banked memory
			fputc(byte, handle);
			addr++;
		}
	}
	else {
		if (TubeType == Tube::Acorn65C02 || TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80) {
			addr &= 0xFFFF;
			if (count > 0xFF00 - addr) count = 0xFF00 - addr; // Prevent wrapround
		}
		if (TubeType == Tube::Acorn65C02) fwrite(TubeRam + addr, 1, count, handle);
		if (TubeType == Tube::TorchZ80) fwrite(z80_ram + addr, 1, count, handle);
		if (TubeType == Tube::AcornZ80) fwrite(z80_ram + addr, 1, count, handle);
		// if (TubeType == Tube::AcornArm) fwrite(ramMemory + addr, 1, count, handle);
		// if (TubeType == Tube::Master512CoPro) fwrite(Tube186 + addr, 1, count, handle);
	}
	return count;
}

// Load Russell format BASIC file to memory from open file, return length actually loaded
// Russell format:
// len lo hi text cr
// Acorn format:
// cr hi lo len text cr

int host_basload(FILE *handle, int addr, int count) {
	count = 0;
	host_bytewr(addr++, 13);
	while (!feof(handle)) {
		int len = fgetc(handle);
		int lo  = fgetc(handle);
		int hi  = fgetc(handle);
		host_bytewr(addr++, hi);
		host_bytewr(addr++, lo);
		host_bytewr(addr++, len);
		lo = 0;
		while(!feof(handle) && lo != 13) host_bytewr(addr++, lo = fgetc(handle));
		count += len;
	}

	return count + 2;
}

// Test if file is Russell format BASIC

int host_isrussell(FILE *handle) {
	int num = 0;

	if (EmulatorTrap & 16) {
		fseek(handle, 0, SEEK_END);
		num=ftell(handle);
		if (num>4) {
			// End of Russell format is: cr, 00, ff, ff
			fseek(handle, num-4, SEEK_SET);
			num=0;
			if (fgetc(handle) == 0x0D) {
				if (fgetc(handle) == 0x00) {
					if (fgetc(handle) == 0xFF) {
						if (fgetc(handle) == 0xFF) {
							num=-1;
						}
					}
				}
			}
			fseek(handle, 0, SEEK_SET);
		}
	}
	return num;
}

// Check if valid channel

int host_channel(int handle) {
	if (handle < VDFS_HANDMIN || handle > VDFS_HANDMAX)
		return host_error(222, "Channel"); // Not one of our channels

	if (handles[handle-VDFS_HANDMIN]==nullptr)
		return host_error(222, "Channel not open"); // Our channel, but not open

	return handle-VDFS_HANDMIN; // Return index into translation table
}

// Filename and pathname translation
// =================================

// Copy leafname string to BBC memory, return updated Addr
// We only ever read leafnames from the host system, so we
// don't have to attempt to translate full path symantics.

int host_strwr(int addr, char *str, int max) {
	if(WholeRam[0x27a]==0) addr|=0xff000000;	// No Tube
	while(*str && max>0) {
		char b = *str++; max--;
#if defined(WIN32) || defined(MSDOS) || defined(UNIX) || defined(MACOS)
		switch (b) { // Convert foreign chars to BBC chars
			case '.': b='/'; break;
			case '#': b='?'; break;
			case '$': b='<'; break;
			case '^': b='>'; break;
			case '/': b='\\'; break;
			case ' ': b='\xA0'; break;
		}
#endif

		if ((addr & 0xff000000) == 0xff000000) { // I/O memory
			WholeRam[(addr++) & 0xffff] = b; // Copy C string to memory
			// Should be WritePaged() ?
		}
		else {
			if (TubeType == Tube::Acorn65C02) TubeRam[(addr++) & 0xffff] = b;
			if (TubeType == Tube::TorchZ80) z80_ram[(addr++) & 0xffff] = b;
			if (TubeType == Tube::AcornZ80) z80_ram[(addr++) & 0xffff] = b;
			// if (ArmTube == Tube::AcornArm) ramMemory[addr++] = b;
			// if (TubeType == Tube::Master512CoPro)  Tube186[addr++] = b;
		}
	}
	return addr;
}

// Translate BBC <cr>-terminated pathname to host pathname
// BBC: :d.$.directory.filename/ext
// WIN: d:\directory\filename.ext
// UNX: d:/directory/filename.ext
// NB: Needs to be able to do "name<space>name"
//
// Implement mount points in here
// eg :H.fred.jim -> <mount point for H>\fred\jim

char *host_pathtrans(int src, char *dst) {
	char b;

	char* dstpath = dst;
	do {
		if ((b=ReadPaged(src))==' ') src++;	// Skip leading spaces
	} while (b==' ');

	while ((b=ReadPaged(src++)) > ' ') {
#if defined(WIN32) || defined(MSDOS) || defined(UNIX) || defined(MACOS)
		switch(b) {
#if defined(WIN32) || defined(MSDOS)
			case '.': b='\\'; break; // directory seperator
#else
			case '.': b='/';  break; // directory seperator
#endif
			case '?': b='#';  break;
			case '#': b='?';  break; // 1-char wildcard
			case '/': b='.';  break; // extension seperator
			case '@': b='.';  break; // current directory
			case '<': b='$';  break;
			case '>': b='^';  break;
			case '$': // root directory
				if ((b=ReadPaged(src)) == '.') src++;
#if defined(WIN32) || defined(MSDOS)
				b='\\';
#else
				b='/';
#endif
				break;
			case '^':
				*dst++='.'; b='.'; // parent directory
				break;
#if defined(WIN32) || defined(MSDOS)
			case ':': // Drive specifier
				if ((b=ReadPaged(src)) <= ' ') { // Get drive character
					b = '\\'; // : -> root
				} else {
					src++; // Step past drive character
					if (b >= '0' && b <= '9') {
						b += 'A' - '0'; // Convert '0'-'9' to 'A'-'J'
					}
					*dst++ = b; // :d -> d:
					b=':';
					if (ReadPaged(src) == '.') {
						if (ReadPaged(src+1) == '$') {
							src += 2; // Step past any ".$"
						}
					}
				}
				break;
#endif
		}
#endif
		*dst++=b;
	}
	*dst='\0';				// C terminator
	return dstpath;
}

// Add .inf to pathname

char *host_inf(char *hostpath, char *buffer) {
	const int len = strlen(hostpath);

	if (len > 251) {
		return hostpath;
	}

	int off = len;
#ifdef WIN32
	while ((hostpath[off] != '.') && (hostpath[off] != '\\') && (hostpath[off] != ':') && (off > 1)) off--;
#endif
#ifdef UNIX
	while ((hostpath[off] != '.') && (hostpath[off] != '/') && (off > 1)) off--;
#endif
	if (hostpath[off] != '.') off=len;
	strcpy(buffer, hostpath);
	strcpy(buffer+off, ".inf");
	return buffer;
}

// Object information
// ==================

// Read object info
int host_readinfo(char *pathname, int *ld, int *ex, int *ln, int *at) {
#ifdef WIN32
	char buffer[256];
#else
	union { char buffer[256]; struct stat statbuf; };
#endif
	FILE *handle;
	int obj, off, type;

#ifdef WIN32
	hFind = FindFirstFile(pathname, &infobuf);	// Look for object
	if (hFind == INVALID_HANDLE_VALUE) return 0;	// Not found
	FindClose(hFind);
	*ld=0; *ex=0;					// Only update info if object exists
	*ln=infobuf.nFileSizeLow;
	if (infobuf.nFileSizeHigh) *ln=-1;		// If len>4G, use 4G

	FileTimeToSystemTime(&infobuf.ftLastWriteTime, &infotime);
	obj=infotime.wYear-1981; if (obj<0 || obj>127) obj=0;
	*at=(infotime.wDay<<8) | (infotime.wMonth<<16) | ((obj & 0x0F)<<20) | ((obj & 0x70)<<9);
	obj=infobuf.dwFileAttributes;
	switch (obj & 0x11) {
		case 0x00: *at|=0x33; break;		// file access WR/wr
		case 0x01: *at|=0x19; break;		// file access LR/r
		case 0x10:
		case 0x11: *at|=0x08; break;		// dir access DL/
	}
	obj=((obj & 16)>>4)+1;			// Object type
#endif
#ifdef UNIX
	if (stat(pathname, &statbuf)) return 0;	// Not found
	*ld=0; *ex=0;					// Only update info if object exists
	if (statbuf.st_size > 0xFFFFFFFF) *ln=0xFFFFFFFF;
	else                              *ln=(int)statbuf.st_size;
	if (statbuf.st_mode & S_IFDIR) {
		obj=2; *at=0x08;				// dir access DL/
	} else {
		obj=1;					// file
// unix          bbc
// b0 x world -> b6 E public
// b1 w       -> b5 W
// b2 r       -> b4 R
// b3 x group -> b6 E public
// b4 w       -> b5 W
// b5 r       -> b4 R
// b6 x owner -> b2 E owner
// b7 w       -> b1 W
// b8 r       -> b0 R
		*at=statbuf.st_mode & 0x1FF;		// get file mode OR:OW:  OE  :  GR  :  GW  :  GE  :  PR  :PW:PE
		*at=(((*at & 0x1F8)>>3)|(*at & 0x007))<<2;	// we now have   00:OR:  OW  :  OE  :GRorPR:GWorPW:GEorPE:00:00

		*at=((*at & 0xF0)>>4) | ((*at & 0x0F)<<4);	// swap bit order
		*at=((*at & 0xCC)>>2) | ((*at & 0x33)<<2);
		*at=((*at & 0xAA)>>1) | ((*at & 0x55)<<1);	// we now have      00:  00  :GEorPE:GWorPW:GRorPR:  OE  :OW:OR

		*at=((*at & 0x38)<<1) | (*at & 7);		// finishes with    00:GEorPE:GWorPW:GRorPR:  00  :  OE  :OW:OR
	}
#endif

	// Check for file header
	if(handle=fopen(pathname, "r")) {
		memset(buffer, 0, 256);
		fread(buffer, 1, 256, handle);
		fclose(handle);
		handle = nullptr;
		off=buffer[7];
		if (buffer[off+0]==0 && buffer[off+1]=='(' && buffer[off+2]=='C' && buffer[off+3]==')') {
			type=buffer[6];
			*ld=0xFFFF8000;				// Sideways ROM
			if (type & 0x40) *ld=0x8000;		// Language ROM
			if ((type & 0x20)==0x20) {		// Language relocation address
				while (buffer[++off] && off<248);	// Step past copyright string
				if (++off<249) *ld=(buffer[off+0]&0xFF) | ((buffer[off+1]&0xFF)<<8) | ((buffer[off+2]&0xFF)<<16) | ((buffer[off+3]&0xFF)<<24);
			}
			*ex=*ld;
		}
	}

	// Check for .inf file, if present overrides any file header
	if (host_inf(pathname, buffer) == buffer) {
		if(handle=fopen(buffer, "r")) {
			memset(buffer, 0, 256);
			fread(buffer, 1, 256, handle);
			fclose(handle);
			handle = nullptr;
			off=0;
			while(buffer[off]>' ') off++;		// Step past filename
			while(buffer[off]==' ') off++;		// Skip spaces
			if (buffer[off]>='0') {
				sscanf(buffer+off, "%x", ld);		// Scan load address
				while(buffer[off]>' ') off++;		// Step past load address
				while(buffer[off]==' ') off++;		// Skip spaces
				if (buffer[off]>='0') {
					sscanf(buffer+off, "%x", ex);		// Scan exec address
				}
			}
		}
	}

	return obj;
}

// Write object info

void host_writeinfo(char *pathname, int ld, int ex, int ln, int at, int action) {
	int Load, Exec, Length, Attr;
	char buffer[256];
	int inp, outp;
	FILE *handle;

	int FileType=host_readinfo(pathname, &Load, &Exec, &Length, &Attr);
	if (action == 2) { ex=Exec; at=Attr; }		// Set load
	if (action == 3) { ld=Load; at=Attr; }		// Set exec
	if (action == 4) { ld=Load; ex=Exec; }		// Set attr
	if ((Load==ld) && (Exec==ex) && ((Attr & 255)==(at & 255))) return;	// No change

	// Only create a .inf file if actually needed because metadata changed
	if (host_inf(pathname, buffer) == buffer) {
		if(handle=fopen(buffer, "r+")) {
			memset(buffer, 0, 256);
			fread(buffer+64, 1, 256-64, handle);
			buffer[255]=0;					// Ensure buffer terminated
			inp=64; outp=0;
			while(buffer[inp]>' ') {
				buffer[outp++]=buffer[inp++];			// Copy filename
			}
			while((buffer[inp]==' ') || (buffer[inp]==9)) inp++; // Step past SPC/TABs
			while(buffer[inp]>' ') inp++;			   // Step past any load address
			while((buffer[inp]==' ') || (buffer[inp]==9)) inp++; // Step past SPC/TABs
			while(buffer[inp]>' ') inp++;			   // Step past any exec address
			while((buffer[inp]==' ') || (buffer[inp]==9)) inp++; // Step past SPC/TABs
			while(buffer[inp]>' ') inp++;			   // Step past any access
			while((buffer[inp]==' ') || (buffer[inp]==9)) inp++; // Step past SPC/TABs
			// inp now points to any remaining trailing metadata
		}
		else {
			if((handle=fopen(buffer, "w"))==nullptr) return;	// Couldn't create .inf file
			inp=0; outp=0;
			while(pathname[inp]>' ') {
				buffer[outp++]=pathname[inp]; // Copy filename to buffer
																			// pathname[] is same size as buffer[]
																			// so won't overflow
#if defined(WIN32) || defined(MSDOS)
				if (pathname[inp]=='\\') outp=0; // Reset to start of leafname
#endif
#ifdef UNIX
				if (pathname[inp]=='/')  outp=0; // Reset to start of leafname
#endif
				inp++;
			}
			inp=0;
		}
		// inp=0 - no more metadata
		// inp>0 - possible extra metadata in buffer[], whitespace skipped
		// outp=immediately after filename in buffer[], where SPC needs to be inserted

		sprintf(buffer+outp, " %08X %08X %02X", ld, ex, at);
		outp=outp+21;
		if (inp) {
			buffer[outp++]=' ';
			while(buffer[inp]>=' ') buffer[outp++]=buffer[inp++];
		}
		buffer[outp++]=0x0a;
		fseek(handle, 0, SEEK_SET);
		fwrite(buffer, 1, outp, handle);
		fclose(handle);
		handle = nullptr;
	}
}

// Emulator-implemented *command core routines
// ===========================================

// Load and run a file
// Some of this could be merged with OSFILE &FF
// Returns:
//    <0  - do not do an RTS, PC already set to address to continue executing at
//    >=0 - do an RTS to return to caller
//
//    -2  - code being run in I/O processor    - continue executing at new PC
//    -1  - error occured                      - continue executing at error PC
//    1   - code being run in second processor - do RTS to return to VDFS *RUN handler

int host_run(int FSCAction, int XYReg) {
	int FileType, Load, Exec, Length, Attr;
	char pathname[256];
	FILE *handle = nullptr;

	CommandLine=XYReg;
	while(ReadPaged(CommandLine) > ' ') CommandLine++;
	while(ReadPaged(CommandLine) == ' ') CommandLine++;

	host_pathtrans(XYReg, pathname);
	if ((FileType=host_readinfo(pathname, &Load, &Exec, &Length, &Attr)) > 1) return host_error(181, "Not a file");

	// todo: exec=ffffffff -> *Exec file
	if (FileType==1) {
		if (Load==0 || Load==0xFFFFFF || (Load & 0xFF000000) != (Exec & 0xFF000000)) {
			// Can only do '*RUN file' if file has a load address
			// and load/exec both refer to same memory
			return host_error(252, "Bad address");
		}
	}
	if (FileType) if ((handle=fopen(pathname, "rb"))==nullptr) FileType=0;
	if (FileType==0) {
		if (FSCAction==3) {
			return host_error(254, "Bad command");
		} else {
			return host_error(214, "File not found");
		}
	}
	host_memload(handle, Load, Length);

	fclose(handle);
	handle = nullptr;

	if (WholeRam[0x27A]==0) Exec|=0xFF000000; // No Tube

	if ((Exec & 0xFF000000) == 0xFF000000) { // I/O memory
		ProgramCounter = Exec & 0xFFFF;
		WholeRam[0x100 + StackReg--] = 0x60; // Need to ensure code returns with A=0
		WholeRam[0x100 + StackReg--] = 0x00; // Stack LDA #0:RTS
		WholeRam[0x100 + StackReg--] = 0xa9;
		WholeRam[0x100 + StackReg] = StackReg;
		StackReg--;
		WholeRam[0x100 + StackReg--] = 0x01;
		return -2; // Running in I/O memory
	}
	else {
		WholeRam[0xbc] = Exec & 0xFF;
		WholeRam[0xbd] = (Exec >> 8) & 0xFF;
		WholeRam[0xbe] = (Exec >> 16) & 0xFF;
		WholeRam[0xbf] = (Exec >> 24) & 0xFF;
		XReg = 0xBC; YReg = 0x00; Accumulator = 0xFF; // XY=>transfer address, A=Tube ID
		return 1; // Running in second processor
	}
}

// Create a directory, no error if exists

int host_cdir(const char *hostpath) {

#ifdef WIN32
	if ((CreateDirectory(hostpath,nullptr)) == 0) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			return host_error(214, "Not found");
		}
	}
	return 1;
#else
	mkdir(hostpath,0);
	return 1; // OK
#endif
}

// Delete an object

int host_delete(char *hostpath, int hint) {
#ifdef WIN32
	int result = 0;
	switch (hint) {
		case 0:
			if (DeleteFile(hostpath) == 0) {
				if ((result=GetLastError()) == ERROR_ACCESS_DENIED) {
					if (RemoveDirectory(hostpath)) {
						result=0;
					} else {
						result=GetLastError();
					}
				}
			}
			break;
		case 1: if (DeleteFile(hostpath)==0) result=GetLastError(); break;
		case 2: if (RemoveDirectory(hostpath)==0) result=GetLastError(); break;
	}
	if (result) {
		if (result == ERROR_FILE_NOT_FOUND) return host_error(214, "Not found");
		else return host_error(189, "Access denied");
	}
	DeleteFile(host_inf(hostpath, hostpath));	// Remove any .inf file
	return 1; // OK
#else
	remove(hostpath);
	rmdir(hostpath);
	remove(host_inf(hostpath, hostpath));
	return 1; // OK
#endif
}

// Filing system commands implemented by emulator
// ==============================================

// Match with filing system command

int cmd_lookup(int *XYReg) {
	static const char commands[]=":ACCESS:CDIR:COPY:CREATE:DELETE:DIR:DRIVE:FREE:GO:LIB:MOUNT:RENAME:WIPE::";
	int lptr;
	char b;

	if (ReadPaged(*XYReg)<'A') return 0;		// Doesn't start with letter
	int num=0;
	int cptr=0;
	do {
		num++;
		lptr=*XYReg;
		while (commands[cptr] != ':') cptr++;	// Step to start of next command
		if (commands[cptr+1] == ':') return 0;	// End of command table
		do {
			if ((b=ReadPaged(lptr++)) >= '\x60') b&=0xDF;
			cptr++;
		} while (b!='.' && b==commands[cptr]);	// Loop until '.' or no match
	} while (b!='.' && commands[cptr]!=':');	// Loop until '.' or end of command

	// ACCESS:          ACCESS:            ACCESS:        ACCESS:
	//  cptr-^           cptr-^             cptr-^         cptr-^
	// ACCESSfredjim    ACCESS fredjim     ACCESS<cr>     ACC.fredjim
	//  lptr-^           lptr-^             lptr-^      lptr-^

	if (b>='A' && b<='Z') return 0;		// Command not terminated
	while(ReadPaged(lptr) == ' ') lptr++;		// Skip spaces
	*XYReg=lptr;
	return num;
}

// Match filing system command
// ---------------------------
// Return <0 if error occured
// Return  0 if command wasn't matched, fall through to *RUN
// Return >0 if command successful

int host_cmd(int FSCAction, int XYReg) {
	char pathname[256];
	int num = cmd_lookup(&XYReg);

	if (num == 0) {
		return 0;
	}

	switch (num) {
		case 6: // *DIR
			host_pathtrans(XYReg, pathname);
			if (SetCurrentDirectory(pathname)) return 1;
			return host_error(214, "Not found");
		case 2: // *CDIR
			return host_cdir(host_pathtrans(XYReg, pathname));
		case 5: // *DELETE
			return host_delete(host_pathtrans(XYReg, pathname), 0);

//    case 1:  return cmd_access(XYReg);
//    case 3:  return cmd_copy(XYReg);
//    case 4:  return cmd_create(XYReg);
//    case 7:  return cmd_drive(XYReg);
//    case 8:  return cmd_free(XYReg);
//    case 9:  return cmd_go(XYReg);
//    case 10: return cmd_lib(XYReg);
//    case 11: return cmd_mount(XYReg);
//    case 12: return cmd_rename(XYReg);
//    case 13: return cmd_wipe(XYReg);
	}

	return 0;
}

// Filing system interface implemented by emulator via emulator traps
// ==================================================================

// Opcode 03 - Acorn MOS_CLI
//           - WSS   MOS_EMT

int host_emt(int dorts) {
	if (EmulatorTrap & 2) { // Warm Silence traps
		switch (ReadPaged(ProgramCounter++)) {
			case 0x00: return host_fsc(1); // OSFSC
			case 0x01: return host_find(1); // OSFIND
			case 0x02: return host_gbpb(1); // OSGBPB
			case 0x03: return host_bput(1); // OSBPUT
			case 0x04: return host_bget(1); // OSBGET
			case 0x05: return host_args(1); // OSARGS
			case 0x06: return host_file(1); // OSFILE

			case 0x40: return host_word(1); // OSWORD
			case 0x41: return host_byte(1); // OSBYTE

			case 0x80: break; // Read CMOS
			case 0x81: break; // Write CMOS
			case 0x82: break; // Read EEPROM
			case 0x83: break; // Write EEPROM

			case 0xD0: break; // *SRLOAD
			case 0xD1: break; // *SRWRITE
			case 0xD2: break; // *DRIVE
			case 0xD3: break; // Load/Run/Exec !Boot
			case 0xD4: break; // nothing
			case 0xD5: break; // *BACK
			case 0xD6: break; // *MOUNT

			case 0xFF: return host_quit(1); // OSQUIT
		}

		rts();
		return 0;
	}

	return host_error(0xFE, "BAD COMMAND");
}

// Opcode 03,00 - MOS_FSC

int host_fsc(int dorts) {
	int idx;

	switch (Accumulator) {
		case 1:
			if ((idx=host_channel(XReg))<0) return -1;
			XReg=feof(handles[idx]);
			break;
		case 3: // *command
			idx=(host_cmd(Accumulator, XReg | (YReg<<8)));
			if (idx<0) return -1;		// Error occured, don't do rts()
			if (idx>0) break;			// Command executed, don't fall through
		case 2: // */filename
		case 4: // *RUN filename
			if ((idx=host_run(Accumulator, XReg | (YReg<<8)))<0) return idx;
			break;
		case 7:
			XReg=VDFS_HANDMIN; YReg=VDFS_HANDMAX;
			break;
		case 8:
			Accumulator=3;			// Tell client *commands are supported
			break;

		//   0 - *OPT
		//   5 - *CAT   - done locally
		//   6 - NewFS  - done locally
		//   9 - *EX    - done locally
		//  10 - *INFO  - done locally
		//  11 - LibRun - done locally
		//  12 - *RENAME
		// 255 - Boot
	}

	if(dorts) rts();
	return 0;
}

// Opcode 53 - MOS_FILE

int host_file(int dorts) {
	int Load, Exec;
	union { int Length; int Start; };
	union { int Attr;   int End;   };
	int addr, num;
	char pathname[256];
	FILE *handle = nullptr;

	XYReg =XReg | (YReg<<8);
	host_pathtrans(ReadPaged(XYReg+0) | (ReadPaged(XYReg+1)<<8), pathname);
	Load  =ReadPaged(XYReg+2)  | (ReadPaged(XYReg+3)<<8)  | (ReadPaged(XYReg+4)<<16)  | (ReadPaged(XYReg+5)<<24);
	Exec  =ReadPaged(XYReg+6)  | (ReadPaged(XYReg+7)<<8)  | (ReadPaged(XYReg+8)<<16)  | (ReadPaged(XYReg+9)<<24);
	Length=ReadPaged(XYReg+10) | (ReadPaged(XYReg+11)<<8) | (ReadPaged(XYReg+12)<<16) | (ReadPaged(XYReg+13)<<24);
	Attr  =ReadPaged(XYReg+14) | (ReadPaged(XYReg+15)<<8) | (ReadPaged(XYReg+16)<<16) | (ReadPaged(XYReg+17)<<24);

	switch(Accumulator) {
		case 0xFF: // Load
			addr=Load; num=Exec;
			if ((Accumulator=host_readinfo(pathname, &Load, &Exec, &Length, &Attr)) > 1) {
				return host_error(181, "Not a file");
			}
			if ((Accumulator==1) && ((num & 0xFF)!=0)) {
				if (Load) {
					addr=Load;				// Load to file's load address
				} else {
					return host_error(252, "Bad address");	// Can only do '*LOAD file' if file has a load address
				}
			}
			if (Accumulator) if ((handle=fopen(pathname, "rb"))==nullptr) Accumulator=0;
			if (Accumulator==0) return host_error(214, "File not found");
			if (host_isrussell(handle)) {
				Length=host_basload(handle, addr, Length);
			} else {
				Length=host_memload(handle, addr, Length);
			}
			fclose(handle);
			handle = nullptr;
			break;
		case 0: // Save
			handle=fopen(pathname, "wb");
			if (handle == nullptr) return host_error(192, "Can't save file");
			host_memsave(handle, Start, End-Start);
			fclose(handle);
			handle = nullptr;
			Length=End-Start;
			Accumulator = 1; // Return A=file
			Attr = 0x33; // Attr=WR/wr
			// Fall through with A=1 to write info

		case 1: // Write object info
		case 2: // Write load address
		case 3: // Write exec address
		case 4: // Write attrs
			host_writeinfo(pathname, Load, Exec, Length, Attr, Accumulator);
			break;
		case 5: // Read object info
			Accumulator=host_readinfo(pathname, &Load, &Exec, &Length, &Attr);
			break;
		case 6: // Delete
			Accumulator=host_readinfo(pathname, &Load, &Exec, &Length, &Attr);
			if (Accumulator) {
				if (host_delete(pathname, Accumulator) < 0) return -1; // Error occured, return
			}
			break;
		case 7: // Create
			break;
		case 8: // CDir
			if (host_cdir(pathname) < 0) return -1;	// Error occured, return
			Accumulator=host_readinfo(pathname, &Load, &Exec, &Length, &Attr);
			break;
	}

	// Update control block, will always be in I/O memory
	// Could be in private RAM, so needs to use WritePaged()
	WritePaged(XYReg+2,  Load & 0xFF);
	WritePaged(XYReg+3,  (Load >> 8) & 0xFF);
	WritePaged(XYReg+4,  (Load >> 16) & 0xFF);
	WritePaged(XYReg+5,  (Load >> 24) & 0xFF);

	WritePaged(XYReg+6,  Exec & 0xFF);
	WritePaged(XYReg+7,  (Exec >> 8) & 0xFF);
	WritePaged(XYReg+8,  (Exec >> 16) & 0xFF);
	WritePaged(XYReg+9,  (Exec >> 24) & 0xFF);

	WritePaged(XYReg+10, Length & 0xFF);
	WritePaged(XYReg+11, (Length >> 8) & 0xFF);
	WritePaged(XYReg+12, (Length >> 16) & 0xFF);
	WritePaged(XYReg+13, (Length >> 24) & 0xFF);

	WritePaged(XYReg+14, Attr & 0xFF);
	WritePaged(XYReg+15, (Attr >> 8) & 0xFF);
	WritePaged(XYReg+16, (Attr >> 16) & 0xFF);
	WritePaged(XYReg+17, (Attr >> 24) & 0xFF);

	if(dorts) rts();
	return 0;
}

// Opcode 63 - MOS_ARGS

int host_args(int dorts) {
	int idx;
	FILE *handle = nullptr;

	// Data will always be in zero page I/O memory
	int Data=WholeRam[XReg+0] | (WholeRam[XReg+1]<<8) | (WholeRam[XReg+2]<<16) | (WholeRam[XReg+3]<<24);
	if (YReg) {
		if (Accumulator==0xFF || Accumulator<=6) {
			if ((idx=host_channel(YReg))<0) return -1;
			handle=handles[idx];
		}
		switch(Accumulator) {
			case 0xFD: // Write context
			case 0xFE: // Read context
				break;
			case 0xFF: // Ensure file
				fflush(handle);
				break;
			case 0: // =PTR
				Data=ftell(handle);
				break;
			case 1: // PTR=
				fseek(handle, Data, SEEK_SET);
				break;
			case 2: // =EXT
			case 4: // =ALLOC
				idx=ftell(handle);
				fseek(handle, 0, SEEK_END);
				Data=ftell(handle);
				fseek(handle, idx, SEEK_SET);
				break;
			case 3: // EXT=
			case 6: // ALLOC=
				idx=ftell(handle);
				fseek(handle, Data, SEEK_SET);
				fseek(handle, idx, SEEK_SET);
				break;
			case 5:				// =EOF
				Data=feof(handle);
				break;
		}
	}
	else {
		switch(Accumulator) {
			case 0xFD: // Version info
			case 0xFE: // Last drive used
				break;
			case 0xFF: // Ensure all files
				fflush(nullptr);
				break;
			case 0: // Read selected filing system
				Accumulator=17; // 17=VDFS
				break;
			case 1: // Read command line address
				Data=CommandLine | 0xFFFF0000;
				break;
			case 2: // Read version
			case 3: // Read LIBFS
			case 4: // Read disk space used
			case 5: // Read disk free space
				break;
		}
	}

	// Update control block, will always be in zero page I/O memory
	WholeRam[XReg+0]=Data & 0xFF;
	WholeRam[XReg+1]=(Data >> 8) & 0xFF;
	WholeRam[XReg+2]=(Data >> 16) & 0xFF;
	WholeRam[XReg+3]=(Data >> 24) & 0xFF;

	if(dorts) rts();
	return 0;
}

// Opcode 73 - MOS_BGET

int host_bget(int dorts) {
	int idx = host_channel(YReg);

	if (idx<0) return -1;
	if (feof(handles[idx])) return host_error(223, "EOF");
	if ((Accumulator=fgetc(handles[idx]))==EOF) {
		Accumulator = 0xfe;
		PSR |= 1; // Set carry flag
	} else {
		PSR &= 0xfe; // Clear carry flag
	}
	if(dorts) rts();
	return 0;
}

// Opcode 83 - MOS_BPUT

int host_bput(int dorts) {
	int idx = host_channel(YReg);

	if (idx<0) return -1;
	fputc(Accumulator, handles[idx]);
	if(dorts) rts();
	return 0;
}

// Opcode 93 - MOS_GBPB

int host_gbpb(int dorts) {
	int idx = 0, res = 0;
	FILE *handle = nullptr;

	XYReg = XReg | (YReg<<8);
	// Control block info
	int Channel=ReadPaged(XYReg+0);
	int Addr   =ReadPaged(XYReg+1) | (ReadPaged(XYReg+2)<<8)  | (ReadPaged(XYReg+3)<<16)  | (ReadPaged(XYReg+4)<<24);
	int Count  =ReadPaged(XYReg+5) | (ReadPaged(XYReg+6)<<8)  | (ReadPaged(XYReg+7)<<16)  | (ReadPaged(XYReg+8)<<24);
	int Offset =ReadPaged(XYReg+9) | (ReadPaged(XYReg+10)<<8) | (ReadPaged(XYReg+11)<<16) | (ReadPaged(XYReg+12)<<24);
	PSR &= 0xFE; // Clear carry flag, not EOF
	int num=Count;

	if (Accumulator>=1 && Accumulator<=4) {	// Write/read from open channel

		// At the moment, BeebEm/VDFS gets confused by *MOVE playing with the memory mapping
		// So, to be safe, detect it happening and bomb out to the user with an error
		if (MachineType == Model::Master128) {
			if (ReadPaged(0xdfd6) != 0) { // ACCCON changed
				return host_error(254,"Bad use of OSGBPB with VDFS");
			}
		}

		if ((idx=host_channel(Channel))<0) return -1; // Check channel
		handle=handles[idx];
		if (Accumulator == 3 || Accumulator == 4) { // Read operation, check for EOF
			idx=ftell(handle);
			fseek(handle, 0, SEEK_END);
			res=ftell(handle); // Find size of file
			if (Accumulator == 2) { // Use current PTR
				fseek(handle, idx, SEEK_SET); // Restore PTR
				if (idx+Count>=res) { // Woah! Crazy overflow potentials
					num=res-idx; // Reduce number to transfer
					PSR |= 1; // Set carry, will end at EOF
				}
			} else { // Use supplied Offset
				if (Offset+Count>=res) { // Woah! Crazy overflow potentials
					num=res-Offset; // Reduce number to transfer
					PSR |= 1; // Set carry, will end at EOF
				}
			}
		}
		if(Accumulator==1 || Accumulator==3) { // Set PTR to supplied Offset before action
			fseek(handle, Offset, SEEK_SET);
		}
	}

	switch(Accumulator) {
		case 1: // Write data
		case 2:
			Count=Count-host_memsave(handle, Addr, num);
			Offset=ftell(handle);
			Addr=Addr+num;
			break;
		case 3: // Read data
		case 4:
			Count=Count-host_memload(handle, Addr, num);
			Offset=ftell(handle);
			Addr=Addr+num;
			break;
		case 8: // Scan directory, standard format
		case 9: // Scan directory, special WSS format
		// NOTE: WSS OSGBPB 9 uses the control block differently to the standard
		// On entry, XY?0=count requested, XY!1=address,   XY!5=buffer length, XY!9=offset
		// On exit,  XY?0=count returned,  XY!1=preserved, XY!5=preserved,     XY!9=updated offset

			idx =  Offset; // Offset into directory
#ifdef WIN32
			PSR |= 1; Channel=0; // Prepare result=no objects returned
			hFind = FindFirstFile("*.*", &infobuf); // Initialise search
			if (hFind != INVALID_HANDLE_VALUE) {
				idx++;
				res=-1;
				while((idx != 0) && (res != 0)) {
					if (idx > 0) if ((res=FindNextFile(hFind, &infobuf)) != 0) idx--;
					// Step to next object, if found decrement index
					if (infobuf.cFileName[0] == '.') {
						idx++; // Skip '.' and '..' (and '.xxxx')
					}
					if ((num = strlen(infobuf.cFileName)) > 4) {
						if (_stricmp(&infobuf.cFileName[num - 4], ".inf") == 0) {
							idx++; // Skip '*.inf'
						}
					}
				}
				FindClose(hFind);
				if (idx == 0) { // Found an object
					PSR &= 0xfe; // Clear carry
					if (Accumulator == 9) {
						res = Addr; // NB: WSS expects Addr to /not/ be updated
						res = host_strwr(res, infobuf.cFileName, Count-1); // Copy string to BBC memory
						res = host_bytewr(res, 0); // Terminate with <null>
						Channel = 1; // One object returned
					} else {
						if((res=strlen(infobuf.cFileName))>30) res=30; // Truncate to 30 characters
						Addr=host_bytewr(Addr, res);
						Addr=host_strwr(Addr, infobuf.cFileName, res); // Copy string to BBC memory
						Count--;
						// Channel=directory cycle number
					}
					Offset++;
				}
			}
#endif
			break; // endcase 8,9
	} // endswitch

	// Update control block, will always be in I/O memory
	// Could be in private RAM, so needs to use WritePaged()
	WritePaged(XYReg+0, Channel);

	WritePaged(XYReg+1, Addr & 0xFF);
	WritePaged(XYReg+2, (Addr >> 8) & 0xFF);
	WritePaged(XYReg+3, (Addr >> 16) & 0xFF);
	WritePaged(XYReg+4, (Addr >> 24) & 0xFF);

	WritePaged(XYReg+5, Count & 0xFF);
	WritePaged(XYReg+6, (Count >> 8) & 0xFF);
	WritePaged(XYReg+7, (Count >> 16) & 0xFF);
	WritePaged(XYReg+8, (Count >> 24) & 0xFF);

	WritePaged(XYReg+9, Offset & 0xFF);
	WritePaged(XYReg+10,(Offset >> 8) & 0xFF);
	WritePaged(XYReg+11,(Offset >> 16) & 0xFF);
	WritePaged(XYReg+12,(Offset >> 24) & 0xFF);

	if(dorts) rts();
	return 0;
}

// Opcode A3 - MOS_FIND

int host_find(int dorts) {
	char pathname[256];
	FILE *handle = nullptr;
	int idx, lp;

	if (Accumulator) { // Open
		idx = -1; // Look for free handle
		for (lp=0; lp<=(VDFS_HANDMAX-VDFS_HANDMIN); lp++) {
			if (handles[lp] == nullptr) idx=lp;
		}
		if (idx<0) return host_error(192, "Too many open files");
		XYReg=XReg | (YReg<<8);
		host_pathtrans(XYReg, pathname);
		switch(Accumulator & 0xC0) {
			case 0x40: // OpenIn
				handle = fopen(pathname, "rb");
				break;
			case 0x80: // OpenOut
				handle = fopen(pathname, "wb");
				break;
			case 0xC0: // OpenUp
				handle = fopen(pathname, "r+");
				break;
		}
		if (handle) {
			handles[idx]=handle;
			Accumulator=idx+VDFS_HANDMIN;
		} else {
			Accumulator=0; // Not found
		}
	} else {
		// Close
		if (YReg) {
			if ((idx=host_channel(YReg))<0) return -1; // Bad channel
			fclose(handles[idx]);
			handles[idx]=nullptr;
		} else {
			// Close all
			for(idx=0; idx<(VDFS_HANDMAX-VDFS_HANDMIN+1); idx++) {
				if (handles[idx]) {
					fclose(handles[idx]);
					handles[idx]=nullptr;
				}
			}
		}
	}

	if (dorts) rts();

	return 0;
}

// Opcode B3 - MOS_QUIT

int host_quit(int dorts) {

#ifdef WIN32
	ShowWindow(GETHWND, SW_MINIMIZE);
#endif

	if (dorts) rts();
	return 0;
}

// Opcode C3 - MOS_LANG
int host_lang(int dorts) { if(dorts) rts(); return 0; }

// Opcode D3
int host_D3(int dorts)   { if(dorts) rts(); return 0;  }

// Opcode E3
int host_E3(int dorts)   { if(dorts) rts(); return 0;  }

// Opcode F3
int host_F3(int dorts)   { if(dorts) rts(); return 0;  }

// Opcode 13 - MOS_BYTE
int host_byte(int dorts) { if(dorts) rts(); return 0;  }

// Opcode 23 - MOS_WORD
int host_word(int dorts) { if(dorts) rts(); return 0;  }

// Opcode 33 - MOS_WRCH
int host_wrch(int dorts) { if(dorts) rts(); return 0;  }

// Opcode 43 - MOS_RDCH
int host_rdch(int dorts) { if(dorts) rts(); return 0;  }
