BeebEm Change History
=====================

Unreleased changes
------------

Contributors: Chris Needham, Mauro Varischetti, Bill Carr, Daniel Beardsmore,
Steve Inglis, Alistair Cree, Ken Lowe, Mark Usher, Martin Mather, Tom Seddon,
Andrew Hague

* Added support for FSD format disk images. This currently only works
  with the 8271 and not the 1770 disk controller.
* Fixed support for double-sided SSD disk images (where side 1 tracks
  follow side 0, as opposed to DSD images, where tracks from side 0 and 1 are
  interleaved). This only works for 80-track double-sided SSDs, not 40-track.
* Added icons for disk and tape image file types. The installer now allows
  you to create file associations for these, and saved state files.
* BeebEm saved state files now use the .uefstate file extension, to distinguish
  them from UEF tape images.
* Saved state files now include the Master 512, Acorn Z80, Torch Z80, ARM,
  ARM7TDMI co-processor state, if currently selected.
* Econet improvements:
  - Added -EconetCfg and -AUNMap options, to allow the location of these
    config files to be set on the command line.
  - Fixed Econet.cfg file parsing.
  - Fixed potential buffer overflow bug.
  - Added support for immediate operations, used by the *VIEW and *NOTIFY
    commands. These currently only work under Master series emulation.
  - Fixed CTS status bit.
* Added Master ET support.
* ROM config files (e.g., Roms.cfg) can now include comments. Note that
  files with comments are not backwards compatible with older BeebEm
  versions.
* Added -Model and -Tube options, to allow the Beeb model and second processor
  to be set on the command line. These options override the settings in the
  preferences file.
* Keyboard mapping improvements:
  - Added Clear button to the key selection dialog box, when defining
    a user keyboard mapping.
  - Added Italian logical keyboard mapping file.
  - Shift Lock is now mapped to the F11 key by default.
* Tape emulation improvements:
  - Improved tape input data carrier detect emulation.
  - Tape state is now preserved when loading and saving savestate files,
    for both UEF and CSW files. Saving state is disabled while the tape
	is recording.
  - Fixed loading CSW files from the command line.
  - Fixed recording (appending) to existing tape image files.
* Improved serial port emulation, and fixed data loss when using
  serial over a TCP connection.
* Rewrote the Windows serial port implementation. Received characters
  are now buffered to prevent being lost.
* Added Set Keyboard Links command, which allows you to change the default
  screen mode and other options (Model B/B+ only).
* Fixed installer to not delete Econet and key map config files.
* Removed the BeebDiscLoad and BeebVideoRefreshFreq environment variables.
* Text to speech improvements:
  - Added menu items to select the text to speech voice.
  - The selected voice and speed are now stored in preferences.
  - Right-Ctrl can now be used instead of Insert to control text to speech
    output.
  - Speech output can now be stopped and the buffer cleared by pressing
    Alt+Num Pad 4.
* Added support for ROMs larger than 16kbytes, such as Inter-Word, that
  use a logic carrier board. Several of these ROM images are now included
  when you install BeebEm.
* Teletext adapter emulation improvements:
  - Improved teletext TCP/IP client resilience, to properly close the
    socket if server has disappeared, and to keep up with the latest
    data from the teletext stream to catch up after the emulation has
    been paused.
  - Added a dialog box to configure the file names or IP address and port
    to connect to.
* The "Freeze when inactive" option is now automatically switched off
  when opening the Debugger window, to ensure the emulation keeps running
  while the Debugger window is active.
* Added Integra-B IBOS1.26 ROM, and modified the default LANG / FILE
  parameters for compatibility with both IBOS1.26 and the original
  Computech IBOS1.20 / 1.20P ROMs. IBOS1.26 is now the default IBOS
  ROM, and Watford DFS has been replaced by Acorn DNFS, in Roms.cfg.
* Replaced the Acorn Z80 Co-processor ROM with Jonathan Harston's patched
  version 1.21, which includes a number of bug fixes.
* Rewrote the Master 146818 RTC and CMOS emulation.
* Improved the User Port real time clock (SAF3019P) emulation. Thanks to
  Ian Wolstenholme for confirming the register write behaviour.
* Fixed the DirectX9 graphics renderer, where BeebEm would revert to GDI
  rendering after the host PC resumes from being suspended or in some cases
  enter a loop continually resetting the screen mode on entering full screen.
* Fixed sound register reinitialisation after changing the sound sample rate.
* Fixed sound muting when the BeebEm window loses focus.
* Fixed teletext mode character alignment.
* Fixed printing to the Windows clipboard, which would previously cause the
  emulator to slow down or stall.
* When printing to file, the file is now appended to, not overwritten.
* Lots of help documentation improvements.

Version 4.19 (1 May 2023)
------------

Contributors: Chris Needham, Mike Wyatt, Steve Inglis, Alistair Cree

* Fixed NMOS 6502 instructions 6B (ARR imm) and EB (SBC imm).
* Fixed timings for instructions 83 (SAX (zp,X)), 8F (SAX abs),
  B3 (LAX (zp),Y), and BB (LAS abs,Y).
* Fixed 65C12 instruction timings for ADC and SBC in decimal mode, ASL, LSR,
  ROL, ROR, and BRA.
* Added keyboard shortcuts for changing text-to-speech reading rate. Enable
  text output reader by default when text-to-speech is enabled.
* Fixed crash in the debugger, where sideways RAM does not contain a normal
  ROM image.
* Fixed Econet source code cross-compatiblity issue.
* Serial and tape emulation improvements:
  - Fixed serial transmit state: Writing a master reset to the ACIA control
    register should stop transmitting data. This was causing extra bytes to
    be written to UEF files when saving to tape.
  - The Tape Control dialog Record button now always prompts the user to
    create a new UEF file, rather than appending to the currently open UEF
    file. This change also fixes a bug where the button would do nothing if
    a CSW file was loaded.
  - Opening the Tape Control dialog no longer causes Block? or Data? errors
    during loading.
  - The BeebEm version number is now stored in UEF tape files.
