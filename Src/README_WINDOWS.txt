BeebEm for Microsoft Windows
============================

Compiling
---------

The source code for BeebEm is available at:

  https://github.com/stardot/beebem-windows

If you want to compile BeebEm yourself then you will need Microsoft Visual
Studio 2019 or later (the free VS2019 Community edition will compile BeebEm).
The following project files are included:

  BeebEm.sln                         - Solution file
  BeebEm.vcxproj                     - BeebEm project file
  Hardware\Watford\Acorn1770.vcxproj - Acorn 1770 FDC project file
  Hardware\Watford\OpusDDOS.vcxproj  - Opus DDOS FDC project file
  Hardware\Watford\Watford.vcxproj   - Watford FDC project file
  InnoSetup\Installer.vcxproj        - Inno Setup installer project file

These project files are set up to target Windows XP. This requires the
following optional Visual Studio 2019 components to be installed:

* MSVC v140 - VS 2015 C++ build tools (v14.00)
* C++ Windows XP Support for VS 2017 (v141) tools [Deprecated]

You also will need to download and install the Microsoft DirectX 9.0 SDK
from https://www.microsoft.com/en-us/download/details.aspx?id=6812.

To build the installer from within Visual Studio, you'll need to download
and install Inno Setup 5.6.1 from https://files.jrsoftware.org/is/5/.

After installing the DirectX 9.0 SDK and Inno Setup, the next step is to
configure the BeebEm Visual Studio project to find the relevant files.

Rename the file Src\BeebEm.user.props.example to Src\BeebEm.user.props,
and then open BeebEm.sln in Visual Studio.

Select the View menu, then Other Windows, then Property Manager. In the
Property Manager window, click to expand BeebEm\Release | Win32 and then
double-click on BeebEm.user.

This opens the BeebEm.user properties. Select User Macros from the list
in the left column, under Common Properties, then set the following macro
values:

* Set DXSDK_Dir to the path to the DirectX SDK, e.g:

  Name:  DXSDK_Dir
  Value: C:\Program Files\Microsoft DirectX SDK (June 2010)

* Set ISCC_Dir to the path to the Inno Setup compiler, e.g:

  Name:  ISCC_Dir
  Value: C:\Program Files\Inno Setup 5

The "Set this macro as an environment variable in the build environment"
option does not need to be ticked.
