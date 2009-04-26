/*
 *  serialdevices.h
 *  BeebEm3
 *
 *  Created by Jon Welch on 28/08/2006.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

extern unsigned char TouchScreenEnabled;
void TouchScreenOpen(void);
bool TouchScreenPoll(void);
void TouchScreenClose(void);
void TouchScreenWrite(unsigned char data);
unsigned char TouchScreenRead(void);
void TouchScreenStore(unsigned char data);
void TouchScreenReadScreen(bool check);



extern unsigned char EthernetPortEnabled;
extern unsigned char IP232localhost;
extern unsigned char IP232custom;
extern unsigned char IP232mode;
extern unsigned int IP232customport;
extern char IP232customip [20];

bool IP232Open(void);
bool IP232Poll(void);
void IP232Close(void);
void IP232Write(unsigned char data);
unsigned char IP232Read(void);
extern char IPAddress[256];
extern int PortNo;

extern DWORD mEthernetPortReadTaskID;
void MyEthernetPortReadThread(void *parameter);
extern DWORD mEthernetPortStatusTaskID;
void MyEthernetPortStatusThread(void *parameter);
unsigned char EthernetPortGet(void);
void EthernetPortStore(unsigned char data);
extern SOCKET mEthernetHandle;
