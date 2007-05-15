/* Teletext Support for Beebem */

extern char TeleTextAdapterEnabled;

void TeleTextWrite(int Address, int Value);
int TeleTextRead(int Address);
void TeleTextPoll(void);
void TeleTextLog(char *text, ...);
void TeleTextInit(void);