* Fixed Master real time clock year handling. The year is stored as the last
  two digits. Removed the "Master 128 RTC Y2K Adjust" option. Note that the
  Master MOS still displays years as 19xx, but will accept any century when
  setting the time using `TIME$`.
* Added debugger support for the BBC Master real-time clock and CMOS RAM.
* Added a new Music 5000 disk image (M5000-4.ssd) that uses a different
  copy protection patch, to fix the staff editor.
* Added ZipFile project, to create BeebEm.zip distribution. The project
  runs a Perl script, so requires a working Perl installation.
* The Write Protect On Load menu option is now also applied to files loaded
  from the command line.
* Improved the Disc Export dialog, which now detects and avoids using file
  names that are not valid on the host filesystem. The dialog box allows you
  to double-click an entry to rename files.
* The BeebEm window on Windows 11 is now drawn with rounded corners disabled.

Version 4.18 (13 Jun 2022)
------------

Contributors: Chris Needham, Alistair Cree, Greg Cook

* Fixed undocumented 6502 opcodes 2B (ANC imm) and CB (ASX imm).
* Fixed memory reads in undocumented 6502 opcodes.
* Hard disk support improvements:
  - Added new Select Hard Drive Folder menu option. This fixes an issue
    with the "Options -> Preferences -> Save Disc/Tape/State Folders"
    menu option which caused BeebEm to remember the last folder used
    to open disk or tape images. This would also set the folder where
    BeebEm looks for hard disk images, causing it to create empty
    *.dat files in the last folder used. This change also adds a new
    HardDrivePath preferences option, separate to DiscsPath.
    An error is now reported if opening a hard disk image file fails.
* Improved the emulation of the teletext adapter to implement the full
  range of possible states (field sync, data entry window, and video
  field). The TFS ROM now acquires pages correctly.
* Fixed key selection in the User Port Breakout Box dialog.
* Fixed 1770 disk emulation to enable automatic disk density selection
  in Opus DDOS.
* Fixed the Sprow ARM7TDMI co-processor emulation to ensure fetches are
  word aligned.
* Removed documentation for the Emulator Traps menu option,
  which has not been implemented yet.

Version 4.17 (13 Jun 2021)
------------

Contributors: Chris Needham, Tadek Kijkowski

* Writes to the sound chip are now allowed while sound is disabled.
  This ensures that the sound chip is in the right state if sound is later
  enabled.
* Fixed sound frequency write. Setting frequency to zero should initalise
  the counter to 1024.
* The cursor is now disabled based on CRTC register R8 (see AUG p.366).
  This fixes the cursor display in Wizzy's Mansion and Pedro.
* Debugger improvements:
  - Fixed instruction 4B (ALR imm).
  - Set the initial input focus so that you can now open the Debugger
    window and type commands without having to click in the edit box.
  - Added 'over' command. This steps over a JSR instruction, i.e., runs to
    the next instruction in memory after the JSR.
  - Added a command to show existing labels.
  - Added a command to clear the output window.
  - Fixed help output to show all commands and aliases.
  - Fixed the 'file' command to allow the full 64k address space to be
    saved.
  - The following commands now prompt for a filename if not given:
    labels load, save, file, script.
* Mouse input improvements:
  - Changed the right mouse button to control joystick button 2, which is
    useful for games such as Phoenix and Ripcord that use both joystick
    buttons.
  - Fixed the "Hide Cursor" option, which caused the cursor to flicker when
    using the mouse-stick options.
  - Added mouse capture option. To enable, select the "Capture Mouse"
    menu item and click in the window area. To release the mouse, press
    Ctrl+Alt.
  - Fixed "AMX L+R for Middle" option.
  - Fixed bug where Master 512 mouse input would sometimes be reversed.
    Master 512 configures interrupt on raising and falling CBx edges
    alternately. Instead of guessing which should be next, check selected
    edge in the PCR register.
  - Improved mouse responsiveness when captured.
* Fixed Mode 7 run-length optimisation in the video renderer
* Fixed the teletext font file. The 'W' character had one pixel missing.
* Improved Mode 7 screen capture to avoid distorting the image.
  Use the 640x512 option for best results.
* Reset double-height state at start of mode 7 screen. This fixes an
  issue where, if double height is enabled on the last character row of
  a mode 7 screen, the top row of the screen would also be treated as
  double height. This effect could be seen in the Twin Kingdom Valley
  instructions pages.
* Fixed Solidisk Sideways RAM emulation. Writes to RAM bank are selected
  by User VIA output port B even if the bank selected by &FE30 is writable.
* Fixed Preferences.cfg file backwards compatibility.
* Fixed reading the tube coprocessor selection from BeebEm 4.14
  Preferences.cfg files.
* Fixed crash when loading multiple ADFS images from the command line.
* Fixed PrinterEnabled preferences option, which was not being
  saved/loaded correctly.
* Added BeebEm.user.props.example file and updated the instructions
  for how to compile BeebEm.

Version 4.16 (7 Nov 2023)
------------

Contributors: Chris Needham, Dominic Beesley

* Reintroduced Master 512 co-processor support, following change to the MAME
  source code license.
* Added Torch Z80 utility discs and renamed ROM files.
* Fixed Integra-B sideways RAM writes.
* Improved Sprow ARM7TDMI co-processor clock speed accuracy.
* Debug watches now update while the emulator is running.
* Fixed disc image auto-boot.
* Fixed Model B MODE 7 emulation with screen memory at &3C00.
* Fixed 6502 opcodes and removed the Ignore Illegal Instructions menu option.
  - Repton tape loader now works (instructions: 04, 0C, 14, 1C, 34, 3C, 44,
    54, 64, 74, 89).
  - Zalaga tape loader now works (instructions: 04, 3C, 44, 54, 5C, 64, 74, 7C).
  - 3D Grand Prix tape loader now works (instruction &80).
  - KIL instructions are executed repeatedly, to emulate hanging the machine.
  - RMBn, SMBn, BBRn, BBSn instructions in 65C02 copro are now implemented.
  - Fixed instruction cycle times.
  - All 6502 and 65C02 opcodes are now recognised in the debugger.
