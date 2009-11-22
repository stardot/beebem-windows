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

The copyright for Beebem is held by David Alan Gilbert and the other authors
and contributors (listed below).  BeebEm is distributed under the terms of
the GNU General Public License, as described in COPYRIGHT.txt.

If you have any problems, questions or suggestions for improvements please
email me here:

  beebem.support@googlemail.com


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


BeebEm FAQ
----------

Some commonly asked questions:

Q1. How do you change the default folder for loading disks or tapes?

A1. On the Options menu, enable this option:

      Preference Options -> Save Disc/Tape/State Folders

    BeebEm will remember the folder you last loaded a disk or tape from
    and default to that the next time you select load.

Q2. How do you open the menus in full screen mode?

A2. Move the mouse cursor to the top left of the screen and press Alt+F to
    open the File menu.

Q3. How do I set up different preferences for different games?

A3. When a disk, tape or state file is run from the command line (or you
    double click on one of these files) BeebEm will check for a
    Preferences.cfg and Roms.cfg file in the same folder as the image file.
    You can create separate folders for the games that required different
    preferences, copy the Preferences.cfg and Roms.cfg files into these
    folders and then setup the preferences as required in BeebEm and save
    them.  BeebEm will also check for file specific Preference and Roms
    files when running a disk image and loading a state file using the
    menus.


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
software.  The BBC Micro operating system, BASIC language and disc filing
system are all supplied on ROM.

The BBC Micro and Master 128 (and BeebEm) support up to 16 ROM slots plus
one for the operating system.  The BBC Micro ROMs are kept in subdirectories
of the 'BeebFile' directory.  The is one subdirectory for each of the four
models emulated.

The ROMs that BeebEm loads are configured in the 'Roms.cfg' file.  You can
edit the ROM configuration within BeebEm using the "Edit ROM Configuration"
option on the Hardware menu.  Note that you usually have to press the Break
key (F12) before a new ROM configuration is recognised.  Changes to the ROM
configuration can be saved to the Roms.cfg file or another file.

To run a ROM you will need to type a command into BeebEm.  The exact command
will depend on the ROM itself and you may need to refer to any documentation
that accompanied the ROM.  Some ROMs list the commands available when you
type '*HELP' into BeebEm.

Any of the 16 ROM slots can be configured as RAM.  Some software
(e.g. games) can make use of this RAM so its a good idea to leave at least
one slot as RAM.

ROMs are not normally writable but some ROM software was supplied on small
expansion boards that had some RAM on them.  These ROMs should be marked as
writable in the Roms.cfg file (use the "Make Writable" option in ROM
configuration).

Note that certain ROMs must be present for each emulation mode to work so do
not remove these:

 BBC Model B          - OS12.ROM, BASIC2.ROM, DNFS.ROM
 BBC Model B IntegraB - OS12.ROM, IBOS.ROM, BASIC2.ROM, WDFS.ROM
 BBC Model B Plus     - B+MOS.ROM, BASIC2.ROM, WDFS.ROM
 Master 128           - MOS.ROM, TERMINAL.ROM, BASIC4.ROM, DFS.ROM

The default set of ROMs configured for the Master 128 mode are MOS 3.20.
The MOS 3.50 ROMs are also included.  To use them either rename the
BeebFile/M128 directory to M128-MOS3.2 and rename the M128-MOS3.5 directory
M128 or create a new configuration using the ROM Configuration option.


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

There are some shortcut keys for various features:

  keypad +      Increase emulation speed
  ALT +         Increase emulation speed
  keypad -      Decrease emulation speed
  ALT -         Decrease emulation speed
  keypad /      Quick save state
  ALT 1         Quick save state
  keypad *      Quick load state
  ALT 2         Quick load state
  ALT keypad 5  Capture screen to file (with NumLock on)
  ALT 5         Capture screen to file
  ALT ENTER     Toggle fullscreen mode

With a logical mapping the key symbols are mapped directly so you get what
you press.  Note that the logical mapping sometimes has to change the shift
key state in order to work so it can do some unexpected things if you use it
while playing a game that uses shift.  Its probably better to use default
mapping when playing games.

