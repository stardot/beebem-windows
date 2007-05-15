BeebEm for Microsoft Windows
============================

BeebEm is a BBC Micro and Master 128 emulator.  It enables you to run BBC
Micro software on your PC.  BeebEm should work on most PC systems running
Windows 98 or later (DirectX9 and IE6 required for Windows 98/ME).

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
PC's hard disc.  The image files are kept in a set of folders in your 
'My Documents\BeebEm' area.  Some disc images are supplied with BeebEm, in
the 'DiscIms' folder.  To run a disc image use the 'Run Disc' option on the
File menu.

You can download more disc images from internet sites such as these:

  http://www.stairwaytohell.com/
  http://www.bbcmicrogames.com/
  http://www.iancgbell.clara.net/elite/

Put the disc image files into the 'DiscIms' directory.


Configuring BeebEm
------------------

To get BeebEm running at an optimal speed there are some configuration
options that can be changed.  BeebEm will display the speed of emulation
relative to a real BBC Micro and the number of frames per second (fps) being
displayed.  You should see these values in the window title bar or at the
bottom of the screen in full screen mode.

You will usually get best performance (and best looking screen) by enabling
the following options on the View menu:

* Select the DirectX9 renderer
* Switch on DirectX Smoothing

You may also find that the best fps rate is achieved in full screen mode by
selecting:

* Full screen ON

Choosing a high resolution DirectX Full Screen Mode will improve the look of
the screen.

[See the Troubleshooting section for notes on menu use in full screen mode.]

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

 BBC Model B          - OS12.ROM, BASIC2.ROM, DNFS.ROM
 BBC Model B IntegraB - OS12.ROM, IBOS.ROM, BASIC2.ROM, WDFS.ROM
 BBC Model B Plus     - B+MOS.ROM, BASIC2.ROM, WDFS.ROM
 Master 128           - MOS.ROM, TERMINAL.ROM, BASIC4.ROM, DFS.ROM

The default set of ROMs configured for the Master 128 mode are MOS 3.20.
The MOS 3.50 ROMs are also included.  To use them rename the BeebFile/M128
directory to M128-MOS3.2 and rename the M128-MOS3.5 directory M128.


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

The keypad / and * keys will save and load a quickstate file.


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

If you want to use ADFS discs in the Master 128 mode then load an ADFS disk
image (such as the MasterWelcome.adl image in discims) and type '*ADFS' to
mount it.  You can also press A & F12 to mount an ADFS disc.

ADFS is also supported in Model B and B Plus modes.  Note that in Model B
mode you will need to select the Acorn 1770 disc controller and add 
ADFS-1.30.rom to Roms.cfg.

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


SCSI & SASI Disk Support
------------------------

BeebEm supports emulated SCSI and SASI hard disks.  There are 4 10MB
pre-formatted ADFS SCSI disk images in the discims directory (the scsi?.dat
files).  The disks can be used in Model B (need to add ADFS-1.30.rom to
Roms.cfg), B Plus or Master 128 mode.  To use the disk images enable hard
drive emulation on the hardware menu and press A & F12 to boot in ADFS mode
(ignore the error for drive 4 if in Master mode).  The hard disks then
appear as drives 0 to 3.  Floppy disks can still be accessed as drives 4 and
5.

There are some demo tunes in scsi0.dat which you can listen to with the PLAY
program.

There is also the sasi0.dat file which is a Torch Z80 hard disc image (see
README_Z80.TXT).


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


Econet Support
--------------

BeebEm emulates an econet network.  This allows multiple instances of BeebEm
to communicate with other.  Enable econet support from the Hardware menu in
BeebEm.  Also switch off 'Freeze when inactive' so multiple instances of
BeebEm can run at the same time.

Each machine attached to an econet has a station number (between 1 and 254)
associated with it.  An econet may also have a file server attached to it
(special station number 254).

BeebEm emulates an econet using IP datagrams.  Each station on the network
needs a unique IP address and port number combination.  Each instance of
BeebEm needs to know the address and port number of the other instances on
the network, this information is configured in the 'Econet.cfg' file.

The default Econet.cfg file has been set up so that the first 5 instances of
BeebEm run on a single PC will get allocated unique station and port
numbers.  The first instance will be allocated a station number 254 (file
server) and next 4 will be allocated station numbers 101 to 104.  See the
notes in Econet.cfg for more details.

An econet file server enables a disk file system to be shared by all of the
stations on the network.  Acorn produced various versions of file server
software.  The level 1 and 2 file server software is included in disk images
with BeebEm.  The server software must be run on station number 254.