* Removed the "Incorrect disc type selected" warning dialog, which was reporting
  false positives on valid .ssd files.
* Include complete video state in UEF save state files.
* Clipboard Paste handling no longer overwrites memory.
* Fixed Music 5000 panning, the left and right channels were reversed.
* Fixed use of Jim page select register with Music 5000.
* Added `-CustomData` command line option.

Version 4.15 (28 Jun 2020)
------------

Contributors: Charles Reilly, Chris Needham, Kieran Mockford, pstnotpd,
Richard Broadhurst, J.G.Harston, Mike Wyatt, Alistair Cree, Steve Insley,
Ken Lowe, Dominic Beesley

* Fixed 320x256 screen capture.
* Enabled DirectX full screen display at current desktop screen resolution.
* Consistent file selection dialogs.
* Added debugger Alt-Tab support.
* Added Sprow ARM7TDMI co-processor emulator.
* Fixed sound register latch bug.
* Fixed wrap-around bug with indirect, Y-indexed addressing mode in the
  6502 emulator.
* Migrated source code to Visual Studio 2015.
* Fixed crash if sound sample files are missing.
* Fixed 8271 disk emulation to show disks as not ready after motor stops.
* Fixed video recording on Windows 8.
* IDE support:
  - If invalid drive selected can still access IDE command register to select a
    different drive.
  - SetGeometry sets IDEStatus correctly.
  - Seek returns correct IDEStatus for invalid/absent drive.
* Z80 support:
  - Torch inrom/outrom selection corrected.
  - Small client ROM loaded multiple times to ensure contents reflected.
* Z80 CPU:
  - Bugs fixed in IN/OUT instructions and block instructions.
  - Added repeated EDxx instructions.
  - Bugs: BIT doesn't set flags as per real hardware.
* Music 5000 Synthesiser emulation.
* General sound emulation improvements.
* Fixed graphics hold in teletext mode.
* Release Shift key on disk read after auto-booting.
* Rework teletext adapter emulation and add support for real-time packet
  streams.
* Fixed reads from CRTC and Video ULA registers.
* Added new DX video modes for full-screen (1440p and 4K).
* Fixed issue where BeebEm would always use the primary monitor when going
  fullscreen.
  - Now uses monitor on which the BeebEm window is positioned. As with all
    DX9 applications, this works best when "Maintain Desktop Resolultion" is
    selected.
* Changed the default fullscreen resolution to "Maintain Desktop Resolution".
* Added `-AutoBootDelay` CLI argument taking a delay parameter set in
  milliseconds creates a delay between initial boot and auto-boot of disk image
  if supplied in arguments. If used with `-StartPaused`, the timer starts after
  un-pausing.
* Added ability to pause the emulation, distinct from freezing when focus lost.
  - Menu item on Speed menu.
  - Alt+F5 will pause/unpause.
  - The `-StartPaused` CLI argument can be used to prevent the emulation from
    starting until unpaused. Implemented to better support
    HyperSpin/RocketLauncher configurations where you want to defer boot until
    the BeebEm window and bezels are shown, and any Fade screen has been
    removed.
  - If in windowed mode and showing fps, pause state is shown in window title.
* Improved Computech IntegraB startup defaults, and correct the current year
  calculation. Also included is a patched IBOS ROM where two digit years are
  assumed to be 20xx instead of 19xx.
* Fixed BCD mode in ADC and SBC instructions.
* Fixed key selection dialog box layout for Windows 10.
* Added `-DebugLabels` command line option to load BeebAsm compatible debug
  labels.

Version 4.14 (12 Feb 2012)
------------

Contributors: J.G.Harston, Steve Pick, Mike Wyatt

* IDE hard drive interface.
  - Limited IDE_Geometry command supported, allows 4 heads x 64 sectors
    (disks up to 512M), and 16 heads x 64 sectors (disks larger than 512M).
  - SCSI and IDE hard drives selectable/configurable.
  - Disk images referenced by DiscsPath setting.
* Double-sided SSD disks supported (image file>&40000 bytes long).
* Debugger updates, bug fixes and addition of "script" command.
* XAudio2 sound support (thanks to "bredbored").
* Added a couple of TV resolutions to the DX video mode menu.
* Fixed bug where disk drive sound sticks on.
* Fixed keyboard issue for Dr Who game.

Version 4.13 (20 Jan 2011)
------------

Contributors: Mike Wyatt

* Added indication of unplugged ROMs to the configuration editor in Master
  128 mode.
* Add a keyboard shortcut to exit (Alt+F4).
* Added support for loading .img files from command line.
* Fixed bug in 1770 controller emulation where second side of a single sided
  disk could be accessed.
* Fixed some character mappings in text view mode.
* Added `-FullScreen` command line option.
* Added command line support for loading second disc image into drive 1.
* Added option to maintain a 5:4 aspect ratio output in full-screen mode.
* Made the BeebEm window resizeable.
* Fixed an issue in the sound code that was causing garbled output.

Version 4.12 (22 Nov 2009)
------------

Contributors: Mike Wyatt

* Added ROM configuration editor.
* Updated sideways RAM support:
  - Changed SW RAM write enable menu to show slots containing RAM and
    disable the ROM slots.
  - RAM slots enabled now saved in preferences.
  - Added write through to the first RAM bank when writing to ROM address
    range (&8000-&BFFF).
  - Added SW RAM board emulation where the active RAM bank is selected by
    the User VIA port B output.
* Added AtoD to saved state. Joystick should now work after restoring a
  state file.