Key mappings are kept in .kmap files stored in the My Documents\BeebEm area.
BeebEm will read the Default.kmap and Logical.kmap files at start up.  These
files contain mappings for a UK PC keyboard and they can be replaced with
alternative mapping files if you are not using a UK keyboard (for example,
the USLogical.kmap file for a US keyboard).

You can create your own mapping to map your PC keyboard to your emulated BBC
one.  Do this as follows:

(1) If you are in full screen mode then switch back to Windowed mode.

(2) Select menu item "Options -> Define User Key Mapping".  A graphic
    showing the BBC keyboard layout will appear within the BeebEm interface.

(3) Use your mouse pointer to click once on the BBC key that you are
    attempting to map to your PC keyboard.
      e.g. Click BBC key 8(

(4) Decide which PC keys you want to map to the unshifted BBC key press and
    which PC key you want to map to the shifted BBC key press.  It could be
    the same PC key for both or it could be different keys.
      e.g. For unshifted BBC key 8( press you would select PC key 8*
           For shifted BBC key 8( press you would select PC key 9(

(5) Now press the PC key you selected for the unshifted BBC key press.
      e.g. Press PC key 8*

(6) If the PC key you selected for the BBC shifted key press requires the PC
    to be shifted then check the 'shift' box in the key dialog.  Now press
    the PC key you selected for the shifted BBC key press (but do not press
    shift).
      e.g. Check the 'shift' box and press PC key 9(

(7) Repeat from step 3 for other keys you want to map.

(8) Save your mapping using menu item "Options -> Save User Key Mapping".
    You can write over the default user key mapping file (DefaultUser.kmap)
    or save a new file.

(9) Select your mapping using menu item "Options -> User Defined Mapping".
	You can also use the "Save Preferences" option to save the default user
	key mapping file that gets loaded when BeebEm starts up.


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
ADFS-1.30.rom to the ROM configuration.

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
files).  The disks can be used in Model B (need to add ADFS-1.30.rom to the
ROM configuration), B Plus or Master 128 mode.  To use the disk images
enable hard drive emulation on the hardware menu and press A & F12 to boot
in ADFS mode (ignore the error for drive 4 if in Master mode).  The hard
disks then appear as drives 0 to 3.  Floppy disks can still be accessed as
drives 4 and 5.

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
to communicate with other.  BeebEm also supports the AUN Econet protocols
that enable BeebEm to communicate with other AUN compatible machines.

Econet support is enabled from the Hardware menu in BeebEm.  Also switch off
'Freeze when inactive' if you intend to run multiple instances of BeebEm so
they can all run at the same time.

Each machine attached to an econet has a station number (between 1 and 254)
associated with it.  An econet may also have a file server attached to it
(special station number 254).

BeebEm emulates an econet using IP datagrams.  Each station on the network
needs a unique IP address and port number combination.  Each instance of
BeebEm needs to know the address and port number of the other instances on
the network, this information is configured in the 'Econet.cfg' and 'AUNMap'
files.

The default Econet.cfg file has been set up to enable AUN mode.  Read the
notes in the Econet.cfg and AUNMap files for more information on configuring
AUN mode.

If you just want to run a few instances of BeebEm on a single PC then the
simplest way to is to edit the Econet.cfg file and disable AUN mode (set
AUNMODE to 0) and uncomment the example network configuration for IP
addresses 127.0.0.1 (near the end of Econet.cfg).  With this configured the
first 5 instances of BeebEm run on a single PC will get allocated unique
station and port numbers.  The first instance will be allocated a station
number 254 (file server) and next 4 will be allocated station numbers 101 to
104.  See the notes in Econet.cfg for more details.

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


Master 512 Co-Processor Support
-------------------------------
Due to code licensing issues the Master 512 Co-Processor emulation is not
included in this version of BeebEm.  The Master 512 Co-Processor emulation
is included in BeebEm version 4.03, available on the BeebEm website.


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

You will need to add the ATS-3.0-1.rom to the ROM configuration and enable
Teletext emulation on the hardware menu to get Teletext to work.


Speech Generator
----------------
Due to code licensing issues the TMS5220 Speech Generator emulation is not
included in this version of BeebEm.  The Speech Generator emulation is
included in BeebEm version 4.03, available on the BeebEm website.


Serial Port IP Options
----------------------

The serial port emulation in BeebEm has options to connect via TCP/IP to a
local or remote IP address.  You can use ROMs such as CommSoft or CommStar
to connect to a Viewdata BBS, MUD server or any other type of server.

The serial IP features are selected via the IP destinations on the RS423
Comms menu.  The "IP: localhost:25232" option is designed for use with the
"tcpser" package, which emulates a modem.  Download a prebuilt Windows
binary (and RC11 sources) here:

  http://www.mkw.me.uk/beebem/tcpser.zip
  (tcpser home is: http://www.jbrain.com/pub/linux/serial)

Run the "go.bat" file to fire up an instance suitable for the localhost
setting to talk to.  This will also allow incoming connections, as that does
all the answering business and passes the caller onto the Beeb via the
pre-existing connection.  This handles the handshake lines if you enable
ip232 mode, so dropping RTS will drop an outward connection.  Similarly DCD
going up will be passed through to CTS.

Add the Commstar.rom to your ROM configuration (see "ROM Software" above) 
and start up BeebEm.  On the Comms menu select the "IP: localhost:25232" 
and "RS432 On/Off".  Type *COMMSTAR to start CommStar.  In Commstar:
  - Press M to switch to mode 0
  - Press A to turn off Auto line feed
  - Press I and then R a few times to set the receive baud to 9600
  - Press C to enter chat mode
You should now be able send commands to the modem (tcpser), try typing "AT",
you should get "OK" back.  You can now "dial" into a server using the "ATD"
command.  Try connecting to the Ishar MUD server using "ATDishar.com:9999".

To make a direct connection to a server select the "IP: Custom destination"
option.  This requires you to manually edit the "IP232customip" and
"IP232customport" parameters in the Preferences.cfg file and reload.

An additional option, "IP: IP232 mode", determines if the handshake lines
are sent down the TCP/IP link or just generated locally from the presence of
a valid connection.

A second additional option, "IP: RAW comms", disables special processing of
charater 255 (used with tcpser, so leave RAW mode off when using tcpser).

If on startup, or when ticking the "RS423 On/Off" menu option, BeebEm cannot
connect to the specified server, or if it loses connection subsequently, it
will pop up a messagebox and disable RS423.  Select the menu option again to
try to re-connect and re-enable.

BeebEm will emulate the correct RX baud rate.  This makes for a very
realistic experience when talking at 1200 baud to a Viewdata host!  TX is
just sent out as fast as it will go.


Realtime Clock Support
----------------------
The RTC can be used in Master 128 mode.  The BASIC TIME$ variable can be
used to read and update the clock:
  PRINT TIME$
  TIME$="Mon,22 Jan 1996"
  TIME$="23:10:42"
  TIME$="Mon,22 Jan 1996.23:10:42"


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
  Save State     saving your position in a game for example.  State files 
                 are put in the 'BeebState' directory by default.

  Quick Load   - Quickly load or save BeebEm state without having to 
  Quick Save     specify the filename.  The state is saved and loaded from 
                 the 'BeebState/quicksave.uef' file.  Quicksave now keeps
                 the last 10 quicksave files so you can go back to an
                 earlier state using the load state menu option.
                 There are keyboard shortcuts for Quicksave (keypad /) and
                 Quickload (keypad *) so you can save and load without 
                 having to go through the menus.

  Screen Capture Options - Select the resolution and file format for screen 
                   capture to file.
  Capture Screen - Capture the BeebEm screen to a file.  You will be 
                   prompted for a file name.  Note that you can also use
                   the ALT+keypad 5 shortcut to capture the screen to file.

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

Edit Menu:

  Copy              - Copy BASIC program to the clipboard. Sets the printer
                      destination to the clipboard, enables the printer 
                      output and lists the current program.
  Paste             - Pastes the clipboard content into BeebEm.
  Translate CR-LF   - Adds/removes linefeed characters as text is copied and
                      pasted from the clipboard.

  Import Files to Disc   - Allows files to be added to a DFS disc image. 
                           BeebEm will look for a .INF file containing file 
                           attributes but files without .INF files can also
                           be imported.  If a file name matches one already 
                           in the disc image the imported file will 
                           overwrite the one in the image.
  Export Files from Disc - Allows files to be exported from a DFS disc
                           image. BeebEm will create a .INF file for each 
                           file exported to hold the file attributes.

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
                        Select an IP option to route data to a TCP/IP 
                        port (see "Serial Port IP Options" above).

View Menu:

  Display Renderer    - Selects how BeebEm draws to the screen.
                        DirectX9 will probably be the fastest.

  DirectX Smoothing   - When using DirectDraw or DirectX9 enabling smoothing
                        will switch on bilinear interpolation.  This will
                        blur the display slightly which will give it a 
                        smoother look.

  Smooth Teletext Only - Enable DX smoothing for mode 7 only.

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

  Sound Effects  - Options for switching on emulation of cassette motor, tape 
                   software loading and disc drive noise.

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

  Allow SW RAM Write  - Enable/disable writes for each sideways RAM slot.
                        See the "ROM Software" section above.

  SW RAM Board On/Off - Enables Solidisk SW RAM board emulation.  The RAM
                        bank that is enabled for writing is selected via the
                        User VIA port B.  Set port B bits 0-3 to output
                        (e.g. ?&FE62=15) and select the bank via port B
                        (e.g. ?&FE60=4).

  Edit ROM Configuration - Allow you to edit the ROM configuration.
                        Note that you will usually have to press the 
                        Break key (F12) before a new ROM configuration
                        is recognised.

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

  Econet On/Off       - Switch Econet emulation on or off.  See the Econet 
                        section above for more details.

  Teletext On/Off     - Switch Teletext emulation on or off.  See the 
                        Teletext section above for more details.

  Floppy Drive On/Off - Switch floppy drive emulation on or off.

  Hard Drive On/Off   - Switch SCSI/SASI hard drive emulation on or off.
                        See the SCSI/SASI section above for more details.

  User Port Breakout Box - Opens the user port breakout box dialog.  The 
                        breakout box allows keys to be assigned for switch 
                        box emulation.

  User Port RTC Module - Enables the real time clock module.  This is used 
                         by the Econet file server software.

  RTC Y2K Adjust On/Off - When enabled the Master 128 real time clock is 
                          adjusted by 20 years to allow for the Y2K issues 
                          in the MOS.

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

  Define User Key Mapping - Allows you to define your own keyboard mapping.
                            See the Keyboard Mappings section above.

  Load User Key Mapping - Loads a user defined key mapping from a file.

  Save User Key Mapping - Saves a user defined key mapping to a file.

  User Defined Mapping - Selects the currently loaded user defined keyboard
                         mapping.

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

  Autosave CMOS RAM    - Automatically saves the Master 128 CMOS RAM 
                         on exit from BeebEm.

  Autosave All Prefs   - Automatically saves all preferences on exit from
                         BeebEm.

  Save Disc/Tape/State Folders - When enabled BeebEm will remember where
                         the folders where you last loaded or saved disc,
                         tape or state files.

  Select User Data Folder - Select the location for the BeebEm data files.
                         When creating a new folder BeebEm will copy a 
                         default set of data files into the folder.
                         BeebEm will default to using the last folder 
                         selected.

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
  -DisMenu
     - Disables the drop down menus
  -KbdCmd <keyboard command string>
  -NoAutoBoot
     - Disable auto-boot when disk image specified
  -DebugScript <file containing debugger commands>
     - Opens the debugger and executes the commands
  <disk image file name>
  <tape file name>
  <state file name>

Note: Command line options -Model and -Tube are no longer supported.
      Use the -Prefs option with different preferences files instead.

e.g. Run Zalaga with its own preferences:
   BeebEm -Prefs ZalagaPrefs.cfg Zalaga.ssd

e.g. Run the Torch Z80 with its own preferences:
   BeebEm -Prefs Torch.cfg -Roms Roms_Torch.cfg

e.g. Run BeebEm with an alternative set of data files:
   BeebEm -Data \Users\Mike\Documents\BeebEmGames

e.g. To run BeebEm from a USB drive and access the local USB drive data:
   BeebEm -Data -

e.g. To load and run the test tape image:
   BeebEm -KbdCmd "OSCLI\s2\STAPE\s2\S\nPAGE=3584\nCH.\s22\S\n" test.uef

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
it will be run automatically (but see -NoAutoBoot option).  The name can
include the full path or it can just be the name of a file in the 'DiscIms'
or 'BeebState' directory.

If a tape image name is passed to BeebEm on the command line it will be
loaded.  The name can include the full path or it can just be the name of a
file in the 'Tapes' directory.  To run the tape image use the -KbdCmd option.

The -KbdCmd option allows a string of key presses to be passed to BeebEm.
The string can include the following key sequences:

  \n           - press and release of enter/return
  \s           - press shift
  \S           - release shift
  \c           - press control
  \C           - release control
  \\           - press and release of \ key
  0-9          - press and release of a number key
  A-Z          - press and release of a letter key
  '-=[];'#,./  - press and release of a symbol key
  \dNNNN       - set inter-keypress delay to NNNN milliseconds.
                 Note that very low delays can result in loss of presses.

So, for example, run a tape image using commands such as:
   BeebEm -KbdCmd "OSCLI\s2\STAPE\s2\S\nPAGE=3584\nCH.\s22\S\n" test.uef
   BeebEm -KbdCmd "OSCLI\s2\STAPE\s2\S\nOSCLI\s2\SRUN\s2\S\n" game.uef

If you create a shortcut to BeebEm.exe on your desktop and edit the
properties (right click the icon) you can add command line options and a
disc, tape or state file name to the target box.  Running the shortcut will
then run the file.

You can also associate files with .ssd, .dsd or .uef extensions with BeebEm
using Windows Explorer.  Double clicking on one of these files will
automatically run it in BeebEm.


Registry Information
--------------------

When BeebEm is installed the following registry entries are created:

  HKEY_LOCAL_MACHINE\SOFTWARE\BeebEm\InstallPath
  HKEY_LOCAL_MACHINE\SOFTWARE\BeebEm\Version

These can be used by other applications to integrate with BeebEm.


Debugger
--------

The Debugger is a 6502 disassembler and monitoring tool.  When the debug
window is first opened it leaves BeebEm running.  Its best to switch off the
'Freeze when inactive' option though otherwise BeebEm will only run when you
bring the main window to the front.

Debugger controls:

Break           - Stops BeebEm running.  If currently executing in the OS or
                  ROM area and OS/ROM debug is not enabled then BeebEm will 
                  only stop when execution moves out of the OS/ROM.  When 
                  execution stops the current instruction is displayed.
Continue/Cancel - Starts BeebEm running.
Trace           - Shows accesses to the various bits of hardware.
Break           - Breaks execution when the hardware is accessed.
BRK instruction - Breaks when the BRK instruction is executed.
Attach debugger to:
  Host          - Debugs the host processor.
  Parasite      - Debugs the second processor.
  ROM           - Debugs the ROM code (addresses 8000-BFFF).
  OS            - Debugs the OS code (addresses C000-FFFF).
Breakpoints     - Breaks execution when the address hits one of the 
                  configured breakpoints.
Watches         - Shows contents of configured memory locations.
Execute Command - Runs the debug command entered into the command box.

To get a list of the debug commands available type ? into the Execute
Command box.  Notes that all values are specified in hex!

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
  12K Additional RAM

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
from http://www.microsoft.com).  For Windows 98 you will need to install an
older releases of DirectX 9.0c, such as the Aug 2006 release.

When using DirectX9 in full screen mode the menu bar is not visible.  The
menus can still be opened by using the shortcut keys (such as Alt-F for the
File menu) but you will also need to click the mouse in the menu bar area
(top left of screen) to bring the menu in front of the BeebEm screen (some
repeated clicking is sometimes required!)


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
  Steve Pick
  Greg Cook
  Theo Lindebaum
  David Sharp
  Rich Talbot-Watkins

Thanks also to Jordon Russell Software for the excellent Inno Setup
installation software: <http://www.jrsoftware.org/isinfo.php>

If there are any other features you would like to see in the Windows version
of BeebEm then send me an email.  I cannot promise anything but I may find
some time to add them.

Mike Wyatt
beebem.support@googlemail.com
http://www.mkw.me.uk/beebem

Copyright (C) 2009  Mike Wyatt
