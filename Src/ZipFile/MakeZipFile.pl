use strict;
use warnings;

use Archive::Zip qw(:ERROR_CODES :CONSTANTS);

my @files = (
    'UserData/AUNMap',
    'UserData/BeebFile/6502Tube.rom',
    'UserData/BeebFile/AMPLE.rom',
    'UserData/BeebFile/ARMeval_100.rom',
    'UserData/BeebFile/BBC/ADFS-1.30.rom',
    'UserData/BeebFile/BBC/ATS-3.0-1.rom',
    'UserData/BeebFile/BBC/basic2.rom',
    'UserData/BeebFile/BBC/dnfs.rom',
    'UserData/BeebFile/BBC/MCP120-CBL.rom',
    'UserData/BeebFile/BBC/OS12.map',
    'UserData/BeebFile/BBC/os12.rom',
    'UserData/BeebFile/BBC/wdfs.rom',
    'UserData/BeebFile/BBCINT/basic2.rom',
    'UserData/BeebFile/BBCINT/dnfs.rom',
    'UserData/BeebFile/BBCINT/IBOS.rom',
    'UserData/BeebFile/BBCINT/IBOS12P.rom',
    'UserData/BeebFile/BBCINT/os12.rom',
    'UserData/BeebFile/BBCINT/wdfs.rom',
    'UserData/BeebFile/BIOS.rom',
    'UserData/BeebFile/BPLUS/ADFS-1.30.rom',
    'UserData/BeebFile/BPLUS/B+MOS.rom',
    'UserData/BeebFile/BPLUS/basic2.rom',
    'UserData/BeebFile/BPLUS/DFS-2.26.rom',
    'UserData/BeebFile/BPLUS/wdfs.rom',
    'UserData/BeebFile/CCPN102.rom',
    'UserData/BeebFile/Commstar.rom',
    'UserData/BeebFile/HiBasic3.rom',
    'UserData/BeebFile/M128/adfs.rom',
    'UserData/BeebFile/M128/ADFS1-53.rom',
    'UserData/BeebFile/M128/ANFS-4.25-2201351.rom',
    'UserData/BeebFile/M128/ATS-3.0-1.rom',
    'UserData/BeebFile/M128/basic4.rom',
    'UserData/BeebFile/M128/dfs.rom',
    'UserData/BeebFile/M128/edit.rom',
    'UserData/BeebFile/M128/MCP122-ABM.rom',
    'UserData/BeebFile/M128/mos.rom',
    'UserData/BeebFile/M128/terminal.rom',
    'UserData/BeebFile/M128/view.rom',
    'UserData/BeebFile/M128/viewsht.rom',
    'UserData/BeebFile/M128-MOS3.5/Adfs.rom',
    'UserData/BeebFile/M128-MOS3.5/ANFS-4.25-2201351.rom',
    'UserData/BeebFile/M128-MOS3.5/BASIC4.rom',
    'UserData/BeebFile/M128-MOS3.5/Dfs.rom',
    'UserData/BeebFile/M128-MOS3.5/Edit.rom',
    'UserData/BeebFile/M128-MOS3.5/mos.rom',
    'UserData/BeebFile/M128-MOS3.5/Terminal.rom',
    'UserData/BeebFile/M128-MOS3.5/View.rom',
    'UserData/BeebFile/M128-MOS3.5/ViewSht.rom',
    'UserData/BeebFile/sprow.rom',
    'UserData/BeebFile/Z80.rom',
    'UserData/BeebState/.keep',
    'UserData/Default.kmap',
    'UserData/DefaultUser.kmap',
    'UserData/DiscIms/armdisc3.adl',
    'UserData/DiscIms/BBCMaster512-Disc1-DosPlusBoot.adl',
    'UserData/DiscIms/BBCMaster512-Disc2-GemApplications.img',
    'UserData/DiscIms/BBCMaster512-Disc3-GemData.img',
    'UserData/DiscIms/BBCMaster512-Disc4-Miscellaneous.img',
    'UserData/DiscIms/CPM_Utilities_Disc.dsd',
    'UserData/DiscIms/econet_level_1_utils.ssd',
    'UserData/DiscIms/econet_level_2_utils.ssd',
    'UserData/DiscIms/Games.ssd',
    'UserData/DiscIms/ide0.dat',
    'UserData/DiscIms/ide1.dat',
    'UserData/DiscIms/ide2.dat',
    'UserData/DiscIms/ide3.dat',
    'UserData/DiscIms/L3-Utils.dsd',
    'UserData/DiscIms/L3FS-ISW.adl',
    'UserData/DiscIms/l3server.adl',
    'UserData/DiscIms/M5000-4.ssd',
    'UserData/DiscIms/MasterWelcome.adl',
    'UserData/DiscIms/Music500.ssd',
    'UserData/DiscIms/sasi0.dat',
    'UserData/DiscIms/scsi0.dat',
    'UserData/DiscIms/scsi0.dsc',
    'UserData/DiscIms/scsi1.dat',
    'UserData/DiscIms/scsi1.dsc',
    'UserData/DiscIms/scsi2.dat',
    'UserData/DiscIms/scsi2.dsc',
    'UserData/DiscIms/scsi3.dat',
    'UserData/DiscIms/scsi3.dsc',
    'UserData/DiscIms/Test.ssd',
    'UserData/DiscIms/Torch_hard_disc_utils.dsd',
    'UserData/DiscIms/Torch_standard_utilities_2.dsd',
    'UserData/DiscIms/Welcome.ssd',
    'UserData/Econet.cfg',
    'UserData/Logical.kmap',
    'UserData/Phroms/PHROMA',
    'UserData/Phroms/phromsus.txt',
    'UserData/Phroms/PHROMUS',
    'UserData/Phroms/VM61002.BIN',
    'UserData/Phroms/VM61003.BIN',
    'UserData/Phroms/VM61004.BIN',
    'UserData/Phroms/VM61005.BIN',
    'UserData/Phroms/VM71003A.BIN',
    'UserData/Phroms.cfg',
    'UserData/Preferences.cfg',
    'UserData/Roms.cfg',
    'UserData/Roms_Torch.cfg',
    'UserData/Tapes/Test.uef',
    'UserData/USLogical.kmap',
    'Help/65c02copro.html',
    'Help/accessibility.html',
    'Help/armcopro.html',
    'Help/bbcmicro.png',
    'Help/beeb.gif',
    'Help/commandline.html',
    'Help/contents.html',
    'Help/debugger.html',
    'Help/disks.html',
    'Help/econet.html',
    'Help/emulation.html',
    'Help/faq.html',
    'Help/gettingstarted.html',
    'Help/harddisks.html',
    'Help/index.html',
    'Help/keyboard.html',
    'Help/keyboard.jpg',
    'Help/m5000.html',
    'Help/mastercopro.html',
    'Help/menus.html',
    'Help/roms.html',
    'Help/rtc.html',
    'Help/serial.html',
    'Help/sources.html',
    'Help/speech.html',
    'Help/sprowcopro.html',
    'Help/startup.jpg',
    'Help/state.html',
    'Help/tapes.html',
    'Help/teletext.html',
    'Help/troubleshooting.html',
    'Help/uninstall.html',
    'Help/z80copro.html',
    'Help/z80torch.html',
    { Src => 'Src/Release/Hardware/Watford.dll', Dest => 'Hardware/Watford.dll' },
    { Src => 'Src/Release/Hardware/OpusDDOS.dll', Dest => 'Hardware/OpusDDOS.dll' },
    { Src => 'Src/Release/Hardware/Acorn1770.dll', Dest => 'Hardware/Acorn1770.dll' },
    'Documents/KeyMapInfo.txt',
    'Documents/Integra-B.zip',
    'Documents/Ibos.txt',
    'Documents/Econet.zip',
    'Teletext.fnt',
    'RelayOn.snd',
    'RelayOff.snd',
    'README.md',
    'HeadUnload.snd',
    'HeadStep.snd',
    'HeadSeek.snd',
    'HeadLoad.snd',
    'DriveMotor.snd',
    'COPYRIGHT.txt',
    'CHANGES.md',
    'BeebEmTorch.vbs',
    'BeebEmLocal.vbs',
    { Src => 'Src/Release/BeebEm.exe', Dest => 'BeebEm.exe' }
);

my $zip = Archive::Zip->new();

foreach my $file (@files)
{
    my ($source_file, $target_file);

    if (ref $file eq ref {})
    {
        $source_file = "../../" . $file->{ Src };
        $target_file = "BeebEm/" . $file->{ Dest };
    }
    else
    {
        $source_file = "../../$file";
        $target_file = "BeebEm/$file";
    }

    print $source_file, " -> ", $target_file, "\n";

    my $zip_entry = $zip->addFile($source_file, $target_file);
    $zip_entry->desiredCompressionLevel(9);
}

my $zip_filename = "Release/BeebEm.zip";

if ($zip->writeToFileNamed($zip_filename) != AZ_OK)
{
    die "Failed to write zip_filename\n";
}

print "Created $zip_filename\n";
