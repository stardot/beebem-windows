# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=BeebEm - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to BeebEm - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "BeebEm - Win32 Release" && "$(CFG)" != "BeebEm - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "BeebEm.mak" CFG="BeebEm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BeebEm - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "BeebEm - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "BeebEm - Win32 Release"
MTL=mktyplib.exe
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BeebEm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\BeebEm.exe"

CLEAN : 
	-@erase "$(INTDIR)\6502core.obj"
	-@erase "$(INTDIR)\atodconv.obj"
	-@erase "$(INTDIR)\BeebEm.res"
	-@erase "$(INTDIR)\beebmem.obj"
	-@erase "$(INTDIR)\beebsound.obj"
	-@erase "$(INTDIR)\beebstate.obj"
	-@erase "$(INTDIR)\beebwin.obj"
	-@erase "$(INTDIR)\disc8271.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\mode7font.obj"
	-@erase "$(INTDIR)\sysvia.obj"
	-@erase "$(INTDIR)\uservia.obj"
	-@erase "$(INTDIR)\via.obj"
	-@erase "$(INTDIR)\video.obj"
	-@erase "$(OUTDIR)\BeebEm.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /GB /Gr /W3 /GX /O2 /Ob2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /GB /Gr /ML /W3 /GX /O2 /Ob2 /D "NDEBUG" /D "WIN32" /D\
 "_WINDOWS" /Fp"$(INTDIR)/BeebEm.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
RSC_PROJ=/l 0x809 /fo"$(INTDIR)/BeebEm.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/BeebEm.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /profile
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib /nologo\
 /subsystem:windows /incremental:no /pdb:"$(OUTDIR)/BeebEm.pdb" /machine:I386\
 /out:"$(OUTDIR)/BeebEm.exe" 
LINK32_OBJS= \
	"$(INTDIR)\6502core.obj" \
	"$(INTDIR)\atodconv.obj" \
	"$(INTDIR)\BeebEm.res" \
	"$(INTDIR)\beebmem.obj" \
	"$(INTDIR)\beebsound.obj" \
	"$(INTDIR)\beebstate.obj" \
	"$(INTDIR)\beebwin.obj" \
	"$(INTDIR)\disc8271.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mode7font.obj" \
	"$(INTDIR)\sysvia.obj" \
	"$(INTDIR)\uservia.obj" \
	"$(INTDIR)\via.obj" \
	"$(INTDIR)\video.obj"

"$(OUTDIR)\BeebEm.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\BeebEm.exe" "$(OUTDIR)\BeebEm.bsc"

CLEAN : 
	-@erase "$(INTDIR)\6502core.obj"
	-@erase "$(INTDIR)\6502core.sbr"
	-@erase "$(INTDIR)\atodconv.obj"
	-@erase "$(INTDIR)\atodconv.sbr"
	-@erase "$(INTDIR)\BeebEm.res"
	-@erase "$(INTDIR)\beebmem.obj"
	-@erase "$(INTDIR)\beebmem.sbr"
	-@erase "$(INTDIR)\beebsound.obj"
	-@erase "$(INTDIR)\beebsound.sbr"
	-@erase "$(INTDIR)\beebstate.obj"
	-@erase "$(INTDIR)\beebstate.sbr"
	-@erase "$(INTDIR)\beebwin.obj"
	-@erase "$(INTDIR)\beebwin.sbr"
	-@erase "$(INTDIR)\disc8271.obj"
	-@erase "$(INTDIR)\disc8271.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\mode7font.obj"
	-@erase "$(INTDIR)\mode7font.sbr"
	-@erase "$(INTDIR)\sysvia.obj"
	-@erase "$(INTDIR)\sysvia.sbr"
	-@erase "$(INTDIR)\uservia.obj"
	-@erase "$(INTDIR)\uservia.sbr"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(INTDIR)\via.obj"
	-@erase "$(INTDIR)\via.sbr"
	-@erase "$(INTDIR)\video.obj"
	-@erase "$(INTDIR)\video.sbr"
	-@erase "$(OUTDIR)\BeebEm.bsc"
	-@erase "$(OUTDIR)\BeebEm.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /FR /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS"\
 /FR"$(INTDIR)/" /Fp"$(INTDIR)/BeebEm.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/"\
 /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
