# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "beebnt.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
# PROP Target_Last_Scanned "Win32 Debug"
MTL=MkTypLib.exe
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WinRel"
# PROP Intermediate_Dir "WinRel"
OUTDIR=.\WinRel
INTDIR=.\WinRel

ALL : $(OUTDIR)/beebnt.exe $(OUTDIR)/beebnt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /G4 /Gr /W3 /GX /YX /Ox /Ot /Oa /Og /Oi /Os /Op /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /c
# SUBTRACT CPP /Fr
CPP_PROJ=/nologo /G4 /Gr /W3 /GX /YX /Ox /Ot /Oa /Og /Oi /Os /Op /Ob2 /D\
 "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp$(OUTDIR)/"beebnt.pch" /Fo$(INTDIR)/ /c 
CPP_OBJS=.\WinRel/
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"beebnt.bsc" 
BSC32_SBRS= \
	

$(OUTDIR)/beebnt.bsc : $(OUTDIR)  $(BSC32_SBRS)
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:windows /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:windows /MACHINE:I386
# SUBTRACT LINK32 /PROFILE
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /NOLOGO /SUBSYSTEM:windows /INCREMENTAL:no\
 /PDB:$(OUTDIR)/"beebnt.pdb" /MACHINE:I386 /OUT:$(OUTDIR)/"beebnt.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/video.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/disc8271.obj \
	$(INTDIR)/beebmem.obj \
	$(INTDIR)/mode7font.obj \
	$(INTDIR)/uservia.obj \
	$(INTDIR)/6502core.obj \
	$(INTDIR)/BEEBWIN.OBJ \
	$(INTDIR)/sysvia.obj \
	$(INTDIR)/via.obj \
	$(INTDIR)/beebbitmap.obj \
	\DATA\WING\LIB\WING32.LIB

$(OUTDIR)/beebnt.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WinDebug"
# PROP BASE Intermediate_Dir "WinDebug"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WinDebug"
# PROP Intermediate_Dir "WinDebug"
OUTDIR=.\WinDebug
INTDIR=.\WinDebug

ALL : $(OUTDIR)/beebnt.exe $(OUTDIR)/beebnt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE CPP /nologo /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /W3 /GX /Zi /YX /O2 /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /c
CPP_PROJ=/nologo /W3 /GX /Zi /YX /O2 /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"beebnt.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"beebnt.pdb"\
 /c 
CPP_OBJS=.\WinDebug/
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"beebnt.bsc" 
BSC32_SBRS= \
	$(INTDIR)/video.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/disc8271.sbr \
	$(INTDIR)/beebmem.sbr \
	$(INTDIR)/mode7font.sbr \
	$(INTDIR)/uservia.sbr \
	$(INTDIR)/6502core.sbr \
	$(INTDIR)/BEEBWIN.SBR \
	$(INTDIR)/sysvia.sbr \
	$(INTDIR)/via.sbr \
	$(INTDIR)/beebbitmap.sbr

$(OUTDIR)/beebnt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"beebnt.pdb" /DEBUG /MACHINE:I386 /OUT:$(OUTDIR)/"beebnt.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/video.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/disc8271.obj \
	$(INTDIR)/beebmem.obj \
	$(INTDIR)/mode7font.obj \
	$(INTDIR)/uservia.obj \
	$(INTDIR)/6502core.obj \
	$(INTDIR)/BEEBWIN.OBJ \
	$(INTDIR)/sysvia.obj \
	$(INTDIR)/via.obj \
	$(INTDIR)/beebbitmap.obj \
	\DATA\WING\LIB\WING32.LIB

$(OUTDIR)/beebnt.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Group "Source Files"

################################################################################
# Begin Source File

SOURCE=.\video.cpp

$(INTDIR)/video.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\main.cpp

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\disc8271.cpp

$(INTDIR)/disc8271.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\beebmem.cpp
DEP_BEEBM=\
	.\6502core.h\
	.\disc8271.h\
	.\main.h\
	.\sysvia.h\
	.\uservia.h\
	.\video.h\
	.\beebwin.h\
	.\port.h

$(INTDIR)/beebmem.obj :  $(SOURCE)  $(DEP_BEEBM) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mode7font.cpp

$(INTDIR)/mode7font.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\uservia.cpp

$(INTDIR)/uservia.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\6502core.cpp

$(INTDIR)/6502core.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\BEEBWIN.CPP

$(INTDIR)/BEEBWIN.OBJ :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\sysvia.cpp

$(INTDIR)/sysvia.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\via.cpp

$(INTDIR)/via.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\beebbitmap.cpp
DEP_BEEBB=\
	.\beebbitmap.h

$(INTDIR)/beebbitmap.obj :  $(SOURCE)  $(DEP_BEEBB) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=\DATA\WING\LIB\WING32.LIB
# End Source File
# End Group
# End Project
################################################################################