* Fixed joystick capture error when enabled for a second time.

Version 4.11
------------

Contributors: Rob O'Donnell, Rich Talbot-Watkins, Mike Wyatt

* Added AUN Econet support.
* Added "RAW comms" option for use with the serial port IP options.
* Updated video emulation to remove screen stretching and fix cursor
  positioning.
* Added some more shortcut keys, for use on laptops primarily:
  - ALT 1  Quick save state
  - ALT 2  Quick load state
  - ALT 5  Capture screen to file
  - ALT +  Increase emulation speed
  - ALT -  Decrease emulation speed
* Added support for the Master 128 numeric keypad.
* Added option to enable/disable floppy drive controller.
* Added option to enable/disable the real time clock Y2K 20 year adjustment.
* Fixed bug in M128 BIT instruction (thanks to Michael Firth). Fixes some
  issues when using MOS3.5 (e.g. `*CONFIGURE`).
* Fixed issue with file lengths in disc import option.

Version 4.10
------------

Contributors: Mike Wyatt

* Added screen capture options to the file menu. Resolution and file format
  can be selected. The BMP, JPEG, GIF and PNG file formats are supported.
  ALT+keypad 5 (with NumLock on) is a shortcut key for capturing the screen
  to file.
* Added option to change the User Data folder location.
* Added keyboard mapping to the saved emulator state.
* Fixed issue with file offsets after break key press in 1770 controller.
* Fixed issue with disc write protection when changing Beeb model.
* Fixed sound muting while Window is being moved.
* Changed source code licensing to the GNU General Public License.
* Removed the Master 512 Co-Processor and TMS5220 Speech Generator emulation
  as the source code licensing for these features is not compatible with the
  GPL. These two features are still available in BeebEm version 4.03, which
  is on the BeebEm website.

Version 4.02 (26 Apr 2009)
------------

Contributors: Mike Wyatt

* Added disc drive sound emulation.
* Added write support for the Master real time clock.
* Fixed teletext mode smoothing at startup in Vista.

Version 4.01
------------

Contributors: Steve Pick, Mike Wyatt

* Rewrote debugger command interpreter:
  - Supports command names longer than one character.
  - Added command history (up/down arrows when command box focused).
  - Commands are now implemented using function pointers, should be more
    expandable.
  - More descriptive help - type `help`, `help <command>` or `help <addr>`.
  - Better label support - any word in command arguments preceded with a '.'
    is interpreted as a label and resolved before the args are passed to
    the command handler.
* Fixed bug that prevented checkboxes enabled/disabled by command from
  actually taking effect.
* Expanded debug memory map support:
  - Maps can now be stored for each ROM, just put a .map file with the same
    name as the ROM in the ROM's directory, for example BeebFile\BBC\os12.map
    corresponds to BeebFile\BBC\OS12.ROM, and address queries (via
    `help <addr>`) will return info from the currently selected ROM's map.
  - Address info queries now understand shadow/private/sideways RAM in
    different machines.
  - Cleaned up MemoryMap.txt and moved it to BeebFile\BBC\OS12.map
* Added `state m` command to get state of memory (ACCCON and Shadow/Sideways
  RAM info).
* Added routines for extracting header info from paged ROMs, and `state r`
  command to dump this info in the debugger.
* Break now shows previous Program Counter address as well as the current
  address. This is useful for finding where the source of a jump was.
* A bit of a hack to allow debug window to go behind main window, but also
  come to foreground with main window.
* Added option to enable DirectX smoothing when only in teletext mode.

Version 4.0
-----------

Contributors: Mike Wyatt, Rob O'Donnell, Steve Pick

* Added clipboard functionality from the Mac port. Clipboard can be set as
  destination for printer output, copy and paste menu options for BASIC
  programs.
* Added file import/export for DFS disc images loaded into BeebEm. The
  exported files use the standard archive format where .INF files store the
  file attributes.
* Added support for disc, state and tape file specific preferences. When
  loading a file from the command line, running a disk image or loading a
  state file BeebEm will check for Preferences.cfg and Roms.cfg files in the
  same folder as the image file. This makes it easier to set up different
  preferences for different programs.
* Added ALT-ENTER keypress to toggle fullscreen mode.
* Creation of registry entries at installation:
  - HKEY_LOCAL_MACHINE\SOFTWARE\BeebEm\InstallPath
  - HKEY_LOCAL_MACHINE\SOFTWARE\BeebEm\Version
* Reduced the tape control window size so it fits on a Netbook screen.
* Added Rob's IP options for serial port emulation. Allows connection via
  TCP/IP to local or remote IP address. Incoming connections can be
  supported via utilities such as tcpser.
* Debugger enhancements from Steve:
  - User-defined breakpoints and watches.
  - Break on BRK instruction.
  - Support for loading VICE format label files, as generated by LD65 - this
    lets you use text labels in place of addresses in all debugger commands.
  - Memory map file allows short descriptions for memory ranges. Debugger
    will use this to describe what location it's entered OS code at, or
    where the break landed.
  - Commands to toggle various UI checkboxes.
  - Command to echo a line of text to debugger output.
  - Debugger commands can now be loaded from a script file specified on the
    BeebEm command line (with `-DebugScript`). Simple comments are also
    supported.
  - Sanitized some code, added `DisplayDebugInfoF(format, ...)` to output
    formatted debugging info, `sprintf()` style.
  - Beautified the debugging UI a bit.

Version 3.85
------------

Contributors: Mike Wyatt

* Fixed disc formatting using the 8271 controller.
* Video update / capture fix from Rich.
* Reduced CPU load when execution is halted in the debugger.
* Added three new debugger commands:
  - `g` to goto an address
  - `fr` to read a file into host RAM
  - `fw` to write a file from host RAM

Version 3.84
------------

Contributors: Mike Wyatt

