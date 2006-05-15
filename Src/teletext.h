/* Teletext Support for Beebem */

void TeleTextWrite(int Address, int Value);
int TeleTextRead(int Address);
void TeleTextPoll(void);
void TeleTextLog(char *text, ...);
void TeleTextInit(void);
