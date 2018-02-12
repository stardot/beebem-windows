BeebEm for Microsoft Windows
============================

Compiling
---------

The source code for BeebEm is available at:

  https://github.com/stardot/beebem-windows

If you want to compile BeebEm yourself then you will need Microsoft Visual
Studio 2015 or later (the free VS2015 Community edition will compile BeebEm).
The following project files are included:

  BeebEm.sln                         - Solution file
  BeebEm.vcxproj                     - BeebEm project file
  Hardware\Watford\Acorn1770.vcxproj - Acorn 1770 FDC project file
  Hardware\Watford\OpusDDOS.vcxproj  - Opus DDOS FDC project file
  Hardware\Watford\Watford.vcxproj   - Watford FDC project file
  InnoSetup\Installer.vcxproj        - Inno Setup installer project file

You will need to download and install the Microsoft DirectX 9.0 SDK, then
configure the project to find it.

To build the installer from within Visual Studio, you'll need to download
and install Inno Setup from http://www.jrsoftware.org/isinfo.php.

Select the View menu, then Other Windows, then Property Manager. In the
Property Manager window, open the BeebEm\Release | Win32\Microsoft.Cpp.Win32.user
property page, then add the following macros:

* Set DXSDK_Dir to the path to the DirectX SDK, e.g:

  Name:  DXSDK_Dir
  Value: C:\Program Files\Microsoft DirectX SDK (August 2009)

* Set ISCC_Path to the path to the Inno Setup compiler, e.g:

  Name:  ISCC_Path
  Value: C:\Program Files (x86)\Inno Setup 5

