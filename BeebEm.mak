# Microsoft Developer Studio Generated NMAKE File, Based on BeebEm.dsp
!IF "$(CFG)" == ""
CFG=BeebEm - Win32 Release
!MESSAGE No configuration specified. Defaulting to BeebEm - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "BeebEm - Win32 Release" && "$(CFG)" != "BeebEm - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BeebEm.mak" CFG="BeebEm - Win32 Release"
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

!IF  "$(CFG)" == "BeebEm - Win32 Release"

OUTDIR=.\intelbin
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\intelbin
# End Custom Macros

ALL : "$(OUTDIR)\BeebEm.exe"


CLEAN :
	-@erase "$(INTDIR)\6502core.obj"
	-@erase "$(INTDIR)\atodconv.obj"
	-@erase "$(INTDIR)\BeebEm.res"
	-@erase "$(INTDIR)\beebmem.obj"
	-@erase "$(INTDIR)\beebsound.obj"
	-@erase "$(INTDIR)\beebstate.obj"
	-@erase "$(INTDIR)\beebwin.obj"
	-@erase "$(INTDIR)\cRegistry.obj"
	-@erase "$(INTDIR)\disc1770.obj"
	-@erase "$(INTDIR)\disc8271.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\mode7font.obj"
	-@erase "$(INTDIR)\serial.obj"
	-@erase "$(INTDIR)\sysvia.obj"
	-@erase "$(INTDIR)\userkybd.obj"
	-@erase "$(INTDIR)\uservia.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\via.obj"
	-@erase "$(INTDIR)\video.obj"
	-@erase "$(OUTDIR)\BeebEm.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /Gr /MTd /W3 /GX /Ox /Ot /Og /Oi /Oy- /Ob2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /Fp"$(INTDIR)\BeebEm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\BeebEm.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\BeebEm.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib ddraw.lib dsound.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\BeebEm.pdb" /machine:I386 /out:"$(OUTDIR)\BeebEm.exe" 
LINK32_OBJS= \
	"$(INTDIR)\6502core.obj" \
	"$(INTDIR)\atodconv.obj" \
	"$(INTDIR)\beebmem.obj" \
	"$(INTDIR)\beebsound.obj" \
	"$(INTDIR)\beebstate.obj" \
	"$(INTDIR)\beebwin.obj" \
	"$(INTDIR)\cRegistry.obj" \
	"$(INTDIR)\disc1770.obj" \
	"$(INTDIR)\disc8271.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mode7font.obj" \
	"$(INTDIR)\sysvia.obj" \
	"$(INTDIR)\userkybd.obj" \
	"$(INTDIR)\uservia.obj" \
	"$(INTDIR)\via.obj" \
	"$(INTDIR)\video.obj" \
	"$(INTDIR)\BeebEm.res" \
	"$(INTDIR)\serial.obj"

"$(OUTDIR)\BeebEm.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

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
	-@erase "$(INTDIR)\cRegistry.obj"
	-@erase "$(INTDIR)\cRegistry.sbr"
	-@erase "$(INTDIR)\disc1770.obj"
	-@erase "$(INTDIR)\disc1770.sbr"
	-@erase "$(INTDIR)\disc8271.obj"
	-@erase "$(INTDIR)\disc8271.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\mode7font.obj"
	-@erase "$(INTDIR)\mode7font.sbr"
	-@erase "$(INTDIR)\serial.obj"
	-@erase "$(INTDIR)\serial.sbr"
	-@erase "$(INTDIR)\sysvia.obj"
	-@erase "$(INTDIR)\sysvia.sbr"
	-@erase "$(INTDIR)\userkybd.obj"
	-@erase "$(INTDIR)\userkybd.sbr"
	-@erase "$(INTDIR)\uservia.obj"
	-@erase "$(INTDIR)\uservia.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\via.obj"
	-@erase "$(INTDIR)\via.sbr"
	-@erase "$(INTDIR)\video.obj"
	-@erase "$(INTDIR)\video.sbr"
	-@erase "$(OUTDIR)\BeebEm.bsc"
	-@erase "$(OUTDIR)\BeebEm.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /G5 /MDd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\BeebEm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\BeebEm.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\BeebEm.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\6502core.sbr" \
	"$(INTDIR)\atodconv.sbr" \
	"$(INTDIR)\beebmem.sbr" \
	"$(INTDIR)\beebsound.sbr" \
	"$(INTDIR)\beebstate.sbr" \
	"$(INTDIR)\beebwin.sbr" \
	"$(INTDIR)\cRegistry.sbr" \
	"$(INTDIR)\disc1770.sbr" \
	"$(INTDIR)\disc8271.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\mode7font.sbr" \
	"$(INTDIR)\sysvia.sbr" \
	"$(INTDIR)\userkybd.sbr" \
	"$(INTDIR)\uservia.sbr" \
	"$(INTDIR)\via.sbr" \
	"$(INTDIR)\video.sbr" \
	"$(INTDIR)\serial.sbr"