To run the level 1 server, load the 'econet_level_1_utils.ssd' disk image
and run the file server by typing 'CH."FS"'.

You will be prompted for:
 - Number of files per user - enter 3
 - Manual configuration - enter Y
 - Directory for first station - enter A
 - Station number - enter 101
 - Directory for second station - enter B
 - Station number - enter 102
 - Directory for third station - just press enter

The file server will then start running and report that it is ready.

The level 2 server is a little more complicated to set up.  It requires the
6502 second processor support to be enabled and requires a special "master"
disk to be formatted (must be a double sided disk).  I suggest you read the
"The Econet Level 2 Fileserver Manager's Guide" (available here
http://www.bbcdocs.com/) if you want to set up a level 2 server.

To use the econet file system from a station you will need to have a ROM
installed that supports the econet file system (such as the Acorn DNFS ROM
installed in Model B mode).  To switch a station into econet file system
mode press N & F12.  The econet station number should be shown on screen.
To switch without pressing the break key type '*NET'.  You can then use
commands such as *CAT, LOAD, SAVE and CHAIN to access files on the server.

The econet emulation in BeebEm is not perfect and is susceptible to timing
issues.  When a message is being sent between stations BeebEm waits for a
short time for the econet to become free again before sending the next
message.  If the delay is too small messages can interfere with each other
and cause a "Net Error" to be reported.  If it is too long a station can
report "Line Jammed".  The waiting time is set by default to 100000 cycles
(0.05s) but can be overridden by a command line option '-EcoFF'.  If you see
"Net Error" report try a '-EcoFF 500000', for "Line Jammed" try '-EcoFF
50000'.


Master 512 Co Processor Support
-------------------------------
The software now supports the Master 512 Co-Processor board.

The Master 512 board was an add-on card to the BBC computers which allowed you
to run DOS Plus (based on CP/M but largely MS-DOS compatible) software. 
It was 80186 based, ran at 10 MHz came with 512K of RAM (or 1MB with third 
party upgrades) and connected to the BBC via the Tube interface.

You start the Master 512 by selecting 80186 Second Processor off the hardware
menu. This will boot off a 10MB hard disc image (ADFS Drive 0) which comes
pre-installed with the original DOS Plus system and the GEM application software.
The emulator will also recognise Drive A and B if a suitable DOS Plus formatted
disc is loaded. The emulator should recognise various disc formats (look at
the DISK program for more info) but I would only recommend using 640K ADFS .adl
format and 800K DOS Plus .img format discs.

GEM can be started by typing 'GEM' from the command prompt. GEM uses the AMX
mouse support which much be enabled for the mouse to work. I would recommend you
start GEM first then enable AMX Mouse support afterwards. Please note that mouse
support works but due to the acceleration factors built into the GEM driver, 
mouse movement can be a bit erratic. I hope to improve this in future versions 
of the emulator.

The Master 512 should run most MS-DOS 2.1 compatible software but no guarantees
are given for what will and what won't work.

I will start a library of known good disc images at :

http://www.g7jjf.com/512_disc_images.htm

Please feel free to send me any more you know work with the Master 512.


Acorn Z80 Co Processor Support
------------------------------
The software now supports the Acorn Z80 Co-Processor board.

The Acorn Z80 was a CP/M based system running with 64KB RAM on a Z80
processor.

You start the Acorn Z80 by selecting Acorn Z80 Co-Processor off the hardware
menu. If you load a CP/M system disc in drive 0, the system will then boot
to an A: prompt.

I have included a CP/M Utilities disc in the installation which will boot
the system. The Acorn Z80 originally came with 7 discs which can be download
from my web site at :

http://www.g7jjf.com/acornz80_disc_images.htm

These discs contain the Acorn Z80 application software including :

CPM Utilities
BBC Basic
Mallard Basic
CIS Cobol
MemoPlan
GraphPlan
FilePlan
Accountant
Nucleus

You will also find some manuals for this software on my site as well.

The Acorn Z80 should also run most CP/M compatible software but no guarantees
are given for what will and what won't work.

I will start a library of known good disc images at :

http://www.g7jjf.com/acornz80_disc_images.htm

Please feel free to send me any more you know work with the Acorn Z80.


Acorn ARM Second Processor Support
----------------------------------
The software now supports the Acorn ARM Second Processor board, also
known as the ARM Evaluation System.

The ARM interpreter and disassembler code is based on software written
by David Sharp to whom I am grateful for allowing me to use it with BeebEm.

The Acorn ARM Second Processor was an 8MHz system running with 4MB RAM on an
ARM 1 RISC processor.

You start the Acorn ARM by selecting ARM Second Processor off the hardware
menu. The ARM Second Processor has no operating system but relies on a sideways
ROM to provide a command line prompt to the user. Once you see the A* prompt, type
*HELP SUPERVISOR to see what inbuilt commands are available.

The ARM Evaluation System came with 6 discs full of software. I have included disc
3 in the installation which contains ARM Basic. Load the armdisc3.adl disc image
(ensuring you are in ADFS mode), type *MOUNT 4 and *AB to load ARM Basic.
The remaining disc images plus a hard disc image containing all 6 disc images can be
downloaded from my web site at :

http://www.g7jjf.com/acornArm_disc_images.htm

These discs contain the Acorn ARM application software including :

TWIN - Editor
ARM Assembler/Linker/Debugger
ARM Basic
Lisp
Prolog
Fortran 77
General File Utilities

You will also find some manuals for the ARM Second Processor on my site as well but
if you have any manuals or other disc images I don't have, please feel free to send me 
copies to add to the library.


Teletext Support
----------------
The software now supports the Acorn Teletext Adapter. Unfortunately, I
haven't got my teletext server software finished yet so the emulation
can only work from teletext data captured to disc files. You can download
some sample teletext data from :

http://www.g7jjf.com/teletext.htm

You will need to add the ATS-3.0-1.rom to Roms.cfg and enable Teletext
emulation on the hardware menu to get Teletext to work.


Speech Generator
----------------
The BBC B and B+ had the facility for installing a TMS5220 Speech Generator
chip and associated TMS6100 PHROM containing the digitised voice of the BBC
newsreader Kenneth Kendall. With these two chips installed, you could easily
make the computer speak using the BASIC SOUND command. BeebEm now supports
the TMS5220 and upto 16 different PHROM's. A new configuration file has been
created to specify which PHROM's are available. The configuration file is
phroms.cfg and contains a list of 16 PHROM filenames or EMPTY if a PHROM
is not available. Two PHROM's are currently included with the emulator and
are stored in the Phroms directory. They are PHROMA which was the original
Kenneth Kendall voice and PHROMUS which is an American voice speaking a
different dictionary.

To say a word, you use the SOUND command, eg :

SOUND -1, 179, 0, 0 would speak COMPUTER using PHROMA.

The -1 specifies the PHROM number to use, from -1 to -16 which corresponds
to each entry in the phroms.cfg file.

A list of words available in PHROMSUS is included in the Phroms directory for
reference. A list for PHROMA can be found in Appendix A and B of the manual
detailed below.

The Torch Z80 Co Processor also supports the speech system. Try the SNAKE game
off the system utilities disc. There is also BACKCHAT.

Please remember that the Master 128 doesn't support speech so you need to be
in Model B/B+ mode for speech to work.

For more information on using the Speech System you are referred to the Acorn 
Speech System User Guide which you can download from :

http://www.g7jjf.com/docs/acorn_speech_user_guide.pdf  (1202KB)

If anyone has any more PHROM images, can they please email them to me so
I can include them with the distribution.


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

  Eject Disc 0 - Ejects the disc image currently loaded.  The name of the 
  Eject Disc 1   file currently loaded is shown next to the menu option.

  Write Protect 0 - Toggles write protection for drive 0 or 1.  Keep discs
  Write Protect 1   write protected unless you intend to write to them.
                    Also see the WARNING above in the 'Using Disc Images'
                    section.

  Protect on Load - Indicates if a disc should be write protected when it 
                    is loaded.

  Reset        - "Power-On" reset for when a game crashes.

  Load State   - Load or save the state of BeebEm.  This is useful for 
  Save State     saving your position in a game for example.  Note that ROM
                 data is not saved so if you have changed the ROMs that are
                 loaded when BeebEm starts (in Roms.cfg) then a restored
                 state may not work.  State files are put in the 'BeebState'
                 directory by default.

  Quick Load   - Quickly load or save BeebEm state without having to 
  Quick Save     specify the filename.  The state is saved and loaded from 
                 the 'BeebState/quicksave.uef' file.  Quicksave now keeps
                 the last 10 quicksave files so you can go back to an
                 earlier state using the load state menu option.
                 There are keyboard shortcuts for Quicksave (keypad /) and
                 Quickload (keypad *) so you can save and load without 
                 having to go through the menus.

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
                        Select Microvitec Touch Screen to enable touch 
                        screen support.

View Menu:

  Display Renderer    - Selects how BeebEm draws to the screen.
                        DirectX9 will probably be the fastest.

  DirectX Smoothing   - When using DirectDraw or DirectX9 enabling smoothing
                        will switch on bilinear interpolation.  This will
                        blur the display slightly which will give it a 
                        smoother look.

  Speed and FPS On/Off - Show or hide the relative speed and the number of
                         frames per second.

  Full Screen         - Switch to full screen mode.

  Window Sizes        - Sets the window size.

  DirectX Full Screen Mode - Screen resolution to use in full screen 
                        mode.  The higher resolutions may look better.

  Monitor Type        - Selects the type of monitor to emulate.

  Hide Menu           - Hides the menu.  Makes full screen mode look just 
                        like a real Beeb!

  LEDs                - The Beeb keyboard and disc (1770 only) LEDs can be 
                        shown at the bottom of the BeebEm window.

  Motion Blur         - Fades out contents of previous frames rather than
                        blanking them out.  Can improve flicker in some 
                        games.  The % intensities of the 8 frames can be 
                        edited in the preferences file.

  Screen Reader Text View - Switch screen reader compatible text view 
                            on or off (see features for visually impaired 
                            users below)

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

  Exponential Volume - Enables an exponential volume scale.  Makes the 
                       sound output better.

  
  Text To Speech - Switch text to speech generation on or off (see features
                   for visually impaired users below)

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

  Econet On/Off       - Switch Econet emulation on or off.  See the Econet 
                        section above for more details.

  Teletext On/Off     - Switch Teletext emulation on or off.  See the 
                        Teletext section above for more details.

  Hard Drive On/Off   - Switch SCSI/SASI hard drive emulation on or off.
                        See the SCSI/SASI section above for more details.

  User Port Breakout Box - Opens the user port breakout box dialog.  The 
                        breakout box allows keys to be assigned for switch 
                        box emulation.

  User Port RTC Module - Enables the real time clock module.  This is used 
                         by the Econet file server software.

Options Menu:

  Joystick             - Switch on or off PC/Beeb analogue joystick support.
                         Calibrate the joystick through the control panel.

  Analogue Mousestick  - Switch on or off the mapping of Mouse position to
                         analogue Beeb joystick position.

  Digital Mousestick   - Switch on or off the mapping of Mouse movements to
                         digital Beeb joystick movements.

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

  Disable Keys         - Allows you to disable selected keys within BeebEm.
                         The Windows (start) keys can also be disabled but
                         note that this affects all Windows applications,
                         not just BeebEm.

  Debugger             - Opens the debugger window (see below).

  Save Preferences     - Saves the BeebEm settings to the current
                         preferences file.  Settings include the selected 
                         menu options, user defined keyboard, Window 
                         position and CMOS RAM contents.

Help Menu:

  View README          - View this file.

  About BeebEm         - Show version number and date of BeebEm.


Command Line Options
--------------------

The following command line options can be passed to BeebEm:

  -Data <data directory>
  -Prefs <preferences file name>
  -Roms <roms configuration file name>
  -EcoStn <Econet station number>
  -EcoFF <Econet flag fill timeout, see the econet section>
  <disk image file name>
  <state file name>

Note: Command line options -Model and -Tube are no longer support.  Use
      the -Prefs option with different preferences files instead.

e.g. Run Zalaga with its own preferences:
   BeebEm -Prefs ZalagaPrefs.cfg Zalaga.ssd

e.g. Run the Torch Z80 with its own preferences:
   BeebEm -Prefs Torch.cfg -Roms Roms_Torch.cfg

e.g. Run BeebEm with an alternative set of data files:
   BeebEm -Data \Users\Mike\Documents\BeebEmGames

e.g. To run BeebEm from a USB drive and access the local USB drive data:
   BeebEm -Data -

These command lines can be put into Windows shortcuts or Windows scripts.
See the BeebEmLocal.vbs and BeebEmTorch.vbs scripts for examples.

If the specified <data directory> does not exist BeebEm will offer to create
it.  BeebEm will copy a default set of data to the data directory.  The
default data directory is "My Documents\BeebEm".  If a data directory of "-"
is specified BeebEm will use the "UserData" directory where the BeebEm.exe
file exists (this is useful for running from a USB drive).

The preferences and ROMs configuration file names are relative to the data
directory.

If a disk image or a state file name is passed to BeebEm on the command line
it will be run automatically.  The name can include the full path or it can
just be the name of a file in the 'DiscIms' or 'BeebState' directory.

If you create a shortcut to BeebEm.exe on your desktop and edit the
properties (right click the icon) you can add command line options and a
disc or state file name to the target box.  Running the shortcut will then
run the file.

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
w file [count]       - Writes the last [count] lines of the debug window
                       to a file.  If count is not specified the entire
                       debug window contents are written.
                         e.g. w /disassembly.txt 64
c start byte [byte] ... - Change memory.  Writes bytes starting at a 
                          particular address.
                         e.g. c 7c00 68 65 6c 6c 6f

NOTE: All values are specified in hex!

The disassembler shows the following information:

  Address OPCodes Instruction A X Y SR Flags

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
  6854 Advanced Data-Link Controller (Econet)

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


Features for Visually Impaired Users
------------------------------------

BeebEm provides two features for use by visually impaired users:

1. A screen reader compatible text view.

2. Text to speech generation.

The screen reader compatible text view can be selected from the view menu.
When enabled the text on the BeebEm screen is converted to a standard
Windows text edit control.  Screen readers such as JAWS can move the edit
cursor around the screen and read the text in the control.  Note that BeebEm
can only convert text when in teletext mode 7.  In other modes the text edit
will contain the string "Not in text mode."  A special key press, ALT + `
(back quote) is available to synchronise the cursor position in the text
view with the BBC Micro cursor position.

The BeebEm text to speech generation can be selected from the sound menu.
When enabled BeebEm will use the text to speech capabilities of Windows XP
to read the text on the BeebEm screen.  Text convertion is driven using the
numeric keypad keys 0 to 9.  The key assignments are based on the basic JAWS
screen reading keys with some additions.  The key assignments are:

  num pad 5                  read current character.
  num pad 6                  read next character.
  num pad 4                  read prior character.

  insert + num pad 5         read current word.
  insert + num pad 6         read next word.
  insert + num pad 4         read prior word.

  insert + num pad 8         read current line.
  num pad 2                  read next line.
  num pad 8                  read prior line.
  insert + num pad 2         read entire screen (say all).

  alt + num pad 5            read current sentence.
  alt + num pad 2            read next sentence.
  alt + num pad 8            read prior sentence.

  num pad 9                  move speech cursor to top of screen.
  insert + num pad 9         speak speech cursor position.

  num pad 3                  move speech cursor to bottom of screen.
  insert + num pad 3         toggle speaking of punctuation.

  insert + num pad 1         toggle auto-read of text as it is written 
                             to the BBC micro screen.
  num pad 1                  read any buffered auto-read text.

If you are running JAWS you may find that it intercepts some of the key
presses listed above and stops BeebEm from receiving them.  In particular it
may intercept num pad 5 presses, so try num pad 7 instead as BeebEm
interprets num pad 7 in the same way as 5.  The BeebEm key assignments will
also work when num lock is switched on so you could try that as well.  You
may be able to configure JAWS so it does not say the num pad key names.

Under normal use you may find switching on the auto-read function useful
(insert + num pad 1).  This will read text as it gets written to the BBC
micro screen.  This works when using the BASIC command prompt and in many of
the text based adventure games available for the BBC micro.

As with the text view support the ALT + ` (back quote) key press is
available to synchronise the speech cursor position with the BBC Micro
cursor position.

END OF SECTION.


Troubleshooting
---------------

This version of BeebEm will work on Windows 98 or later.  If you have
Windows 98 or ME then you will need to install DirectX9 and IE6 (available
from http://www.microsoft.com).

When using DirectX9 in full screen mode the menu bar is not visible.  The
menus can still be opened by using the shortcut keys (such as Alt-F for the
File menu) but you will also need to click the mouse in the menu bar area
(top left of screen) to bring the menu in front of the BeebEm screen (some
repeated clicking is sometimes required!)

There is a problem in Vista with AVI video file generation.  The AVI files
are corrupted and cannot be played.  This will be fixed by Microsoft and may
be delivered via Windows Update or appear in Vista SP1.

If you have any problems please email me and I'll try to help out:

  mikebuk@dsl.pipex.com


Uninstalling BeebEm
-------------------

If you wish to uninstall BeebEm then select the 'Uninstall' option in the
BeebEm start menu options or use the 'Add or Remove Programs' option in the
Windows Control Panel/Settings.  Note that any files you have added to your
"My Documents\BeebEm" directory will not be removed.


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
  Rob O'Donnell

If there are any other features you would like to see in the Windows version
of BeebEm then send me an email.  I cannot promise anything but I may find
some time to add them.

Mike Wyatt
mikebuk@dsl.pipex.com
http://www.mikebuk.dsl.pipex.com/beebem
