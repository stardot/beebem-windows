ReadMe-IMPORTANT.txt :
Notes for the Win32 port of Beebem

********************************************************************
This is my port of David Gilbert's BBC Emulator "beebem" onto Win32.
Copyright and Distribution for the Emulator resides with him as 
described in the Readme.1st text file.
********************************************************************

Notes:
======

The supplied source files should compile properly under Microsoft
Visual C++ Version 2.0. The resulting binary should be compatible
with Windows NT, Windows95 and Windows3.1 running Win32s.

The example-intel-binaries directory contains [surprise] two
example Intel binaries
BEEBWIN.EXE	- Uses WinG
BEEBNWG.EXE	- Doesn't use WinG
You will need to fill the beebfile and discims directories (see
ReadMe.1st).

Important:
=========
Note that a 256-colour depth video mode is required!!

After compilation, create a directory to keep the emulator
in, E.G. C:\BEEB.
Create 2 subdirectories
C:\Beeb\beebfile	- Rom Images [basic, dnfs, os12 kept here]
C:\Beeb\discims		- Disc Images [compiled to be 'elite' here]
It is important to run the emulator from that directory (I.E.
Click on it in filemanager, create an icon in programmanager or
change into that directory before running it; DO NOT use a path,
like C:\> \beeb\beebnt), otherwise the emulator won't know
where to get it's images from - it takes the paths from the calling
directory. There should be a .ini file to tell it where to get
them from.. perhaps when I get time :-)

Other files required:
====================
If you want to run under Windows3.11, get a copy of Win32s; freely
avaliable from Microsoft.
If you want to build/run a winG version, get a copy of WinG; freely
avaliable from Microsoft.
You will need beeb rom images and a disk file (see ReadMe.1st).

Build Notes:
===========
Unpack with a system supporting long filenames. Not really a problem
since systems that don't support long filenames won't be able
to compile it anyway...

It is possible to build the source with or without using WinG.

In order to build a WinG version;
---------------------------------
copy beebbitmap.cpp.wing beebbitmap.cpp
copy beebbitmap.h.wing beebbitmap.h.wing
edit the file beebwin.cpp, make sure the line that defines the
colours reads "cols[i] = 20+i"
make sure WinG.Lib is in the project

In order to build a non-WinG version;
---------------------------------
copy beebbitmap.cpp.notwing beebbitmap.cpp
copy beebbitmap.h.notwing beebbitmap.h.wing
edit the file beebwin.cpp, make sure the line that defines the
colours reads "cols[i] = i"
make sure WinG.Lib is not in the project

Other Compilers:
===============
It is possible to use this source under Watcom, but you will need
to edit port.h to change the EightByteType from __int64 (which is
Microsoft Specific) to a structure of two long words).

Changes from Original & Problems:
================================
There are no particular problems with this Win32 port, but
an .ini file should be added to sort out the directory problem.

The keyboard mapping might be slightly incorrect; I don't have
a BBC here to check if it's right.

The Non-wing version doesn't use palettes, so the colours are the
fixed ones in the windows palette.

The source differs from the original in a number of insignificant
ways neccesary to get MSVC to compile it.
X was hacked entirely out, and now contains a new BeebWin class
for dealing with the windows specific parts.
Main.cpp now has a Windows message handler instead of an X one.
All of the unix bits like signal handlers have been hacked out. The
environment variable handling is probably broken too.

Finally:
=======
I'd be interested to hear how well this runs on other Windows NT
platforms, such as DEC Alpha, MIPS and PowerPC..

// Nigel Magnay // magnayn@cs.man.ac.uk // 95/5/21