* Added menu option to display cursor line at bottom of screen.
* Added `\d` option to `-KbdCmd` command line parameters.
* Made `-KbdCmd` processing more resilient.

Version 3.83
------------

Contributors: Mike Wyatt

* Added `-NoAutoBoot` command line option.

Version 3.82 (30 Mar 2008)
------------

Contributors: Rich Talbot-Watkins, Mike Wyatt

* Periodic noise emulation tweak from Rich.
* Video emulation fixes from Rich.
* AVI capture fix. When AVI is enabled the video frame rate is now fixed
  to match the capture rate. This should stop video and sound getting out
  of sync.
* Changed CMOS default for print destination to the parallel port. Printer
  options should work correctly in Master mode now.
* Added support for loading tape images from the command line.
* Added `-KbdCmd` command line option to specify a key press sequence to run
  at start up (can be used to run a tape image).

Version 3.81
------------

Contributors: Mike Wyatt, Jon Welch

* Added `-DisMenu` command line option to disable the menus.
* Fixed issue where sound output stops after about 10 mins.
* Corrected the fix in v3.8 for sound artifacts. Sampled sound works
  correctly again.
* Added periodic noise emulation tweak from Rich Talbot-Watkins.
* Updated instructions for defining user keyboard mappings (thanks to
  Jonathan Bluestone).
* Bug fix - Cursor position wrong when in column 1 in editing mode.

Version 3.8
-----------

Contributors: Mike Wyatt, Jon Welch

* Enhanced user defined key mapping support. Shifted and unshifted key
  presses can now be defined separately so custom logical mappings can be
  defined.
* Added load and save options for user defined key mappings.
* Corrected emulation of PLY processor instruction.
* Fixed bug in sound emulation that was causing sound artifacts.
* Set root directory to $ for new ADFS images.
* Fixed keyboard interrupt handling (can now enter name in Super Pool
  high score table).
* Added some preference options to autosave CMOS and all of the prefs on
  exit. Also, by popular demand, added a prefs option to remember the last
  folder used for loading disc/tape/state files.
* Fixed disc write protect menu update when ejecting a disc.
* Increased default Econet flag fill timeout to 250000. Improves Econet
  comms a bit (still not 100% though).

Version 3.7
-----------

Contributors: Jon Welch

* Added ARM Second Processor support.

Version 3.6
-----------

Contributors: Mike Wyatt, Jon Welch

* Made BeebEm Vista compatible.
* Added support for DirectX9 so image output looks good in Vista.
* Moved all preferences from the Window registry to the Preferences.cfg
  file. CMOS settings also moved from cmos.ram to the prefs file.
* Preferences such as Window position, FDC selection and CMOS settings
  are now only saved when "Save Preferences" is used.
* Moved all "user" data such as disk images, tape images, state files,
  config files, etc. to the user's "My Documents\BeebEm" directory.
  BeebEm will copy a default set of data files to "My Documents\BeebEm"
  if the directory does not exist. Each user will have their own data
  files and preferences.
* Added command line options to specify user data directory, preferences
  file and roms configuration file. This allows different BeebEm
  preferences and configurations to be set up and selected via the command
  line. Also allows BeebEm to be run from a USB drive without affecting
  the host PC (run via the BeebEmLocal.vbs script).
* Added "Protect on Load" to disc options menu to select default write
  protect state when a disc is loaded.
* Added some more Window and full screen sizes.
* Change to use Inno Setup for installation. Installer will move any user
  data files from "\Program File\BeebEm" to "My Documents\BeebEm".
* Fixed keyboard handling bug with the shift key.
* Fixed write protection toggling when using 1770 DFS emulation.
* Fixed error handling for open file failure in 1770 DFS emulation.
* Fixed write protection reporting in 8271 DFS emulation.
* Fixed preference saving for sound on/off and AMX on/off.
* Added preliminary 300 baud support for CSW, Swarm now loads.
* Added hard disc activity LEDs.
* Added CSW support to Tape Control window.
* Added support for Level 3 Econet User Port RTC Module.
* Added support for mixed mode ADFS/NETFS format discs.

Version 3.5
-----------

Contributors: Mike Wyatt, Theo Lindebaum, Jon Welch

* Added text to speech support and screen reader compatible text view for
  use by visually impaired people.
* Fixed support for MOS 3.50 in Master 128 mode.
* Fixed a bunch of compiler warnings thrown up by VS2005.
* Fixed some odd keyboard behaviour associated with AltGr key presses.
* Fixed bug with speech output changing speed/pitch.
* Fixed problem of saving to ADFS hard disc under 65C02 emulation.
* Fixed user defined keyboard support so that shift key re-assignments
  work.
* Added menu options to disable selected keys within BeebEm.

Version 3.4
-----------

Contributors: Jon Welch

* Added preliminary support for loading CSW format tape images.
* Added emulation of Microvitec touch screen (can be used with software
  from Brilliant Computing).
* Added user port breakout box.
* Added digital mousestick option.
* Fixed joystick emulation in Master 128 mode.
* Fixed problem of ESC key not always being detected.
* Fixed problem of accessing files when ADFS and DFS discs loaded side by side.
* Fixed a couple of tape related game loading problems.
* Fixed interrupt clearing issue in 8271 disk emulation (fixes save/restore
  state in Twin Kingdom Valley).
* Minor VIA timing tweak to make Snapper work again.

Version 3.3
-----------

Contributors: Mike Wyatt

* Added menu options to enable/disable Teletext adapter and hard drive
  emulation. When enabled they cause corruption of Alien8 data.
* Removed ADFS and ATS ROMs from Model-B configuration as they were causing
  a few problems.
* Improved VIA and interrupt timing and fixed instruction cycle count for
  branches. The following programs now run:
    Nightshade (tape), Lancelot, The Empire Strikes Back, Dabs Fingerprint,
    Yie Ar Kung-Foo (tape)
* Added "Eject Disc" options to the file menu. The name of the currently
  loaded image file is shown next to the menu option.
