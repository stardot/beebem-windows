/* IDE Support for Beebem       */
/* Written by Jon Welch         */
/* Integrated into v4.xx by JGH */

#ifndef IDE_HEADER
#define IDE_HEADER
extern char IDEDriveEnabled;
void IDEWrite(int Address, int Value);
int IDERead(int Address);
void IDEReset(void);
void DoIDESeek(void);
void DoIDEGeometry(void);
#endif