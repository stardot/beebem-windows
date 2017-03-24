BeebEm for Microsoft Windows
============================

Compiling
---------

This distribution of BeebEm contains Windows executables but if you want to
compile BeebEm yourself then you will need Microsoft Visual Studio 2015 or
later (the free VS2015 Community edition will compile BeebEm). The following
project files are included:

  BeebEm.sln     - Solution file
  BeebEm.vcxproj - Project file

You will need to download and install the Microsoft DirectX 9.0 SDK, then
configure the project to find it. Select the Project menu, then Properties.
Under Common Properties, select User Macros and set the DXSDK_Dir value to
the path to the DirectX SDK, e.g:

  C:\Program Files\Microsoft DirectX SDK (August 2009)