* Added two new debugger commands:
  - `c` to change memory contents
  - `w` to write the debug output buffer to a file
* Change sound volume to be exponential (suggested by Rich Talbot-Watkins).
  Seems to be a definite improvement on the linear scale. There is a menu
  option to switch between the two.

Version 3.2 (15 May 2006)
-----------

Contributors: Jon Welch

* Added support for Acorn Z80 Co-Processor.
* Added support for Master 512 Co-Processor.
* Added support for Acorn Teletext Adapter.
* Added some enhancements to the debugger from Thomas Horsten. Display of
  stack register and op codes for undocumented instructions.

Version 3.11
------------

Contributors: Mike Wyatt

* Fixed random crashes.
* Fixed display of LEDs.

Version 3.1
-----------

Contributors: Mike Wyatt, Rob O'Donnell, Jon Welch

* Added econet emulation. It works in Model B mode and Master 128 mode.
  The Acorn level 1 and 2 file server software runs and stations can
  read/write files to the server. Note that the default DFS ROM in Model B
  mode has been changed to the Acorn DNFS ROM.
  (Note also that TORCHNET works using the Torch Z80 Co-Processor.)
* Added some more command line options. The hardware model and tube support
  can be specified on the command line. See the notes in readme.txt.
* Fixed default read value for IO pages FRED and JIM. Fixes graphics
  corruption in tape version of Alien 8.
* Fixed bug in video emulation where it was attempting to display one too
  many lines causing visual artifacts (thanks to Phil Sainty for
  highlighting this one).
* Added SCSI ADFS Hard Disc support.
* Added SASI Torch Z80 Hard Disc support.
* Removed IDE support as no longer needed.
* Added Acorn ADFS 1.3 ROM to Model B and B Plus modes so SCSI hard disks
  can be used.
* Fixed problem with Alt key where certain keys get locked down.
* Fixed econet station allocation on PCs with multiple network
  interfaces/cards (thanks to Sam Skivington).
* Added keyboard shortcuts for Quicksave (keypad /) & Quickload (keypad *).
  Quicksave now keeps the last 10 quicksave files so you can go back to an
  earlier state using the load state menu option.
* Changed to ignore DirectX errors during screen update. Stops BeebEm
  reporting an error when switching between a full and non-full screen mode
  instances of BeebEm. DirectX appears to sort itself out after a second
  or two!
* Fixed disk initialisation in the 1770 controller when creating a new disk
  image.
* Fixed disk formatting with the 1770 controller.

Version 3.0 (20 Nov 2005)
-----------

Contributors: Mike Wyatt

* Improved VIA timing emulation and fixed some instruction cycle counts.
  Its still not perfect but its good enough to run various versions of the
  infamous "Kevin Edwards" protection code!  The following tapes now load
  and run: Knight Lore, Alien 8, Daley Thompson Supertest, Strykers Run,
  Exile, Joust, Galaforce. These are the only ones I've tried, others may
  work as well.
* Fixed bug in horizontal displayed register emulation. Joust tape loading
  screen now appears centered correctly.
* Fixed bug in virtical sync position register emulation. Stops DirectX
  errors occurring when running Micropower Roulette.
* Fixed bug in virtical displayed/total register emulation for mode 7.
  Screen is now cleared below lines that are displayed.
* Added AVI video capture to the file menu (now with resolution and frame
  skip options).
* Changed command line parsing so it works when paths are enclosed in
  quotes and it now supports loading of UEF state files (thanks to Jasper
  for this one). Disk and state files can now be associated with BeebEm
  and run by double clicking on them.
* Fixed bug in IDE hard disk file access that prevented BeebEm from running
  when logged in as an unprivileged user on Windows XP.
* Fixed bug with window positioning when task bar is on left or top of
  screen.
* Bumped version to 3.0 to keep in sync with the Mac version of BeebEm.

Version 2.3
-----------

Contributors: Mike Wyatt, Greg Cook, Jon Welch

* Added "Motion Blur" option to view menu (suggested by Ian Bell). This
  all but stops the spaceship flicker in Elite.
* More 1770 disc controller fixes from Greg.
* Preliminary IDE hard disk support from Jon Welch.
* Torch Z80 second processor emulation from Jon Welch (see README_Z80.TXT).

Version 2.2 (12 Feb 2005)
-----------

Contributors: Greg Cook, Mike Wyatt

* Fixed BeebEm hang when RS432 is enabled (Acorn Basic Editor works again).
* Fixed LED positioning in modes other than 7.
* Integrated Greg's 1770 disc controller fixes.
* Fixed keyboard handling so Revs works correctly.

Version 2.1
-----------

Contributors: Mike Wyatt

* Added a tape control window for moving the tape position.
* Added support for saving files to UEF tape images.
* Added support for more UEF tape chunk types.
* Added menu option to mute sound chip so tape sound can be heard.
* Added menu option to automatically unlock tape files.
* Tape speed can now be changed without having to reload tape.
* Added tape settings to the UEF state.
* Added BeebEm speed shortcut keys, keypad +/- change speed.

Version 2.0 (3 Dec 2004)
-----------

Contributors: Mike Wyatt

* Added tube and 65C02 second processor emulation. It runs the Executive
  version of Elite quite nicely but you will need a reasonably fast PC.
* Added Debugger (see Readme.txt for details).
* Fixed more bugs in the video emulation (fixes Uridium, improves Level 9
  graphical adventures in Master mode).
* Fixed bugs in the joystick and AtoD emulation (SuperPool now works and it
  may fix problems with GamePads).
* Tweaked sound tuning and noise generation.
* Floppy controller selection is now saved for each Model B type.
* Changed default DFS for B+ to the V2.26 Acorn 1770 DFS.
* Rearranged menus a bit as options was getting too big.

Version 1.6 (16 Oct 2004)
-----------

Contributors: Ken Lowe, Mike Wyatt

