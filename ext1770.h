// Opus DDFS Board Drive Controller Chip DLL
// (C) September 2001 - Richard Gellman

struct DriveControlBlock {
	int FDCAddress; // 1770 FDC chip address
	int DCAddress; // Drive Control Register Address
	char *BoardName; // FDC Board name
	bool TR00_ActiveHigh; // Set TRUE if the TR00 input is Active High
};

#define EDCB struct DriveControlBlock