"$(OUTDIR)\BeebEm.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib dsound.lib ddraw.lib  uef.lib /nologo /subsystem:windows /profile /debug /machine:I386 /out:"$(OUTDIR)\BeebEm.exe" 
LINK32_OBJS= \
	"$(INTDIR)\6502core.obj" \
	"$(INTDIR)\atodconv.obj" \
	"$(INTDIR)\beebmem.obj" \
	"$(INTDIR)\beebsound.obj" \
	"$(INTDIR)\beebstate.obj" \
	"$(INTDIR)\beebwin.obj" \
	"$(INTDIR)\cRegistry.obj" \
	"$(INTDIR)\disc1770.obj" \
	"$(INTDIR)\disc8271.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mode7font.obj" \
	"$(INTDIR)\sysvia.obj" \
	"$(INTDIR)\userkybd.obj" \
	"$(INTDIR)\uservia.obj" \
	"$(INTDIR)\via.obj" \
	"$(INTDIR)\video.obj" \
	"$(INTDIR)\BeebEm.res" \
	"$(INTDIR)\serial.obj"

"$(OUTDIR)\BeebEm.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("BeebEm.dep")
!INCLUDE "BeebEm.dep"
!ELSE 
!MESSAGE Warning: cannot find "BeebEm.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "BeebEm - Win32 Release" || "$(CFG)" == "BeebEm - Win32 Debug"
SOURCE=.\6502core.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\6502core.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\6502core.obj"	"$(INTDIR)\6502core.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\atodconv.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\atodconv.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\atodconv.obj"	"$(INTDIR)\atodconv.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\BeebEm.rc

"$(INTDIR)\BeebEm.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\beebmem.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebmem.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebmem.obj"	"$(INTDIR)\beebmem.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\beebsound.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebsound.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebsound.obj"	"$(INTDIR)\beebsound.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\beebstate.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebstate.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebstate.obj"	"$(INTDIR)\beebstate.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\beebwin.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\beebwin.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\beebwin.obj"	"$(INTDIR)\beebwin.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\cRegistry.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\cRegistry.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\cRegistry.obj"	"$(INTDIR)\cRegistry.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\disc1770.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\disc1770.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\disc1770.obj"	"$(INTDIR)\disc1770.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\disc8271.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\disc8271.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\disc8271.obj"	"$(INTDIR)\disc8271.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\main.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\mode7font.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\mode7font.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\mode7font.obj"	"$(INTDIR)\mode7font.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\serial.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\serial.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\serial.obj"	"$(INTDIR)\serial.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\sysvia.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\sysvia.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\sysvia.obj"	"$(INTDIR)\sysvia.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\userkybd.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\userkybd.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\userkybd.obj"	"$(INTDIR)\userkybd.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\uservia.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\uservia.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\uservia.obj"	"$(INTDIR)\uservia.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\via.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\via.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\via.obj"	"$(INTDIR)\via.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\video.cpp

!IF  "$(CFG)" == "BeebEm - Win32 Release"


"$(INTDIR)\video.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "BeebEm - Win32 Debug"


"$(INTDIR)\video.obj"	"$(INTDIR)\video.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 


!ENDIF 

