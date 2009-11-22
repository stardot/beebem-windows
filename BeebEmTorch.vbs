Dim WshShell
Set WshShell = CreateObject("WScript.Shell")
WshShell.Exec("BeebEm.exe -Roms Roms_Torch.cfg")
