// Opus DDFS Board Drive Controller Chip DLL
// (C) September 2001 - Richard Gellman

#ifdef __cplusplus
#define EXPORT extern "C" __declspec (dllexport)
#else
#define EXPORT __declspec (dllexport)
#endif

#define WIN_LEAN_AND_MEAN

struct DriveControlBlock {
	int FDCAddress; // 1770 FDC chip address
	int DCAddress; // Drive Control Register Address
	char *BoardName;
	bool TR00_ActiveHigh;
};

EXPORT unsigned char SetDriveControl(unsigned char value);
EXPORT unsigned char GetDriveControl(unsigned char value);
EXPORT void GetBoardProperties(struct DriveControlBlock *FDBoard);

#define DCB struct DriveControlBlock
      