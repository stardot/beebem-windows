// Serial/Cassette Support for BeebEm
// Written by Richard Gellman

#ifndef SERIAL_HEADER
#define SERIAL_HEADER

#define TAPECYCLES 357 // 2000000/5600 - 5600 is normal tape speed

#define MAX_MAP_LINES 4096
extern int map_lines;
extern char map_desc[MAX_MAP_LINES][40];
extern int map_time[MAX_MAP_LINES];
extern unsigned char Clk_Divide;

extern CycleCountT TapeTrigger;
void Write_ACIA_Control(unsigned char CReg);
void Write_ACIA_Tx_Data(unsigned char Data);
void Write_SERPROC(unsigned char Data);
unsigned char Read_ACIA_Status(void);
unsigned char Read_ACIA_Rx_Data(void);
unsigned char Read_SERPROC(void);
extern unsigned char SerialPortEnabled;
extern unsigned char SerialPort;
void Serial_Poll(void);
void InitSerialPort(void);
void Kill_Serial(void);
void LoadUEF(char *UEFName);
void RewindTape(void);
extern int TapeClockSpeed;
extern void SerialThread(void *lpParam);
void StatThread(void *lpParam);
void InitThreads(void);
extern volatile bool bSerialStateChanged;
extern bool TapeControlEnabled;
extern char UEFTapeName[256];
extern int UnlockTape;
extern unsigned char TxD,RxD;
extern int TapeClock,OldClock;
extern int TapeClockSpeed;

void SetTapeSpeed(int speed);
void SetUnlockTape(int unlock);
void TapeControlOpenDialog(HINSTANCE hinst, HWND hwndMain);
void TapeControlOpenFile(char *UEFName);
void TapeControlCloseDialog(void);
void SaveSerialUEF(FILE *SUEF);
void LoadSerialUEF(FILE *SUEF);
void SetACIAStatus(unsigned char bit);
void ResetACIAStatus(unsigned char bit);
#endif
