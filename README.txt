BeebEm for Microsoft Windows
============================

BeebEm is a BBC Micro and Master 128 emulator.  It enables you to run BBC
Micro software on your PC.  BeebEm should work on most PC systems running
Windows 98 or later.

BeebEm is distributed with everything you need to get going, just run BeebEm
from the Start menu (or run BeebEm.exe if you unzipped it).  If BeebEm does
not run see the troubleshooting section below.

BeebEm is distributed with source code in the Src.zip file so you can
compile and modify BeebEm yourself.

The copyright on everything in BeebEm resides with David Gilbert, the
original author, as described in COPYRIGHT.txt.


Running BBC Micro Software
--------------------------

BeebEm runs BBC Micro software held in disc or tape image files kept on your
PC's hard disc.  Some disc images are supplied with BeebEm, in the 'DiscIms'
directory.  To run a disc image use the 'Run Disc' option on the File menu.

You can download more disc images from internet sites such as these:

  http://www.stairwaytohell.com/
  http://www.rubybay.com/users/drbeeb/
  http://www.users.waitrose.com/~sharx/beeb.htm
  http://www.nvg.ntnu.no/bbc/
  http://www.iancgbell.clara.net/elite/

Put the disc image files into the DiscIms directory where BeebEm was
installed or unzipped.  The default location is:

  C:\Program Files\BeebEm


Configuring BeebEm
------------------

To get BeebEm running at an optimal speed there are some configuration
options that can be changed.  BeebEm will display the speed of emulation
relative to a real BBC Micro and the number of frames per second (fps) being
displayed.  You should see these values in the window title bar or at the
bottom of the screen in full screen mode.

You will usually get best performance (and best looking screen) by enabling
the following options on the View menu:

* DirectDraw ON
* Buffer in Video RAM ON

You may also find that the best fps rate is achieved in full screen mode by
selecting:

* 32-bit DirectDraw ON
* Full screen ON

Choosing a high resolution DirectDraw Full Screen Mode will improve the look
of the screen.

If you have a fast PC you can also improve the sound quality by selecting
the 44.1kHz sample rate on the Sound menu.

For further information on speeding up BeebEm see the notes for the
'Hardware' menu below.

Once you have decided on a set up you can save it using 'Options - Save
Preferences'.


ROM Software
------------

Although most games software is supplied on disc images there is a large
selection of ROM software available for the BBC Micro.  There are
application ROMs such as word processors and drawing packages, there are
programing language ROMs such as FORTH and BCPL, and a whole range of other
software.  The BBC Micro's operating system, BASIC language and disc filing
system are all supplied on ROM.

The BBC Micro and Master 128 (and BeebEm) support up to 16 ROM slots (plus
one for the operating system).  The BBC Micro ROMs are kept in
subdirectories of the 'BeebFile' directory.  The is one subdirectory for
each of the four models emulated.

The ROMs that BeebEm loads are configured in the 'Roms.cfg' file.  You can
edit this file using Notepad to add a ROM.  Change one of the 'EMPTY' lines
to the name of the ROM file you have put one of the BeebFile directories.
The first 17 lines in Roms.cfg are for the BBC Micro Model B emulation, the
second 17 are for the IntegraB emulation, the third for the B Plus emulation
and the last 17 for the Master 128 emulation.

To run a ROM you will need to type a command into BeebEm.  The exact command
will depend on the ROM itself and you may need to refer to any documentation
that accompanied the ROM.  Some ROMs list the commands available when you
type '*HELP' into BeebEm.

Any of the 16 ROM slots can be configured as RAM.  Some software
(e.g. games) can make use of this RAM so its a good idea to leave at least
one slot as RAM.

Note that certain ROMs must be present for each emulation mode to work so do
not remove these:

 BBC Model B          - OS12.ROM, BASIC2.ROM, WDFS.ROM
 BBC Model B IntegraB - OS12.ROM, IBOS.ROM, BASIC2.ROM, WDFS.ROM
 BBC Model B Plus     - B+MOS.ROM, BASIC2.ROM, WDFS.ROM
 Master 128           - MOS.ROM, TERMINAL.ROM, BASIC4.ROM, DFS.ROM


Keyboard Mappings
-----------------

