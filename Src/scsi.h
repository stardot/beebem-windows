/* SASI Support for Beebem */
/* Written by Jon Welch */

#ifndef SCSI_HEADER
#define SCSI_HEADER

void SCSIReset(void);
void SCSIWrite(int Address, int Value) ;
int SCSIRead(int Address);
int ReadData(void);
void WriteData(int data);
void BusFree(void);
void Message(void);
void Selection(int data);
void Command(void);
void Execute(void);
void Status(void);
void TestUnitReady(void);
void RequestSense(void);
int DiscRequestSense(unsigned char *cdb, unsigned char *buf);
void Read6(void);
void Write6(void);
int ReadSector(unsigned char *buf, int block);
bool WriteSector(unsigned char *buf, int block);
void StartStop(void);
void ModeSense(void);
int DiscModeSense(unsigned char *cdb, unsigned char *buf);
void ModeSelect(void);
bool WriteGeometory(unsigned char *buf);
bool DiscFormat(unsigned char *buf);
void Format(void);
bool DiscVerify(unsigned char *buf);
void Verify(void);
void Translate(void);
#endif