* Added BBC Computech IntegraB Support:
  - RTC Memory
  - Private Memory 1K from &8000 to &83FF
  - Private Memory 4K from &8000 to &8FFF
  - Private Memory 8K from &9000 to &AFFF
  - 20K Shadow RAM from &3000 to &7FFF
* Added BBC Plus (128) Support
  - 20K Shadow RAM from &3000 to &7FFF
  - 12K Paged RAM
* (Level 9 graphical adventures work in IntegraB & B Plus modes.)
* Fixed various bugs in VIA timers (Volcano, Nutcraka, Pedro and Skirmish
  now work).
* Fixed various bugs in UEF state store/restore, particularly for SW RAM
  usage and Master 128 mode (Elite & Exile save/restore now reliable).
* Added disc settings to the UEF state. When a state is restored BeebEm
  will load the disc images that were loaded when the state was stored.
* Fixed various bugs in the video emulation (fixes Psycastria and Level 9
  graphical adventures, improves FireTrack).
* Changed video emulation so that `*TV255` does not move screen down (`*TV254`
  does though). Some programs do a `*TV255` and the bottom line of the screen
  is lost.
* Fixed reload of drive 1 disc image after a File -> Reset.
* Fixed BeebDiscLoad environment variables in Model B modes.
* Fixed ADFS boot (A+Break) in Master 128 mode.
* Removed old BeebState stuff.
* Removed 'Allow ROM Writes - All' option, it was not implemented.

Version 1.5
-----------

Contributors: Mike Wyatt

* Updated the speed regulation code again (should fix hangs).
* Added some more fixed speed settings (you can now play Elite at a
  decent speed!).
* Put quick load and save back the right way!
* Add a link to the README file to the help menu.
* Changed keyboard maps to default and logical layouts. The default
  mapping is the same as the old mapping 1. Logical mapping maps symbol
  by symbol (for a UK PC keyboard at least) so you get what you press.
* Added keyboard mapping options for A & S and function keys.
* Changed ALT and F10 to normal keys so they no longer select the menu.
  F10 is mapped to f0.
* Made more of the PC keys available for the user defined keyboard.
* Added the File -> Run Disc menu option.
* Added ability to pass name of disc image on command line to run it
  automatically.
* Removed Use Host Clock option from sound menu - its not used.
* Fixed Windows cursor when BeebEm starts up.
* Add some sleeps to the serial comms threads so BeebEm is a bit
  easier on CPU usage.
* Added some extra error checking for file handling.
* Stopped BeebEm from creating the C:\crtc.log file (you can
  remove this file).
* Tidied up the BeebEm installation directory. The source code
  is now kept in a zip file.
* Moved all preference settings into the Windows registry.

Version 1.4 (20 Dec 2001)
-----------

Contributors: Richard Gellman

Windows update only:
* New teletext font.
* Teletext Aspect ratio and centering fixed.
* Improved sound code timing.
* Sound "Part-Sample Compensation".
* Screen displacement in Master 128 mode after CTRL-BREAK fixed.
* Shadow RAM/Other Master 128 mode memory problems fixed.
* Read Address and Force Interrupt commands added to 1770 FDC.
* Timing of disc access on 1770 FDC made more accurate.
* Option to create ADFS images with New Disc added.
* Cassette Relay Sound Effects fixed.
* Cassette Input Sound added.
* Keyboard and Disc LEDs added.
* Disc LEDs can be red or green.
* Fixed (I think) BufferInVideoRAM option.
* Implemented Teletext Half-Scan mode.
* Fixed HSync/VSync derived screen displacement.
* Added option to change Tape loading speed.
* Redid Serial Port code.
* Disc images now remain loaded between Model/FDC changes.
* Added plug-innable 1770 FDCs.
* Fixed Teletext "Hold Graphics" and Double Height bugs.
* More 6845 Registers implemented, including Cursor delay, and Sync delay.
* UEF State save/load implemented.
* Sound system tweeks implemented.
* Speed optimisations implemented.
* Non-Directsound code removed.
* BeebDiscLoad variable in Master 128 mode fixed.
* Window retains position between running.
* Re-worked HSync/VSync centering routine.
* Added option to use DirectSound primary buffer.

Version 1.35
------------

Contributors: Richard Gellman

Windows update only:
* Fixed rom menu corruption during reset bug.
* Improved DirectSound code, makes sound smoother.
* Added hide menu option to allow the whole screen to be used for the BBC.
* Added tape interface (and some code for the serial port).
* Added preliminary tube support (not currently functional).
* Added HSync and VSync handling to enable screen positioning.
* Added screen clear on Frame 0 to remove "redundant" screen data.
* Added the ability to display "Wide" screens (makes Boffin work correctly).
* Corrected a 6502 Addressing mode bug (makes the sticky explosions in
  Elite go away).
* Revised 6502 Instruction/IRQ/NMI timing.
* Corrected 6522 VIA Timer reload timing (makes Revs work correctly).
* Corrected 6522 VIA Timer pre-reload action (makes Volcano work.. ish).
* Corrected VSync timing. Removes flicker from Revs and some versions
  of Elite.
* Added writable option for ROMS in the roms.cfg file.
* Modified roms.cfg handling to save having to re-edit between releases
* Minor Master 128 memory handling bug corrected.
* Added End Of Conversion to VIA emulation (allows the analogue port to
  generate interrupts).

Version 1.32
------------

Contributors: Richard Gellman

Windows update only:
* *All* undocumented 6502 codes implemented.
* Almost full 1770 FDC Support (read/write track (format) and force
  interrupt still not supported yet).
* ADFS Discs (Single or Double sided (auto detected)) now supported.
* Fixed cursor positioning problem in Modes 3 and 6.
* Fixed mode 7 7-bit control code handling.
* Fixed cmos.ram file open bug.
* Fixed sound menu corruption on model change/reset.
* Fixed rom menu.
* Added more flexible rom control via use of roms.cfg and optionally
  the supplied ROM Manager.
