/* IDE Support for Beebem */
/* Written by Jon Welch */

#ifndef IDE_HEADER
#define IDE_HEADER
void IDEWrite(int Address, int Value);
int IDERead(int Address);
void IDEReset(void);
void DoIDESeek(void);
#endif