RSC_PROJ=/l 0x809 /fo"$(INTDIR)/BeebEm.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/BeebEm.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\6502core.sbr" \
	"$(INTDIR)\atodconv.sbr" \
	"$(INTDIR)\beebmem.sbr" \
	"$(INTDIR)\beebsound.sbr" \
	"$(INTDIR)\beebstate.sbr" \
	"$(INTDIR)\beebwin.sbr" \
	"$(INTDIR)\disc8271.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\mode7font.sbr" \
	"$(INTDIR)\sysvia.sbr" \
	"$(INTDIR)\uservia.sbr" \
	"$(INTDIR)\via.sbr" \
	"$(INTDIR)\video.sbr"

"$(OUTDIR)\BeebEm.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib /nologo /subsystem:windows /profile /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib /nologo\
 /subsystem:windows /profile /debug /machine:I386 /out:"$(OUTDIR)/BeebEm.exe" 
LINK32_OBJS= \
	"$(INTDIR)\6502core.obj" \
	"$(INTDIR)\atodconv.obj" \
	"$(INTDIR)\BeebEm.res" \
	"$(INTDIR)\beebmem.obj" \
	"$(INTDIR)\beebsound.obj" \
	"$(INTDIR)\beebstate.obj" \
	"$(INTDIR)\beebwin.obj" \
	"$(INTDIR)\disc8271.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mode7font.obj" \
	"$(INTDIR)\sysvia.obj" \
	"$(INTDIR)\uservia.obj" \
	"$(INTDIR)\via.obj" \
	"$(INTDIR)\video.obj"

"$(OUTDIR)\BeebEm.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "BeebEm - Win32 Release"
# Name "BeebEm - Win32 Debug"

!IF  "$(CFG)" == "BeebEm - Win32 Release"

