/* !770 FDC Support for Beebem */
/* Written by Richard Gellman */

#ifndef DISC1770_HEADER
#define DISC1770_HEADER
extern unsigned char DWriteable[2]; // Write Protect
unsigned char Read1770Register(unsigned char Register1770);
void Write1770Register(unsigned char Register1770, unsigned char Value);
void Load1770DiscImage(char *DscFileName,int DscDrive,unsigned char DscType,HMENU dmenu);
void WriteFDCControlReg(unsigned char Value);
unsigned char ReadFDCControlReg(void);
void Reset1770(void);
void Kill1770(void);
void Poll1770(int NCycles);
void CreateADFSImage(char *ImageName,unsigned char Drive,unsigned char Tracks, HMENU dmenu);
void Close1770Disc(char Drive);
extern bool InvertTR00;
#endif