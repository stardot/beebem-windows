/* SASI Support for Beebem */
/* Written by Jon Welch */

#ifndef SASI_HEADER
#define SASI_HEADER

void SASIReset(void);
void SASIWrite(int Address, int Value) ;
int SASIRead(int Address);
int SASIReadData(void);
void SASIWriteData(int data);
void SASIBusFree(void);
void SASIMessage(void);
void SASISelection(int data);
void SASICommand(void);
void SASIExecute(void);
void SASIStatus(void);
void SASITestUnitReady(void);
void SASIRequestSense(void);
int SASIDiscRequestSense(unsigned char *cdb, unsigned char *buf);
void SASIRead(void);
void SASIWrite(void);
int SASIReadSector(unsigned char *buf, int block);
bool SASIWriteSector(unsigned char *buf, int block);
void SASIStartStop(void);
bool SASIDiscFormat(unsigned char *buf);
void SASIFormat(void);
void SASIVerify(void);
void SASITranslate(void);
void SASIRezero(void);
void SASIRamDiagnostics(void);
void SASIControllerDiagnostics(void);
void SASISetGeometory(void);
void SASISeek(void);
bool SASIWriteGeometory(unsigned char *buf);
#endif
