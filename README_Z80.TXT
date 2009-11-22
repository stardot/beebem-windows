Torch-Z80 Co Processor Support
------------------------------
The software now fully supports the Torch-Z80 co-processor board. If you do not
know what this is, you can safely ignore this section :)

The Torch-Z80 board was an add-on card to the BBC computers which allowed you
to run CPN (a clone of CP/M) software. It was Z80 based, came with 64K of RAM
and connected to the BBC via the Tube interface. It had no hardware I/O of it's
own but used the display, keyboard, disc drives etc of the host processor.
Although it used the host disc drives, the disc formats were different to DFS
or ADFS. It still used 80 track, double sided discs (400K) but these discs
cannot be read on the BBC without special disc converting software. If you
wish to create your own disc images of original Torch Z80 software, I use
XFER 5.1 (http://www.g7jjf.com).

The emulator comes as default with Torch-Z80 support disabled (to prevent
confusion for new users). To configure support, you need to load a sideways
ROM image for the Host Tube support. A configuration file has already been
created for this so the simplest thing to do is replace the Roms.cfg file
with the Roms_Torch.cfg file (backing up Roms.cfg first of course !) This
file simply loads the required MCP122.ABM ROM into ROM Slot 0.

With this new configuration, running the emulator in Master 128 mode, will
default to a blue Mode 0 screen. You need to select Torch Z80 Second
Processor off the Hardware menu (the emulator will reboot) and type *MCP to
enter CPN. If you try and run *MCP without enabling the Torch Z80 Second
Processor off the Hardware menu, you will get a 'No Z80!' error message on
screen.

By default, the system will try and autoboot off Drive C and if you don't
have a suitable disc image mounted, it will bring back an error. To prevent
this, when typing *MCP, hold down the Alt key (Caps Lock) and this will
prevent CPN from autobooting.

I cannot give you a full overview of CPN but typing HELP is a good start
as well as exploring the disc images found at :

http://www.g7jjf.com/disc_images.htm

The original Torch Standard Utilities Ver 2.0 disc is included on
drive C and the Torch Hard Disc Utilities Ver 4.0 are on Drive D and
both contain several useful programs to get you going.

The Torch Z80 fully supports Econet emulation which provides TORCHNET
facilities for sharing drives amongst multiple computers or instances of
BeebEm.

If you have any specific questions about the Torch-Z80 support, please
drop me an e-mail and I will try to answer any queries you might have.

Jon Welch
jon.welch@ntlworld.com
http://www.g7jjf.com
7th August 2005
