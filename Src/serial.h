// Serial/Cassette Support for BeebEm
// Written by Richard Gellman

#ifndef SERIAL_HEADER
#define SERIAL_HEADER
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
#endif