There are two main keyboard mappings available on the Options menu, default
mapping and logical mapping.

For default mapping most of the keys are the same on the Beeb and PC but
these are not:

   PC               Beeb

   F10 & F11        f0
   F1-F9            f1-f9
   F12              Break
   -_               -=
   =+               ^~
   `                @
   #~               _
   ;:               ;+
   '@               :*
   End              Copy

With logical mapping the key symbols are mapped directly (for a UK PC
keyboard at least) so you get what you press.  Note that logical mapping
sometimes has to change the shift key state in order to work so it can do
some unexpected things if you use it while playing a game that uses shift.
Its probably better to use default mapping when playing games.

If you do not use a UK keyboard then you may want to set up your own
mapping.  Use the 'user keyboard' mapping options in BeebEm to do this and
remember to save it using the 'save preferences' option.

The keypad +/- keys will change between the BeebEm fixed speeds.


Using Disc Images
-----------------

BeebEm emulates two double sided floppy disc drives, know as drives 0 and 1
(the second sides are drive 2 and 3).  You can load a disc image file into
either drive 0 or 1 using the File menu in BeebEm.

Some useful disc commands are:

  *CAT              - List files on the disc
  *DRIVE <drive>    - Change default drive to number <drive>
  *TITLE <title>    - Put a title on the disc
  *DELETE <file>    - Delete a file
  LOAD "<file>"     - Load a BASIC program
  CHAIN "<file>"    - Load and run a BASIC program
  SAVE "<file>"     - Save a BASIC program
  *RUN <file>       - Load and run a binary program

To boot (i.e. run) a disc you press Shift & F12 (hold the shift key down and
press F12).  If a disc does not boot it is probably because it has not been
set up to be bootable.

A bootable disc needs to have a file called !BOOT on it and it needs to be
configured to run the !BOOT file when its booted.  If a disc image has a
!BOOT file on it but it does not boot then try running the following command
(you will need to switch off write protection first):

  *OPT4,3

If you want to write data to a disc image (e.g. save a game position) you
will need to make sure that 'Write Protect' is switched off on the File menu
for the drive you are using.  Disc writes will then write data straight back
to the image file on your PC disc.

If you want to use ADFS discs in the Master 128 mode then load the ADFS disk
image and type '*ADFS' to mount it.  You can also press A & F12 to mount an
ADFS disc.

You can find out more information about using discs by reading the DFS (Disc
Filing System) guides available on the internet.

WARNING - some of the disc images you will find on the Internet or that you
create yourself will have an invalid catalogue (they contain just enough
information for disc reads to work but not writes).

When you write enable a disc BeebEm checks the disc catalogues and it will
display a warning message if either are invalid.  If you get a warning
message then do NOT write to the disc image, you will loose data.

To fix an invalid catalogue do the following:

1. Start BeebEm and load the invalid image into drive 1.

2. Create a new disc image file in drive 0.

3. If the invalid image uses a Watford DFS 62 file catalogue then
   format the new disc image with a 62 file catalogue (you need the
   Watford DFS ROM installed), type:
     *ENABLE
     *FORM80 0

4. Copy all the files from the invalid disc to the new one and set the
   shift break option and title, type:
     *COPY 1 0 *.*
     *OPT4,3
     *TITLE <title>

The new image will now have a valid catalogue.  If you are using double
sided images then remember to format and copy both sides (the second sides
of disc 0 and 1 are 2 and 3).


IDE Disk Support
----------------

Jon Welch has added some preliminary support for emulated IDE hard disks to
BeebEm.  There are 4 blank IDE drive images, pre-formatted to 4MB each, in
the discims directory.  To use the images you need to change the ADFS ROM
used in Master 128 mode to a modified version (produced by Jonathan
Harston).  Change "ADFS.rom" in the Roms.cfg file to "ADFS1-53.rom".

After selecting Master 128 emulation, press A & F12 to boot in ADFS mode and
type:
     *MOUNT 0

This will mount the IDE disks.  The hard disks can then be used as drives 0
to 3.  Floppy disks can still be accessed as drives 4 to 7.

For more infomation on the patched ADFS ROM see:
     http://www.mdfsnet.f9.co.uk/Info/Comp/BBC/IDE/ADFS/

Jon Welch has written a tool for building IDE drive images, its called HADFS
Explorer and can be downloaded from Jon's web page:
     http://www.g7jjf.com


Using Tape Images
-----------------

BeebEm emulates a cassette recorder.  You can load a tape image file using
the File menu in BeebEm.

Some useful tape commands are:

  *TAPE             - Select tape filing system
  *CAT              - List files on the tape
  LOAD ""           - Load a BASIC program
  CHAIN ""          - Load and run a BASIC program
  SAVE "<file>"     - Save a BASIC program
  *RUN              - Load and run a binary program

Most tapes can be run by typing:

  *TAPE
  CH.""

if that does not work, rewind the tape and try:

  *TAPE
  *RUN

Tape loading can be quite slow so BeebEm has menu options to artificially
speed up loading.  You can also use the speed menu (or keypad +/-) to speed
up the whole emulation, which speeds up tape loading.

BeebEm has a Tape Control window that enables you to move the tape position
around by clicking on the block you want to load next.  If a block is missed
when loading from a tape you will need to move the tape position back to
retry the block.

If you want to save files to a tape image then you can use the 'Record'
button on the Tape Control.  If you have a tape loaded then pressing Record
will append saved files to the end of the tape image.  If you want to create
a new tape image then eject the current tape and press Record.  The tape
block list will be updated when recording finishes (press Stop when the save
completes).


Menu Options
------------

File Menu:

  Run Disc     - Loads a disc image into drive 0 and boots (runs) it.

  Load Disc 0  - Load a disc image into drive 0 or 1.  Discs are write
  Load Disc 1    protected when loaded to prevent any accidental data loss.
                 Boot a disc by pressing Shift & F12.

  Load Tape    - Load a tape image.  To load software off a tape type:
                   *TAPE
                   PAGE=&E00
                   CH.""

  New Disc 0   - Creates a new disc image in drive 0 or 1.  Use the file
  New Disc 1     type field to select the type of disc image to create.
                 New disc images are write enabled when created.  The images
                 have a standard 31 file catalogue by default.  If you want 
                 a 62 file catalogue (Watford DFS) then format the disc.

  Write Protect 0 - Toggles write protection for drive 0 or 1.  Keep discs
  Write Protect 1   write protected unless you intend to write to them.
                    Also see the WARNING above in the 'Using Disc Images'
                    section.

  Reset        - "Power-On" reset for when a game crashes.

  Load State   - Load or save the state of BeebEm.  This is useful for 
  Save State     saving your position in a game for example.  Note that ROM
                 data is not saved so if you have changed the ROMs that are
                 loaded when BeebEm starts (in Roms.cfg) then a restored
                 state may not work.  State files are put in the 'BeebState'
                 directory by default.

  Quick Load   - Quickly load or save BeebEm state without having to 
  Quick Save     specify the filename.  The state is saved and loaded from 
                 the 'BeebState/quicksave.uef' file.

  Video Options - Select the resolution and how many frames get skipped for 
                  video capture.  If video and audio get out of sync when
                  playing back then try selecting a lower resolution and 
                  lower frame rate (more skipped frames).
  Capture Video - Start video capture.  Video is recorded at a maximum of 25
                  frames per second at a resolution of 640x512.  Sound is
                  captured at the sample rate selected in the sound menu. If
                  sound is disabled when capture is started then sound is
                  not recorded in the AVI file.  If you have Windows XP you 
                  can edit the AVIs using Movie Maker and save them as more
                  compact WMV files.
  End Video     - Stops video capture.

  Exit         - Exit BeebEm

Comms Menu:

  Tape Speed          - Select the speed at which tape software loads and 
                        saves.
  Rewind Tape         - Reset the tape position to the start.
  Unlock Tape         - Removed the lock flag from files as they are loaded.
                        This enables you to *LOAD a locked file.
  Tape Control        - Opens the tape control (see Tape section above).

  Printer On/Off      - Switches printer capture on or off.  To start and 
                        stop printing within BeebEm use the VDU2 and VDU3 
                        commands or Ctrl B and Ctrl C.

  Printer Destination - Select where to send the printer output.
                        WARNING - if you direct printer output to an LPT 
                        port that is not attached to anything BeebEm may
                        hang.

  RS423 On/Off        - Switches the Beeb's serial communications port on 
                        or off.

  RS423 Destination   - Select where to send the serial port data.

View Menu:

  DirectDraw On/Off   - Switches DirectDraw screen output on or off. 
                        DirectDraw should be faster than normal output but 
                        this is not always the case.  Choose whatever gives 
                        the best frame rate.

  Buffer In Video RAM - When using DirectDraw each frame can be prepared in
                        either video or system RAM.  This option switches
                        between the two.  Choose whatever gives the best
                        frame rate or look.

  32-bit DirectDraw   - Switches between 8 bit and 32 bit full screen mode.
                        A 32 bit mode may look better than 8 bit.

  Speed and FPS On/Off - Show or hide the relative speed and the number of
                         frames per second.

  Full Screen         - Switch to full screen mode.

  Window Sizes        - Sets the window size.  The bigger the window the
                        slower it gets!

  DirectDraw Full Screen Mode - Screen resolution to use in DD full screen 
                        mode.  The higher resolutions may look better.

  Monitor Type        - Selects the type of monitor to emulate.

  Hide Menu           - Hides the menu.  Makes full screen mode look just 
                        like a real Beeb!

  LEDs                - The Beeb keyboard and disc (1770 only) LEDs can be 
                        shown at the bottom of the BeebEm window.

  Motion Blur         - Fades out contents of previous frames rather than
                        blanking them out.  Can improve flicker in some 
                        games.  The % intensities of the 8 frames can be 
                        edited in the following registry key:
                        HKEY_CURRENT_USER\Software\BeebEm\
                                                MotionBlurIntensities

Speed Menu:

  Real Time    - Runs BeebEm at the same speed as a real BBC Micro.

  Fixed Speed  - Runs BeebEm at a fixed speed relative to a real BBC Micro.
                 The frame rate may need to be reduced for the higher 
                 speeds.  The keypad +/- keys will change between fixed
                 speeds.

  50 FPS       - Runs BeebEm at a constant frame rate.  The slower the frame
  25 FPS         rate the faster BeebEm runs relative to a BBC Micro.
  10 FPS
  5 FPS
  1 FPS

Sound Menu:

  Sound On/Off   - Switch sound on or off.

  Sound Chip     - Switches the sound chip on or off.  Useful when you want
                   to hear the cassette sounds.

  Sound Effects  - Switches on the sound of the cassette motor and the
                   sound of tape software loading.

  44.1 kHz       - Sets the sound sample rate.  The higher it is the better
  22.05 kHz        the sound quality but the slower BeebEm runs.
  11.025 kHz

  Full Volume    - Set the sound volume.
  High Volume
  Medium Volume
  Low Volume

  Use Primary Buffer - Grabs exclusive use of your sound card so any other 
                   applications playing music will stop.  May make BeebEm
                   run faster.

  Part Samples   - Smooths sound sampling.  Using part samples usually
                   sounds better.

AMX Menu:

  On/Off          - Switch AMX mouse on or off.  Note that you may find the
                    AMX mouse easier to use if you reduce the BeebEm speed
                    to 0.9 or 0.75 speed.  It may also be useful to hide the
                    Windows cursor as well (see the Options menu).

  L+R for Middle  - Simulates a middle button press when you press the left
                    and right buttons together.

  Map to 160x256  - Coordinate range to map the Windows mouse position to.
  Map to 320x256    Pick the one that gives AMX mouse movements nearest to
  Map to 640x256    your Windows mouse movements.

  Adjust +50%     - Percentage to increase or decrease the AMX map sizes.
  Adjust +30%       Pick the one that gives AMX mouse movements that are
  Adjust +10%       slightly greater than the corresponding Windows mouse
  Adjust -10%       movements.  You can then match up the AMX and Windows
  Adjust -30%       pointer positions by moving the Windows pointer to the
  Adjust -50%       edges of the BeebEm Window.  This is easiest to do in
                    full screen mode.

Hardware Menu:

  BBC Model           - Allows you to switch between the BBC Model B types
                        and Master 128 emulation.

  Model B Floppy Controller - Allows you to select the Model B disc hardware
                        emulation.  To use an alternative floppy disc 
                        controller (FDC) board you will need to use an
                        appropriate ROM (e.g. Acorn DFS2.26 for the Acorn 
                        1770 board).

  65C02 Second Processor - Enables/disables the 65C02 second processor 
                        emulation.  Note that in Master 128 mode the second 
                        processor  may be disabled in the CMOS settings.
                        To enable it type this command and press 
                        break (F12):
                            *CONFIGURE TUBE

  Torch Z80 Second Processor - Enables/disables the Z80 second processor 
                        emulation.  See the README_Z80.TXT file for more
                        details.

  Allow ROM writes    - Enable/disable ROM writes for each ROM slot.
                        ROMs read at start-up are write protected by
                        default.  Some ROM software was supplied on 
                        small expansion boards that had some RAM on them.
                        These ROMs require write access in order to work.

  Ignore Illegal Instructions - When disabled a dialog appears detailing
                                the opcode and program counter.

  Undocumented Instructions - Some games use undocumented instructions so
                        try enabling the full set if something does not 
                        work.  Selecting documented instructions only may
                        speed BeebEm up.

  Teletext Half Mode  - Uses half the number of screen lines for teletext
                        mode 7.  May speed BeebEm up.

  Basic Hardware Only - Switches off Analogue to Digital (Joystick) and 
                        Serial (printing, comms & tape) emulation.  May 
                        speed BeebEm up.

  Sound Block Size    - Selects the amount of sound data that is written 
                        to the sound buffers at any one time.  The smaller
                        block size may speed BeebEm up.

Options Menu:

  Joystick             - Switch on or off PC/Beeb analogue joystick support.
                         Calibrate the joystick through the control panel.

  Mousestick           - Switch on or off the mapping of Mouse position to
                         Beeb joystick.

  Freeze when inactive - When selected BeebEm will freeze when you switch 
                         to another Window.

  Hide Cursor          - Show or hide the mouse cursor while it is over the
                         BeebEm window (useful when using the Mousestick or
                         the AMX mouse).

  Define User Keyboard - Allows you to define your own keyboard mapping.
                         Click on one of the BBC Micro keys and then press
                         the key you want mapped to it (most will already
                         be mapped to the correct keys).  Once you have
                         defined the keys you want select the 'user mapping'
                         and remember to save it using the 'Save 
                         Preferences' option.

  User Defined Mapping - Selects the user defined keyboard mapping.

  Default Keyboard Mapping - See the Keyboard Mappings section above.
  Logical Keyboard Mapping

  Map A,S to CAPS,CTRL - Maps the A and S keys to CAPS and CTRL keys on the
                         Beeb keyboard. This is good for some games
                         (e.g. Zalaga).

  Map F1-F10 to f0-f9  - Selects a slightly different mapping for the 
                         function keys.

  Debugger             - Opens the debugger window (see below).

  Save Preferences     - Saves the BeebEm menu options so they are preserved
                         for when you need start BeebEm.

Help Menu:

  View REAME           - View this file.

  About BeebEm         - Show version number and date of BeebEm.


Command Line Options
--------------------

The file name of a disc image or a state file can be passed to BeebEm on the
command line and it will be run automatically.  The name can include the
full path or it can just be the name of a file in the 'DiscIms' or
'BeebState' directory.

If you create a shortcut to BeebEm.exe on your desktop and edit the
properties (right click the icon) you can add a disc or state file name to
the target box.  Running the shortcut will then run the file.

You can also associate files with .ssd, .dsd or .uef extensions with BeebEm
using Windows Explorer.  Double clicking on one of these files will
automatically run it in BeebEm.


Debugger
--------

The Debugger is a 6502 disassembler and monitoring tool.  When the debug
window is first opened it leaves BeebEm running.  Its best to switch off the
'Freeze when inactive' option though otherwise BeebEm will only run when you
bring the main window to the front.

Debugger controls:

Break Execution - Stops BeebEm running.  If currently executing in the OS or
                  ROM area and OS/ROM debug is not enabled then BeebEm will 
                  only stop when execution moves out of the OS/ROM.  When 
                  execution stops the current instruction is displayed.
Restart         - Starts BeebEm running.
Trace           - Shows accesses to the various bits of hardware.
Break           - Breaks execution when the hardware is accessed.
Debug Host      - Debugs the host processor.
Debug Parasite  - Debugs the second processor.
Debug ROM       - Debugs the ROM code (addresses 8000-BFFF).
Debug OS        - Debugs the OS code (addresses C000-FFFF).
Breakpoints     - Breaks execution when the address hits one of the 
                  configured breakpoints.
Execute Command - Runs the debug command entered into the command box.

Debug commands ([] = optional parameter):

n [count]            - Execute the next [count] instructions.
m[p] [start] [count] - Memory hex dump.  Use 'mp' to dump parasite memory.
d[p] [start] [count] - Disassemble instructions.  Use 'dp' for parasite.
b start [end]        - Set/clear break point.  Can be a single address or
                       a range of addresses.
sv                   - Show video registers.
su                   - Show user via registers.
ss                   - Show system via registers.
st                   - Show tube registers.

The disassembler shows the following information:

  Address OPCodes Instruction A X Y Flags

Parasite instructions are shifted right so it easier to follow both host and
parasite when debugging tube code.


Emulated Hardware
-----------------

The hardware emulated by BeebEm is that of a standard BBC Micro Model B, a
Model B with IntegraB board, Model B Plus or Master 128 with a few small
additions.  An optional 65C02 second processor is also emulated.  The
emulation is near enough accurate to run most software.

Hardware common to all the Model B types and Master 128:

  74689 Sound chip with 3 tone channels and one noise channel.
  uPD7002 Analogue to Digital Converter
  32K RAM
  6845 Cathode Ray Tube Controller (CRTC)
  Acorn proprietary VIDPROC (Video Processor)
  SAA5050 Teletext generator
  System and User 6522 Versatile Interface Adaptors (VIAs)
  "IC32" Addressable latch
  Full BBC Micro keyboard
  ROMSELect Register

Model B Specific hardware:

  Full 6502 Processor with all undocumented opcodes
  Sixteen 16K Paged ROM banks, with Sideways RAM option
  16K OS ROM
  8271 Floppy Disc Controller

Model B Plus Specific hardware:

  Four 16K Sideways RAM banks
  16K B+ MOS ROM
  20K Video Shadow RAM
  8K Filing System RAM
  4K Screen Operations RAM

Master 128 Specific hardware:

  65C12 Processor with all undocumented opcodes filled in with
    corresponding undocumented 6502 opcodes
  Seven 16K Paged ROMs
  Four 16K Sideways RAM banks
  16K MOS ROM
  20K Video Shadow RAM
  8K Filing System RAM
  4K Screen Operations RAM
  1770 Floppy Disc controller
  146818 Realtime Clock and CMOS RAM (50 bytes)
  ACCess CONtrol register

65C02 Second Processor:

  65C12 Processor
  64K on RAM
  2K boot ROM
  TUBE ULA chip

For information on the IntegraB see the documentation in the 'Documents'
directory.


Troubleshooting
---------------

If BeebEm reports a problem with DirectX or DirectSound when starting up
then try downloading the latest version of the DirectX software from
Microsoft's web page:

  http://www.microsoft.com/windows/directx/

If BeebEm reports that it cannot find a file when its starting up then try
re-installing BeebEm.

If you have any other problems then email me and I'll try to help out:

  mikebuk@dsl.pipex.com


Uninstalling BeebEm
-------------------

If you wish to uninstall BeebEm then use the 'Add or Remove Programs' option
in the Windows Control Panel/Settings.  Note that any files you have placed
in the BeebEm directory yourself will not be removed.


Authors
-------

Thanks go to the following people for contributing to BeebEm:

  David Gilbert
  Nigel Magnay
  Robert Schmidt
  Mike Wyatt
  Laurie Whiffen
  Richard Gellman
  Ken Lowe
  Jon Welch

If there are any other features you would like to see in the Windows version
of BeebEm then send me an email.  I cannot promise anything but I may find
some time to add them.

Mike Wyatt
mikebuk@dsl.pipex.com
http://www.mikebuk.dsl.pipex.com/beebem