* Fixed that ini file bug... I put my settings into the registry now.
* Supplied all required DLLs and ROMs in with the download.
* Fixed the sideways RAM bug I accidentally put in the previous version.
  Digital audio in Exile has returned!

Version 1.3 (3 May 2001)
-----------

Contributors: Richard Gellman

Windows update only:
* Added full cursor support to enable cursor on/off/blink and blink rates.
* Corrected mode 7 flashing text rate.
* Corrected minor fault in sound register write edge detect.
* Redesigned mode 7 font, looks ugly, pending further work
  (Original font found in mode7font.alt).
* Added reset function to file menu.
* Added Master 128 Support:
  - 8K Filing system RAM from &C000 to &DFFF
  - 4K Private RAM from &8000 to &8FFF
  - 20K Shadow RAM from &3000 to &7FFF (still very slightly buggy, but usable)
  - Added all documented 65C12 Opcodes extra to the 6502
  - Added _simple_ 1770 FDC - Read only at present
  - Made some hardware re-arrange to alternate locations on a Master
    (The A to D convertor is accessed the same, but at a different location)
* Implemented "available models" detection, only models that you have the
  roms for will be available, if neither available, the program exits.
* Added BBC Model Type Menu to Options Menu.

Version 1.04 (15 Sep 2000)
------------

Contributors: Robert Schmidt

Windows update only:
* Remember last used directories for disc images and emulator states.
* Dialog for opening disc images has 3 new filters:
  - Automatic *.ssd / *.dsd detection
  - *.ssd only
  - *.dsd only
* Support for higher DirectDraw resolutions, through new sub menu.
  (1280x1024 makes aliasing effects unnoticable, but requires a fast
  DirectDraw implementation.)
* Added option to use 32-bit DirectDraw modes in full screen, as opposed
  to 8-bit. (In a window, DirectDraw uses the desktop color depth.)
  When combined with "Buffer in video RAM", the result is a *superior*,
  interpolated display! Scaling artifacts "disappear" at any resolution.
* Made "Full Screen" a toggle, not a standalone mode/"window size".
* Fixed a bug that didn't show speed/FPS in a windowed DirectDraw mode.

Version 1.03 (20 Aug 2000)
------------

Contributors: Robert Schmidt

Windows update only:
* Monochrome monitor/B&W TV emulation.

Version 1.02 (30 Apr 1998)
------------

Contributors: Mike Wyatt

* Added Robert's Freeze when Inactive option.

Version 1.01 ()
------------

Contributors: Mike Wyatt

* Fixes the problem with BeebEm hanging after about 35 minutes.

Version 1.0 (2 Feb 1998)
-----------

Contributors: Mike Wyatt

* Switchable DirectDraw and DirectSound.
* Fixed Windows message boxes when using DirectDraw.
* Printer support.
* Improved ROM size checking.
* Improved speed regulation code.

Version 0.9 (20 Jan 1998)
-----------

Contributors: Mike Wyatt

* Added AMX mouse support.
* Converted to use DirectX and added a full screen option.
* Fixed a few bugs (thanks to Piers Haken for pointing out a bug in the video
  code).

Version 0.8 (1 Sep 1997)
-----------

Contributors: Mike Wyatt, Laurie Whiffen

* Added disc write and format capability.
* Changed disc load code to give a warning if disc image loaded looks like its
  been loaded using wrong type (single/doubled sided).
* MS Windows version only:
  - Fixed so all key presses are released when windows looses focus.
  - Rom slots can be individually changed between ROM and RAM using the menus.
  - Added user definable keyboard mapping options.
  - User preferences (including the keyboard mapping) can be saved in an INI
    file using a new menu option.
  - Added menu options for write protection and creating new disc images.

Version 0.71 (5 Aug 1997)
------------

Contributors: Mike Wyatt

* Changed VIA timer latches initial value to 0xffff (Castle Quest now works).
* Fixed BCD addition and substraction and prevented mode 7 code from dividing
  by zero (Exile now works).
* Changed VIA code so the data direction registers can be read (Codename Droid
  now works).
* Fixed cursor colour in UNIX version.
* Added sideways RAM support (so Exile can be played full screen).

Version 0.7 (24 Jun 1996)
-----------

Contributors: Mike Wyatt

* Added conditional compilation for all the Microsoft Windows code. Should only
  need to maintain one set of source files for both the X and MS Win versions.
* Changed sound code so it does not play a sound until both the high and low
  bytes of the frequency have been set (eliminates spurious high pitch beeps).
* Video module now generates a cursor.
* Implemented a load of undocumented 6502 instructions.
* Directed read and writes to the 16 bytes above each VIA back down to
  the VIAs (Castle Quest uses the higher addresses).
* Added the atodconv module to provide Analogue to Digital support. Added the
  beebstate module to provide Save and Restore of Beeb State files. These two
  new modules should compile under UNIX but there is currently no code in the
  X Windows version to use them.
* MS Windows version now provides:
  - Load of menus to control everything
  - Dynamic disc selection
  - Beeb State save and restore
  - Various Window sizes
  - Real time and fixed frame rate modes
  - Sound support
  - Joystick support
  - Mousestick support
  - ROM write protection
  - Multiple ROM initialisation
  - Games keyboard mapping
  - Pentium optimised version

Version 0.6
-----------
* A few fiddles with the config for linux - now compiles on 2.7.0 with the ELF-GCC
  release; but could cause probs for older releases.
* Started adding sound card.

Version 0.5
-----------
* Improved timing - mode 7 bodged to exactly correct cycle count
* Interlace timing accounted for in modes 0-6.
* Interrupt latency added
* fixed window.cc (took out 'virtual' on most functions) - now OK in gcc 2.7.0
* Changed default position of g++ includes to /usr/include/g++

----------------------------------------------------------------------------
End