!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\video.cpp
DEP_CPP_VIDEO=\
	".\6502core.h"\
	".\beebmem.h"\
	".\beebwin.h"\
	".\main.h"\
	".\mode7font.h"\
	".\port.h"\
	".\sysvia.h"\
	".\via.h"\
	".\video.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\video.obj" : $(SOURCE) $(DEP_CPP_VIDEO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\video.obj" : $(SOURCE) $(DEP_CPP_VIDEO) "$(INTDIR)"

"$(INTDIR)\video.sbr" : $(SOURCE) $(DEP_CPP_VIDEO) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\beebmem.cpp
DEP_CPP_BEEBM=\
	".\6502core.h"\
	".\atodconv.h"\
	".\beebwin.h"\
	".\disc8271.h"\
	".\main.h"\
	".\port.h"\
	".\sysvia.h"\
	".\uservia.h"\
	".\via.h"\
	".\video.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebmem.obj" : $(SOURCE) $(DEP_CPP_BEEBM) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebmem.obj" : $(SOURCE) $(DEP_CPP_BEEBM) "$(INTDIR)"

"$(INTDIR)\beebmem.sbr" : $(SOURCE) $(DEP_CPP_BEEBM) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\beebsound.cpp
DEP_CPP_BEEBS=\
	".\6502core.h"\
	".\beebsound.h"\
	".\port.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebsound.obj" : $(SOURCE) $(DEP_CPP_BEEBS) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebsound.obj" : $(SOURCE) $(DEP_CPP_BEEBS) "$(INTDIR)"

"$(INTDIR)\beebsound.sbr" : $(SOURCE) $(DEP_CPP_BEEBS) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\beebwin.cpp
DEP_CPP_BEEBW=\
	".\6502core.h"\
	".\atodconv.h"\
	".\beebmem.h"\
	".\beebsound.h"\
	".\beebstate.h"\
	".\beebwin.h"\
	".\disc8271.h"\
	".\main.h"\
	".\port.h"\
	".\sysvia.h"\
	".\uservia.h"\
	".\via.h"\
	".\video.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebwin.obj" : $(SOURCE) $(DEP_CPP_BEEBW) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebwin.obj" : $(SOURCE) $(DEP_CPP_BEEBW) "$(INTDIR)"

"$(INTDIR)\beebwin.sbr" : $(SOURCE) $(DEP_CPP_BEEBW) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\disc8271.cpp
DEP_CPP_DISC8=\
	".\6502core.h"\
	".\disc8271.h"\
	".\port.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\disc8271.obj" : $(SOURCE) $(DEP_CPP_DISC8) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\disc8271.obj" : $(SOURCE) $(DEP_CPP_DISC8) "$(INTDIR)"

"$(INTDIR)\disc8271.sbr" : $(SOURCE) $(DEP_CPP_DISC8) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\main.cpp
DEP_CPP_MAIN_=\
	".\6502core.h"\
	".\atodconv.h"\
	".\beebmem.h"\
	".\beebsound.h"\
	".\beebwin.h"\
	".\disc8271.h"\
	".\port.h"\
	".\sysvia.h"\
	".\uservia.h"\
	".\via.h"\
	".\video.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"

"$(INTDIR)\main.sbr" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mode7font.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\mode7font.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\mode7font.obj" : $(SOURCE) "$(INTDIR)"

"$(INTDIR)\mode7font.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\sysvia.cpp
DEP_CPP_SYSVI=\
	".\6502core.h"\
	".\beebsound.h"\
	".\port.h"\
	".\sysvia.h"\
	".\via.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\sysvia.obj" : $(SOURCE) $(DEP_CPP_SYSVI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\sysvia.obj" : $(SOURCE) $(DEP_CPP_SYSVI) "$(INTDIR)"

"$(INTDIR)\sysvia.sbr" : $(SOURCE) $(DEP_CPP_SYSVI) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\uservia.cpp
DEP_CPP_USERV=\
	".\6502core.h"\
	".\port.h"\
	".\uservia.h"\
	".\via.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\uservia.obj" : $(SOURCE) $(DEP_CPP_USERV) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\uservia.obj" : $(SOURCE) $(DEP_CPP_USERV) "$(INTDIR)"

"$(INTDIR)\uservia.sbr" : $(SOURCE) $(DEP_CPP_USERV) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\via.cpp
DEP_CPP_VIA_C=\
	".\via.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\via.obj" : $(SOURCE) $(DEP_CPP_VIA_C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\via.obj" : $(SOURCE) $(DEP_CPP_VIA_C) "$(INTDIR)"

"$(INTDIR)\via.sbr" : $(SOURCE) $(DEP_CPP_VIA_C) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\6502core.cpp
DEP_CPP_6502C=\
	".\6502core.h"\
	".\atodconv.h"\
	".\beebmem.h"\
	".\beebsound.h"\
	".\disc8271.h"\
	".\port.h"\
	".\sysvia.h"\
	".\uservia.h"\
	".\via.h"\
	".\video.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\6502core.obj" : $(SOURCE) $(DEP_CPP_6502C) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\6502core.obj" : $(SOURCE) $(DEP_CPP_6502C) "$(INTDIR)"

"$(INTDIR)\6502core.sbr" : $(SOURCE) $(DEP_CPP_6502C) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\BeebEm.rc

"$(INTDIR)\BeebEm.res" : $(SOURCE) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\atodconv.cpp
DEP_CPP_ATODC=\
	".\6502core.h"\
	".\atodconv.h"\
	".\port.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\atodconv.obj" : $(SOURCE) $(DEP_CPP_ATODC) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\atodconv.obj" : $(SOURCE) $(DEP_CPP_ATODC) "$(INTDIR)"

"$(INTDIR)\atodconv.sbr" : $(SOURCE) $(DEP_CPP_ATODC) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\beebstate.cpp
DEP_CPP_BEEBST=\
	".\6502core.h"\
	".\atodconv.h"\
	".\beebmem.h"\
	".\beebsound.h"\
	".\beebstate.h"\
	".\port.h"\
	".\sysvia.h"\
	".\uservia.h"\
	".\via.h"\
	".\video.h"\
	

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebstate.obj" : $(SOURCE) $(DEP_CPP_BEEBST) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebstate.obj" : $(SOURCE) $(DEP_CPP_BEEBST) "$(INTDIR)"

"$(INTDIR)\beebstate.sbr" : $(SOURCE) $(DEP_CPP_BEEBST) "$(INTDIR)"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
