Sprow ARM7TDMI co-processor emulation on BeebEm

Sprow Co Processor code added by Kieran Mockford. (C) 2010

http://lists.cloud9.co.uk/pipermail/bbc-micro/2010-May/008517.html

For the original 4.12 patched version visit and the initial newlib stubs please visit

http://home.kindredintellect.com/beeb/Software/ARM7TDMI/

==============================================================================================
Welcome to Sprow's ARM7TDMI Co-Processor Emulation!

Here's a preview of the Windows version of BeebEm with support
added for Sprow's amazing ARM7TDMI Co-Processor.
Details here: http://www.sprow.co.uk/bbc/armcopro.htm

Please do give it a try and let me know if you find problems.

Why did I do this?

Debugging of my Co-Processor code! Cross compiling and wandering
back and forth with a USB key was a barrier to productivity!

In the future it might be interesting to incorporate ARM 
debugging support into BeebEm.

There are two .zip files provided:

1. BeebEm_ARMCoPro_src.zip
   This contains all the sources required to build the version
   of BeebEm, including the Visual Studio 2008 project/solution
   etc.
   
2. BeebEm_ARMCoPro_bin.zip
   A compiled binary (with the optimization turned all the way
   up).
   
NOTE: In order for the ARM7TDMI entry on the Second Processor
menu to become enabled, you must get a ROM image from your
actual ARM7TDMI Co-Processor equipped machine.

You can get this in the following way:

1. Enable the real Co-Pro on your Beeb
2. *SAVE SPROWRM C8000000+80000

This will create a 512kB file which needs to be placed into
your BeebFile directory (next to 6502Tube.rom etc.) and renamed
from 'SPROWRM' to 'SPROW.ROM'.

Which ARM emulator is being used?

The code is the ARMulator that ARM distribute under the GPL. 
Including a couple of minor modifications that I found
necessary to make it work correctly.

How much of the Co-Pro hardware is supported?

Enough of the OKI SoC to run all of the code I've tried that
works on the real hardware. See below.

If something doesn't work, then let me know and I'll do my
best to fix it!

Here's what I've tried:
- Built-in ROM software including BASIC.
- All the C samples from Sprow's website compiled with Yagarto)
- Various test code from Sprow
- My own extended NewLib support for C-Library using the latest
  Yagarto (EABI, 4.5.0)
- I even have Microsoft's .NET Micro Framework ported and working with
  a basic framebuffer->host system for graphics.
  
Credits where credits are due:
-----------------------------
sprow:  for a splendid piece of hardware that everyone with
        a Beeb should get, i.e. the ARM7TDMI Co-Processor :)
        Also, a serious thanks for Sprow's support in getting
        this emulation going.
          
  
Changelog
=========

v0.1a: 20th May 2010
	- Initial Preview
==============================================================================================        

Sprow rom image added with permission of Robert Sprowson

For more information and purchase of the ARM7TDMI co-processor as well as further support
material please visit http://www.sprow.co.uk/bbc/armcopro.htm


